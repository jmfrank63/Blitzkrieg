#include "StdAfx.h"

#include "InterfaceMPCreateGame.h"

#include "..\UI\UIMessages.h"
#include "CommonId.h"
#include "WorldClient.h"
#include "UIConsts.h"
#include "MinimapCreation.h"
#include "SaveLoadCommon.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum
{
	E_OPTIONS_LIST												= 1005,
	E_STATIC_GAME_TYPE										= 20002,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_cancel"		, IMC_CANCEL		},
	{ "inter_ok"		,			IMC_OK				},
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ "show_console"		, MC_SHOW_CONSOLE		},
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
	{ 0									,	0							}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceMPCreateGame::SComparePredicate::operator()( const struct SLoadFileDesc & f1, const struct SLoadFileDesc & f2 ) const
{
	return f1.szFileName == f2.szFileName;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EControlIDs
{
	E_MAPS_LIST					= 1000,
	E_MINIMAP						= 100,
	E_BUTTON_BACK				= 10001,
	E_BUTTON_CREATE			= 10004,
	E_BUTTON_SETTINGS		= 10005,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceMPCreateGame::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceMultiplayerScreen::ProcessMessage( msg ) )
		return true;

	if ( WCC_MULTIPLAYER_TO_UI_UPDATE == msg.nEventID )
	{
		SToUICommand cmd;
		while ( pCommandManager->GetCommandToUI( &cmd ) && ProcessMPCommand( cmd ) )
		{
		}
		
		return true;
	}

	if ( pMapSettingsWrapper && pMapSettingsWrapper->ProcessMessage( msg ) ) return true;
	
	//process buttons pressings
	switch( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );

		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );

		break;

	case E_BUTTON_BACK:
		if ( pCommandManager->GetConnectionType() == EMCT_INTERNET )
			FinishInterface( MISSION_COMMAND_ADDRESS_BOOK, 0 );
		else
			FinishInterface( MISSION_COMMAND_MULTIPLAYER_GAMESLIST, 0 );

		return true;
	case IMC_OK:
	case E_BUTTON_CREATE:
		{
			CreateGame();
		}

		return true;
	case E_BUTTON_SETTINGS:
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MP_MAP_SETTINGS, NStr::Format( "%d;0", false ) );
		break;

	case UI_NOTIFY_WINDOW_CLICKED:

		return true;
	case UI_NOTIFY_SELECTION_CHANGED:
		OnSelectionChanged();
		
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceMPCreateGame::CreateGame()
{
	SUIMapInfo * pInfo = mapsList.GetCurInfo();
	if ( pInfo )
	{
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_SWITCH_MODE_TO,
				NStr::Format( "%s;%s;%d;%d;0",	// 0 - not silent switch
												pInfo->mapInfo.szMODName.c_str(),
												pInfo->mapInfo.szMODVersion.c_str(),
												MISSION_COMMAND_MULTYPLAYER_CREATEGAME,
												E_DELAYED_UPDATE ) );

		pMapSettingsWrapper = new CMapSettingsWrapper( true, OPTION_FLAG_BEFORE_MULTIPLAYER_START );
		pMapSettingsWrapper->Init( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_OPTIONS_LIST ) ),
		checked_cast<IUIStatic*>( pUIScreen->GetChildByID( E_STATIC_GAME_TYPE ) ) );

		variant_t varPassword;
		GetSingleton<IOptionSystem>()->Get( "Multiplayer.ServerPassword", &varPassword );

		const std::string szPassword = NStr::ToAscii( std::wstring( _bstr_t( varPassword ) ) );
		//send info to Vitalik about selected map;
		CPtr<SNewMapInfo> pMapInfo = new SNewMapInfo( pInfo->szPath.c_str(), pInfo->mapInfo, pMapSettingsWrapper->GetSettings(), szPassword );
		SFromUINotification notify( EUTMN_CREATE, pMapInfo );
		pCommandManager->DelayedNotification( notify );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceMPCreateGame::Init()
{
	CInterfaceMultiplayerScreen::Init();
	//SetBindSection( "loadmission" );
	commandMsgs.Init( pInput, commands );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceMPCreateGame::Create( const /*ECreateGameMode*/ int eMode )
{
	CInterfaceMultiplayerScreen::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\MuptiplayerCreateGame" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	mapsList.SetListControl( checked_cast< IUIListControl *> ( pUIScreen->GetChildByID( E_MAPS_LIST ) ) );

	pMapSettingsWrapper = new CMapSettingsWrapper( true, OPTION_FLAG_BEFORE_MULTIPLAYER_START );
	pMapSettingsWrapper->Init( checked_cast<IUIListControl*>( pUIScreen->GetChildByID( E_OPTIONS_LIST ) ),
		checked_cast<IUIStatic*>( pUIScreen->GetChildByID( E_STATIC_GAME_TYPE ) ) );

	if ( eMode != E_DELAYED_UPDATE )
		PrepareMapsList();
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );

	if ( GetSingleton<IMPToUICommandManager>()->GetConnectionType() == EMCT_GAMESPY )
	{
		IUIElement * pGameSpyLogo = pUIScreen->GetChildByID( E_GAMESY_LOGO );
		if ( pGameSpyLogo )
			pGameSpyLogo->ShowWindow( UI_SW_SHOW );
	}

	if ( eMode == E_DELAYED_UPDATE )
		pCommandManager->SendDelayedNotification();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceMPCreateGame::PrepareMapsList()
{
	std::unordered_set<std::string> szFileNames;
	const std::string szPathBegin = "maps\\multiplayer\\";
	const std::string szMask1 = ".bzm";

	IFilesInspectorEntryCollector *pC = checked_cast<IFilesInspectorEntryCollector*>( GetSingleton<IFilesInspector>()->GetEntry( "maps_multiplayer_xml" ) );
	for ( std::vector<std::string>::const_iterator it = pC->GetCollected().begin(); it != pC->GetCollected().end(); ++it )
	{
		std::string szName = ((*it).c_str() + szPathBegin.size());
		szName.replace( szName.size() - szMask1.size(), 1, "", 1 );
		szFileNames.insert( szName );
	}
	pC = checked_cast<IFilesInspectorEntryCollector*>( GetSingleton<IFilesInspector>()->GetEntry( "maps_multiplayer_bzm" ) );
	for ( std::vector<std::string>::const_iterator it = pC->GetCollected().begin(); it != pC->GetCollected().end(); ++it )
	{
		std::string szName = ((*it).c_str() + szPathBegin.size());
		szName.replace( szName.size() - szMask1.size(), 1, "", 1 );
		szFileNames.insert( szName );
	}
	
	CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
	pProgress->Init( IMovieProgressHook::PT_MINIMAP );
	pProgress->SetNumSteps( szFileNames.size() );
	for ( std::unordered_set<std::string>::const_iterator it = szFileNames.begin(); it != szFileNames.end(); ++it )
	{
		const std::string &szFileName = *it;

		CPtr<SUIMapInfo> pInfo = new SUIMapInfo;
		if ( pInfo->LoadMapInfo( szFileName.c_str() ) )
		{
			IUIListRow * pRow = mapsList.Add( pInfo );
			//init row with data
			IUIStatic * pMapName = checked_cast<IUIStatic*>( pRow->GetElement( 0 ) );
			pMapName->SetWindowText( 0, pInfo->GetVisualName());
			
			IUIStatic *pGameType = checked_cast<IUIStatic*>( pRow->GetElement( 1 ) );
			pGameType->SetWindowText( 0, CUIConsts::GetMapTypeString( pInfo->mapInfo.nType ) );
			
			IUIStatic * pMaxPlayers = checked_cast<IUIStatic*>( pRow->GetElement( 2 ) );
			pMaxPlayers->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d", pInfo->mapInfo.playerParties.size() ) ).c_str() );

			IUIStatic *pMapSize = checked_cast<IUIStatic*>( pRow->GetElement( 3 ) );
			pMapSize->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d", pInfo->mapInfo.size.x ) ).c_str() );

			const std::string szMapName = "maps\\" + pInfo->szPath;
			CMinimapCreation::Create1Minimap( szMapName, szMapName );
		}
		pProgress->Step();
	}
	
	//	SELECT FIRST MAP
	if ( !szFileNames.empty() )
		mapsList.SelectFirst();

	OnSelectionChanged();
	pProgress->Stop();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceMPCreateGame::ProcessMPCommand( const SToUICommand &cmd )
{
	switch( cmd.eCommandID )
	{
	case EMTUC_CREATE_STAGING_ROOM:
		{
			FinishInterface( MISSION_COMMAND_MULTIPLAYER_STARTINGGAME, "0" );
		}
		return false;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceMPCreateGame::OnSelectionChanged()
{
	// apply parametres about newly selected map
	SUIMapInfo * pInfo = mapsList.GetCurInfo();

	IUIElement * pCreateButton = pUIScreen->GetChildByID( E_BUTTON_CREATE );

	if ( pInfo )
	{
		const std::string szMapName = "maps\\" + pInfo->szPath;
		CMinimapCreation::Create1Minimap( szMapName, szMapName );
		
		//��������� ���������� ������ ��� map image control
		IUIObjMap *pMap = checked_cast<IUIObjMap *> ( pUIScreen->GetChildByID( E_MINIMAP ) );
		IGFXTexture *pTexture = GetSingleton<ITextureManager>()->GetTexture(  CUIConsts::CreateTexturePathFromMapPath( pInfo->szPath.c_str() ).c_str() );
		if ( pTexture )
		{
			NI_ASSERT_T( pTexture != 0, "Mission map texture is invalid" );
			pMap->SetMapTexture( pTexture );
			pMap->Init();
		}	

		// map type
		// map size
		// map name
		
	}
	pCreateButton->EnableWindow
	(
		pInfo && 
		( ( pInfo->mapInfo.nType == CMapInfo::TYPE_FLAGCONTROL ) || ( pInfo->mapInfo.nType == CMapInfo::TYPE_SABOTAGE ) )
	);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
