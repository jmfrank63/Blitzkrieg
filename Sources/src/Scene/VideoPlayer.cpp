#include "StdAfx.h"

#include "VideoPlayer.h"
#include "BinkVideoPlayer.h"
#include "OpenVideoPlayer.h"

#include "..\SFX\SFX.h"

namespace
{
	bool HasExtension( const char *pszFileName, const char *pszExtension )
	{
		if ( (pszFileName == 0) || (pszExtension == 0) )
			return false;
		const char *pszDot = strrchr( pszFileName, '.' );
		return (pszDot != 0) && (_stricmp( pszDot, pszExtension ) == 0);
	}

	bool IsOpenVideoFile( const char *pszFileName )
	{
		return HasExtension( pszFileName, ".ogv" ) ||
					 HasExtension( pszFileName, ".ogg" ) ||
					 HasExtension( pszFileName, ".theora" );
	}

	bool IsBinkVideoFile( const char *pszFileName )
	{
		return HasExtension( pszFileName, ".bik" );
	}

	bool DataStreamExists( const std::string &szFileName )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szFileName.c_str(), STREAM_ACCESS_READ );
		if ( pStream != 0 && pStream->GetSize() > 0 )
			return true;
		pStream = OpenFileStream( szFileName, STREAM_ACCESS_READ );
		return pStream != 0 && pStream->GetSize() > 0;
	}

	std::string RemoveExtension( const std::string &szFileName )
	{
		const std::string::size_type nSlash = szFileName.find_last_of( "\\/" );
		const std::string::size_type nDot = szFileName.find_last_of( "." );
		if ( (nDot == std::string::npos) || ((nSlash != std::string::npos) && (nDot < nSlash)) )
			return szFileName;
		return szFileName.substr( 0, nDot );
	}

	std::string ResolveOpenVideoName( const std::string &szOriginalFileName )
	{
		NStr::DebugTrace( "Open video resolver: requested \"%s\".\n", szOriginalFileName.c_str() );
		if ( !IsBinkVideoFile( szOriginalFileName.c_str() ) )
		{
			NStr::DebugTrace( "Open video resolver: \"%s\" is already an open/non-Bink name.\n", szOriginalFileName.c_str() );
			return szOriginalFileName;
		}
		const std::string szBaseName = RemoveExtension( szOriginalFileName );
		const char *pszExtensions[] = { ".ogv", ".ogg", ".theora" };
		for ( int i = 0; i < sizeof(pszExtensions) / sizeof(pszExtensions[0]); ++i )
		{
			const std::string szCandidate = szBaseName + pszExtensions[i];
			NStr::DebugTrace( "Open video resolver: checking \"%s\".\n", szCandidate.c_str() );
			if ( DataStreamExists(szCandidate) )
			{
				NStr::DebugTrace( "Open video resolver: using replacement \"%s\" for \"%s\".\n", szCandidate.c_str(), szOriginalFileName.c_str() );
				return szCandidate;
			}
		}
		NStr::DebugTrace( "Open video resolver: no open replacement for \"%s\".\n", szOriginalFileName.c_str() );
		return szOriginalFileName;
	}
}

CVideoPlayer::CVideoPlayer()
{
	pTargetGFX = 0;
	rcDstRect.SetEmpty();
	bHasTarget = false;
	bHasDstRect = false;
	bMaintainAspect = true;
	bLooped = false;
	nShadingEffectStart = 17;
	nShadingEffectFinish = 18;
	dwPlayFlags = 0;
}

void CVideoPlayer::CreateBackend( const char *pszFileName )
{
	pPlayer = IsOpenVideoFile( pszFileName ) ? static_cast<IVideoPlayer*>( new COpenVideoPlayer ) : static_cast<IVideoPlayer*>( new CBinkVideoPlayer );
	ApplyState();
}

void CVideoPlayer::ApplyState()
{
	if ( pPlayer == 0 )
		return;
	if ( bHasTarget )
		pPlayer->SetTarget( pTargetTexture, pTargetGFX );
	if ( bHasDstRect )
		pPlayer->SetDstRect( rcDstRect, bMaintainAspect );
	pPlayer->SetLoopMode( bLooped );
	pPlayer->SetShadingEffect( nShadingEffectStart, true );
	pPlayer->SetShadingEffect( nShadingEffectFinish, false );
}

void CVideoPlayer::SetTarget( IGFXTexture *pTexture, IGFX *pGFX )
{
	pTargetTexture = pTexture;
	pTargetGFX = pGFX;
	bHasTarget = true;
	if ( pPlayer != 0 )
		pPlayer->SetTarget( pTexture, pGFX );
}

void CVideoPlayer::SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect )
{
	rcDstRect = _rcDstRect;
	bMaintainAspect = _bMaintainAspect;
	bHasDstRect = true;
	if ( pPlayer != 0 )
		pPlayer->SetDstRect( _rcDstRect, _bMaintainAspect );
}

void CVideoPlayer::SetLoopMode( bool _bLooped )
{
	bLooped = _bLooped;
	if ( pPlayer != 0 )
		pPlayer->SetLoopMode( _bLooped );
}

int CVideoPlayer::GetCurrentFrame() const
{
	return pPlayer == 0 ? -1 : pPlayer->GetCurrentFrame();
}

bool CVideoPlayer::SetCurrentFrame( const int nFrame )
{
	return pPlayer != 0 && pPlayer->SetCurrentFrame( nFrame );
}

void CVideoPlayer::SetShadingEffect( const int nEffect, bool bStart )
{
	if ( bStart )
		nShadingEffectStart = nEffect;
	else
		nShadingEffectFinish = nEffect;
	if ( pPlayer != 0 )
		pPlayer->SetShadingEffect( nEffect, bStart );
}

bool CVideoPlayer::Update( const NTimer::STime &time, bool bForcedUpdate )
{
	return pPlayer != 0 && pPlayer->Update( time, bForcedUpdate );
}

int CVideoPlayer::Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, ISFX *pSFX )
{
	Stop();
	const std::string szOriginalFileName = pszFileName == 0 ? "" : pszFileName;
	const std::string szResolvedFileName = ResolveOpenVideoName( szOriginalFileName );
	szFileName = szResolvedFileName;
	dwPlayFlags = dwFlags;
	CreateBackend( szResolvedFileName.c_str() );
	int nMovieLength = pPlayer == 0 ? 0 : pPlayer->Play( szResolvedFileName.c_str(), dwFlags, pGFX, pSFX );
	if ( (nMovieLength == 0) && (szResolvedFileName != szOriginalFileName) )
	{
		NStr::DebugTrace( "Open video resolver: fallback to Bink \"%s\" after open backend returned 0.\n", szOriginalFileName.c_str() );
		szFileName = szOriginalFileName;
		pPlayer = static_cast<IVideoPlayer*>( new CBinkVideoPlayer );
		ApplyState();
		nMovieLength = pPlayer->Play( szOriginalFileName.c_str(), dwFlags, pGFX, pSFX );
	}
	return nMovieLength;
}

bool CVideoPlayer::Stop()
{
	if ( pPlayer == 0 )
		return true;
	const bool bResult = pPlayer->Stop();
	pPlayer = 0;
	return bResult;
}

bool CVideoPlayer::Pause( bool bPause )
{
	return pPlayer != 0 && pPlayer->Pause( bPause );
}

bool CVideoPlayer::IsPlaying() const
{
	return pPlayer != 0 && pPlayer->IsPlaying();
}

int CVideoPlayer::GetLength() const
{
	return pPlayer == 0 ? 0 : pPlayer->GetLength();
}

int CVideoPlayer::GetNumFrames() const
{
	return pPlayer == 0 ? 0 : pPlayer->GetNumFrames();
}

bool CVideoPlayer::GetMovieSize( CVec2 *pSize ) const
{
	return pPlayer != 0 && pPlayer->GetMovieSize( pSize );
}

bool CVideoPlayer::Draw( IGFX *pGFX )
{
	return pPlayer != 0 && pPlayer->Draw( pGFX );
}

void CVideoPlayer::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}

int CVideoPlayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	int nLastFrame = GetCurrentFrame();
	bool bPlaying = IsPlaying();
	saver.Add( 1, &rcDstRect );
	saver.Add( 2, &bMaintainAspect );
	saver.Add( 3, &bLooped );
	saver.Add( 4, &nLastFrame );
	saver.Add( 5, &szFileName );
	saver.Add( 6, &dwPlayFlags );
	saver.Add( 7, &nShadingEffectStart );
	saver.Add( 8, &nShadingEffectFinish );
	saver.Add( 9, &bHasDstRect );
	saver.Add( 20, &bPlaying );

	if ( saver.IsReading() )
	{
		pPlayer = 0;
		if ( bPlaying )
		{
			Play( szFileName.c_str(), dwPlayFlags, GetSingleton<IGFX>(), GetSingleton<ISFX>() );
			SetCurrentFrame( nLastFrame );
		}
	}
	return 0;
}
