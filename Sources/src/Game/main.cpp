#include "StdAfx.h"

#include <shellapi.h>
#include <crtdbg.h>

#include "WinFrame.h"
#include "SysKeys.h"

#include "..\GFX\GFX.h"
#include "..\SFX\SFX.h"
#include "..\Input\Input.h"
#include "..\Scene\Scene.h"
#include "..\GameTT\iMission.h"
#include "..\Misc\FileUtils.h"

#include "..\Net\NetDriver.h"

#include "..\StreamIO\OptionSystem.h"
#include "..\StreamIO\RandomGen.h"
#include "..\StreamIO\OptionSystem.h"

#include "..\Main\iMain.h"
#include "..\Main\GameDB.h"
#include "..\Main\Transceiver.h"
#include "..\Main\Multiplayer.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\CommandsHistoryInterface.h"

#include "..\GameTT\CutScenesHelper.h"
#include "..\Misc\TimeMeter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float Clamp1( float fVal, float fMin, float fMax )
{
	union { float f; int hex; };
	f = fVal - fMin;
	hex &= ~hex>>31;
	f += fMin - fMax;
	hex &= hex>>31;
	f += fMax;

	return f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCmdParams
{
	// graphics
	int nScreenSizeX;
	int nScreenSizeY;
	int nScreenBPP;
	int nStencilBPP;
	int nFreq;
	EGFXFullscreen eFullscreenMode;
	bool bUseDXT;
	// misc
	bool bMultiplayer;
	bool bCycledLaunch;
	int nGuaranteeFPS;
	int nAutoSavePeriod;
	std::string szMovieDir;
	// game spy support
	std::string szIPToGameSpyConnect;
	int nGameSpyHostPort;
	bool bGameSpyPasswordRequired;
	std::string szGameSpyPassword;

	ITextureManager::ETextureQuality eTextureQuality;
	//
	std::string szMapName;								// map file name (for direct map launch)
	std::string szBindName;								// config file name - obsolete - unsupported
	std::string szSaveFile;								// save file name - for direct save launch
	std::string szModName;								// mod file name - to lauch game with particular mod added

	SCmdParams() : nGameSpyHostPort( 0 ), bGameSpyPasswordRequired( false ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCommandLine( LPSTR lpCmdLine, SCmdParams *pCmdParams );
void ReadAndSetSunlight( CTableAccessor &table, const std::string &szSeason );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::string szLaunchDirectory;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	CTimeMeter<> timeMeter;
	// disable system-critical errors displaying - just send it to calling process
	SetErrorMode( SEM_FAILCRITICALERRORS );
	//
	if ( !NMain::CanLaunch() )
		return 0xDEAD;
	{
		char buffer[2048] = { 0 };
		if ( ::GetModuleFileName( 0, buffer, sizeof(buffer) ) != 0 )
		{
			char *pFileName = strrchr( buffer, '\\' );
			if ( pFileName != 0 )
			{
				*pFileName = 0;
				::SetCurrentDirectory( buffer );
			}
		}
	}
	//
	NWinFrame::ShowSplashScreen( hInstance, true );
	//
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	
	int nLeakId = -1;
	_CrtSetBreakAlloc( nLeakId );
	// _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_LEAK_CHECK_DF
#if defined( _DO_SEH ) && !defined( _DEBUG )
	// set StructuredExceptionHandler 
	SetCrashHandlerFilter( CrashHandlerFilter );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
	std::string szLogFileName, szErrorFileName;
	{
		char buffer[1024];
		GetCurrentDirectory( 1024, buffer );
		szLaunchDirectory = buffer;
		if ( !szLaunchDirectory.empty() ) 
		{
			if ( szLaunchDirectory[szLaunchDirectory.size() - 1] != '\\' ) 
				szLaunchDirectory += '\\';
		}
		//
		szLogFileName = std::string(buffer) + "\\log.txt";
		szErrorFileName = std::string(buffer) + "\\error.txt";
		DeleteFile( szErrorFileName.c_str() );
		DeleteFile( szLogFileName.c_str() );
	}
	// configure console buffer
	if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
	{
		pConsole->Configure( NStr::Format("logfile;%s", szLogFileName.c_str()) );
		pConsole->Configure( NStr::Format("name;%d;World Commands", CONSOLE_STREAM_WORLD) );
		pConsole->Configure( NStr::Format("name;%d;Script Commands", CONSOLE_STREAM_SCRIPT) );
		pConsole->Configure( NStr::Format("name;%d;Console Feedbacks", CONSOLE_STREAM_CONSOLE) );
		pConsole->Configure( NStr::Format("name;%d;Console Commands", CONSOLE_STREAM_COMMAND) );
		pConsole->Configure( NStr::Format("name;%d;Chat", CONSOLE_STREAM_CHAT) );
		//
		pConsole->Configure( NStr::Format("dublicate;%d;%d", CONSOLE_STREAM_CHAT, CONSOLE_STREAM_CONSOLE) );
	}
	//
	timeMeter.Reset();
	SCmdParams cmdp;
	ProcessCommandLine( lpCmdLine, &cmdp );
	GetSingleton<IRandomGen>()->Init();
	timeMeter.Sample( "random & cmd line" );
	//
	if ( !NWinFrame::InitApplication( hInstance, " Blitzkrieg Game", "A7_ENGINE", cmdp.nScreenSizeX, cmdp.nScreenSizeY ) )
		return 0xDEAD;
	// open main resource system and register as '0'
	timeMeter.Reset();
	{
		// CRAP{ for multiplayer testing
		CPtr<IDataStorage> pStorage;

		std::string szDataDir = GetGlobalVar( "DataDir" );
		if ( szDataDir.size() != 0 )
		{
			NStr::ToLower( szDataDir );
			if ( szDataDir == "s:\\versions\\current" )
			{
				::MessageBox( 0, "Сказано же, что нельзя использовать ресурсы с \"s:\\versions\\current\"!", "ERROR", MB_OK | MB_ICONEXCLAMATION );
				return 0xDEAD;
			}
			else
				pStorage = OpenStorage( (szDataDir + "\\data\\*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
		}
		else
			pStorage = OpenStorage( ".\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
		// CRAP}
		//
		RegisterSingleton( IDataStorage::tidTypeID, pStorage );
		// MOD will be added later...
		/*
		// add mod, if it is
		if ( !cmdp.szModName.empty() ) 
		{
			CPtr<IDataStorage> pMod = OpenStorage( (cmdp.szModName + "*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
			if ( pMod == 0 ) 
			{
				::MessageBox( 0, NStr::Format("Can't open mod storage \"%s\" to use with game", cmdp.szModName.c_str()), "ERROR", MB_OK | MB_ICONEXCLAMATION );
				return 0xDEAD;
			}
			pStorage->AddStorage( pMod, cmdp.szModName.c_str() );
			CPtr<IDataStream> pXMLStream = pStorage->OpenStream( "mod.xml", STREAM_ACCESS_READ );
			if ( pXMLStream )
			{
				CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
				if ( pDT )
				{
					CTreeAccessor saver = pDT;
					std::string szName = "MyMOD";
					std::string szVersion = "1.0";
					saver.Add( "MODName", &szName );
					saver.Add( "MODVersion", &szVersion );
					SetGlobalVar( "MOD.Active", 1 );
					SetGlobalVar( "MOD.Name", szName.c_str() );
					SetGlobalVar( "MOD.Version", szVersion.c_str() );
				}
			}
		}
		*/
	}
	timeMeter.Sample( "resource system" );
	// check for demo version
	if ( CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream("demo\\demo.xml", STREAM_ACCESS_READ) ) 
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		std::vector<std::string> missionNames;
		saver.Add( "Missions", &missionNames );
		//
		if ( !missionNames.empty() ) 
		{
			SetGlobalVar( "demoversion", 1 );
			SetGlobalVar( "demomission.number", int(missionNames.size()) );
			for ( int i = 0; i < missionNames.size(); ++i )
			{
				const std::string szVarName = NStr::Format( "demomission.%d", i );
				SetGlobalVar( szVarName.c_str(), missionNames[i].c_str() );
			}
		}
	}
	// load constants and set global vars from it
	timeMeter.Reset();
	{
		CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
		// consts
		NMain::SetupGlobalVarConsts( table );
		// video mode
		// in-mission
		SetGlobalVar( "GFX.Mode.Mission.SizeX", cmdp.nScreenSizeX );
		SetGlobalVar( "GFX.Mode.Mission.SizeY", cmdp.nScreenSizeY );
		SetGlobalVar( "GFX.Mode.Mission.BPP", cmdp.nScreenBPP );
		SetGlobalVar( "GFX.Mode.Mission.Stencil", cmdp.nStencilBPP );
		SetGlobalVar( "GFX.Mode.Mission.FullScreen", int(cmdp.eFullscreenMode) );
		SetGlobalVar( "GFX.Mode.Mission.Frequency", cmdp.nFreq );
		// in-interface
		SetGlobalVar( "GFX.Mode.InterMission.SizeX", 1024 );
		SetGlobalVar( "GFX.Mode.InterMission.SizeY", 768 );
		SetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP );
		SetGlobalVar( "GFX.Mode.InterMission.Stencil", -1 );
		SetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) );
		SetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq );
		// current
		SetGlobalVar( "GFX.Mode.Current.SizeX", GetGlobalVar( "GFX.Mode.InterMission.SizeX", 1024 ) );
		SetGlobalVar( "GFX.Mode.Current.SizeY", GetGlobalVar( "GFX.Mode.InterMission.SizeY", 768 ) );
		SetGlobalVar( "GFX.Mode.Current.BPP", GetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP ) );
		SetGlobalVar( "GFX.Mode.Current.Stencil", GetGlobalVar( "GFX.Mode.InterMission.Stencil", cmdp.nStencilBPP ) );
		SetGlobalVar( "GFX.Mode.Current.FullScreen", GetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) ) );
		SetGlobalVar( "GFX.Mode.Current.Frequency", GetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq ) );
	}
	timeMeter.Sample( "consts table" );
	// create game database object
	{
		CPtr<IObjectsDB> pGDB = CreateObjectsDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pGDB );
		GetSLS()->SetGDB( pGDB );
	}
	// create and register net driver
	{
		SetGlobalVar( "GameSpyGameName", "blitzkrieg" );
		SetGlobalVar( "GameSpyEngineName", "blitzkrieg" );
		SetGlobalVar( "GameSpyChatName", "#GSP!blitzkrieg" );
		
		CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
		const int nNetGameVersion = constsTbl.GetInt( "Net", "GameVersion", 1 );
		SetGlobalVar( "NetGameVersion", nNetGameVersion );
		
		INetDriver *pNetDriver = CreateObject<INetDriver>( INetDriver::tidTypeID );
		RegisterSingleton( INetDriver::tidTypeID, pNetDriver );
	}
	// initialize all game system
	timeMeter.Reset();
	NWinFrame::ShowAppWindow( true );
	if ( NMain::Initialize(NWinFrame::GetHWnd(), NWinFrame::GetHWnd(), NWinFrame::GetHWnd(), true) != true )
	{
		::MessageBox( 0, "Can't initialize game...", "ERROR", MB_OK | MB_ICONEXCLAMATION );
		return 0xDEAD;
	}
	timeMeter.Sample( "game system init" );
	// CRAP{ load game database
	if ( GetSingleton<IObjectsDB>()->LoadDB() == false )
	{
		NI_ASSERT_T( false, "Can't opent objects.xml to load game database" );
		return 0xDEAD;
	}
	// CRAP}
	// inspect storage
	timeMeter.Reset();
	{
		IFilesInspector *pInspector = GetSingleton<IFilesInspector>();

		IFilesInspectorEntryCollector *pTutorial = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pTutorial->Configure( "scenarios\\tutorials\\;.xml" );
		pInspector->AddEntry( "tutorial", pTutorial );

		IFilesInspectorEntryCollector *pCustomMissions = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pCustomMissions->Configure( "scenarios\\custom\\missions\\;.xml" );
		pInspector->AddEntry( "custom_missions", pCustomMissions );

		IFilesInspectorEntryCollector *pCustomChapters = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pCustomChapters->Configure( "scenarios\\custom\\chapters\\;.xml" );
		pInspector->AddEntry( "custom_chapters", pCustomChapters );
		
		IFilesInspectorEntryCollector *pCustomCampaigns = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pCustomCampaigns->Configure( "scenarios\\custom\\campaigns\\;.xml" );
		pInspector->AddEntry( "custom_campaigns", pCustomCampaigns );

		IFilesInspectorEntryCollector *pC = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pC->Configure( "maps\\multiplayer\\;.xml" );
		pInspector->AddEntry( "maps_multiplayer_xml", pC );

		pC = CreateObject<IFilesInspectorEntryCollector>( MAIN_FILES_INSPECTOR_ENTRY_COLLECTOR );
		pC->Configure( "maps\\multiplayer\\;.bzm" );
		pInspector->AddEntry( "maps_multiplayer_bzm", pC );
		
		pInspector->InspectStorage( GetSingleton<IDataStorage>() );
	}
	timeMeter.Sample( "inspecting storage" );
	// hide splash screen
	NWinFrame::ShowSplashScreen( hInstance, false );
	timeMeter.Reset();
	// init graphics 
	{
		cmdp.nScreenSizeX = GetGlobalVar( "GFX.Mode.InterMission.SizeX", 1024 );
		cmdp.nScreenSizeY = GetGlobalVar( "GFX.Mode.InterMission.SizeY", 768 );
		cmdp.nScreenBPP = GetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP );
		cmdp.nStencilBPP = GetGlobalVar( "GFX.Mode.InterMission.Stencil", 0 );
		cmdp.eFullscreenMode = (EGFXFullscreen)GetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) );
		cmdp.nFreq = GetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq );
		// mode
		IGFX *pGFX = GetSingleton<IGFX>();
		if ( pGFX->SetMode( cmdp.nScreenSizeX, cmdp.nScreenSizeY, cmdp.nScreenBPP, cmdp.nStencilBPP, cmdp.eFullscreenMode, cmdp.nFreq ) == false )
			return 0xDEAD;
		// some GFX setup
		pGFX->SetCullMode( GFXC_CW );	// setup right-handed coordinate system
		SHMatrix matrix;
		CreateOrthographicProjectionMatrixRH( &matrix, cmdp.nScreenSizeX, cmdp.nScreenSizeY, 1, 1024*8 + cmdp.nScreenSizeY*2 );
		pGFX->SetProjectionTransform( matrix );
		pGFX->EnableLighting( false );
		// texture quality
		GetSingleton<ITextureManager>()->SetQuality( cmdp.eTextureQuality );
	}
	timeMeter.Sample( "graphics init" );
	// 
	timeMeter.Reset();
	SerializeConfig( true, SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_HELPCALLS );
	// check video card and set 'optimized buffers' option for first time
	{
		const int nOldVideoCard = GetSingleton<IUserProfile>()->GetVar( "Autodetect.VideoCard", GFXVC_DEFAULT );
		const int nNewVideoCard = GetSingleton<IGFX>()->GetVideoCard();
		if ( nOldVideoCard != nNewVideoCard ) 
		{
			GetSingleton<IUserProfile>()->AddVar( "Autodetect.VideoCard", nNewVideoCard );
			if ( (nNewVideoCard == GFXVC_RADEON9500) || (nNewVideoCard == GFXVC_RADEON9700) ) 
			{
				GetSingleton<IOptionSystem>()->Set( "GFX.OptBuffers", "ON" );
			}
		}
	}
	// всё через жопу
	{
		std::string szGameSpyServer = GetGlobalVar( "Options.Multiplayer.GameSpyServerName", "" );
		if ( !szGameSpyServer.empty() )
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerName", szGameSpyServer.c_str() );

		if ( cmdp.bGameSpyPasswordRequired )
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerPassword", cmdp.szGameSpyPassword.c_str() );
	}
	timeMeter.Sample( "serialize config" );
	// cursor - set bounds and default mode
	{
		CPtr<ICursor> pCursor = GetSingleton<ICursor>();
		pCursor->SetBounds( 0, 0, cmdp.nScreenSizeX, cmdp.nScreenSizeY );
		pCursor->SetMode( 0 );
		//pCursor->SetSensitivity( float(cmdp.nScreenSizeX) / 800.0f );
	}
	// create and set main general purpose font
	{
		CPtr<IGFXFont> pFont = GetSingleton<IFontManager>()->GetFont( "fonts\\medium" );
		GetSingleton<IGFX>()->SetFont( pFont );
	}
	//
	// setup sounds
	{
		ISFX *pSFX = GetSingleton<ISFX>();
		pSFX->SetSFXMasterVolume( 1.0f );
		pSFX->SetStreamMasterVolume( GetGlobalVar( "Sound.StreamMasterVolume", 1.0f ) );
		pSFX->EnableSFX( GetGlobalVar( "Sound.EnableSFX", 1 ) );
		pSFX->EnableStreaming( GetGlobalVar( "Sound.EnableStream", 1 ) );
	}
	// execute autoexec.cfg
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_COMMAND, "Exec( \"autoexec.cfg\" )", 0xff0000ff );
	// load and apply options 
	timeMeter.Reset();
	{
		IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();
		pOptionSystem->Init();
	}
	timeMeter.Sample( "options init" );
	// disable task switching
	//NSysKeys::EnableSystemKeys( false, hInstance );
	// run main loop
	int nGuaranteeFPSTime = 0;
#ifdef _FINALRELEASE
	try
	{
#endif // _FINALRELEASE
	if ( NMain::CheckBetaKey() ) 
	{
		IMainLoop *pMainLoop = CreateMainLoop();
		RegisterSingleton( IMainLoop::tidTypeID, pMainLoop );
		//
		GetSingleton<ICursor>()->Acquire( true );
		// MOD support
		{
			const std::string szMOD = !cmdp.szModName.empty() ? cmdp.szModName : GetSingleton<IUserProfile>()->GetMOD();
			if ( !szMOD.empty() ) 
				pMainLoop->Command( MAIN_COMMAND_CHANGE_MOD, szMOD.c_str() );
		}
		//
		IInput *pInput = GetSingleton<IInput>();

		if ( cmdp.nGameSpyHostPort )
		{
			std::string szCommandStr = std::string(NStr::Format( "%i", cmdp.nGameSpyHostPort )) + '"' + cmdp.szGameSpyPassword;
			pMainLoop->Command( MISSION_COMMAND_GAMESPY_HOST, szCommandStr.c_str() );
		}
		else if ( !cmdp.szIPToGameSpyConnect.empty() )
		{
			pMainLoop->Command( MISSION_COMMAND_GAMESPY_CLIENT, (cmdp.szIPToGameSpyConnect + '"' + cmdp.szGameSpyPassword).c_str() );
		}
		else if ( !cmdp.szSaveFile.empty() ) 
			pMainLoop->Command( MAIN_COMMAND_LOAD, cmdp.szSaveFile.c_str() );
		else if ( cmdp.szMapName.empty() || cmdp.bMultiplayer )
		{
			pMainLoop->Command( MISSION_COMMAND_VIDEO, NStr::Format("%s;%d", "movies\\intro", MISSION_COMMAND_MAIN_MENU) );
			NCutScenes::AddCutScene( "movies\\intro_only" );
		}
		else
		{
			GetSingleton<IScenarioTracker>()->StartCampaign( "custom_mission", CAMPAIGN_TYPE_CUSTOM_MISSION );
			GetSingleton<IScenarioTracker>()->StartChapter( "custom_mission" );
			pMainLoop->Command( MISSION_COMMAND_MISSION, NStr::Format("%s;%d", cmdp.szMapName.c_str(), cmdp.bCycledLaunch) );
		}
		//
		for (;;)
		{
			if ( !cmdp.szMovieDir.empty() ) 
				SetGlobalVar( "MovieDir", cmdp.szMovieDir.c_str() );
			//
			NWinFrame::PumpMessages();
			bool bActive = NWinFrame::IsActive();
			pInput->PumpMessages( bActive );
			//
			if ( NWinFrame::IsExit() )
			{
				NWinFrame::ResetExit();
				pMainLoop->Command( MAIN_COMMAND_EXIT_GAME, 0 );				// generate 'EXIT' command
			}
			if ( !pMainLoop->StepApp( bActive ) )
				break;
			if ( !bActive )
				Sleep( 40 );
		}
		pMainLoop->ResetStack();
		UnRegisterSingleton( IMainLoop::tidTypeID );
		// save config
		SerializeConfig( false, SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_HELPCALLS );
	}
#ifdef _FINALRELEASE
	}
	catch ( ... ) 
	{
	}
#endif // _FINALRELEASE
	// dump log and show it in the window
#ifdef _DO_ASSERT_SLOW
/*	
	if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
	{
		if ( pConsole->DumpLog( -1 ) )
			ShellExecute( 0, "open", szLogFileName.c_str(), 0, ".\\", SW_SHOWNORMAL );
	}
*/
#endif // _DO_ASSERT_SLOW
	// re-enable task switching
	//NSysKeys::EnableSystemKeys( true, hInstance );
	//
	GetSingleton<ICommandsHistory>()->Save();
	//
	NMain::Finalize();
	
	//
#if defined( _DO_SEH ) && !defined( _DEBUG )
	// reset StructuredExceptionHandler 
	SetCrashHandlerFilter( 0 );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsParamMapName( const std::string &_szParam )
{
	if ( _szParam.size() < 4 ) 
		return false;
	//
	std::string szParam = _szParam;
	if ( szParam[0] == '-' ) 
		szParam.erase( 0, 1 );
	NStr::TrimBoth( szParam, "\n\r\t\" " );
	if ( szParam.size() < 4 ) 
		return false;
	//
	return ( szParam.find(".xml") == szParam.size() - 4 ) || ( szParam.find(".bzm") == szParam.size() - 4 );
}
std::string ExtractMapName( const std::string &_szParam )
{
	std::string szParam = _szParam;
	if ( szParam[0] == '-' ) 
		szParam.erase( 0, 1 );
	NStr::TrimBoth( szParam, "\n\r\t\" " );
	NI_ASSERT_T ( szParam.size() > 4, NStr::Format("Wrong param \"%s\" as map name", szParam.c_str()) );
	return szParam;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCommandLine( LPSTR lpCmdLine, SCmdParams *pCmdParams )
{
	pCmdParams->nScreenSizeX = 1024;
	pCmdParams->nScreenSizeY = 768;
	pCmdParams->nScreenBPP = 16;
	pCmdParams->nStencilBPP = 0;
	pCmdParams->nFreq = 0;
	pCmdParams->bUseDXT = false;
	pCmdParams->eFullscreenMode = GFXFS_FULLSCREEN;
	pCmdParams->szBindName = "bind.cfg";
	pCmdParams->bMultiplayer = false;
	pCmdParams->bCycledLaunch = false;
	pCmdParams->nGuaranteeFPS = -1;
	pCmdParams->nAutoSavePeriod = 0;
	pCmdParams->eTextureQuality = ITextureManager::TEXTURE_QUALITY_HIGH;
	pCmdParams->szMapName = "";
	//
	std::vector<std::string> szParams;
	NStr::SplitStringWithMultipleBrackets( lpCmdLine, szParams, ' ' );
	// parse command line params
	for ( int i=0; i<szParams.size(); ++i )
	{
		const std::string realStr = szParams[i];
		NStr::ToLower( szParams[i] );
		if ( (szParams[i].size()) > 4 && IsParamMapName(szParams[i]) && pCmdParams->szMapName == "" )
		{
			pCmdParams->szMapName = ExtractMapName( szParams[i] );
			SetGlobalVar( "Map.Current.Name", pCmdParams->szMapName.c_str() );
		}
		else if ( (szParams[i].size()) > 4 && (szParams[i].find(".sav") == szParams[i].size() - 4) ) 
		{
			pCmdParams->szSaveFile = szParams[i].c_str() + 1;
		}
		else if ( szParams[i].compare(0, 5, "-freq") == 0 )
		{
			pCmdParams->nFreq = atoi( szParams[i].c_str() + 5 );
			SetGlobalVar( "freq", szParams[i].c_str() + 5 );
		}
		else if ( szParams[i] == "-mp" )
		{
			pCmdParams->bMultiplayer = true;
		}
		else if ( szParams[i].compare(0, 4, "-mod") == 0 )
		{
			std::string szModDir = szParams[i].c_str() + 4;
			NStr::TrimBoth( szModDir, '"' );
			if ( !szModDir.empty() && szModDir[szModDir.size() - 1] != '\\' ) 
				szModDir += '\\';
			pCmdParams->szModName = szModDir;
		}
//#ifndef _FINALRELEASE		
		else if ( szParams[i] == "-windowed" )
		{
			pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
			SetGlobalVar( "windowed", "1" );
		}
		else if ( szParams[i] == "-fullscreen" )
		{
			pCmdParams->eFullscreenMode = GFXFS_FULLSCREEN;
			SetGlobalVar( "fullscreen", "1" );
		}
		else if ( szParams[i].compare(0, 9, "-autosave") == 0 )
		{
			pCmdParams->nAutoSavePeriod = atoi( szParams[i].c_str() + 9 );
			SetGlobalVar( "autosave", szParams[i].c_str() + 9 );
		}
		else if ( szParams[i] == "-cycled" )
			pCmdParams->bCycledLaunch = true;
		else if ( szParams[i].compare(0, 4, "-fps") == 0 )
		{
			pCmdParams->nGuaranteeFPS = int( 1000.0f / float( pCmdParams->nGuaranteeFPS ) + 0.5f );
			pCmdParams->nGuaranteeFPS = atoi( szParams[i].c_str() + 4 );
			SetGlobalVar( "GuaranteeFPS", pCmdParams->nGuaranteeFPS );
		}
		else if ( szParams[i].compare(0, 6, "-movie") == 0 )
		{
			std::string szMovieDir = szParams[i].c_str() + 6;
			NStr::TrimBoth( szMovieDir, '"' );
			if ( !szMovieDir.empty() && szMovieDir[szMovieDir.size() - 1] != '\\' ) 
				szMovieDir += '\\';
			pCmdParams->szMovieDir = szMovieDir;
			SetGlobalVar( "MovieDir", szMovieDir.c_str() );
		}
		else if ( szParams[i].compare( 0, 14, "-showscripterr" ) == 0 )
		{
			SetGlobalVar( "ShowScriptErrors", 1 );
		}
		// save history file
		else if ( szParams[i].compare( 0, 3, "-sh" ) == 0 )
		{
			std::string szSaveHistoryFileName = szParams[i].c_str() + 3;
			NStr::TrimBoth( szSaveHistoryFileName, '"' );
			if ( szSaveHistoryFileName.empty() ) 
				szSaveHistoryFileName = szLaunchDirectory + "history.xml";
			SetGlobalVar( "SaveHistoryFileName", szSaveHistoryFileName.c_str() );
		}
		else if ( szParams[i].compare( 0, 9, "-lhclient" ) == 0 )
		{
			std::string szLoadHistoryFireName = szParams[i].c_str() + 9;
			NStr::TrimBoth( szLoadHistoryFireName, '"' );
			if ( szLoadHistoryFireName.empty() ) 
				szLoadHistoryFireName = szLaunchDirectory + "history.xml";
			SetGlobalVar( "LoadHistoryFileName", szLoadHistoryFireName.c_str() );
			SetGlobalVar( "HistoryClient", 1 );
		}
		else if ( szParams[i].compare( 0, 3, "-lh" ) == 0 )
		{
			std::string szLoadHistoryFireName = szParams[i].c_str() + 3;
			NStr::TrimBoth( szLoadHistoryFireName, '"' );
			if ( szLoadHistoryFireName.empty() ) 
				szLoadHistoryFireName = szLaunchDirectory + "history.xml";
			SetGlobalVar( "LoadHistoryFileName", szLoadHistoryFireName.c_str() );
		}

//#endif // _FINALRELEASE
		else if ( szParams[i].compare( 0, 8, "-datadir") == 0 )
		{
			std::string szDataDir = szParams[i].c_str() + 8;
			NStr::TrimBoth( szDataDir, '"' );
			SetGlobalVar( "DataDir", szDataDir.c_str() );
		}
		else if ( szParams[i].compare( 0, 8, "-connect" ) == 0 )
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII( 100, szParams[i].c_str(), 0, true );
			std::string szConnectParams = szParams[i].c_str() + 8;
			//NStr::TrimBoth( szConnectParams, '"' );
			NStr::TrimRight( szConnectParams, ':' );
			pCmdParams->szIPToGameSpyConnect = szConnectParams;

			// CRAP{ for debug
//			pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
//			SetGlobalVar( "windowed", "1" );
//			SetGlobalVar( "DataDir", "j:" );
			// CRAP}
		}
		else if ( szParams[i].compare( 0, 5, "-host" ) == 0 )
		{
			std::string szConnectParams = realStr.c_str() + 5;
			if ( szConnectParams == "" )
				pCmdParams->nGameSpyHostPort = -1;
			else
				pCmdParams->nGameSpyHostPort = NStr::ToInt( szConnectParams );

			// CRAP{ for debug
//			pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
//			SetGlobalVar( "windowed", "1" );
			// CRAP}
		}
		else if ( szParams[i].compare( 0, 9, "-password" ) == 0 )
		{
			std::string szPassword = realStr.c_str() + 9;
			NStr::TrimBoth( szPassword, '"' );

			pCmdParams->bGameSpyPasswordRequired = true;
			pCmdParams->szGameSpyPassword = szPassword;
		}
		else if ( szParams[i].compare( 0, 5, "-name") == 0 )
		{
			std::string szNick = realStr.c_str() + 5;
			NStr::TrimBoth( szNick, '"' );

			const std::wstring szWideNick = NStr::ToUnicode( szNick );
			SetGlobalVar( "Options.Multiplayer.GameSpyPlayerName", reinterpret_cast<const WORD*>( szWideNick.c_str() ) );
		}
		else if ( szParams[i].compare(0,5, "-room") == 0 )
		{
			std::string szRoom = realStr.c_str() + 5;
			NStr::TrimBoth( szRoom, '"' );
			SetGlobalVar( "Options.Multiplayer.GameSpyServerName", szRoom.c_str() );
		}
#ifndef _FINALRELEASE		
		// for debug purposes!
		else if ( szParams[i].compare( 0, 7, "-cheats" ) == 0 )
		{
			SetGlobalVar( "EnableCheats", 1 );
		}
		else if ( szParams[i].compare( 0, 9, "-numsaves" ) == 0 )
		{
			SetGlobalVar( "NumSaves", 1 );
		}
		//
		else if ( szParams[i][0] == '-' )
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII( 100, szParams[i].c_str(), 0, true );
			
			std::string szParam = szParams[i].c_str() + 1;
			const int nPos = szParam.find_first_of( "-+.0123456789" );
			if ( nPos == std::string::npos )
				SetGlobalVar( szParam.c_str(), "1" );
			else
				SetGlobalVar( szParam.substr(0, nPos).c_str(), szParam.substr(nPos).c_str() );
		}
		else
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII( 100, szParams[i].c_str(), 0, true );
		}
#endif // _FINALRELEASE
	}
	// in the fullscreen mode we can't assign any freq. but default
	if ( pCmdParams->eFullscreenMode == GFXFS_WINDOWED )
		pCmdParams->nFreq = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
