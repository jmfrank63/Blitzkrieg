#include "StdAfx.h"

#include "InterfaceScreenBase.h"
#include "../Platform/Clock.h"
#include "../Main/iMainCommands.h"
#include "../StreamIO/OptionSystem.h"
#include "../AILogic/AILogic.h"
#include "../GameTT/CommonId.h"
#include "../Main/ScenarioTracker.h"
#include "../GameTT/iMission.h"
BASIC_REGISTER_CLASS( CInterfaceScreenBase );
CInterfaceScreenBase::CInterfaceScreenBase( const std::string &_szInterfaceType )
: szInterfaceType( _szInterfaceType ), bInterfaceClosed( false )
{
	bEnableStatistics = getenv( "BK_STATS_OVERLAY" ) != 0;
	fTotalTime = 1;
	nFrameCounter = 1;
	nTriCounter = 0;
	fAveFPS = 0;
	fAveTPS = 0;
	NHPTimer::GetTime( &time );
	nCPUFreq = int( NHPTimer::GetClockRate() / 1000000.0 + 0.5 );

	vLastCursorPos.Set( 0, 0 );
	bLastCursorScreenMoveRes = false;
	timeToolTip = 0;
}
bool CInterfaceScreenBase::Init()
{
	pGFX = GetSingleton<IGFX>();
	pSFX = GetSingleton<ISFX>();
	pInput = GetSingleton<IInput>();
	pScene = GetSingleton<IScene>();
	pCamera = GetSingleton<ICamera>();
	pCursor = GetSingleton<ICursor>();
	pTimer = GetSingleton<IGameTimer>();
	{
		IStatSystem *pStat = pScene->GetStatSystem();
		pStat->SetPosition( 0, 0 );

		pStat->AddEntry( pGFX->GetAdapterName() );
		pStat->AddEntry( "resolution" );
		pStat->AddEntry( "CPU freq." );
		pStat->AddEntry( "verts" );
		pStat->AddEntry( "tris" );
		pStat->AddEntry( "tps" );
		pStat->AddEntry( "fps" );
		pStat->UpdateEntry( "CPU freq.", NStr::Format("%dMHz", int( NHPTimer::GetClockRate() / 1000000.0 + 0.5 )) );
	}
	ChangeResolution();
	pCursor->SetMode( 0 );
	pInput->ClearMessages();
	pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
	timeToolTipShowTime = GetGlobalVar( "Scene.ToolTipTime.Show", 1000 );
	timeToolTipHideTime = GetGlobalVar( "Scene.ToolTipTime.Hide", 10000 );
	SuspendAILogic( true );
	nHelpContextNumber = 0;
	return true;
}
void CInterfaceScreenBase::Done() 
{  
	if ( (pScene != 0) && (pUIScreen != 0) ) 
	{
		pScene->RemoveUIScreen( pUIScreen );
		pUIScreen = 0;
		RestoreScreen();
	}
	GetSingleton<ICursor>()->SetMode( 0 );
	pInput->ClearMessages();
	pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
}
void CInterfaceScreenBase::SetWindowText( const int nElementID, const WORD *pszText )
{
	IUIElement * pElement = pUIScreen->GetChildByID( nElementID );
	if ( pElement )
	{
		pElement->SetWindowText( 0, pszText );
	}
}
void CInterfaceScreenBase::SetWindowText( const int nElementID, IText *pText )
{
	SetWindowText( nElementID, pText->GetString() );
}
void CInterfaceScreenBase::SuspendAILogic( bool bSuspend )
{
	if ( bSuspend ) 
		GetSingleton<IAILogic>()->Suspend();
	else
		GetSingleton<IAILogic>()->Resume();
}
bool CInterfaceScreenBase::OnCursorMove( const CVec2 &vPos )
{
	const bool bScreenActive = (pScene && (pScene->GetUIScreen() == pUIScreen));
	if ( bScreenActive && vLastCursorPos != vPos )
	{
		vLastCursorPos = vPos;
		if ( pUIScreen )
			bLastCursorScreenMoveRes = pUIScreen->OnMouseMove( vPos, E_MOUSE_FREE );
	}
	if ( pUIScreen && bScreenActive ) 
	{
		const NTimer::STime timeAbs = pTimer->GetAbsTime();
		CVec2 vLastPos;
		NTimer::STime time;
		pCursor->GetLastPos( &vLastPos, &time );
		CTRect<float> rcRect;
		IText *pText = pUIScreen->GetHelpContext( vPos, &rcRect );
		if ( (timeAbs - time >= timeToolTipShowTime) || (pText && pText->IsChanged()) ) 
		{
			if ( (pLastToolTip.GetPtr() != pText) || (pText && pText->IsChanged()) ) 
			{
				const DWORD dwColor = GetGlobalVar( ("Scene.Colors.ToolTip." + szInterfaceType + ".Color").c_str(), 0 );
				pLastToolTip = pText;
				pScene->SetToolTip( pText, vPos + CVec2(0, 28), rcRect, dwColor );
				timeToolTip = timeAbs;
			}
			else if ( timeAbs - timeToolTip >= timeToolTipHideTime ) 
				pScene->SetToolTip( 0, vPos, rcRect );
		}
		else if ( pLastToolTip.GetPtr() != pText ) 
		{
			pScene->SetToolTip( 0, vPos, rcRect );
			pLastToolTip = 0;
		}
	}
	return bLastCursorScreenMoveRes;
}
void CInterfaceScreenBase::AddDelayedCommand( IInterfaceCommand *pCmd, const NTimer::STime &timeToPerform ) 
{ 
	pCmd->SetDelayedTime( timeToPerform );
	GetSingleton<IMainLoop>()->Command( pCmd );
}
void CInterfaceScreenBase::EnableMessageProcessingDelayed( const bool bEnable, const NTimer::STime &timeToPerform )
{
	IInterfaceCommand *pCmd = CreateObject<IInterfaceCommand>( MAIN_COMMAND_ENABLE_MESSAGE_PROCESSING );
	pCmd->Configure( bEnable ? "1" : "0" );
	AddDelayedCommand( pCmd, timeToPerform );
}
int CInterfaceScreenBase::PlayOverInterface( const char *pszName, const DWORD dwAddFlags, const bool bFadeIn )
{
	CPtr<ITransition> pTransition = CreateObject<ITransition>( SCENE_TRANSITION );
	const int nLength = pTransition->Start( pszName, dwAddFlags, NPlatform::MonotonicMilliseconds(), bFadeIn );
	if ( nLength > 0 ) 
		pScene->AddSceneObject( pTransition );
	return nLength;
}
void CInterfaceScreenBase::OpenCurtainsForced()
{
	GetSingleton<IMainLoop>()->EnableMessageProcessing( false );
	const int nLength = PlayOverInterface( "movies\\transition\\open.bik", 0, false );
	EnableMessageProcessingDelayed( true, NPlatform::MonotonicMilliseconds() + nLength );
}
bool CInterfaceScreenBase::OpenCurtains()
{
	RemoveTransition();
	if ( szInterfaceType == "Current" ) 
		return false;
	if ( GetGlobalVar("notransition", 0) == 0 ) 
	{
		OpenCurtainsForced();
	}
	return true;
}
void CInterfaceScreenBase::StartInterface()
{
	RemoveTransition();
}
int CInterfaceScreenBase::FinishInterface( const int nInterfaceCommandTypeID, const char *pszCommandConfig )
{
	CPtr<IInterfaceCommand> pCmd;
	if ( nInterfaceCommandTypeID != 0 ) 
	{
		pCmd = CreateObject<IInterfaceCommand>( nInterfaceCommandTypeID );
		pCmd->Configure( pszCommandConfig );
	}
	return FinishInterface( pCmd );
}
int CInterfaceScreenBase::FinishInterface( IInterfaceCommand *pCmdNextInterface )
{
	if ( GetGlobalVar("notransition", 0) == 0 ) 
	{
		SetGlobalVar( "CurtainsClosed", 1 );
		GetSingleton<IMainLoop>()->EnableMessageProcessing( false );
		const int nLength = PlayOverInterface( "movies\\transition\\close.bik", IVideoPlayer::PLAY_INFINITE, true );
		const int nTime = NPlatform::MonotonicMilliseconds();
		EnableMessageProcessingDelayed( true, nTime + nLength );
		if ( pCmdNextInterface ) 
			AddDelayedCommand( pCmdNextInterface, nTime + nLength );
		return nLength;
	}
	else
	{
		GetSingleton<IMainLoop>()->Command( pCmdNextInterface );
		return 0;
	}
}
void CInterfaceScreenBase::Step( bool bAppActive )
{
	// The OS moved the window to another display (drag, arrangement change,
	// unplug) - GameMain's event pump has already pointed GFX.Monitor.Index at
	// it. In fullscreen the mode must follow so the picture covers the display
	// it now lives on at that display's resolution; the option value is
	// updated too so the settings screen agrees with where the game really is.
	if ( GetGlobalVar( "GFX.DisplayChanged", 0 ) != 0 )
	{
		SetGlobalVar( "GFX.DisplayChanged", 0 );
		if ( GetGlobalVar( "GFX.Mode.Current.FullScreen", 0 ) == int( GFXFS_FULLSCREEN ) )
		{
			GetSingleton<IOptionSystem>()->Set( "GFX.Monitor",
				variant_t( NStr::Format( "Monitor%d", GetGlobalVar( "GFX.Monitor.Index", 0 ) + 1 ) ) );
			if ( ChangeResolution() )
				GetSingleton<IScene>()->Reposition();
		}
	}
	// A live window resize changes the drawable; the mission scene follows it
	// 1:1 and cfg_eff re-clamps, and menus/videos re-clamp their cfg_eff cap
	// too (docs/superpowers/specs/2026-08-12-resolution-presentation-design.md).
	// The flag is still consumed/reset here for anything else watching it;
	// the diff itself no longer gates on it - the unconditional check below
	// already re-runs every frame regardless of what changed.
	if ( GetGlobalVar( "GFX.DrawableChanged", 0 ) != 0 )
		SetGlobalVar( "GFX.DrawableChanged", 0 );
	// The active screen notices a mode diff every frame, not only on a focus
	// change or a drawable/display event: an options screen is a different
	// screen instance (and often a different szInterfaceType, e.g. "Current"
	// for the in-mission overlay) than the one whose GFX.Mode.<type>.* it
	// just edited, so the screen that owns that type needs to pick the
	// change up on its own next step rather than wait for a focus change.
	// ChangeResolution() is already a cheap, internally-diffed no-op when
	// nothing changed (a handful of GetGlobalVar reads), so this costs
	// nothing in the common case.
	if ( ChangeResolution() )
		GetSingleton<IScene>()->Reposition();
	// Asserted every frame, not only in OnGetFocus: the direct save launch
	// activates the mission screen without a focus notification, and a stale
	// fit mode would scale gameplay or clip a menu.
	if ( szInterfaceType == "Mission" )
		SetGlobalVar( "GFX.Present.Fit", 0 );
	else if ( szInterfaceType != "Current" )
		SetGlobalVar( "GFX.Present.Fit", 1 );
	// The texture-quality option action runs in the StreamIO bridge, which
	// cannot reach ITextureManager; it leaves the chosen value here instead.
	// Applies to textures loaded from now on - already-resident ones keep the
	// files they were created from.
	{
		const std::string szTextureQuality = GetGlobalVar( "GFX.Texture.QualityPending", "" );
		if ( !szTextureQuality.empty() )
		{
			SetGlobalVar( "GFX.Texture.QualityPending", "" );
			ITextureManager::ETextureQuality eQuality = ITextureManager::TEXTURE_QUALITY_HIGH;
			if ( szTextureQuality == "Ultra" ) eQuality = ITextureManager::TEXTURE_QUALITY_ULTRA;
			else if ( szTextureQuality == "Compressed" ) eQuality = ITextureManager::TEXTURE_QUALITY_COMPRESSED;
			else if ( szTextureQuality == "Low" ) eQuality = ITextureManager::TEXTURE_QUALITY_LOW;
			GetSingleton<ITextureManager>()->SetQuality( eQuality );
		}
	}
	if ( GetGlobalVar( "X64.ReferenceScene", 0 ) == 0 )
		pCamera->Update();
	pGFX->SetViewTransform( pCamera->GetPlacement() );
	const bool bStepLocalResult = GetGlobalVar( "X64.ReferenceScene", 0 ) != 0 ? true : StepLocal( bAppActive );
	const bool bGFXActive = pGFX->IsActive();
	if ( (bStepLocalResult == false) || (bAppActive == false) || !bGFXActive )
	{
		static DWORD dwLastLoadTrace = 0;
		const DWORD dwNow = NPlatform::MonotonicMilliseconds();
		if ( NPlatform::MillisecondsElapsed( dwLastLoadTrace, dwNow ) >= 1000 )
		{
			dwLastLoadTrace = dwNow;
			const char *pszBaseDir = GetSingleton<IMainLoop>() ? GetSingleton<IMainLoop>()->GetBaseDir() : 0;
			if ( pszBaseDir )
			{
				const std::string szTraceFileName = std::string( pszBaseDir ) + "load_trace.log";
				FILE *pFile = fopen( szTraceFileName.c_str(), "ab" );
				if ( pFile )
				{
					fprintf( pFile, "%lu CInterfaceScreenBase::Step skipped draw app=%d local=%d gfx=%d\n", (unsigned long)dwNow, bAppActive, bStepLocalResult, bGFXActive );
					fclose( pFile );
				}
			}
			// Keep diagnostics in load_trace.log only to avoid debugger output stalls while recovering from draw-skip states.
		}
		NPlatform::SleepMilliseconds( 10 );
		return;
	}
	pScene->UpdateSound( pCamera );

	static const bool bPerf = getenv( "BK_PERF" ) != 0;
	LARGE_INTEGER pf_freq, pf0, pf1, pf2, pf3, pf4;
	if ( bPerf ) { QueryPerformanceFrequency( &pf_freq ); QueryPerformanceCounter( &pf0 ); }

	if ( !pGFX->BeginScene() )
		return;
	pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0 );
	if ( bPerf ) QueryPerformanceCounter( &pf1 );
  pScene->Draw( pCamera );
	if ( bPerf ) QueryPerformanceCounter( &pf2 );
	DrawAdd();
	AddStatistics();
	pGFX->EndScene();
	if ( bPerf ) QueryPerformanceCounter( &pf3 );
	pGFX->Flip();
	if ( bPerf )
	{
		QueryPerformanceCounter( &pf4 );
		static double acc_begin = 0, acc_draw = 0, acc_end = 0, acc_flip = 0;
		static int acc_frames = 0;
		const double s = 1000.0 / double( pf_freq.QuadPart );
		acc_begin += ( pf1.QuadPart - pf0.QuadPart ) * s;
		acc_draw  += ( pf2.QuadPart - pf1.QuadPart ) * s;
		acc_end   += ( pf3.QuadPart - pf2.QuadPart ) * s;
		acc_flip  += ( pf4.QuadPart - pf3.QuadPart ) * s;
		if ( ++acc_frames >= 60 )
		{
			fprintf( stderr, "BK_PERF: phases/frame  begin=%.2f  sceneDraw=%.2f  endScene=%.2f  flip=%.2f ms\n",
				acc_begin / acc_frames, acc_draw / acc_frames, acc_end / acc_frames, acc_flip / acc_frames );
			fflush( stderr );
			acc_begin = acc_draw = acc_end = acc_flip = 0; acc_frames = 0;
		}
	}
}
void CInterfaceScreenBase::AddStatistics()
{
	double fTimePassed = NHPTimer::GetTimePassed( &time );
	fTotalTime += fTimePassed;
	nTriCounter += pGFX->GetNumPassedPrimitives();
	++nFrameCounter;
	if ( fTotalTime > 1.0 )
	{
		fAveFPS = float( nFrameCounter ) / fTotalTime;
		fAveTPS = float( nTriCounter ) / fTotalTime;
		fTotalTime = 0;
		nFrameCounter = 0;
		nTriCounter = 0;
	}
	if ( bEnableStatistics )
	{
		IStatSystem *pStat = pScene->GetStatSystem();
		CTRect<long> rcScreen = pGFX->GetScreenRect();
		pStat->UpdateEntry( "resolution", NStr::Format( "%dx%dx%d", rcScreen.GetSizeX(), rcScreen.GetSizeY(), pGFX->GetScreenBPP() ) );
		pStat->UpdateEntry( "verts", pGFX->GetNumPassedVertices() );
		pStat->UpdateEntry( "tris", pGFX->GetNumPassedPrimitives() );
		pStat->UpdateEntry( "tps", fAveTPS );
		pStat->UpdateEntry( "fps", fAveFPS );
		pStat->UpdateEntry( "game time", pTimer->GetGameTime() );
		pStat->UpdateEntry( "segment time", pTimer->GetGameSegmentTime() );
		pStat->UpdateEntry( "speed", pTimer->GetSpeed() );

		pGFX->SetShadingEffect( 3 );
		pStat->Draw( pGFX );
	}
}
bool CInterfaceScreenBase::ProcessTextMessage( const STextMessage &msg )
{
	if ( pUIScreen )
	{
		pUIScreen->OnChar( msg.wChars[0], msg.nVirtualKey, msg.bPressed, E_KEYBOARD_FREE );
		SGameMessage uiMessage;
		while ( pUIScreen->GetMessage( &uiMessage) )
			ProcessMessage( uiMessage );
		return true;
	}
	return false;
}
void CInterfaceScreenBase::AddMessage( const SGameMessage &msg ) 
{ 
	switch( msg.nEventID )
	{
	case TUTORIAL_TRY_SHOW_IF_NOT_SHOWN:
		ShowTutorialIfNotShown();
		return;
	case HIDE_TUTORIAL_WINDOW_ID:
		break;
	case TUTORIAL_WINDOW_ID:
	case TUTORIAL_BUTTON_ID:
		{
			ShowTutorial();
		}
		break;
	default:
		messages.push_back( msg ); 
	}
}
CVec2 GetPosFromMsg( ICursor *pCursor, const SGameMessage &msg )
{
	if ( (msg.nParam & 0x40000000) == 0 )
		return pCursor->GetPos();
	else
		return CVec2( msg.nParam & 0x7fff, (msg.nParam >> 15) & 0x7fff );
}
bool CInterfaceScreenBase::ProcessUIMessage( const SGameMessage &msg )
{
	if ( pUIScreen )
	{
		switch ( msg.nEventID )
		{

		case CMD_MOUSE0_DBLCLK:
			if ( pUIScreen->OnLButtonDblClk( GetPosFromMsg(pCursor, msg) ) == false )
				ProcessAndAdd( msg );
			break;
		case CMD_BEGIN_ACTION1:
				if ( pUIScreen->OnLButtonDown( GetPosFromMsg(pCursor, msg), E_LBUTTONDOWN ) == false )
					ProcessAndAdd( msg );
				break;
			case CMD_END_ACTION1:
				if ( pUIScreen->OnLButtonUp( GetPosFromMsg(pCursor, msg), E_MOUSE_FREE ) == false )
					ProcessAndAdd( msg );
				break;
			case CMD_BEGIN_ACTION2:
				if ( pUIScreen->OnRButtonDown( GetPosFromMsg(pCursor, msg), E_RBUTTONDOWN ) == false )
					ProcessAndAdd( msg );
				break;
			case CMD_END_ACTION2:
				if ( pUIScreen->OnRButtonUp( GetPosFromMsg(pCursor, msg), E_MOUSE_FREE ) == false )
					ProcessAndAdd( msg );
				break;
			case -1:
				break;
			default:
				pUIScreen->ProcessGameMessage( msg );
		}

		SGameMessage uiMessage;
		while ( pUIScreen->GetMessage( &uiMessage) )
			ProcessAndAdd( uiMessage );

		return messages.empty();
	}
	else
		return ProcessAndAdd( msg );
}
bool CInterfaceScreenBase::GetMessage( SGameMessage *pMsg )
{
	if ( messages.empty() )
		return false;
	*pMsg = messages.front();
	messages.pop_front();
	return true;
}
bool CInterfaceScreenBase::ChangeResolution()
{
	int nDesiredSizeX = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".SizeX").c_str(), GFX_DEFAULT_SCREEN_WIDTH );
	int nDesiredSizeY = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".SizeY").c_str(), GFX_DEFAULT_SCREEN_HEIGHT );
	// Captured before the Mission override below replaces nDesiredSizeX/Y with
	// the drawable: SetMode's write-back must persist the *configured* value
	// (0 means Auto) back to GFX.Mode.<type>.SizeX/Y, the same global this read
	// just consumed as cfg. Persisting the drawable there instead would corrupt
	// cfg on the next call - a second live resize would clamp against the
	// previous drawable rather than the real setting, and Auto would stop
	// tracking and pin to whatever the drawable happened to be.
	const int nConfiguredSizeX = nDesiredSizeX;
	const int nConfiguredSizeY = nDesiredSizeY;
	// Needed below (windowed vs fullscreen changes how the clamp works, not
	// just the diff further down where this is read again for nCurrentFullScreen's
	// default): the *desired* value, not GFX.Mode.Current.FullScreen's old one -
	// during a fullscreen<->windowed toggle this call is exactly what decides
	// which mode SetMode below actually runs in, so the clamp must follow the
	// same target, not the mode we are about to leave.
	const int nDesiredFullScreen = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".FullScreen").c_str(), 0 );
	const bool bWindowed = nDesiredFullScreen != int( GFXFS_FULLSCREEN );
	// Shared "GFX.Mode.<type>." prefix - built once and reused both by the
	// AppliedSizeX/Y read just below and by the write-back inside the diff
	// block further down.
	const std::string szModePrefix = "GFX.Mode." + szInterfaceType + ".";
	// The configured resolution is not a literal render size any more
	// (docs/superpowers/specs/2026-08-12-resolution-presentation-design.md):
	// missions render at the drawable and use the configuration as the world
	// view / HUD base; menus and videos render at cfg_eff = min(cfg, drawable)
	// per axis - a resolution lower than the screen still renders 1:1 (the
	// existing shrink-only blit puts it in a black frame), a larger one now
	// caps at the drawable instead of rendering oversized and shrinking.
	// That cap is a fullscreen rule though: the drawable there IS the display,
	// a fixed ceiling. In WINDOWED mode the drawable is just the window's
	// *current* size, and capping the SetMode request to it means an explicit
	// resolution bigger than today's window can never ask the window to grow
	// - the request clamps down to the very thing it is supposed to change,
	// so only shrinking ever worked. An explicit (non-Auto) windowed request
	// therefore skips the cap and asks SetMode for cfg outright; SetMode's own
	// windowed path (GraphicsEngineGpu.cpp) already clamps the actual window
	// resize to the display's usable bounds and keeps the scene at the
	// request, so this can never ask for an off-screen window. Auto (0) keeps
	// tracking the drawable as before - there is no larger configured size to
	// grow toward.
	const int nDrawableX = GetGlobalVar( "GFX.Drawable.SizeX", 0 );
	const int nDrawableY = GetGlobalVar( "GFX.Drawable.SizeY", 0 );
	const bool bDrawableValid = nDrawableX > 0 && nDrawableY > 0;
	// One-shot guard on the windowed cfg-changed clamp bypass: without it, once
	// bWindowedCfgChanged goes true it never goes false again (cfg keeps
	// re-matching itself forever), which pins nDesiredSizeX/Y at cfg on every
	// call - including calls that only re-diffed because something ELSE
	// changed (Mission's world-base term reacts to the drawable every frame).
	// A manual window shrink after a windowed apply is exactly that: the
	// drawable changes, world-base's diff fires, and this call would
	// otherwise re-request the old cfg and snap the window straight back -
	// the window could never be shrunk below the last applied size. Comparing
	// against the size actually asked for last time (AppliedSizeX/Y, written
	// alongside SizeX/Y below) means the bypass fires exactly once per
	// genuine configured-value change - including a change TO Auto (cfg 0/0)
	// - once SetMode has been asked for this cfg, later calls fall back to
	// normal drawable-tracking and a manual resize sticks.
	const int nAppliedSizeX = GetGlobalVar( (szModePrefix + "AppliedSizeX").c_str(), -1 );
	const int nAppliedSizeY = GetGlobalVar( (szModePrefix + "AppliedSizeY").c_str(), -1 );
	// Read here (ahead of nCurrentFullScreen's other use in the diff further
	// down) so the windowed cfg-changed bypass can also fire on a fullscreen
	// mode-family transition, not only on a genuine cfg change. Without this,
	// a fullscreen->windowed toggle leaves bWindowedCfgChanged false (cfg ==
	// AppliedSize, both untouched by the toggle), the Mission branch below
	// requests the drawable instead of cfg, and the window comes back at
	// display size instead of the configured preset - re-selecting the same
	// resolution afterward is then a permanent no-op (cfg, AppliedSize and
	// the option value never change).
	const int nCurrentFullScreen = GetGlobalVar( "GFX.Mode.Current.FullScreen", nDesiredFullScreen );
	// No "cfg > 0" guard here (there was one): Auto (cfg 0/0) needs the same
	// one-shot bypass an explicit resolution gets below, or a windowed
	// explicit->Auto switch is a permanent no-op. Without the bypass, the
	// branches below substitute nDesiredSizeX/Y with nDrawableX/Y - the
	// window's CURRENT drawable size - before SetMode ever sees the change,
	// so switching windowed 800x600 to Auto asks SetMode for 800x600 again
	// (today's drawable) instead of 0/0, the diff against
	// GFX.Mode.Current.SizeX/Y never fires, and the window never grows. With
	// the guard dropped, cfg 0/0 rides through to SetMode unclamped this one
	// call, which resolves it to the desktop size (GraphicsEngineGpu.cpp's
	// own auto path) and grows the window; AppliedSizeX/Y then records 0/0,
	// so the very next call falls back to the stable drawable-tracking path
	// below (now tracking the desktop size it just grew to).
	const bool bWindowedCfgChanged = bWindowed
		&& ( nDesiredSizeX != nAppliedSizeX || nDesiredSizeY != nAppliedSizeY
			|| nDesiredFullScreen != nCurrentFullScreen );
	int nWorldBaseX = 0, nWorldBaseY = 0;
	if ( szInterfaceType == "Mission" && bDrawableValid )
	{
		nWorldBaseX = nDesiredSizeX > 0 ? Min( nDesiredSizeX, nDrawableX ) : nDrawableX;
		nWorldBaseY = nDesiredSizeY > 0 ? Min( nDesiredSizeY, nDrawableY ) : nDrawableY;
		if ( !bWindowedCfgChanged )
		{
			nDesiredSizeX = nDrawableX;
			nDesiredSizeY = nDrawableY;
		}
		// else: the windowed cfg just changed - an explicit resolution or a
		// switch to/from Auto - so nDesiredSizeX/Y stays cfg (unclamped: 0/0
		// for Auto, the requested value for explicit) and the SetMode call
		// below asks for cfg outright (a mission player applying 800x600
		// windowed expects the window to become 800x600; switching to Auto
		// expects it to grow to the desktop). The world base above, which is
		// what missions render at for HUD/zoom purposes, still tracks
		// min(cfg, drawable) exactly as before and catches up to the resized
		// drawable on the very next call.
	}
	// "Current" is excluded: its own GFX.Mode.Current.SizeX/Y read above is
	// the same global the diff below compares against (nCurrentSizeX/Y), so
	// clamping it here would manufacture a diff against itself every frame
	// instead of staying the permanent no-op "inherit the screen below me"
	// is designed to be.
	else if ( szInterfaceType != "Current" && bDrawableValid && !bWindowedCfgChanged )
	{
		nDesiredSizeX = nDesiredSizeX > 0 ? Min( nDesiredSizeX, nDrawableX ) : nDrawableX;
		nDesiredSizeY = nDesiredSizeY > 0 ? Min( nDesiredSizeY, nDrawableY ) : nDrawableY;
	}
	// Whether THIS call's nDesiredSizeX/Y was rewritten away from cfg above
	// (Mission's drawable override, or a menu/video's cfg_eff clamp): the
	// write-back below must persist nConfiguredSizeX/Y in that case, exactly
	// as Task 4 established for Mission - otherwise the clamped/adopted size
	// would echo back into GFX.Mode.<type>.SizeX/Y and corrupt cfg for the
	// next call (see the comment on nConfiguredSizeX/Y above).
	const bool bCfgClamped = (szInterfaceType == "Mission") || (szInterfaceType != "Current" && bDrawableValid);
	const int nDesiredStencil = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".Stencil").c_str(), 0 );
	const int nDesiredBPP = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".BPP").c_str(), 16 );
	const int nCurrentSizeX = GetGlobalVar( "GFX.Mode.Current.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
	const int nCurrentSizeY = GetGlobalVar( "GFX.Mode.Current.SizeY", GFX_DEFAULT_SCREEN_HEIGHT );
	const int nCurrentStencil = GetGlobalVar( "GFX.Mode.Current.Stencil", 0 );
	const int nCurrentBPP = GetGlobalVar( "GFX.Mode.Current.BPP", 16 );
	// The monitor choice participates in the mode diff so that picking another
	// display in the options re-runs SetMode (which moves the window) even when
	// the resolution itself is unchanged. "Current" defaults to the desired
	// value: an unset copy means SetMode already honored the choice at startup.
	const int nDesiredMonitor = GetGlobalVar( "GFX.Monitor.Index", 0 );
	const int nCurrentMonitor = GetGlobalVar( "GFX.Monitor.Current.Index", nDesiredMonitor );
	// The fullscreen flag is part of the diff so the options' Fullscreen
	// ON/OFF row takes effect even when nothing else changed. (nDesiredFullScreen
	// and nCurrentFullScreen were already read above, before the clamp block -
	// the clamp needs windowed-vs-fullscreen too, not just this diff, and the
	// windowed-explicit bypass needs the transition itself.)
	// The world base has to be part of the diff too, for Mission only: its
	// nDesiredSizeX/Y IS the drawable, not cfg, so a resolution change that
	// does not also happen to change the drawable (the ordinary case - the
	// window does not resize just because the option changed) would
	// otherwise leave every other input above identical to last call. Without
	// this, the whole block below - including the SetGlobalVar that publishes
	// GFX.World.BaseSizeX/Y - would never run, so the HUD and world zoom
	// (which read that global, not cfg directly) would silently keep the old
	// resolution's size until something else changed the drawable. This is
	// the mechanism behind "the resolution option does not apply in a
	// mission": the option's value changed, but nothing downstream noticed.
	// Scoped to Mission: every other type's own nWorldBaseX/Y stays the fixed
	// 0 initializer (world base is Mission-only), so comparing it against the
	// shared GFX.World.BaseSizeX/Y global - which Mission may have legitimately
	// published a nonzero value into on an earlier step, and Step() now calls
	// every interface on the stack every frame, not just the focused one -
	// would otherwise manufacture a permanent diff for every other type,
	// fighting Mission's own publish every frame (SetMode called forever,
	// each side repeatedly overwriting the global back to its own 0/nonzero).
	const bool bMission = szInterfaceType == "Mission";
	const int nCurrentWorldBaseX = bMission ? GetGlobalVar( "GFX.World.BaseSizeX", 0 ) : nWorldBaseX;
	const int nCurrentWorldBaseY = bMission ? GetGlobalVar( "GFX.World.BaseSizeY", 0 ) : nWorldBaseY;
	if ( (nDesiredSizeX != nCurrentSizeX) || (nDesiredSizeY != nCurrentSizeY) ||
		   (nDesiredStencil != nCurrentStencil) || (nDesiredBPP != nCurrentBPP) ||
		   (nDesiredMonitor != nCurrentMonitor) || (nDesiredFullScreen != nCurrentFullScreen) ||
		   (nWorldBaseX != nCurrentWorldBaseX) || (nWorldBaseY != nCurrentWorldBaseY) )
	{
		const EGFXFullscreen eFullScreen = (EGFXFullscreen)nDesiredFullScreen;
		const int nDesiredFreq = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".Frequency").c_str(), 0 );
		if ( pGFX->SetMode( nDesiredSizeX, nDesiredSizeY, nDesiredBPP, nDesiredStencil, eFullScreen, nDesiredFreq ) == false )
		{
			NI_ASSERT_T( false, NStr::Format("Can't set mode (%d:%d:%d:%d) for interface screen type \"%s\"", nDesiredSizeX, nDesiredSizeY, nDesiredBPP, nDesiredStencil, szInterfaceType.c_str()) );
		}
		const CTRect<long> rcScreen = pGFX->GetScreenRect();
		const int nActualSizeX = rcScreen.Width();
		const int nActualSizeY = rcScreen.Height();
		const int nActualBPP = pGFX->GetScreenBPP();
		// A clamped type (Mission always; menus/videos when the drawable is
		// valid) writes the configured size back here, not the actual/adopted
		// one: nActualSizeX/Y is the drawable-derived, clamped value in that
		// case (see cfg_eff above), and echoing it back into
		// GFX.Mode.<type>.SizeX/Y would overwrite cfg with it. "Current" and
		// any type seen before the drawable is published still echo the
		// actual adopted size, unchanged from before this task.
		SetGlobalVar( (szModePrefix + "SizeX").c_str(), bCfgClamped ? nConfiguredSizeX : nActualSizeX );
		SetGlobalVar( (szModePrefix + "SizeY").c_str(), bCfgClamped ? nConfiguredSizeY : nActualSizeY );
		SetGlobalVar( (szModePrefix + "BPP").c_str(), nActualBPP );
		// One-shot marker consumed by bWindowedCfgChanged above: records the
		// configured size SetMode was actually asked for this call, so the
		// windowed cfg-changed clamp bypass fires once per genuine cfg change
		// (including a change to/from Auto) and then gets out of the way for
		// manual window resizes (see the comment on bWindowedCfgChanged).
		SetGlobalVar( (szModePrefix + "AppliedSizeX").c_str(), nConfiguredSizeX );
		SetGlobalVar( (szModePrefix + "AppliedSizeY").c_str(), nConfiguredSizeY );
		SetGlobalVar( "GFX.Mode.Current.SizeX", nActualSizeX );
		SetGlobalVar( "GFX.Mode.Current.SizeY", nActualSizeY );
		SetGlobalVar( "GFX.Mode.Current.BPP", nActualBPP );
		SetGlobalVar( "GFX.Mode.Current.Stencil", nDesiredStencil );
		SetGlobalVar( "GFX.Mode.Current.FullScreen", int( eFullScreen ) );
		SetGlobalVar( "GFX.Mode.Current.Frequency", nDesiredFreq );
		SetGlobalVar( "GFX.Monitor.Current.Index", nDesiredMonitor );
		SetGlobalVar( "GFX.World.BaseSizeX", nWorldBaseX );
		SetGlobalVar( "GFX.World.BaseSizeY", nWorldBaseY );
		// World base and the cfg written back to GFX.Mode.<type>.SizeX/Y are
		// otherwise invisible outside a debugger; mirrors the GraphicsEngineGpu
		// SetMode trace so a live resize's effect on both is visible in the
		// same log.
		static const bool bGfxTrace = getenv( "BK_GFX_TRACE" ) != 0;
		if ( bGfxTrace )
			fprintf( stderr, "BK_GFX_TRACE: ChangeResolution type=%s cfg %dx%d world-base %dx%d\n",
				szInterfaceType.c_str(), nConfiguredSizeX, nConfiguredSizeY, nWorldBaseX, nWorldBaseY );
		pGFX->SetCullMode( GFXC_CW );	// setup right-handed coordinate system
		SHMatrix matrix;
		CreateOrthographicProjectionMatrixRH( &matrix, rcScreen.Width(), rcScreen.Height(), 1, 1024*8 + rcScreen.Height()*2 );
		pGFX->SetProjectionTransform( matrix );
		pGFX->EnableLighting( false );
		ICursor *pCursor = GetSingleton<ICursor>();
		pCursor->SetBounds( 0, 0, rcScreen.Width(), rcScreen.Height() );
		pCursor->SetPos( rcScreen.Width()/2, rcScreen.Height()/2 );
		return true;
	}
	return false;
}
void CInterfaceScreenBase::OnGetFocus( bool bFocus )
{
	if ( bFocus )
	{
		// How the scene is presented when its resolution differs from the
		// window: gameplay keeps exact pixels (centered, borders/crop), menus
		// and videos aspect-fit scale so no control is ever clipped away.
		// "Current" overlays (in-mission dialogs, list screens) keep whatever
		// presentation the screen below them established.
		if ( szInterfaceType == "Mission" )
			SetGlobalVar( "GFX.Present.Fit", 0 );
		else if ( szInterfaceType != "Current" )
			SetGlobalVar( "GFX.Present.Fit", 1 );
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
		if ( (pInput != 0) && !szBindSection.empty() )
			pInput->SetBindSection( szBindSection.c_str() );
		if ( ChangeResolution() )
			GetSingleton<IScene>()->Reposition();
		if ( GetGlobalVar("CurtainsClosed", 0) != 0 )
		{
			if ( OpenCurtains() )
				RemoveGlobalVar( "CurtainsClosed" );
		}
		else
			ShowTutorialIfNotShown();
	}
}
void CInterfaceScreenBase::StoreScreen()
{
	if ( pScene ) 
	{
		CPtr<IUIScreen> pScreen2Store = pScene->GetUIScreen();
		NI_ASSERT_T( pScreen2Store.GetPtr() != pUIScreen.GetPtr(), "Can't store UI screen here - call it before setting own screen!!!" );
		pStoredScreen = pScreen2Store;
		if ( pStoredScreen != 0 ) 
			pScene->RemoveUIScreen( pStoredScreen );
	}
}
void CInterfaceScreenBase::RestoreScreen()
{
	if ( pScene && pStoredScreen ) 
	{
		pScene->AddUIScreen( pStoredScreen );
		pStoredScreen = 0;
	}
}
void CInterfaceScreenBase::ShowTutorialIfNotShown()
{
	if ( !GetSingleton<IUserProfile>()->IsHelpCalled( GetCommonFactory()->GetObjectTypeID( this ), nHelpContextNumber ) )
	{	//ShowTutorial();
		pInput->AddMessage( SGameMessage( TUTORIAL_WINDOW_ID ) );
	}
}
bool CInterfaceScreenBase::ShowTutorial()
{
	if ( pUIScreen == 0 ) 
		return true;

	IUIElement *pTutorial = pUIScreen->GetChildByID( TUTORIAL_WINDOW_ID );
	if ( pTutorial != 0 )
	{
		GetSingleton<IUserProfile>()->HelpCalled( GetCommonFactory()->GetObjectTypeID( this ), nHelpContextNumber );

		const WORD *pText = pTutorial->GetWindowText( nHelpContextNumber );
		if ( pText && pText[0] != 0 )
		{
			SetGlobalVar( "TutorialText", pText );
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_IM_TUTORIAL, 0 );
			return true;
		}
	}
	return false;
}
void CInterfaceScreenBase::CloseInterface( const bool bCurtains )
{
	if ( !bInterfaceClosed )
	{
		if ( bCurtains )
			FinishInterface( MAIN_COMMAND_POP, 0 );
		else
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_POP, 0 );
		bInterfaceClosed = true;
	}
}
int CInterfaceScreenBase::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &bEnableStatistics );
	saver.Add( 3, &fTotalTime );
	saver.Add( 4, &nFrameCounter );
	saver.Add( 5, &nTriCounter );
	saver.Add( 6, &fAveFPS );
	saver.Add( 7, &fAveTPS );
	saver.Add( 8, &vLastCursorPos );
	saver.Add( 9, &bLastCursorScreenMoveRes );
	saver.Add( 10, &pLastToolTip );
	saver.Add( 11, &timeToolTip );
	saver.Add( 12, &szBindSection );
	saver.Add( 13, &messages );
	saver.Add( 14, &pStoredScreen );
	saver.Add( 15, &pUIScreen );
	saver.Add( 16, &nHelpContextNumber );
	saver.Add( 17, &bInterfaceClosed );
	if ( saver.IsReading() ) 
	{
		timeToolTipShowTime = GetGlobalVar( "Scene.ToolTipTime.Show", 1000 );
		timeToolTipHideTime = GetGlobalVar( "Scene.ToolTipTime.Hide", 10000 );
		NHPTimer::GetTime( &time );
		nCPUFreq = int( NHPTimer::GetClockRate() / 1000000.0 + 0.5 );
	}
	return 0;
}
