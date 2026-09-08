#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceCloudShutdown.h"
#include "../Platform/Clock.h"

enum
{
	E_STATIC_CAPTION						= 3101,
	E_STATIC_WAIT								= 3102,
	E_STATIC_WARNING						= 3103,
};
// How long the screen waits for the sync before leaving anyway. A person is
// reading a notice, so this is generous - a first upload of a large profile
// is minutes, not seconds - but finite: a daemon that never settles must not
// hold the exit forever. The main loop's own exit path then abandons the run.
static const std::uint64_t EXIT_SYNC_CAP_MS = 10 * 60 * 1000;

bool CInterfaceCloudShutdown::Init()
{
	CInterfaceScreenBase::Init();
	return true;
}
void CInterfaceCloudShutdown::StartInterface()
{
	bNoticeShown = false;
	bLeft = false;
	nStartedMs = NPlatform::MonotonicMilliseconds64();
	CInterfaceScreenBase::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\CloudShutdown" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
	// The backdrop alone until the main loop confirms a run: a player with
	// no cloud sync due passes through here for one frame on the way to the
	// exit video, and must not see a notice flash by.
	ShowNotice( false );
}
void CInterfaceCloudShutdown::ShowNotice( bool bShow )
{
	static const int nIDs[] = { E_STATIC_CAPTION, E_STATIC_WAIT, E_STATIC_WARNING };
	for ( int i = 0; i < 3; ++i )
		if ( IUIElement *pElement = pUIScreen->GetChildByID( nIDs[i] ) )
			pElement->ShowWindow( bShow ? UI_SW_SHOW : UI_SW_HIDE );
	bNoticeShown = bShow;
}
void CInterfaceCloudShutdown::Leave()
{
	if ( bLeft )
		return;
	bLeft = true;
	// The exit video, exactly as the exit command played it before this
	// screen existed; the -1 ends the main loop after it.
	FinishInterface( MISSION_COMMAND_VIDEO, "demo\\exit;-1" );
}
bool CInterfaceCloudShutdown::StepLocal( bool bAppActive )
{
	pUIScreen->Update( pTimer->GetAbsTime() );
	if ( bLeft )
		return bAppActive;
	const int nExitSync = GetGlobalVar( "CloudSync.ExitSync", -1 );
	if ( nExitSync == 0 )
		Leave();
	else if ( nExitSync == 1 )
	{
		if ( !bNoticeShown )
		{
			NStr::DebugTrace( "cloud shutdown: waiting for the exit sync\n" );
			ShowNotice( true );
		}
		if ( NPlatform::MonotonicMilliseconds64() - nStartedMs > EXIT_SYNC_CAP_MS )
		{
			NStr::DebugTrace( "cloud shutdown: exit sync still running after the cap; leaving\n" );
			Leave();
		}
	}
	return bAppActive;
}
// The activity bar under the notice: a gold sweep on a dark track while the
// exit sync holds the handle. The notice's three lines end at canvas y=232
// (upper third), so the track is authored just below them, centred.
void CInterfaceCloudShutdown::DrawAdd()
{
	CInterfaceScreenBase::DrawSyncActivityBar( CTRect<float>( 312.0f, 252.0f, 712.0f, 262.0f ), bNoticeShown && !bLeft );
}
