#include "StdAfx.h"

#include "BinkVideoPlayer.h"

#include "..\SFX\SFX.h"
#include "..\GFX\GFXHelper.h"
#include "..\Misc\Intersection.h"
#include <mmreg.h>
#include <dsound.h>
CBinkVideoPlayer::CBinkVideoPlayer()
{
	dwCopyFlags = 0;
	hBink = 0;
	rcDstRect.SetEmpty();
	bMaintainAspect = true;
	nLastPlayedFrame = -1;
	bLooped = false;
	nShadingEffectStart = 17;
	nShadingEffectFinish = 18;
	bStopped = false;
}
CBinkVideoPlayer::~CBinkVideoPlayer()
{
	if ( hBink ) 
		BinkClose( hBink );
}
void CBinkVideoPlayer::SetupRects()
{
	if ( rcDstRect.IsEmpty() || (hBink == 0) || images.empty() || images[0].rcSrcRect.IsEmpty() ) 
		return;
	if ( bMaintainAspect ) 
	{
		const float fCoeffX = rcDstRect.Width() / float( hBink->Width );
		const float fCoeffY = rcDstRect.Height() / float( hBink->Height );
		if ( (fCoeffX < fCoeffY) && (fabsf(fCoeffX - fCoeffY) > 0.001) ) 
		{
			const float fNewSizeY = hBink->Height * fCoeffX;
			rcDstRect.y1 = ( rcDstRect.Height() - fNewSizeY ) / 2.0f;
			rcDstRect.y2 = rcDstRect.y1 + fNewSizeY;
		}
		else if ( (fCoeffY < fCoeffX) && (fabsf(fCoeffY - fCoeffX) > 0.001) ) 
		{
			const float fNewSizeX = hBink->Height * fCoeffY;
			rcDstRect.x1 = ( rcDstRect.Width() - fNewSizeX ) / 2.0f;
			rcDstRect.x2 = rcDstRect.x1 + fNewSizeX;
		}
	}
	const float fCoeffX = rcDstRect.Width() / float( hBink->Width );
	const float fCoeffY = rcDstRect.Height() / float( hBink->Height );
	for ( CImagesList::iterator it = images.begin(); it != images.end(); ++it )
	{
		it->rcRect.x1 = int( rcDstRect.x1 + it->rcSrcRect.x1*fCoeffX ) - 0.5f;
		it->rcRect.y1 = int( rcDstRect.y1 + it->rcSrcRect.y1*fCoeffY ) - 0.5f;
		it->rcRect.x2 = int( rcDstRect.x1 + it->rcSrcRect.x2*fCoeffX ) - 0.5f;
		it->rcRect.y2 = int( rcDstRect.y1 + it->rcSrcRect.y2*fCoeffY ) - 0.5f;
	}
}
void CBinkVideoPlayer::SetTarget( IGFXTexture *pTexture, IGFX *pGFX )
{
	SImagePart image;
	image.pTexture = pTexture;
	images.clear();
	images.push_back( image );
}
void CBinkVideoPlayer::SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect )
{
	rcDstRect = _rcDstRect;
	bMaintainAspect = _bMaintainAspect;
	SetupRects();
}
bool CBinkVideoPlayer::OpenBink( const char *pszFileName, DWORD dwOpenFlags, DWORD dwFlags )
{
	if ( dwFlags & IVideoPlayer::PLAY_FROM_MEMORY ) 
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( pszFileName, STREAM_ACCESS_READ );
		if ( (pStream == 0) || (pStream->GetSize() == 0) ) 
			return false;
		buffer.resize( pStream->GetSize() );
		pStream->Read( &(buffer[0]), buffer.size() );
		hBink = BinkOpen( &(buffer[0]), dwOpenFlags | BINKFROMMEMORY );
	}
	else
		hBink = BinkOpen( pszFileName, dwOpenFlags );
	dwPlayFlags = dwFlags;
	if ( hBink != 0 )
	{
		s32 nVolume = 32768 * GetSingleton<ISFX>()->GetSFXMasterVolume() / 255.0f;
		BinkSetVolume( hBink, 0, nVolume );
	}
	return hBink != 0;
}
int CBinkVideoPlayer::Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, interface ISFX *pSFX )
{
	Stop();
	if ( pSFX ) 
	{
		LPDIRECTSOUND pDS = reinterpret_cast<LPDIRECTSOUND>( pSFX->QI(0) );
		if ( pDS != 0 ) 
			BinkSoundUseDirectSound( pDS );
	}
	szFileName = pszFileName;
	DWORD dwOpenFlags = 0;
	if ( images.size() == 1 )							// if we have set 'external' render target
	{
		switch ( images[0].pTexture->GetFormat() ) 
		{
			case GFXPF_ARGB8888:
				if ( dwFlags & IVideoPlayer::PLAY_WITH_ALPHA ) 
					dwOpenFlags |= BINKALPHA;
				dwCopyFlags |= BINKSURFACE32A;
				break;
			case GFXPF_ARGB4444:
				if ( dwFlags & IVideoPlayer::PLAY_WITH_ALPHA ) 
					dwOpenFlags |= BINKALPHA;
				dwCopyFlags |= BINKSURFACE4444;
				break;
			case GFXPF_ARGB1555:
				dwCopyFlags |= BINKSURFACE5551;
				break;
			case GFXPF_ARGB0565:
				dwCopyFlags |= BINKSURFACE565;
				break;
			default:
				NI_ASSERT_T( false, "Unsupported format in Bink video player" );
		}
		if ( OpenBink(pszFileName, dwOpenFlags, dwFlags) == false ) 
			return 0;
		const int nSizeX = Min( images[0].pTexture->GetSizeX(0), int(hBink->Width) );
		const int nSizeY = Min( images[0].pTexture->GetSizeY(0), int(hBink->Height) );
		images[0].rcSrcRect.Set( 0, 0, nSizeX, nSizeY );
		images[0].rcDstRect.Set( 0, 0, nSizeX, nSizeY );
		images[0].rcMaps.Set( 0, 0, float(nSizeX) / float(images[0].pTexture->GetSizeX(0)), float(nSizeY) / float(images[0].pTexture->GetSizeY(0)) );
	}
	else																	// create 'internal' render target
	{
		dwCopyFlags |= BINKSURFACE32A;
		if ( dwFlags & IVideoPlayer::PLAY_WITH_ALPHA ) 
			dwOpenFlags |= BINKALPHA;
		if ( OpenBink(pszFileName, dwOpenFlags, dwFlags) == false ) 
			return 0;
		const bool bHasNonPow2Textures = (GetGlobalVar( "GFX.Caps.Texture.NonPow2Conditional", 0 ) != 0) || (GetGlobalVar( "GFX.Caps.Texture.NonPow2", 0 ) != 0);
		if ( bHasNonPow2Textures )					// create one non-pow2 texture, if device supports it
		{
			images.clear();

			SImagePart image;
			image.pTexture = pGFX->CreateTexture( hBink->Width, hBink->Height, 1, GFXPF_ARGB8888, GFXD_STATIC );
			image.rcSrcRect.Set( 0, 0, hBink->Width, hBink->Height );
			image.rcDstRect.Set( 0, 0, hBink->Width, hBink->Height );
			image.rcMaps.Set( 0, 0, 1, 1 );
			images.push_back( image );
		}
		else																// create serie of 256x256 textures to cover all render target
		{
			const int nNumTexturesX = fmod( hBink->Width, 256 ) == 0 ? hBink->Width / 256 : hBink->Width / 256 + 1;
			const int nNumTexturesY = fmod( hBink->Height, 256 ) == 0 ? hBink->Height / 256 : hBink->Height / 256 + 1;
			const bool bSquareOnly = GetGlobalVar( "GFX.Caps.Texture.SquareOnly", 0 ) != 0;
			int nRestSizeY = hBink->Height;
			int nPosY = 0;
			for ( int i = 0; i < nNumTexturesY; ++i )
			{
				const int nSrcSizeY = nRestSizeY >= 256 ? 256 : nRestSizeY;
				int nRestSizeX = hBink->Width;
				int nPosX = 0;
				for ( int j = 0; j < nNumTexturesX; ++j )
				{
					const int nSrcSizeX = nRestSizeX >= 256 ? 256 : nRestSizeX;
					int nTextureSizeX = nRestSizeX < 256 ? GetNextPow2( nRestSizeX ) : 256;
					int nTextureSizeY = nRestSizeY < 256 ? GetNextPow2( nRestSizeY ) : 256;
					if ( bSquareOnly ) 
						nTextureSizeX = nTextureSizeY = Max( nTextureSizeX, nTextureSizeY );
					SImagePart image;
					image.pTexture = pGFX->CreateTexture( nTextureSizeX, nTextureSizeY, 1, GFXPF_ARGB8888, GFXD_STATIC );
					image.rcSrcRect.Set( nPosX, nPosY, nPosX + nSrcSizeX, nPosY + nSrcSizeY );
					image.rcDstRect.Set( 0, 0, nSrcSizeX, nSrcSizeY );
					image.rcMaps.Set( 0, 0, float(image.rcDstRect.Width()) / float(nTextureSizeX), float(image.rcDstRect.Height()) / float(nTextureSizeY) );
					images.push_back( image );
					nRestSizeX -= 256;
					nPosX += 256;
				}
				nRestSizeY -= 256;
				nPosY += 256;
			}
		}
	}
	if ( ((hBink->Width % 16) != 0) || ((hBink->Height % 16) != 0) )
	{
		NStr::DebugTrace( "****** WARNING: movie \"%s\" has non-multiple to 16 size (%d : %d)! Possible performance hit!\n", pszFileName, hBink->Width, hBink->Height );
		dwCopyFlags |= BINKCOPYALL;
	}
	if ( dwFlags & IVideoPlayer::COPY_ALL ) 
		dwCopyFlags |= BINKCOPYALL;
	for ( CImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
	{
		SSurfaceLockInfo lock;
		it->pTexture->Lock( 0, &lock );
		for ( int i = 0; i < it->pTexture->GetSizeY(0); ++i )
			memset( ((char*)lock.pData) + i*lock.nPitch, 0, it->pTexture->GetSizeX(0) );
		it->pTexture->Unlock( 0 );
	}
	if ( rcDstRect.IsEmpty() ) 
		rcDstRect.Set( 0, 0, hBink->Width, hBink->Height );
	SetupRects();
	DoOneFrame( false );
	CopyRects();
	return (hBink != 0) && (hBink->FrameRate > 0) ? 1000 * hBink->Frames / hBink->FrameRate : 0;
}
bool CBinkVideoPlayer::Stop()
{
	if ( hBink ) 
	{
		BinkClose( hBink );
		hBink = 0;
		buffer.clear();
	}
	return true;
}
bool CBinkVideoPlayer::Pause( bool bPause )
{
	if ( hBink == 0 ) 
		return false;
	return BinkPause( hBink, bPause );
}
bool CBinkVideoPlayer::DoOneFrame( bool bCheckForStop )
{
	if ( hBink == 0 ) 
		return false;
	if ( bCheckForStop && (hBink->FrameNum == hBink->Frames) && !bLooped ) 
	{
		bStopped = true;
		BinkDoFrame( hBink );
		BinkNextFrame( hBink );
		return true;
	}
	BinkDoFrame( hBink );
	BinkNextFrame( hBink );
	if ( bCheckForStop && (nLastPlayedFrame > hBink->FrameNum) && !bLooped )
	{
		CopyRects();
		return Stop();
	}
	nLastPlayedFrame = hBink->FrameNum;
	return true;
}
bool CBinkVideoPlayer::Update( const NTimer::STime &time, bool bForcedUpdate )
{
	if ( hBink == 0 ) 
		return ( dwPlayFlags & IVideoPlayer::PLAY_INFINITE ) != 0;
	if ( bForcedUpdate ) 
	{
		DoOneFrame();
		CopyRects();
	}
	else if ( IsPlaying() )
	{
		bool bNeedCopyRects = false;
		while ( BinkWait(hBink) == 0 ) 
		{
			DoOneFrame();
			bNeedCopyRects = true;
			if ( !IsPlaying() ) 
				break;
		}
		if ( bNeedCopyRects ) 
			CopyRects();
	}
	return IsPlaying() || ((dwPlayFlags & IVideoPlayer::PLAY_INFINITE) != 0);
}
static CTRect<long> rcDirtyRects[BINKMAXDIRTYRECTS];
bool HasIntersection( const CTRect<long> &rcRect, const int nNumDirtyRects )
{
	for ( int i = 0; i < nNumDirtyRects; ++i )
	{
		if ( rcRect.IsIntersect(rcDirtyRects[i]) )
			return true;
	}
	return nNumDirtyRects == 0;
}
void CopyRect( HBINK hBink, const SImagePart &image, const DWORD dwCopyFlags, const int nNumDirtyRects )
{
	if ( !HasIntersection(image.rcSrcRect, nNumDirtyRects) ) 
		return;
	if ( image.pTexture == 0 || image.rcSrcRect.IsEmpty() || image.rcDstRect.IsEmpty() )
		return;
	const int nCopyWidth = image.rcSrcRect.Width();
	const int nCopyHeight = image.rcSrcRect.Height();
	const int nTextureWidth = image.pTexture->GetSizeX( 0 );
	const int nTextureHeight = image.pTexture->GetSizeY( 0 );
	if ( nCopyWidth <= 0 || nCopyHeight <= 0 ||
		   image.rcDstRect.x1 < 0 || image.rcDstRect.y1 < 0 ||
		   image.rcDstRect.x1 + nCopyWidth > nTextureWidth ||
		   image.rcDstRect.y1 + nCopyHeight > nTextureHeight )
	{
		NStr::DebugTrace( "****** WARNING: invalid Bink copy rect dst=(%d,%d) size=(%d,%d) texture=(%d,%d)\n",
		                  image.rcDstRect.x1, image.rcDstRect.y1, nCopyWidth, nCopyHeight, nTextureWidth, nTextureHeight );
		return;
	}
	SSurfaceLockInfo lock = { 0, 0 };
	if ( !image.pTexture->Lock( 0, &lock ) || lock.pData == 0 || lock.nPitch <= 0 )
		return;
	BinkCopyToBufferRect( hBink, lock.pData, lock.nPitch, image.rcDstRect.Height(), image.rcDstRect.x1, image.rcDstRect.y1,
												image.rcSrcRect.x1, image.rcSrcRect.y1, image.rcSrcRect.Width(), image.rcSrcRect.Height(), dwCopyFlags );
	image.pTexture->Unlock( 0 );
	image.pTexture->AddDirtyRect( 0 );
}
void CBinkVideoPlayer::CopyRects()
{
	if ( hBink == 0 ) 
		return;
	const int nNumDirtyRects = BinkGetRects( hBink, BINKSURFACEFAST );
	for ( int i = 0; i < nNumDirtyRects; ++i )
	{
		
		rcDirtyRects[i].Set( hBink->FrameRects[i].Left,
												 hBink->FrameRects[i].Top,
												 hBink->FrameRects[i].Left + hBink->FrameRects[i].Width,
												 hBink->FrameRects[i].Top + hBink->FrameRects[i].Height );
	}
	IGFX *pGFX = GetSingleton<IGFX>();
	for ( CImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
		CopyRect( hBink, *it, dwCopyFlags, nNumDirtyRects );
}
bool CBinkVideoPlayer::Draw( IGFX *pGFX )
{
	pGFX->SetShadingEffect( nShadingEffectStart );
	for ( CImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
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

/*	
	const SHMatrix &matProjection = pGFX->GetProjectionMatrix();
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	const float fWidth = Max( rcScreen.Width(), 1.0f );
	const float fHeight = Max( rcScreen.Height(), 1.0f );
	SHMatrix matrix;
	Zero( matrix );
	matrix._11 = 1;
	matrix._14 = -fWidth / 2;
	matrix._22 = -1;
	matrix._24 = fHeight / 2;
	matrix._33 = 1.0f / matProjection._33;//-( far_plane - near_plane );
	matrix._34 = -matProjection._34 / matProjection._33;//-near_plane;
	matrix._44 = 1;
	SHMatrix matWorld;
	Multiply( &matWorld, pGFX->GetInverseViewMatrix(), matrix );
	pGFX->SetWorldTransforms( 0, &matWorld, 1 );
	pGFX->SetShadingEffect( nShadingEffectStart );
	for ( CImagesList::const_iterator it = images.begin(); it != images.end(); ++it )
	{
		const DWORD color = 0xffffffff;
		const DWORD specular = 0xff000000;
		const float fDepth = 0;
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 4, SGFXLVertex::format, GFXPT_TRIANGLELIST );
		vertices[0].Setup( it->rcRect.minx, it->rcRect.maxy, fDepth, color, specular, it->rcMaps.minx, it->rcMaps.maxy );
		vertices[1].Setup( it->rcRect.minx, it->rcRect.miny, fDepth, color, specular, it->rcMaps.minx, it->rcMaps.miny );
		vertices[2].Setup( it->rcRect.maxx, it->rcRect.maxy, fDepth, color, specular, it->rcMaps.maxx, it->rcMaps.maxy );
		vertices[3].Setup( it->rcRect.maxx, it->rcRect.miny, fDepth, color, specular, it->rcMaps.maxx, it->rcMaps.miny );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		indices[0] = 2;
		indices[1] = 1;
		indices[2] = 0;
		indices[3] = 1;
		indices[4] = 2;
		indices[5] = 3;
		pGFX->SetTexture( 0, it->pTexture );
		pGFX->DrawTemp();
	}
	pGFX->SetShadingEffect( nShadingEffectFinish );
	return true;
*/
}
void CBinkVideoPlayer::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
bool CBinkVideoPlayer::IsPlaying() const
{
	return (hBink != 0) && !bStopped;
}
int CBinkVideoPlayer::GetCurrentFrame() const
{
	if ( !IsPlaying() ) 
		return -1;
	return hBink->FrameNum;
}
bool CBinkVideoPlayer::SetCurrentFrame( const int nFrame )
{
	if ( !IsPlaying() ) 
		return false;
	BinkGoto( hBink, nFrame, 0 );
	DoOneFrame();
	CopyRects();
	return true;
}
int CBinkVideoPlayer::GetLength() const
{
	return (hBink != 0) && (hBink->FrameRate > 0) ? 1000 * hBink->Frames / hBink->FrameRate : 0;
}
int CBinkVideoPlayer::GetNumFrames() const
{
	return hBink != 0? hBink->Frames : 0;
}
bool CBinkVideoPlayer::GetMovieSize( CVec2 *pSize ) const
{
	if ( (hBink == 0) || (pSize == 0) ) 
		return false;
	pSize->x = hBink->Width;
	pSize->y = hBink->Height;
	return true;
}
int CBinkVideoPlayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rcDstRect );
	saver.Add( 2, &bMaintainAspect );
	saver.Add( 3, &dwCopyFlags );
	saver.Add( 4, &bLooped );
	saver.Add( 5, &nLastPlayedFrame );
	saver.Add( 6, &szFileName );
	saver.Add( 7, &dwPlayFlags );
	saver.Add( 8, &nShadingEffectStart );
	saver.Add( 9, &nShadingEffectFinish );
	saver.Add( 10, &bStopped );
	bool bPlaying = IsPlaying();
	saver.Add( 20, &bPlaying );
	
	if ( saver.IsReading() ) 
	{
		buffer.clear();
		if ( bPlaying ) 
		{
			const int nStartFromFrame = nLastPlayedFrame;
			Play( szFileName.c_str(), dwPlayFlags, GetSingleton<IGFX>(), GetSingleton<ISFX>() );
			if ( hBink ) 
				BinkGoto( hBink, nStartFromFrame, 0 );
		}
	}
	return 0;
}
