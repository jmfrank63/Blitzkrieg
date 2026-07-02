#include "StdAfx.h"

#include "UIVideoButton.h"

int CUIVideoButton::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 2, &szVideoFile );
	saver.Add( 3, &pVideoPlayer );

	if ( saver.IsReading() )
		Play();
	return 0;
}
int CUIVideoButton::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "VideoFile", &szVideoFile );

	if ( saver.IsReading() )
		InitVideoPlayer();

	return 0;
}
void CUIVideoButton::InitVideoPlayer()
{
	pVideoPlayer = CreateObject<IVideoPlayer>( SCENE_VIDEO_PLAYER );
	pVideoPlayer->SetLoopMode( true );
}
void CUIVideoButton::Reposition( const CTRect<float> &rcParent )
{
	NI_ASSERT_TF( pVideoPlayer != 0, "CUIVideoButton() error: VideoPlayer is not initialized", return );
	CSimpleWindow::Reposition( rcParent );
	pVideoPlayer->SetDstRect( GetScreenRect(), false );
	Play();
}
void CUIVideoButton::Visit( interface ISceneVisitor *pVisitor )
{
	pVisitor->VisitUICustom( dynamic_cast<IUIElement*>(this) );
}
void CUIVideoButton::Draw( IGFX *pGFX )
{
	pVideoPlayer->Draw( pGFX );
}
bool CUIVideoButton::Update( const NTimer::STime &currTime )
{
	return pVideoPlayer->Update( currTime );
}
void CUIVideoButton::Play()
{
	pVideoPlayer->Play( (szVideoFile + ".ogv").c_str(), IVideoPlayer::PLAY_FROM_MEMORY, GetSingleton<IGFX>(), GetSingleton<ISFX>() );
}
int CUIVideoButton::GetCurrentFrame()
{
	return pVideoPlayer->GetCurrentFrame();
}
bool CUIVideoButton::SetCurrentFrame( int nFrame )
{
	return pVideoPlayer->SetCurrentFrame( nFrame );
}
