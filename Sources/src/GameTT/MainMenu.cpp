#include "StdAfx.h"

#include "MainMenu.h"

#include "../Main/ScenarioTracker.h"
#include "MultiplayerCommandManager.h"
#include "../StreamIO/OptionSystem.h"
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
CInterfaceMainMenu::CInterfaceMainMenu() : CInterfaceInterMission( "InterMission" ), nActiveState( 0 )
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
	char buffer[2048];
	GetModuleFileName( 0, buffer, 2048 );
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
bool CInterfaceMainMenu::ProcessMessage( const SGameMessage &msg )
{
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