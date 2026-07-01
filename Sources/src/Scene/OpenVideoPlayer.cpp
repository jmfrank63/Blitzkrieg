#include "StdAfx.h"

#include "OpenVideoPlayer.h"

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

	int ReadBE32( const unsigned char *pData )
	{
		return (int(pData[0]) << 24) | (int(pData[1]) << 16) | (int(pData[2]) << 8) | int(pData[3]);
	}

	bool ParseTheoraIdentificationHeader( const unsigned char *pPacket, const int nPacketSize, CVec2 *pMovieSize, int *pnFPSNumerator, int *pnFPSDenominator )
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
		return (pMovieSize->x > 0) && (pMovieSize->y > 0);
	}

	bool FindTheoraIdentificationHeader( const std::vector<char> &data, CVec2 *pMovieSize, int *pnFPSNumerator, int *pnFPSDenominator )
	{
		if ( data.size() < 42 )
			return false;
		const unsigned char *pData = reinterpret_cast<const unsigned char*>( &(data[0]) );
		const int nLastStart = data.size() - 42;
		for ( int i = 0; i <= nLastStart; ++i )
		{
			if ( ParseTheoraIdentificationHeader(pData + i, data.size() - i, pMovieSize, pnFPSNumerator, pnFPSDenominator) )
				return true;
		}
		return false;
	}
}

bool COpenVideoPlayer::ProbeOpenVideo( const char *pszFileName )
{
	bHasMovieInfo = false;
	vMovieSize.Set( 0, 0 );
	nFrameRateNumerator = 0;
	nFrameRateDenominator = 0;
	CPtr<IDataStream> pStream = OpenVideoStream( pszFileName );
	if ( (pStream == 0) || (pStream->GetSize() == 0) )
	{
		NStr::DebugTrace( "Open video probe failed: \"%s\" could not be opened.\n", pszFileName );
		return false;
	}
	std::vector<char> data( pStream->GetSize() );
	pStream->Read( &(data[0]), data.size() );
	if ( FindTheoraIdentificationHeader(data, &vMovieSize, &nFrameRateNumerator, &nFrameRateDenominator) )
	{
		bHasMovieInfo = true;
		NStr::DebugTrace( "Open video probe: \"%s\" Theora %dx%d fps=%d/%d.\n", pszFileName, int(vMovieSize.x), int(vMovieSize.y), nFrameRateNumerator, nFrameRateDenominator );
		return true;
	}
	NStr::DebugTrace( "Open video probe failed: \"%s\" has no Theora identification header.\n", pszFileName );
	return false;
}

void COpenVideoPlayer::SetTarget( IGFXTexture *pTexture, IGFX *pGFX )
{
}

void COpenVideoPlayer::SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect )
{
	rcDstRect = _rcDstRect;
	bMaintainAspect = _bMaintainAspect;
}

int COpenVideoPlayer::GetCurrentFrame() const
{
	return -1;
}

bool COpenVideoPlayer::SetCurrentFrame( const int nFrame )
{
	return false;
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
	return false;
}

int COpenVideoPlayer::Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, ISFX *pSFX )
{
	szFileName = pszFileName == 0 ? "" : pszFileName;
	dwPlayFlags = dwFlags;
	ProbeOpenVideo( szFileName.c_str() );
	NStr::DebugTrace( "Open video backend scaffold cannot render \"%s\" yet.\n", szFileName.c_str() );
	return 0; // Rendering is not implemented yet, so the router can fall back to Bink.
}

bool COpenVideoPlayer::Stop()
{
	return true;
}

bool COpenVideoPlayer::Pause( bool bPause )
{
	return false;
}

bool COpenVideoPlayer::IsPlaying() const
{
	return false;
}

int COpenVideoPlayer::GetLength() const
{
	return 0;
}

int COpenVideoPlayer::GetNumFrames() const
{
	return 0;
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
	return false;
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
	return 0;
}
