#include "StdAfx.h"

#include <mmsystem.h>

#include "InterfaceScreenBase.h"
#include "..\Main\iMainCommands.h"
#include "..\AILogic\AILogic.h"
#include "..\GameTT\CommonID.h"
#include "..\Main\ScenarioTracker.h"
#include "..\GameTT\IMission.h"
BASIC_REGISTER_CLASS( CInterfaceScreenBase );
CInterfaceScreenBase::CInterfaceScreenBase( const std::string &_szInterfaceType )
: szInterfaceType( _szInterfaceType ), bInterfaceClosed( false )
{
	bEnableStatistics = false;
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
	const int nLength = pTransition->Start( pszName, dwAddFlags, timeGetTime(), bFadeIn );
	if ( nLength > 0 ) 
		pScene->AddSceneObject( pTransition );
	return nLength;
}
void CInterfaceScreenBase::OpenCurtainsForced()
{
	GetSingleton<IMainLoop>()->EnableMessageProcessing( false );
	const int nLength = PlayOverInterface( "movies\\transition\\open.bik", 0, false );
	EnableMessageProcessingDelayed( true, timeGetTime() + nLength );
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
		const int nTime = timeGetTime();
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
	if ( GetGlobalVar( "X64.ReferenceScene", 0 ) == 0 )
		pCamera->Update();
	pGFX->SetViewTransform( pCamera->GetPlacement() );
	const bool bStepLocalResult = GetGlobalVar( "X64.ReferenceScene", 0 ) != 0 ? true : StepLocal( bAppActive );
	const bool bGFXActive = pGFX->IsActive();
	if ( (bStepLocalResult == false) || (bAppActive == false) || !bGFXActive )
	{
		static DWORD dwLastLoadTrace = 0;
		const DWORD dwNow = GetTickCount();
		if ( dwNow > dwLastLoadTrace + 1000 )
		{
			dwLastLoadTrace = dwNow;
			const char *pszBaseDir = GetSingleton<IMainLoop>() ? GetSingleton<IMainLoop>()->GetBaseDir() : 0;
			if ( pszBaseDir )
			{
				const std::string szTraceFileName = std::string( pszBaseDir ) + "load_trace.log";
				FILE *pFile = fopen( szTraceFileName.c_str(), "ab" );
				if ( pFile )
				{
					fprintf( pFile, "%lu CInterfaceScreenBase::Step skipped draw app=%d local=%d gfx=%d\n", dwNow, bAppActive, bStepLocalResult, bGFXActive );
					fclose( pFile );
				}
			}
			// Keep diagnostics in load_trace.log only to avoid debugger output stalls while recovering from draw-skip states.
		}
		Sleep( 10 );
		return;
	}
	pScene->UpdateSound( pCamera );

	if ( !pGFX->BeginScene() )
		return;
	pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0 );
  pScene->Draw( pCamera );
	DrawAdd();
	AddStatistics();
	pGFX->EndScene();
	pGFX->Flip();
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
	const int nDesiredSizeX = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".SizeX").c_str(), GFX_DEFAULT_SCREEN_WIDTH );
	const int nDesiredSizeY = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".SizeY").c_str(), GFX_DEFAULT_SCREEN_HEIGHT );
	const int nDesiredStencil = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".Stencil").c_str(), 0 );
	const int nDesiredBPP = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".BPP").c_str(), 16 );
	const int nCurrentSizeX = GetGlobalVar( "GFX.Mode.Current.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
	const int nCurrentSizeY = GetGlobalVar( "GFX.Mode.Current.SizeY", GFX_DEFAULT_SCREEN_HEIGHT );
	const int nCurrentStencil = GetGlobalVar( "GFX.Mode.Current.Stencil", 0 );
	const int nCurrentBPP = GetGlobalVar( "GFX.Mode.Current.BPP", 16 );
	if ( (nDesiredSizeX != nCurrentSizeX) || (nDesiredSizeY != nCurrentSizeY) || 
		   (nDesiredStencil != nCurrentStencil) || (nDesiredBPP != nCurrentBPP) )
	{
		const EGFXFullscreen eFullScreen = (EGFXFullscreen)GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".FullScreen").c_str(), 0 );
		const int nDesiredFreq = GetGlobalVar( ("GFX.Mode." + szInterfaceType + ".Frequency").c_str(), 0 );
		if ( pGFX->SetMode( nDesiredSizeX, nDesiredSizeY, nDesiredBPP, nDesiredStencil, eFullScreen, nDesiredFreq ) == false )
		{
			NI_ASSERT_T( false, NStr::Format("Can't set mode (%d:%d:%d:%d) for interface screen type \"%s\"", nDesiredSizeX, nDesiredSizeY, nDesiredBPP, nDesiredStencil, szInterfaceType.c_str()) );
		}
		const CTRect<long> rcScreen = pGFX->GetScreenRect();
		const int nActualSizeX = rcScreen.Width();
		const int nActualSizeY = rcScreen.Height();
		const int nActualBPP = pGFX->GetScreenBPP();
		const std::string szModePrefix = "GFX.Mode." + szInterfaceType + ".";
		SetGlobalVar( (szModePrefix + "SizeX").c_str(), nActualSizeX );
		SetGlobalVar( (szModePrefix + "SizeY").c_str(), nActualSizeY );
		SetGlobalVar( (szModePrefix + "BPP").c_str(), nActualBPP );
		SetGlobalVar( "GFX.Mode.Current.SizeX", nActualSizeX );
		SetGlobalVar( "GFX.Mode.Current.SizeY", nActualSizeY );
		SetGlobalVar( "GFX.Mode.Current.BPP", nActualBPP );
		SetGlobalVar( "GFX.Mode.Current.Stencil", nDesiredStencil );
		SetGlobalVar( "GFX.Mode.Current.FullScreen", int( eFullScreen ) );
		SetGlobalVar( "GFX.Mode.Current.Frequency", nDesiredFreq );
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
