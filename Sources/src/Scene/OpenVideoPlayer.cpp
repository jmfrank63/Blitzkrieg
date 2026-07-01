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
	NStr::DebugTrace( "Open video backend scaffold cannot play \"%s\" yet.\n", szFileName.c_str() );
	return 0;
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
	return 0;
}
