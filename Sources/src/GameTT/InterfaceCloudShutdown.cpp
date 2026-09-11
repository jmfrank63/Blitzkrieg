#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "InterfaceCloudShutdown.h"
#include "CommonId.h"
#include "../Platform/Clock.h"

static const NInput::SRegisterCommandEntry commands[] =
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
enum
{
	E_STATIC_CAPTION						= 3101,
	E_STATIC_WAIT								= 3102,
	E_STATIC_WARNING						= 3103,
	E_BUTTON_SKIP								= 3104,
};
// How long the screen waits for the sync before leaving anyway. A person is
// reading a notice, so this is generous - a first upload of a large profile
// is minutes, not seconds - but finite: a daemon that never settles must not
// hold the exit forever. The main loop's own exit path then abandons the run.
static const std::uint64_t EXIT_SYNC_CAP_MS = 10 * 60 * 1000;
// How long a skip waits for the cancelled run to settle. Cancel interrupts
// the worker and the run lands as failed/"Cancelled" within a poll or two;
// if it does not, the player asked to leave and leaves. Either way out with
// the run unsettled raises CloudSync.ExitAbandoned, and the main loop's
// post-loop path then cancels and releases the run instead of waiting on it.
static const std::uint64_t SKIP_GRACE_MS = 5 * 1000;

bool CInterfaceCloudShutdown::Init()
{
	CInterfaceScreenBase::Init();
	msgs.Init( pInput, commands );
	return true;
}
void CInterfaceCloudShutdown::StartInterface()
{
	bNoticeShown = false;
	bLeft = false;
	bSkipRequested = false;
	nStartedMs = NPlatform::MonotonicMilliseconds64();
	nSkipRequestedMs = 0;
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
	static const int nIDs[] = { E_STATIC_CAPTION, E_STATIC_WAIT, E_STATIC_WARNING, E_BUTTON_SKIP };
	for ( int i = 0; i < 4; ++i )
		if ( IUIElement *pElement = pUIScreen->GetChildByID( nIDs[i] ) )
			pElement->ShowWindow( bShow && !( nIDs[i] == E_BUTTON_SKIP && bSkipRequested ) ? UI_SW_SHOW : UI_SW_HIDE );
	bNoticeShown = bShow;
}
// The skip button, or Escape. Only meaningful while a run holds the handle -
// that is the only time the notice is up - and once: the main loop consumes
// the request on its next frame and cancels the run, whose settle flips
// CloudSync.ExitSync to 0 and takes the screen out through Leave() exactly
// as a finished sync does. The button goes away at once so the click reads
// as taken.
void CInterfaceCloudShutdown::RequestSkip()
{
	if ( bLeft || bSkipRequested || !bNoticeShown )
		return;
	bSkipRequested = true;
	nSkipRequestedMs = NPlatform::MonotonicMilliseconds64();
	SetGlobalVar( "CloudSync.SkipToOffline", 1 );
	if ( IUIElement *pButton = pUIScreen->GetChildByID( E_BUTTON_SKIP ) )
		pButton->ShowWindow( UI_SW_HIDE );
	NStr::DebugTrace( "cloud shutdown: skip requested, cancelling the exit sync\n" );
}
bool CInterfaceCloudShutdown::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case E_BUTTON_SKIP:
	case IMC_CANCEL:
		RequestSkip();
		return true;
	}
	return false;
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
			SetGlobalVar( "CloudSync.ExitAbandoned", 1 );
			Leave();
		}
		else if ( bSkipRequested && NPlatform::MonotonicMilliseconds64() - nSkipRequestedMs > SKIP_GRACE_MS )
		{
			NStr::DebugTrace( "cloud shutdown: cancelled exit sync did not settle; leaving\n" );
			SetGlobalVar( "CloudSync.ExitAbandoned", 1 );
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
