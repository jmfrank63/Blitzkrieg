#include "StdAfx.h"

#include "OpenVideoPlayer.h"
#include "../Platform/Clock.h"
#include "../GFX/GFXHelper.h"
#include "../SFX/SFX.h"

#include <ogg/ogg.h>
#include <theora/theoradec.h>

struct SOpenVideoDecoderState
{
	ogg_sync_state oy;
	ogg_stream_state vo;
	th_info ti;
	th_comment tc;
	th_setup_info *pSetup;
	th_dec_ctx *pDecoder;
	bool bSyncInitialized;
	bool bStreamInitialized;

	SOpenVideoDecoderState() : pSetup( 0 ), pDecoder( 0 ), bSyncInitialized( false ), bStreamInitialized( false )
	{
	}
};

COpenVideoPlayer::COpenVideoPlayer()
{
	rcDstRect.SetEmpty();
	bMaintainAspect = true;
	bLooped = false;
	nShadingEffectStart = 17;
	nShadingEffectFinish = 18;
	dwPlayFlags = 0;
	bHasMovieInfo = false;
	vMovieSize.Set( 0, 0 );
	nFrameRateNumerator = 0;
	nFrameRateDenominator = 0;
	nGranuleShift = 0;
	nMovieLength = 0;
	nNumFrames = 0;
	dwStartTime = 0;
	bPlaying = false;
	bPaused = false;
	pDecoderState = 0;
	nDecodedFrame = -1;
	bAudioStreamPlaying = false;
}

COpenVideoPlayer::~COpenVideoPlayer()
{
	DestroyDecoder();
}

namespace
{
	IDataStream* OpenVideoStream( const char *pszFileName )
	{
		IDataStream *pStream = GetSingleton<IDataStorage>()->OpenStream( pszFileName, STREAM_ACCESS_READ );
		if ( pStream != 0 )
			return pStream;
		return OpenFileStream( pszFileName, STREAM_ACCESS_READ );
	}

	int ReadBE16( const unsigned char *pData )
	{
		return (int(pData[0]) << 8) | int(pData[1]);
	}

	int ReadBE24( const unsigned char *pData )
	{
		return (int(pData[0]) << 16) | (int(pData[1]) << 8) | int(pData[2]);
	}

	// The top byte must shift as unsigned: a value >= 0x80 shifted into the
	// sign bit of an int is UB.
	int ReadBE32( const unsigned char *pData )
	{
		return int( (unsigned(pData[0]) << 24) | (unsigned(pData[1]) << 16) | (unsigned(pData[2]) << 8) | unsigned(pData[3]) );
	}

	int ReadLE32( const unsigned char *pData )
	{
		return int( unsigned(pData[0]) | (unsigned(pData[1]) << 8) | (unsigned(pData[2]) << 16) | (unsigned(pData[3]) << 24) );
	}

	long long ReadLE64( const unsigned char *pData )
	{
		unsigned long long nValue = 0;
		for ( int i = 7; i >= 0; --i )
			nValue = (nValue << 8) | pData[i];
		return static_cast<long long>( nValue );
	}

	bool ParseTheoraIdentificationHeader( const unsigned char *pPacket, const int nPacketSize, CVec2 *pMovieSize, int *pnFPSNumerator, int *pnFPSDenominator, int *pnGranuleShift )
	{
		if ( (pPacket == 0) || (nPacketSize < 42) || (pMovieSize == 0) )
			return false;
		if ( (pPacket[0] != 0x80) || (memcmp(pPacket + 1, "theora", 6) != 0) )
			return false;
		const int nPictureWidth = ReadBE24( pPacket + 14 );
		const int nPictureHeight = ReadBE24( pPacket + 17 );
		const int nFrameWidth = ReadBE16( pPacket + 10 ) * 16;
		const int nFrameHeight = ReadBE16( pPacket + 12 ) * 16;
		pMovieSize->x = nPictureWidth > 0 ? nPictureWidth : nFrameWidth;
		pMovieSize->y = nPictureHeight > 0 ? nPictureHeight : nFrameHeight;
		if ( pnFPSNumerator != 0 )
			*pnFPSNumerator = ReadBE32( pPacket + 22 );
		if ( pnFPSDenominator != 0 )
			*pnFPSDenominator = ReadBE32( pPacket + 26 );
		if ( pnGranuleShift != 0 )
			*pnGranuleShift = ((int(pPacket[40]) & 0x03) << 3) | ((int(pPacket[41]) & 0xe0) >> 5);
		return (pMovieSize->x > 0) && (pMovieSize->y > 0);
	}

	bool FindTheoraIdentificationHeader( const unsigned char *pData, const int nSize, CVec2 *pMovieSize, int *pnFPSNumerator, int *pnFPSDenominator, int *pnGranuleShift )
	{
		if ( (pData == 0) || (nSize < 42) )
			return false;
		const int nLastStart = nSize - 42;
		for ( int i = 0; i <= nLastStart; ++i )
		{
			if ( ParseTheoraIdentificationHeader(pData + i, nSize - i, pMovieSize, pnFPSNumerator, pnFPSDenominator, pnGranuleShift) )
				return true;
		}
		return false;
	}

	int DecodeTheoraGranuleFrame( const long long nGranulePosition, const int nGranuleShift )
	{
		if ( nGranulePosition < 0 )
			return 0;
		if ( nGranuleShift <= 0 )
			return int(nGranulePosition);
		const long long nPFrameMask = ((long long)1 << nGranuleShift) - 1;
		return int((nGranulePosition >> nGranuleShift) + (nGranulePosition & nPFrameMask));
	}

	bool FindLastOggGranulePosition( const std::vector<char> &data, CVec2 *pMovieSize, int *pnFPSNumerator, int *pnFPSDenominator, int *pnGranuleShift, int *pnNumFrames )
	{
		if ( data.size() < 27 )
			return false;
		const unsigned char *pData = reinterpret_cast<const unsigned char*>( &(data[0]) );
		int nOffset = 0;
		int nTheoraSerial = 0;
		bool bHasTheoraSerial = false;
		long long nLastGranulePosition = -1;
		while ( nOffset + 27 <= int(data.size()) )
		{
			if ( memcmp(pData + nOffset, "OggS", 4) != 0 )
			{
				++nOffset;
				continue;
			}
			const int nSegments = pData[nOffset + 26];
			if ( nOffset + 27 + nSegments > int(data.size()) )
				break;
			int nPayloadSize = 0;
			for ( int i = 0; i < nSegments; ++i )
				nPayloadSize += pData[nOffset + 27 + i];
			const int nPayloadOffset = nOffset + 27 + nSegments;
			if ( nPayloadOffset + nPayloadSize > int(data.size()) )
				break;

			const int nSerial = ReadLE32( pData + nOffset + 14 );
			if ( !bHasTheoraSerial && FindTheoraIdentificationHeader(pData + nPayloadOffset, nPayloadSize, pMovieSize, pnFPSNumerator, pnFPSDenominator, pnGranuleShift) )
			{
				nTheoraSerial = nSerial;
				bHasTheoraSerial = true;
			}
			if ( bHasTheoraSerial && (nSerial == nTheoraSerial) )
			{
				const long long nGranulePosition = ReadLE64( pData + nOffset + 6 );
				if ( nGranulePosition >= 0 )
					nLastGranulePosition = nGranulePosition;
			}
			nOffset = nPayloadOffset + nPayloadSize;
		}
		if ( !bHasTheoraSerial )
			return false;
		if ( pnNumFrames != 0 )
			*pnNumFrames = DecodeTheoraGranuleFrame( nLastGranulePosition, pnGranuleShift == 0 ? 0 : *pnGranuleShift );
		return true;
	}

	int ClampByte( const int nValue )
	{
		if ( nValue < 0 )
			return 0;
		if ( nValue > 255 )
			return 255;
		return nValue;
	}

	DWORD YCbCrToARGB( const int y, const int cb, const int cr )
	{
		const int c = y - 16;
		const int d = cb - 128;
		const int e = cr - 128;
		const int r = ClampByte( (298 * c + 409 * e + 128) >> 8 );
		const int g = ClampByte( (298 * c - 100 * d - 208 * e + 128) >> 8 );
		const int b = ClampByte( (298 * c + 516 * d + 128) >> 8 );
		return 0xff000000 | (r << 16) | (g << 8) | b;
	}

	bool LoadOpenVideoBytes( const char *pszFileName, std::vector<char> *pData )
	{
		if ( pData == 0 )
			return false;
		CPtr<IDataStream> pStream = OpenVideoStream( pszFileName );
		if ( (pStream == 0) || (pStream->GetSize() == 0) )
			return false;
		pData->resize( pStream->GetSize() );
		pStream->Read( &((*pData)[0]), pData->size() );
		return true;
	}

	bool OpenVideoStreamExists( const std::string &szFileName )
	{
		CPtr<IDataStream> pStream = OpenVideoStream( szFileName.c_str() );
		return pStream != 0 && pStream->GetSize() > 0;
	}

	std::string RemoveOpenVideoExtension( const std::string &szFileName )
	{
		const std::string::size_type nSlash = szFileName.find_last_of( "\\/" );
		const std::string::size_type nDot = szFileName.find_last_of( "." );
		if ( (nDot == std::string::npos) || ((nSlash != std::string::npos) && (nDot < nSlash)) )
			return szFileName;
		return szFileName.substr( 0, nDot );
	}

	std::string NormalizeOpenVideoStreamNameForSFX( const std::string &szStreamName )
	{
		std::string szResult = szStreamName;
		const char *pszStorageName = GetSingleton<IDataStorage>()->GetName();
		if ( pszStorageName != 0 )
		{
			const std::string szStorageName = pszStorageName;
			if ( szResult.size() > szStorageName.size() &&
			     _strnicmp(szResult.c_str(), szStorageName.c_str(), szStorageName.size()) == 0 )
			{
				szResult = szResult.substr( szStorageName.size() );
			}
		}
		while ( !szResult.empty() && (szResult[0] == '\\' || szResult[0] == '/') )
			szResult.erase( szResult.begin() );
		return szResult;
	}
}

void COpenVideoPlayer::SetupRects()
{
	if ( rcDstRect.IsEmpty() || images.empty() || (vMovieSize.x <= 0) || (vMovieSize.y <= 0) )
		return;
	CTRect<float> rcRender = rcDstRect;
	if ( bMaintainAspect )
	{
		const float fCoeffX = rcDstRect.Width() / float(vMovieSize.x);
		const float fCoeffY = rcDstRect.Height() / float(vMovieSize.y);
		if ( (fCoeffX < fCoeffY) && (fabsf(fCoeffX - fCoeffY) > 0.001f) )
		{
			const float fNewSizeY = vMovieSize.y * fCoeffX;
			rcRender.y1 = rcDstRect.y1 + ( rcDstRect.Height() - fNewSizeY ) / 2.0f;
			rcRender.y2 = rcRender.y1 + fNewSizeY;
		}
		else if ( (fCoeffY < fCoeffX) && (fabsf(fCoeffY - fCoeffX) > 0.001f) )
		{
			const float fNewSizeX = vMovieSize.x * fCoeffY;
			rcRender.x1 = rcDstRect.x1 + ( rcDstRect.Width() - fNewSizeX ) / 2.0f;
			rcRender.x2 = rcRender.x1 + fNewSizeX;
		}
	}
	const float fCoeffX = rcRender.Width() / float(vMovieSize.x);
	const float fCoeffY = rcRender.Height() / float(vMovieSize.y);
	for ( COpenVideoImagesList::iterator it = images.begin(); it != images.end(); ++it )
	{
		it->rcRect.x1 = int( rcRender.x1 + it->rcSrcRect.x1*fCoeffX ) - 0.5f;
		it->rcRect.y1 = int( rcRender.y1 + it->rcSrcRect.y1*fCoeffY ) - 0.5f;
		it->rcRect.x2 = int( rcRender.x1 + it->rcSrcRect.x2*fCoeffX ) - 0.5f;
		it->rcRect.y2 = int( rcRender.y1 + it->rcSrcRect.y2*fCoeffY ) - 0.5f;
	}
}

bool COpenVideoPlayer::ProbeOpenVideo( const char *pszFileName )
{
	bHasMovieInfo = false;
	vMovieSize.Set( 0, 0 );
	nFrameRateNumerator = 0;
	nFrameRateDenominator = 0;
	nGranuleShift = 0;
	nMovieLength = 0;
	nNumFrames = 0;
	CPtr<IDataStream> pStream = OpenVideoStream( pszFileName );
	if ( (pStream == 0) || (pStream->GetSize() == 0) )
	{
		NStr::DebugTrace( "Open video probe failed: \"%s\" could not be opened.\n", pszFileName );
		return false;
	}
	std::vector<char> data( pStream->GetSize() );
	pStream->Read( &(data[0]), data.size() );
	if ( FindLastOggGranulePosition(data, &vMovieSize, &nFrameRateNumerator, &nFrameRateDenominator, &nGranuleShift, &nNumFrames) )
	{
		bHasMovieInfo = true;
		if ( (nNumFrames > 0) && (nFrameRateNumerator > 0) && (nFrameRateDenominator > 0) )
			nMovieLength = 1000 * nNumFrames * nFrameRateDenominator / nFrameRateNumerator;
		NStr::DebugTrace( "Open video probe: \"%s\" Theora %dx%d fps=%d/%d frames=%d length=%d.\n", pszFileName, int(vMovieSize.x), int(vMovieSize.y), nFrameRateNumerator, nFrameRateDenominator, nNumFrames, nMovieLength );
		return true;
	}
	NStr::DebugTrace( "Open video probe failed: \"%s\" has no Theora identification header.\n", pszFileName );
	return false;
}

bool COpenVideoPlayer::OpenDecoder( const char *pszFileName, IGFX *pGFX )
{
	if ( pGFX == 0 )
		return false;
	DestroyDecoder();
	pRenderGFX = pGFX;
	nDecodedFrame = -1;
	std::vector<char> data;
	if ( !LoadOpenVideoBytes(pszFileName, &data) )
		return false;

	ogg_page og;
	ogg_packet op;
	pDecoderState = new SOpenVideoDecoderState;
	th_info_init( &pDecoderState->ti );
	th_comment_init( &pDecoderState->tc );
	ogg_sync_init( &pDecoderState->oy );
	pDecoderState->bSyncInitialized = true;
	char *pBuffer = ogg_sync_buffer( &pDecoderState->oy, data.size() );
	memcpy( pBuffer, &(data[0]), data.size() );
	ogg_sync_wrote( &pDecoderState->oy, data.size() );

	while ( pDecoderState->pDecoder == 0 && ogg_sync_pageout(&pDecoderState->oy, &og) == 1 )
	{
		if ( !ogg_page_bos(&og) )
			continue;
		ogg_stream_state test;
		ogg_stream_init( &test, ogg_page_serialno(&og) );
		ogg_stream_pagein( &test, &og );
		if ( ogg_stream_packetout(&test, &op) == 1 && th_decode_headerin(&pDecoderState->ti, &pDecoderState->tc, &pDecoderState->pSetup, &op) >= 0 )
		{
			pDecoderState->vo = test;
			pDecoderState->bStreamInitialized = true;
			int nHeaders = 1;
			while ( nHeaders < 3 && ogg_sync_pageout(&pDecoderState->oy, &og) == 1 )
			{
				if ( ogg_page_serialno(&og) != pDecoderState->vo.serialno )
					continue;
				ogg_stream_pagein( &pDecoderState->vo, &og );
				while ( nHeaders < 3 && ogg_stream_packetout(&pDecoderState->vo, &op) == 1 )
				{
					const int nHeaderResult = th_decode_headerin( &pDecoderState->ti, &pDecoderState->tc, &pDecoderState->pSetup, &op );
					if ( nHeaderResult < 0 )
						break;
					++nHeaders;
				}
			}
			if ( nHeaders >= 3 )
				pDecoderState->pDecoder = th_decode_alloc( &pDecoderState->ti, pDecoderState->pSetup );
			break;
		}
		ogg_stream_clear( &test );
	}
	if ( pDecoderState->pDecoder != 0 )
		return true;
	DestroyDecoder();
	return false;
}

bool COpenVideoPlayer::DecodeNextFrame( bool bConvertFrame )
{
	if ( (pDecoderState == 0) || (pDecoderState->pDecoder == 0) || (pRenderGFX == 0) )
		return false;
	ogg_page og;
	ogg_packet op;
	while ( true )
	{
		while ( ogg_stream_packetout(&pDecoderState->vo, &op) == 1 )
		{
			if ( th_decode_packetin(pDecoderState->pDecoder, &op, 0) == 0 )
			{
				if ( !bConvertFrame )
				{
					// seek support: the decoder must consume every packet to keep its
					// reference frames valid, but the YCbCr->ARGB conversion and the
					// texture uploads of intermediate frames are pure waste
					++nDecodedFrame;
					return true;
				}
				th_ycbcr_buffer ycbcr;
				if ( th_decode_ycbcr_out(pDecoderState->pDecoder, ycbcr) == 0 )
				{
					const th_info &ti = pDecoderState->ti;
					const int nWidth = ti.pic_width > 0 ? ti.pic_width : ti.frame_width;
					const int nHeight = ti.pic_height > 0 ? ti.pic_height : ti.frame_height;
					const int hdec = !(ti.pixel_fmt & 1);
					const int vdec = !(ti.pixel_fmt & 2);
					const int nLumaBaseX = ti.pic_x & ~hdec;
					const int nLumaBaseY = ti.pic_y & ~vdec;
					const int nChromaBaseX = ti.pic_x >> hdec;
					const int nChromaBaseY = ti.pic_y >> vdec;
					vMovieSize.Set( nWidth, nHeight );
					if ( images.empty() )
					{
						const bool bHasNonPow2Textures = (GetGlobalVar( "GFX.Caps.Texture.NonPow2Conditional", 0 ) != 0) || (GetGlobalVar( "GFX.Caps.Texture.NonPow2", 0 ) != 0);
						if ( bHasNonPow2Textures )
						{
							SOpenVideoImagePart image;
							image.pTexture = pRenderGFX->CreateTexture( nWidth, nHeight, 1, GFXPF_ARGB8888, GFXD_STATIC );
							image.rcSrcRect.Set( 0, 0, nWidth, nHeight );
							image.rcDstRect.Set( 0, 0, nWidth, nHeight );
							image.rcMaps.Set( 0, 0, 1, 1 );
							images.push_back( image );
						}
						else
						{
							const int nNumTexturesX = fmod( nWidth, 256 ) == 0 ? nWidth / 256 : nWidth / 256 + 1;
							const int nNumTexturesY = fmod( nHeight, 256 ) == 0 ? nHeight / 256 : nHeight / 256 + 1;
							const bool bSquareOnly = GetGlobalVar( "GFX.Caps.Texture.SquareOnly", 0 ) != 0;
							int nRestSizeY = nHeight;
							int nPosY = 0;
							for ( int i = 0; i < nNumTexturesY; ++i )
							{
								const int nSrcSizeY = nRestSizeY >= 256 ? 256 : nRestSizeY;
								int nRestSizeX = nWidth;
								int nPosX = 0;
								for ( int j = 0; j < nNumTexturesX; ++j )
								{
									const int nSrcSizeX = nRestSizeX >= 256 ? 256 : nRestSizeX;
									int nTextureSizeX = nRestSizeX < 256 ? GetNextPow2( nRestSizeX ) : 256;
									int nTextureSizeY = nRestSizeY < 256 ? GetNextPow2( nRestSizeY ) : 256;
									if ( bSquareOnly )
										nTextureSizeX = nTextureSizeY = Max( nTextureSizeX, nTextureSizeY );
									SOpenVideoImagePart image;
									image.pTexture = pRenderGFX->CreateTexture( nTextureSizeX, nTextureSizeY, 1, GFXPF_ARGB8888, GFXD_STATIC );
									image.rcSrcRect.Set( nPosX, nPosY, nPosX + nSrcSizeX, nPosY + nSrcSizeY );
									image.rcDstRect.Set( 0, 0, nSrcSizeX, nSrcSizeY );
									image.rcMaps.Set( 0, 0, float(nSrcSizeX) / float(nTextureSizeX), float(nSrcSizeY) / float(nTextureSizeY) );
									images.push_back( image );
									nRestSizeX -= 256;
									nPosX += 256;
								}
								nRestSizeY -= 256;
								nPosY += 256;
							}
						}
					}
					for ( COpenVideoImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
					{
						SSurfaceLockInfo lock = { 0, 0 };
						if ( it->pTexture == 0 || !it->pTexture->Lock(0, &lock) || lock.pData == 0 )
							continue;
						for ( int y = 0; y < it->rcDstRect.Height(); ++y )
						{
							DWORD *pDst = reinterpret_cast<DWORD*>( reinterpret_cast<char*>(lock.pData) + y * lock.nPitch );
							const int nSourceY = it->rcSrcRect.y1 + y;
							const int nLumaY = nLumaBaseY + nSourceY;
							const int nChromaY = nChromaBaseY + (nSourceY >> vdec);
							for ( int x = 0; x < it->rcDstRect.Width(); ++x )
							{
								const int nSourceX = it->rcSrcRect.x1 + x;
								const int nLumaX = nLumaBaseX + nSourceX;
								const int nChromaX = nChromaBaseX + (nSourceX >> hdec);
								const int yv = ycbcr[0].data[nLumaY * ycbcr[0].stride + nLumaX];
								const int cb = ycbcr[1].data[nChromaY * ycbcr[1].stride + nChromaX];
								const int cr = ycbcr[2].data[nChromaY * ycbcr[2].stride + nChromaX];
								pDst[x] = YCbCrToARGB( yv, cb, cr );
							}
						}
							it->pTexture->Unlock( 0 );
						it->pTexture->AddDirtyRect( 0 );
					}
					if ( rcDstRect.IsEmpty() )
						rcDstRect.Set( 0, 0, nWidth, nHeight );
					SetupRects();
					++nDecodedFrame;
					return true;
				}
			}
		}
		if ( ogg_sync_pageout(&pDecoderState->oy, &og) != 1 )
			return false;
		if ( ogg_page_serialno(&og) != pDecoderState->vo.serialno )
			continue;
		ogg_stream_pagein( &pDecoderState->vo, &og );
	}
}

bool COpenVideoPlayer::DecodeFirstFrame( const char *pszFileName, IGFX *pGFX )
{
	if ( !OpenDecoder(pszFileName, pGFX) )
		return false;
	const bool bFrameDecoded = DecodeNextFrame();
	if ( !bFrameDecoded )
		DestroyDecoder();
	NStr::DebugTrace( "Open video first frame %s: \"%s\".\n", bFrameDecoded ? "decoded" : "failed", pszFileName );
	return bFrameDecoded;
}

void COpenVideoPlayer::DestroyDecoder()
{
	if ( pDecoderState == 0 )
		return;
	if ( pDecoderState->pDecoder != 0 )
		th_decode_free( pDecoderState->pDecoder );
	if ( pDecoderState->pSetup != 0 )
		th_setup_free( pDecoderState->pSetup );
	if ( pDecoderState->bStreamInitialized )
		ogg_stream_clear( &pDecoderState->vo );
	if ( pDecoderState->bSyncInitialized )
		ogg_sync_clear( &pDecoderState->oy );
	th_comment_clear( &pDecoderState->tc );
	th_info_clear( &pDecoderState->ti );
	delete pDecoderState;
	pDecoderState = 0;
	nDecodedFrame = -1;
}

bool COpenVideoPlayer::FindOpenVideoAudioStreamName( const char *pszFileName, std::string *pAudioStreamName ) const
{
	if ( (pszFileName == 0) || (pAudioStreamName == 0) )
		return false;
	const std::string szAudioBaseName = RemoveOpenVideoExtension( pszFileName ) + ".audio";
	if ( !OpenVideoStreamExists(szAudioBaseName + ".ogg") )
		return false;
	*pAudioStreamName = NormalizeOpenVideoStreamNameForSFX( szAudioBaseName );
	return !pAudioStreamName->empty();
}

void COpenVideoPlayer::PlayVideoAudioStream( ISFX *pSFX )
{
	StopVideoAudioStream();
	pVideoSFX = pSFX;
	szAudioStreamName.clear();
	if ( (pVideoSFX == 0) || !FindOpenVideoAudioStreamName(szFileName.c_str(), &szAudioStreamName) )
		return;
	pVideoSFX->PlayVideoStream( szAudioStreamName.c_str(), bLooped );
	bAudioStreamPlaying = true;
	NStr::DebugTrace( "Open video audio stream started: \"%s\".\n", szAudioStreamName.c_str() );
}

void COpenVideoPlayer::StopVideoAudioStream()
{
	if ( bAudioStreamPlaying && pVideoSFX != 0 )
		pVideoSFX->StopStream( 0 );
	bAudioStreamPlaying = false;
	szAudioStreamName.clear();
	pVideoSFX = 0;
}

void COpenVideoPlayer::SetTarget( IGFXTexture *pTexture, IGFX *pGFX )
{
	SOpenVideoImagePart image;
	image.pTexture = pTexture;
	images.clear();
	images.push_back( image );
}

void COpenVideoPlayer::SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect )
{
	rcDstRect = _rcDstRect;
	bMaintainAspect = _bMaintainAspect;
	SetupRects();
}

int COpenVideoPlayer::GetCurrentFrame() const
{
	if ( !bPlaying || (nMovieLength <= 0) || (nNumFrames <= 0) )
		return -1;
	const DWORD dwElapsed = NPlatform::MillisecondsElapsed( dwStartTime, NPlatform::MonotonicMilliseconds() );
	const int nFrame = int((long long)dwElapsed * nNumFrames / nMovieLength);
	return nFrame < nNumFrames ? nFrame : nNumFrames;
}

bool COpenVideoPlayer::SetCurrentFrame( const int nFrame )
{
	if ( (nMovieLength <= 0) || (nNumFrames <= 0) || (nFrame < 0) )
		return false;
	const int nClampedFrame = Min( nFrame, nNumFrames );
	dwStartTime = NPlatform::MonotonicMilliseconds() - DWORD((long long)nClampedFrame * nMovieLength / nNumFrames);
	const int nTargetFrame = Min( nClampedFrame, nNumFrames - 1 );
	if ( nDecodedFrame > nTargetFrame )
	{
		if ( !OpenDecoder(szFileName.c_str(), pRenderGFX) || !DecodeNextFrame() )
			return false;
	}
	while ( nDecodedFrame < nTargetFrame )
	{
		if ( !DecodeNextFrame( nDecodedFrame + 1 >= nTargetFrame ) )
			return false;
	}
	return true;
}

void COpenVideoPlayer::SetShadingEffect( const int nEffect, bool bStart )
{
	if ( bStart )
		nShadingEffectStart = nEffect;
	else
		nShadingEffectFinish = nEffect;
}

bool COpenVideoPlayer::Update( const NTimer::STime &time, bool bForcedUpdate )
{
	if ( !bPlaying || bPaused )
		return false;
	int nTargetFrame = GetCurrentFrame();
	if ( !bLooped && (nTargetFrame >= nNumFrames) )
	{
		bPlaying = false;
		StopVideoAudioStream();
		return false;
	}
	if ( bLooped && (nMovieLength > 0) )
	{
		const DWORD dwElapsed = NPlatform::MillisecondsElapsed( dwStartTime, NPlatform::MonotonicMilliseconds() );
		if ( dwElapsed >= DWORD(nMovieLength) )
		{
			dwStartTime = NPlatform::MonotonicMilliseconds() - (dwElapsed % nMovieLength);
			if ( !OpenDecoder(szFileName.c_str(), pRenderGFX) || !DecodeNextFrame() )
			{
				bPlaying = false;
				StopVideoAudioStream();
				return false;
			}
			nTargetFrame = GetCurrentFrame();
		}
	}
	if ( nTargetFrame >= nNumFrames )
		nTargetFrame = nNumFrames - 1;
	if ( nDecodedFrame < nTargetFrame )
		DecodeNextFrame();
	return true;
}

int COpenVideoPlayer::Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, ISFX *pSFX )
{
	szFileName = pszFileName == 0 ? "" : pszFileName;
	dwPlayFlags = dwFlags;
	bPlaying = false;
	bPaused = false;
	StopVideoAudioStream();
	DestroyDecoder();
	if ( !ProbeOpenVideo( szFileName.c_str() ) || (nMovieLength <= 0) )
		return 0;
	if ( !DecodeFirstFrame(szFileName.c_str(), pGFX) )
		return 0;
	dwStartTime = NPlatform::MonotonicMilliseconds();
	bPlaying = true;
	PlayVideoAudioStream( pSFX );
	NStr::DebugTrace( "Open video backend timing \"%s\" length=%d frames=%d.\n", szFileName.c_str(), nMovieLength, nNumFrames );
	return nMovieLength;
}

bool COpenVideoPlayer::Stop()
{
	bPlaying = false;
	bPaused = false;
	StopVideoAudioStream();
	DestroyDecoder();
	return true;
}

bool COpenVideoPlayer::Pause( bool bPause )
{
	bPaused = bPause;
	if ( bAudioStreamPlaying && pVideoSFX != 0 )
		pVideoSFX->PauseStreaming( bPause );
	return true;
}

bool COpenVideoPlayer::IsPlaying() const
{
	if ( !bPlaying )
		return false;
	return bLooped || (GetCurrentFrame() < nNumFrames);
}

int COpenVideoPlayer::GetLength() const
{
	return nMovieLength;
}

int COpenVideoPlayer::GetNumFrames() const
{
	return nNumFrames;
}

bool COpenVideoPlayer::GetMovieSize( CVec2 *pSize ) const
{
	if ( (pSize != 0) && bHasMovieInfo )
	{
		*pSize = vMovieSize;
		return true;
	}
	return false;
}

bool COpenVideoPlayer::Draw( IGFX *pGFX )
{
	if ( !bPlaying || images.empty() )
		return false;
	pGFX->SetShadingEffect( nShadingEffectStart );
	for ( COpenVideoImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
	{
		SGFXRect2 rect;
		rect.rect = it->rcRect;
		rect.maps = it->rcMaps;
		rect.color = 0xffffffff;
		rect.specular = 0xff000000;
		rect.fZ = 0;
		pGFX->SetTexture( 0, it->pTexture );
		pGFX->DrawRects( &rect, 1 );
	}
	pGFX->SetShadingEffect( nShadingEffectFinish );
	return true;
}

void COpenVideoPlayer::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}

int COpenVideoPlayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rcDstRect );
	saver.Add( 2, &bMaintainAspect );
	saver.Add( 3, &bLooped );
	saver.Add( 4, &szFileName );
	saver.Add( 5, &dwPlayFlags );
	saver.Add( 6, &nShadingEffectStart );
	saver.Add( 7, &nShadingEffectFinish );
	saver.Add( 8, &bHasMovieInfo );
	saver.Add( 9, &vMovieSize );
	saver.Add( 10, &nFrameRateNumerator );
	saver.Add( 11, &nFrameRateDenominator );
	saver.Add( 12, &nGranuleShift );
	saver.Add( 13, &nMovieLength );
	saver.Add( 14, &nNumFrames );
	saver.Add( 15, &dwStartTime );
	saver.Add( 16, &bPlaying );
	saver.Add( 17, &bPaused );
	return 0;
}
