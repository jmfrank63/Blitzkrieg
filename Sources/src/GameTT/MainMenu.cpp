#include "StdAfx.h"

#include "MainMenu.h"

#include "../Main/ScenarioTracker.h"
#include "../Main/iMainCommands.h"
#include "MultiplayerCommandManager.h"
#include "../StreamIO/OptionSystem.h"
#include "../Platform/System.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "show_console"		, MC_SHOW_CONSOLE		},
	{ "inter_cancel_credits", MC_CANCEL_CREDITS},
	{ 0									,	0									}
};
void ResetCampaignStatus()
{
	IGlobalVars *pGV = GetSingleton<IGlobalVars>();
	pGV->RemoveVarsByMatch( "Campaign." );
	pGV->RemoveVarsByMatch( "Chapter." );
	pGV->RemoveVarsByMatch( "Mission." );
	pGV->RemoveVarsByMatch( "Map." );
	pGV->RemoveVarsByMatch( "temp." );
	if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
	{
		pGV->DumpVars( NStr::Format( "%s%s", pDataStorage->GetName(), "logs\\vars.txt" ) );
	}
	/*
	IScenarioTracker *pScenarioTracker = GetSingleton<IScenarioTracker>();
	pScenarioTracker->Clear();
	
	pScenarioTracker->AddDefaultPlayers();
	
	IOptionSystem *pOptions = GetSingleton<IOptionSystem>();
	const SOptionDesc *pDesc = pOptions->GetDesc( "GamePlay.PlayerName" );
	
	variant_t varPlayerName;
	pOptions->Get( "GamePlay.PlayerName", &varPlayerName );
	std::wstring wszNameFromOptions = (wchar_t*)(bstr_t)varPlayerName;
	IPlayerProfile *pProfile = pScenarioTracker->GetPlayer( pScenarioTracker->GetCurrentPlayer() );
	pProfile->SetPlayerName( wszNameFromOptions.c_str() );
	*/
}
void CICMainMenu::Configure( const char *pszConfig )
{
	if ( pszConfig == 0 )
		return;

	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ';' );
	nState = NStr::ToInt( szStrings[0] );
	if ( szStrings.size() > 1 ) 
	{
		nNextIC = NStr::ToInt( szStrings[1] );
		for ( int i = 2; i < szStrings.size(); ++i )
			szNextICConfig += szStrings[i] + ';';
		if ( !szNextICConfig.empty() && szNextICConfig[szNextICConfig.size() - 1] == ';' ) 
			szNextICConfig.resize( szNextICConfig.size() - 1 );
	}
	else
		nNextIC = -1;
}
void CICMainMenu::PostCreate( IMainLoop *pML, CInterfaceMainMenu *pIMM )
{
	if ( GetGlobalVar( "demoversion", 0 ) )
	{
		nState = CInterfaceMainMenu::E_DEMOVERSION_MAIN_MENU;
	}
	pIMM->Create( nState );
	pML->PushInterface( pIMM );
	if ( nNextIC != -1 ) 
		pML->Command( nNextIC, szNextICConfig.c_str() );
}
int CICMainMenu::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nState );
	saver.Add( 2, &nNextIC );
	saver.Add( 3, &szNextICConfig );
	return 0;
}
CInterfaceMainMenu::CInterfaceMainMenu() : CInterfaceInterMission( "InterMission" ), nActiveState( 0 ), nCloudLastState( 0 ), bCloudSkipRequested( false )
{
	mainMenuState.Init( this );
	newGameState.Init( this );
	selectCampaignState.Init( this );
	optionsState.Init( this );
	multiplayerState.Init( this );
	customGameState.Init( this );
	loadGameState.Init( this );
	creditsState.Init( this );
	demoMainMenu.Init( this );
	
	states.push_back( &mainMenuState );
	states.push_back( &newGameState );
	states.push_back( &selectCampaignState );
	states.push_back( &optionsState );
	states.push_back( &multiplayerState );
	states.push_back( &customGameState );
	states.push_back( &loadGameState );
	states.push_back( &creditsState );
	states.push_back( &demoMainMenu );
}
CInterfaceMainMenu::~CInterfaceMainMenu()
{
}
void CInterfaceMainMenu::PlayIntermissionSound()
{
	std::string szInterMissionStreamSound = GetGlobalVar( "InterMissionStreamSound", "" );
	int nTimeToFade = GetGlobalVar( "Sound.TimeToFade", 5000 );
	if ( szInterMissionStreamSound.size() > 0 )
	{
		GetSingleton<IScene>()->SetSoundSceneMode( ESSM_INTERMISSION_INTERFACE );
		GetSingleton<ISFX>()->PlayStream( szInterMissionStreamSound.c_str(), true, nTimeToFade );
	}
}
void CInterfaceMainMenu::OnGetFocus( bool bFocus )
{
	CInterfaceInterMission::OnGetFocus( bFocus );
	if ( GetGlobalVar( "MOD.Active", 0 ) )
	{
		IUIElement * pEl = pUIScreen->GetChildByID( 667 );
		std::string szModInfo = GetGlobalVar( "MOD.Name", "" );
		szModInfo += GetGlobalVar( "MOD.Version", "" );
		const std::wstring wszModInfo = NStr::ToUnicode( szModInfo );
		pEl->SetWindowText( 0, ToWordString( wszModInfo ) );
	}

	if ( bFocus && GetGlobalVar( "EnterMultiplauer.Confirm", 0 ) )
	{
		RemoveGlobalVar( "EnterMultiplauer.Confirm" );
		SetActiveState( CInterfaceMainMenu::E_MULTIPLAYER );
	}
}
bool CInterfaceMainMenu::Init()
{
	ResetCampaignStatus();
	PlayIntermissionSound();
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );

	return true;
}
void CInterfaceMainMenu::RefreshCursor()
{
	OnCursorMove( VNULL2 );
	OnCursorMove( pCursor->GetPos() );
}
bool GetFileVersion( const std::string &szFileName, VS_FIXEDFILEINFO *pVersionInfo )
{
	char pszLocalFileName[2048];
	strcpy( pszLocalFileName, szFileName.c_str() );
	DWORD dwLength = 0;
	const int nVersionSize = GetFileVersionInfoSize( pszLocalFileName, &dwLength );
	if ( nVersionSize == 0 ) 
		return false;
	std::vector<BYTE> buffer( nVersionSize );
	if ( GetFileVersionInfo(pszLocalFileName, 0, nVersionSize, &(buffer[0])) == FALSE )
		return false;
	VS_FIXEDFILEINFO *pFFI = 0;
	UINT uLength = 0;
	if ( VerQueryValue(&(buffer[0]), TEXT("\\"), (void**)&pFFI, &uLength) == FALSE )
		return false;
	if ( pFFI == 0 ) 
		return false;
	*pVersionInfo = *pFFI;
	return true;
}
std::string GetMainModuleVersion()
{
#ifdef BLITZKRIEG_VERSION
	return BLITZKRIEG_VERSION;
#else
	const std::string buffer = NPlatform::ExecutablePath() +
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
		"Game.exe";
#else
		"Game";
#endif
	VS_FIXEDFILEINFO version;
	if ( GetFileVersion(buffer, &version) != false )
		return NStr::Format( "%d.%d.%d", (version.dwProductVersionMS >> 16) & 0xffff, version.dwProductVersionMS & 0xffff, (version.dwProductVersionLS >> 16) & 0xffff );
	return "\"UNKNOWN\"";
#endif
}
void CInterfaceMainMenu::Create( int nState )
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\mainmenu" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	const std::string szCopyUserName = GetGlobalVar( "CopyUserName", "" );
	const std::string szVersion = GetMainModuleVersion();
	if ( !szCopyUserName.empty() ) 
	{		
		if ( IUIElement *pElement = pUIScreen->GetChildByID(666) )
		{
			const std::wstring szCopyOwner = NStr::ToUnicode( "Copy Owner: " + szCopyUserName );			
			pElement->SetWindowText( 0, ToWordString( szCopyOwner ) );
		}
	}
	else if ( !szVersion.empty() )
	{
		if ( IUIElement *pElement = pUIScreen->GetChildByID(666) )
		{
			std::wstring wszVersion;
			if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetDialog( "textes\\strings\\version" ) )
				wszVersion = MakeWideStringFromWordString( pText->GetString() );
			else
				wszVersion = L"Version:";
			wszVersion += L" " + NStr::ToUnicode( szVersion.c_str() );
			pElement->SetWindowText( 0, ToWordString( wszVersion ) );
		}
	}
	// Active profile name in the lower left, opposite the version label
	// (id 21000, the same convention as the settings screen). The name is
	// printable ASCII by contract (NProfile::Sanitize), so widening per
	// character is exact.
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	if ( !szProfile.empty() )
	{
		if ( IUIElement *pElement = pUIScreen->GetChildByID( 21000 ) )
		{
			std::wstring wszProfile;
			if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetDialog( "textes\\strings\\profile" ) )
				wszProfile = MakeWideStringFromWordString( pText->GetString() );
			else
				wszProfile = L"Profile:";
			wszProfile += L" ";
			for ( int i = 0; i < szProfile.size(); ++i )
				wszProfile += wchar_t( (unsigned char)szProfile[i] );
			pElement->SetWindowText( 0, ToWordString( wszProfile ) );
		}
	}
	pScene->AddUIScreen( pUIScreen );

	if ( nState == 99 )
	{
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUTSCENE_LIST, 0 );
		SetActiveState( E_OPTIONS );
		PlayIntermissionSound();
		return;
	}
	SetActiveState( nState );
	if ( GetGlobalVar( "X64.StartupSmoke", 0 ) != 0 )
	{
		SetGlobalVar( "X64.StartupSmoke.MainMenu", 1 );
		::OutputDebugStringA( "BK_STARTUP: main menu created\n" );
	}
}
// Which text under textes\ui\cloudsync\ a failure maps to. The classified
// outcome leads the failure text ("auth_failed: ..." - the worker's
// contract), so the branch reads the first word; "Cancelled" is the
// player's own skip and reads as offline, and anything unrecognized falls
// back to the generic message rather than a raw error on the menu.
// "unconfigured" is not a run outcome - the main loop publishes it when a
// provider is chosen but the saved credentials do not name it.
static const char *CloudFailureTextKey( const std::string &szError )
{
	if ( szError == "Cancelled" )
		return "offline";
	static const char *pszOutcomes[] = { "unconfigured", "needs_resync", "too_many_deletes", "name_too_long",
		"out_of_sync", "auth_failed", "remote_unreachable", "remote_missing",
		"daemon_gone", "timed_out", 0 };
	for ( int i = 0; pszOutcomes[i] != 0; ++i )
	{
		const int nLen = strlen( pszOutcomes[i] );
		if ( szError.compare( 0, nLen, pszOutcomes[i] ) == 0 )
			return pszOutcomes[i];
	}
	return "failed";
}
// The lower-left sync indicator (element 21001). The sync itself is owned
// by the main loop, which publishes CloudSync.State/Outcome/Error as
// global vars and honours CloudSync.SkipToOffline; the menu only renders
// and clicks. While a run is live the label is a button - the click is
// the skip - and once it settles it goes inert and just reports.
void CInterfaceMainMenu::RefreshCloudIndicator()
{
	if ( pUIScreen == 0 )
		return;
	IUIElement *pElement = pUIScreen->GetChildByID( 21001 );
	if ( pElement == 0 )
		return;
	const int nState = GetGlobalVar( "CloudSync.State", 0 );
	const bool bRunning = nState >= 1 && nState <= 3;
	// The skip choice outlives the run it cancelled: the eventual settle
	// text ("Cancelled", or whatever a hung transfer dies of later) must
	// not overwrite the player's answer. What clears it: a clean finish,
	// idle, or a settled->running edge - that is a new run's beginning.
	if ( nState == 4 || nState == 0 || ( bRunning && !( nCloudLastState >= 1 && nCloudLastState <= 3 ) ) )
		bCloudSkipRequested = false;
	nCloudLastState = nState;
	std::string szKey;
	bool bClickable = false;
	switch ( nState )
	{
		case 1:		// starting
		case 3:		// syncing
			szKey = "syncing";
			bClickable = true;
			break;
		case 2:		// pairing
			szKey = "pairing";
			bClickable = true;
			break;
		case 4:		// done
			szKey = GetGlobalVar( "CloudSync.Outcome", 0 ) == 1 ? "paired" : "synced";
			break;
		case 5:		// failed
			szKey = bCloudSkipRequested ? "offline" : CloudFailureTextKey( GetGlobalVar( "CloudSync.Error", "" ) );
			break;
		default:	// idle, or a state this menu does not narrate
			break;
	}
	if ( bCloudSkipRequested && bRunning )
	{
		// The click answers immediately; the cancel settles a moment later
		// and lands on the same text, so nothing flickers.
		szKey = "offline";
		bClickable = false;
	}
	if ( szKey == szCloudShownKey )
		return;
	szCloudShownKey = szKey;
	if ( szKey.empty() )
	{
		pElement->ShowWindow( UI_SW_HIDE );
		return;
	}
	std::wstring wszText;
	if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetDialog( ( "textes\\ui\\cloudsync\\" + szKey ).c_str() ) )
		wszText = MakeWideStringFromWordString( pText->GetString() );
	else
		wszText = L"Cloud: " + NStr::ToUnicode( szKey );
	pElement->SetWindowText( 0, ToWordString( wszText ) );
	pElement->EnableWindow( bClickable );
	pElement->ShowWindow( UI_SW_SHOW_DONT_MOVE_UP );
}
bool CInterfaceMainMenu::StepLocal( bool bAppActive )
{
	RefreshCloudIndicator();
	return CInterfaceInterMission::StepLocal( bAppActive );
}
bool CInterfaceMainMenu::ProcessMessage( const SGameMessage &msg )
{
	if ( msg.nEventID == CMD_END_ACTION1 )
	{
		// Skip to offline. The menu screen is modal (the exit-confirm
		// dialog's ModalFlag), so mouse picking only ever reaches the
		// active state dialog - a click on the lower-left indicator is
		// never consumed by the UI, and the button-up falls through to
		// here carrying its position. Hit-test it against the element's
		// own rect. The main loop owns the handle and does the
		// cancelling; the profile simply stays ahead of the cloud and a
		// later sync converges. Only meaningful mid-run.
		const int nState = GetGlobalVar( "CloudSync.State", 0 );
		if ( nState >= 1 && nState <= 3 && !bCloudSkipRequested && pUIScreen )
		{
			if ( IUIElement *pElement = pUIScreen->GetChildByID( 21001 ) )
			{
				const CVec2 vClick = ( msg.nParam & 0x40000000 )
					? CVec2( msg.nParam & 0x7fff, ( msg.nParam >> 15 ) & 0x7fff )
					: pCursor->GetPos();
				if ( pElement->IsVisible() && pElement->IsInside( vClick ) )
				{
					SetGlobalVar( "CloudSync.SkipToOffline", 1 );
					bCloudSkipRequested = true;
					RefreshCloudIndicator();
					return true;
				}
			}
		}
	}
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	return states[nActiveState]->ProcessMessage( msg );
}
void CInterfaceMainMenu::SetActiveState( int nState )
{
	states[nActiveState]->Hide();
	nActiveState = nState;
	states[nActiveState]->Show();
}
