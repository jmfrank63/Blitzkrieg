#include "StdAfx.h"

#include "GameMain.h"

#include <shellapi.h>
#include <crtdbg.h>

#include "WinFrame.h"
#include "SysKeys.h"

#include "../GFX/GFX.h"
#include "../Image/Image.h"
#include "../SFX/SFX.h"
#include "../Input/Input.h"
#include "../Input/InputTypes.h"
#include "../Scene/Scene.h"
#include "../GameTT/iMission.h"
#include "../Misc/FileUtils.h"

#include "../Net/NetDriver.h"

#include "../StreamIO/OptionSystem.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/OptionSystem.h"

#include "../Main/iMain.h"
#include "../Main/GameDB.h"
#include "../Main/Transceiver.h"
#include "../Main/Multiplayer.h"
#include "../Main/ScenarioTracker.h"
#include "../Main/CommandsHistoryInterface.h"

#include "../GameTT/CutScenesHelper.h"
#include "../Misc/TimeMeter.h"
#include "../Platform/Paths.h"
#include "../Platform/System.h"
#include "../Platform/Debug.h"
#include "../Platform/Clock.h"
#ifdef BK_STARTUP_TRACE
#define BK_STARTUP_MARKER(name) NPlatform::DebugWrite("BK_STARTUP: " name "\n")
#else
#define BK_STARTUP_MARKER(name) ((void)0)
#endif
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
struct SCmdParams
{
	int nScreenSizeX;
	int nScreenSizeY;
	int nScreenBPP;
	int nStencilBPP;
	int nFreq;
	EGFXFullscreen eFullscreenMode;
	bool bUseDXT;
	bool bMultiplayer;
	bool bCycledLaunch;
	int nGuaranteeFPS;
	int nAutoSavePeriod;
	std::string szMovieDir;
	std::string szIPToGameSpyConnect;
	int nGameSpyHostPort;
	bool bGameSpyPasswordRequired;
	bool bStartupSmoke;
	bool bReferenceScene;
	std::string szReferenceScenePath;
	int nReferenceWidth;
	int nReferenceHeight;
	std::string szGameSpyPassword;

	ITextureManager::ETextureQuality eTextureQuality;
	std::string szMapName;								// map file name (for direct map launch)
	std::string szBindName;								// config file name - obsolete - unsupported
	std::string szSaveFile;								// save file name - for direct save launch
	std::string szModName;								// mod file name - to lauch game with particular mod added

	SCmdParams() : nGameSpyHostPort( 0 ), bGameSpyPasswordRequired( false ), bStartupSmoke( false ), bReferenceScene( false ), nReferenceWidth( 0 ), nReferenceHeight( 0 ) { }
};
void ProcessCommandLine( const char *lpCmdLine, SCmdParams *pCmdParams );
void ReadAndSetSunlight( CTableAccessor &table, const std::string &szSeason );
static std::string szLaunchDirectory;
int GameMain( const NPlatform::Arguments &arguments )
{
	CTimeMeter<> timeMeter;
	SetErrorMode( SEM_FAILCRITICALERRORS );
	if ( !NMain::CanLaunch() )
		return 0xDEAD;
	if ( !NPlatform::Paths::Initialize() )
		return 0xDEAD;
	NWinFrame::ShowSplashScreen( NWinFrame::GetHInstance(), true );
	// no _CRTDBG_LEAK_CHECK_DF: refcounted objects still alive when process
	// teardown begins are leaked on purpose (see NRefCount::LeakObjectsOnExit),
	// so an exit-time leak dump would only flood the debugger output
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	// registered after static init, so this runs before Game.exe static
	// destructors at exit (atexit is LIFO) — from then on every module leaks
	// refcounted objects instead of running destruction cascades. The DLL flags
	// must be armed here too: modules detach one after another at exit, so e.g.
	// GameTT's static teardown can release AILogic-compiled objects before
	// AILogic's own DllMain(DETACH) has armed its flag.
	atexit( []{
		NRefCount::LeakObjectsOnExit() = true;
		const char *pszModules[] = { "AILogic.dll", "GameTT.dll", "UI.dll", "Scene.dll" };
		for ( const char *pszModule : pszModules )
		{
			if ( HMODULE hModule = ::GetModuleHandleA( pszModule ) )
			{
				typedef void (*ArmFunc)();
				if ( ArmFunc pfnArm = reinterpret_cast<ArmFunc>( ::GetProcAddress( hModule, "ArmRefCountLeakOnExit" ) ) )
					pfnArm();
			}
		}
	} );
	// CRT assert/abort dialogs open behind the fullscreen game window; route
	// asserts to stderr and let abort() raise a fail-fast exception so an
	// attached debugger breaks instead of the process exiting with code 3.
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( _CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	
	int nLeakId = -1;
	_CrtSetBreakAlloc( nLeakId );
#if defined( _DO_SEH ) && !defined( _DEBUG )
	SetCrashHandlerFilter( CrashHandlerFilter );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
	const std::string szLogFileName = NPlatform::Paths::LogPath();
	const std::string szErrorFileName = NPlatform::Paths::ErrorLogPath();
	szLaunchDirectory = NPlatform::Paths::UserRoot();
	NFile::CFile::Remove( szErrorFileName.c_str() );
	NFile::CFile::Remove( szLogFileName.c_str() );
	if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
	{
		pConsole->Configure( NStr::Format("logfile;%s", szLogFileName.c_str()) );
		pConsole->Configure( NStr::Format("name;%d;World Commands", CONSOLE_STREAM_WORLD) );
		pConsole->Configure( NStr::Format("name;%d;Script Commands", CONSOLE_STREAM_SCRIPT) );
		pConsole->Configure( NStr::Format("name;%d;Console Feedbacks", CONSOLE_STREAM_CONSOLE) );
		pConsole->Configure( NStr::Format("name;%d;Console Commands", CONSOLE_STREAM_COMMAND) );
		pConsole->Configure( NStr::Format("name;%d;Chat", CONSOLE_STREAM_CHAT) );
		pConsole->Configure( NStr::Format("dublicate;%d;%d", CONSOLE_STREAM_CHAT, CONSOLE_STREAM_CONSOLE) );
	}
	timeMeter.Reset();
	SCmdParams cmdp;
	std::string commandLine;
	for ( int index = 1; index < arguments.argc; ++index )
	{
		if ( index > 1 ) commandLine += ' ';
		if ( arguments.argv[index] ) commandLine += arguments.argv[index];
	}
	ProcessCommandLine( commandLine.c_str(), &cmdp );
	if ( cmdp.bReferenceScene )
		SetGlobalVar( "fixrandom", 1 );
	GetSingleton<IRandomGen>()->Init();
	timeMeter.Sample( "random & cmd line" );
	BK_STARTUP_MARKER("before InitApplication");
	if ( !NWinFrame::InitApplication( NWinFrame::GetHInstance(), " Blitzkrieg Game", "A7_ENGINE", cmdp.nScreenSizeX, cmdp.nScreenSizeY ) )
		return 0xDEAD;
	BK_STARTUP_MARKER("after InitApplication");
	timeMeter.Reset();
	BK_STARTUP_MARKER("before OpenStorage");
	{
		CPtr<IDataStorage> pStorage;

		std::string szDataDir = GetGlobalVar( "DataDir" );
		if ( szDataDir.size() != 0 )
		{
			NStr::ToLower( szDataDir );
			if ( szDataDir == "s:\\versions\\current" )
			{
				NPlatform::ShowError( "ERROR", "Can't use \"s:\\versions\\current\" as data directory!" );
				return 0xDEAD;
			}
			else
				pStorage = OpenStorage( (szDataDir + "\\data\\*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
		}
		else
			pStorage = OpenStorage( NPlatform::Paths::DataArchivePattern().c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
		RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	}
	BK_STARTUP_MARKER("after OpenStorage");
	timeMeter.Sample( "resource system" );
	BK_STARTUP_MARKER("before demo.xml");
	if ( CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream("demo\\demo.xml", STREAM_ACCESS_READ) ) 
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		std::vector<std::string> missionNames;
		saver.Add( "Missions", &missionNames );
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
	BK_STARTUP_MARKER("after demo.xml");
	timeMeter.Reset();
	BK_STARTUP_MARKER("before consts.xml");
	{
		CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
		NMain::SetupGlobalVarConsts( table );
		SetGlobalVar( "GFX.Mode.Mission.SizeX", cmdp.nScreenSizeX );
		SetGlobalVar( "GFX.Mode.Mission.SizeY", cmdp.nScreenSizeY );
		SetGlobalVar( "GFX.Mode.Mission.BPP", cmdp.nScreenBPP );
		SetGlobalVar( "GFX.Mode.Mission.Stencil", cmdp.nStencilBPP );
		SetGlobalVar( "GFX.Mode.Mission.FullScreen", int(cmdp.eFullscreenMode) );
		SetGlobalVar( "GFX.Mode.Mission.Frequency", cmdp.nFreq );
		SetGlobalVar( "GFX.Mode.InterMission.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
		SetGlobalVar( "GFX.Mode.InterMission.SizeY", GFX_DEFAULT_SCREEN_HEIGHT );
		SetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP );
		SetGlobalVar( "GFX.Mode.InterMission.Stencil", -1 );
		SetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) );
		SetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq );
		SetGlobalVar( "GFX.Mode.Current.SizeX", GetGlobalVar( "GFX.Mode.InterMission.SizeX", GFX_DEFAULT_SCREEN_WIDTH ) );
		SetGlobalVar( "GFX.Mode.Current.SizeY", GetGlobalVar( "GFX.Mode.InterMission.SizeY", GFX_DEFAULT_SCREEN_HEIGHT ) );
		SetGlobalVar( "GFX.Mode.Current.BPP", GetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP ) );
		SetGlobalVar( "GFX.Mode.Current.Stencil", GetGlobalVar( "GFX.Mode.InterMission.Stencil", cmdp.nStencilBPP ) );
		SetGlobalVar( "GFX.Mode.Current.FullScreen", GetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) ) );
		SetGlobalVar( "GFX.Mode.Current.Frequency", GetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq ) );
	}
	BK_STARTUP_MARKER("after consts.xml");
	timeMeter.Sample( "consts table" );
	BK_STARTUP_MARKER("before CreateObjectsDB");
	{
		CPtr<IObjectsDB> pGDB = CreateObjectsDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pGDB );
		GetSLS()->SetGDB( pGDB );
	}
	BK_STARTUP_MARKER("after CreateObjectsDB");
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
	timeMeter.Reset();
	// The app window stays hidden here: it has nothing to paint until the GFX
	// device exists, and IGFX::SetMode() shows it itself (SWP_SHOWWINDOW in
	// ResizeDeviceWindow). The splash screen covers the whole startup instead.
	BK_STARTUP_MARKER("before NMain::Initialize");
	if ( NMain::Initialize(reinterpret_cast<HWND>( NWinFrame::GetSDLWindow() ), NWinFrame::GetHWnd(), NWinFrame::GetHWnd(), true) != true )
	{
		NPlatform::ShowError( "ERROR", "Can't initialize game..." );
		return 0xDEAD;
	}
	BK_STARTUP_MARKER("after NMain::Initialize");
	timeMeter.Sample( "game system init" );
	BK_STARTUP_MARKER("before IObjectsDB::LoadDB");
	if ( GetSingleton<IObjectsDB>()->LoadDB() == false )
	{
		NI_ASSERT_T( false, "Can't opent objects.xml to load game database" );
		return 0xDEAD;
	}
	BK_STARTUP_MARKER("after IObjectsDB::LoadDB");
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
	timeMeter.Reset();
	{
		cmdp.nScreenSizeX = GetGlobalVar( "GFX.Mode.InterMission.SizeX", GFX_DEFAULT_SCREEN_WIDTH );
		cmdp.nScreenSizeY = GetGlobalVar( "GFX.Mode.InterMission.SizeY", GFX_DEFAULT_SCREEN_HEIGHT );
		if ( cmdp.bReferenceScene && cmdp.nReferenceWidth > 0 && cmdp.nReferenceHeight > 0 )
		{
			cmdp.nScreenSizeX = cmdp.nReferenceWidth;
			cmdp.nScreenSizeY = cmdp.nReferenceHeight;
		}
		cmdp.nScreenBPP = GetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP );
		cmdp.nStencilBPP = GetGlobalVar( "GFX.Mode.InterMission.Stencil", 0 );
		cmdp.eFullscreenMode = (EGFXFullscreen)GetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) );
		cmdp.nFreq = GetGlobalVar( "GFX.Mode.InterMission.Frequency", cmdp.nFreq );
		IGFX *pGFX = GetSingleton<IGFX>();
		if ( pGFX->SetMode( cmdp.nScreenSizeX, cmdp.nScreenSizeY, cmdp.nScreenBPP, cmdp.nStencilBPP, cmdp.eFullscreenMode, cmdp.nFreq ) == false )
			return 0xDEAD;
		// SetMode just made the window visible; present a black frame right away
		// so it never flashes unpainted (white) content before the intro video
		// renders its first frame.
		const CTRect<long> rcScreen = pGFX->GetScreenRect();
		cmdp.nScreenSizeX = rcScreen.Width();
		cmdp.nScreenSizeY = rcScreen.Height();
		cmdp.nScreenBPP = pGFX->GetScreenBPP();
		SetGlobalVar( "GFX.Mode.InterMission.SizeX", cmdp.nScreenSizeX );
		SetGlobalVar( "GFX.Mode.InterMission.SizeY", cmdp.nScreenSizeY );
		SetGlobalVar( "GFX.Mode.InterMission.BPP", cmdp.nScreenBPP );
		SetGlobalVar( "GFX.Mode.Current.SizeX", cmdp.nScreenSizeX );
		SetGlobalVar( "GFX.Mode.Current.SizeY", cmdp.nScreenSizeY );
		SetGlobalVar( "GFX.Mode.Current.BPP", cmdp.nScreenBPP );
		SHMatrix matrix;
		CreateOrthographicProjectionMatrixRH( &matrix, cmdp.nScreenSizeX, cmdp.nScreenSizeY, 1, 1024*8 + cmdp.nScreenSizeY*2 );
		if ( pGFX->BeginScene() )
		{
			pGFX->SetCullMode( GFXC_CW );	// setup right-handed coordinate system
			pGFX->SetProjectionTransform( matrix );
			pGFX->EnableLighting( false );
			pGFX->Clear( 0, 0, GFXCLEAR_TARGET, 0 );
			pGFX->EndScene();
			pGFX->Flip();
		}
		GetSingleton<ITextureManager>()->SetQuality( cmdp.eTextureQuality );
	}
	timeMeter.Sample( "graphics init" );
	timeMeter.Reset();
	SerializeConfig( true, SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_HELPCALLS );
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
	{
		std::string szGameSpyServer = GetGlobalVar( "Options.Multiplayer.GameSpyServerName", "" );
		if ( !szGameSpyServer.empty() )
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerName", szGameSpyServer.c_str() );

		if ( cmdp.bGameSpyPasswordRequired )
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerPassword", cmdp.szGameSpyPassword.c_str() );
	}
	timeMeter.Sample( "serialize config" );
	{
		CPtr<ICursor> pCursor = GetSingleton<ICursor>();
		pCursor->SetBounds( 0, 0, cmdp.nScreenSizeX, cmdp.nScreenSizeY );
		pCursor->SetMode( 0 );
		GetSingleton<IInput>()->SetDeviceEmulationStatus( DEVICE_TYPE_MOUSE, true );
		pCursor->SetUpdateMode( ICursor::UPDATE_MODE_INPUT );
	}
	{
		CPtr<IGFXFont> pFont = GetSingleton<IFontManager>()->GetFont( "fonts\\medium" );
		GetSingleton<IGFX>()->SetFont( pFont );
	}
	{
		ISFX *pSFX = GetSingleton<ISFX>();
		pSFX->SetSFXMasterVolume( GetGlobalVar( "Sound.SFXMasterVolume", 1.0f ) );
		pSFX->SetStreamMasterVolume( GetGlobalVar( "Sound.StreamMasterVolume", 1.0f ) );
		pSFX->EnableSFX( GetGlobalVar( "Sound.EnableSFX", 1 ) );
		pSFX->EnableStreaming( GetGlobalVar( "Sound.EnableStream", 1 ) );
	}
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_COMMAND, "Exec( \"autoexec.cfg\" )", 0xff0000ff );
	timeMeter.Reset();
	{
		IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();
		pOptionSystem->Init();
	}
	timeMeter.Sample( "options init" );
	int nGuaranteeFPSTime = 0;
#ifdef _FINALRELEASE
	try
	{
#endif // _FINALRELEASE
	if ( NMain::CheckBetaKey() ) 
	{
		IMainLoop *pMainLoop = CreateMainLoop();
		RegisterSingleton( IMainLoop::tidTypeID, pMainLoop );
		if ( !cmdp.bReferenceScene )
			GetSingleton<ICursor>()->Acquire( true );
		{
			const std::string szMOD = !cmdp.szModName.empty() ? cmdp.szModName : GetSingleton<IUserProfile>()->GetMOD();
			if ( !szMOD.empty() ) 
				pMainLoop->Command( MAIN_COMMAND_CHANGE_MOD, szMOD.c_str() );
		}
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
		else if ( cmdp.bStartupSmoke )
		{
			pMainLoop->Command( MISSION_COMMAND_MAIN_MENU, "0" );
		}
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
		// Startup is done and the first main-loop step will begin the intro
		// video — only now take the splash logo down.
		NWinFrame::ShowSplashScreen( NWinFrame::GetHInstance(), false );
		if ( cmdp.bReferenceScene )
			GetSingleton<ICursor>()->Show( false );
		int nReferenceCaptureDelay = 0; // allow the deterministic menu frame to settle
		for (;;)
		{
			if ( !cmdp.szMovieDir.empty() ) 
				SetGlobalVar( "MovieDir", cmdp.szMovieDir.c_str() );
			NWinFrame::PumpMessages();
			bool bActive = NWinFrame::IsActive();
			pInput->PumpMessages( bActive );
			if ( NWinFrame::IsExit() )
			{
				NWinFrame::ResetExit();
				pMainLoop->Command( MAIN_COMMAND_EXIT_GAME, 0 );				// generate 'EXIT' command
			}
			if ( !pMainLoop->StepApp( bActive ) )
				break;
			if ( cmdp.bReferenceScene && GetGlobalVar( "X64.StartupSmoke.MainMenu", 0 ) != 0 && ++nReferenceCaptureDelay >= 5 )
			{
				IGFX *pGFX = GetSingleton<IGFX>();
				const CTRect<long> rcScreen = pGFX->GetScreenRect();
				CPtr<IImage> referenceImage = GetImageProcessor()->CreateImage( rcScreen.Width(), rcScreen.Height() );
				if ( !pGFX->Flip() && !cmdp.bReferenceScene )
					return 0xDEAD;
				bool captured = pGFX->TakeScreenShot( referenceImage );
				if ( captured && !cmdp.szReferenceScenePath.empty() )
				{
					FILE *referenceFile = fopen( cmdp.szReferenceScenePath.c_str(), "wb" );
					if ( referenceFile != 0 )
					{
						const SColor *pixels = referenceImage->GetLFB();
						for ( int y = 0; y < referenceImage->GetSizeY() && captured; ++y )
						{
							for ( int x = 0; x < referenceImage->GetSizeX(); ++x )
							{
								const SColor &pixel = pixels[y * referenceImage->GetSizeX() + x];
								// Reference captures use the legacy contract: alpha is not part
								// of the scene comparison and is emitted as zero by D3D9.
								const BYTE rgba[4] = { pixel.r, pixel.g, pixel.b, 0 };
								if ( fwrite( rgba, sizeof( rgba ), 1, referenceFile ) != 1 )
								{
									captured = false;
									break;
								}
							}
						}
						fclose( referenceFile );
					}
					else
						captured = false;
				}
				if ( captured )
				{
					NPlatform::DebugWrite( "BK_REFERENCE_SCENE: capture complete\n" );
					break;
				}
					NPlatform::DebugWrite( "BK_REFERENCE_SCENE: capture failed\n" );
				return 0xDEAD;
			}
			if ( cmdp.bStartupSmoke && !cmdp.bReferenceScene && GetGlobalVar( "X64.StartupSmoke.MainMenu", 0 ) != 0 )
			{
					NPlatform::DebugWrite( "BK_STARTUP: C6 main menu smoke checkpoint passed\n" );
				break;
			}
			if ( !bActive )
				NPlatform::SleepMilliseconds( 40 );
		}
		// Catch-all: any exit path that bypassed CICExitGame (e.g. smoke-test
		// break) still tears the world down here. Leak refcounted objects from
		// now on — see ArmAllModulesLeakOnExit / CICExitGame::Exec.
		NRefCount::LeakObjectsOnExit() = true;
		pMainLoop->ResetStack();
		UnRegisterSingleton( IMainLoop::tidTypeID );
		SerializeConfig( false, SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_HELPCALLS );
	}
#ifdef _FINALRELEASE
	}
	catch ( ... ) 
	{
	}
#endif // _FINALRELEASE
#ifdef _DO_ASSERT_SLOW
/*	
	if ( IConsoleBuffer *pConsole = GetSingleton<IConsoleBuffer>() )
	{
		if ( pConsole->DumpLog( -1 ) )
			NPlatform::OpenFile( szLogFileName.c_str() );
	}
*/
#endif // _DO_ASSERT_SLOW
	GetSingleton<ICommandsHistory>()->Save();
	NMain::Finalize();
	
#if defined( _DO_SEH ) && !defined( _DEBUG )
	SetCrashHandlerFilter( 0 );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )

	return 0;
}
bool IsParamMapName( const std::string &_szParam )
{
	if ( _szParam.size() < 4 ) 
		return false;
	std::string szParam = _szParam;
	if ( szParam[0] == '-' ) 
		szParam.erase( 0, 1 );
	NStr::TrimBoth( szParam, "\n\r\t\" " );
	if ( szParam.size() < 4 ) 
		return false;
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
void ProcessCommandLine( const char *lpCmdLine, SCmdParams *pCmdParams )
{
	pCmdParams->nScreenSizeX = GFX_DEFAULT_SCREEN_WIDTH;
	pCmdParams->nScreenSizeY = GFX_DEFAULT_SCREEN_HEIGHT;
	pCmdParams->nScreenBPP = 16;
	pCmdParams->nStencilBPP = 0;
	pCmdParams->nFreq = 0;
	pCmdParams->bUseDXT = false;
	pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
	SetGlobalVar( "windowed", "1" );
	pCmdParams->szBindName = "bind.cfg";
	pCmdParams->bMultiplayer = false;
	pCmdParams->bCycledLaunch = false;
	pCmdParams->nGuaranteeFPS = -1;
	pCmdParams->nAutoSavePeriod = 0;
	pCmdParams->eTextureQuality = ITextureManager::TEXTURE_QUALITY_HIGH;
	pCmdParams->szMapName = "";
	pCmdParams->bStartupSmoke = false;
	std::vector<std::string> szParams;
	NStr::SplitStringWithMultipleBrackets( lpCmdLine, szParams, ' ' );
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
		else if ( szParams[i] == "-x64-startup-smoke" )
		{
			pCmdParams->bStartupSmoke = true;
			SetGlobalVar( "X64.StartupSmoke", 1 );
			SetGlobalVar( "novideo", 1 );
		}
		else if ( szParams[i].compare( 0, 16, "-reference-scene" ) == 0 )
		{
			pCmdParams->bReferenceScene = true;
			pCmdParams->bStartupSmoke = true;
			pCmdParams->szReferenceScenePath = realStr.substr( 16 );
			NStr::TrimBoth( pCmdParams->szReferenceScenePath, "\"" );
			if ( pCmdParams->szReferenceScenePath.empty() && i + 1 < szParams.size() )
			{
				pCmdParams->szReferenceScenePath = szParams[++i];
				NStr::TrimBoth( pCmdParams->szReferenceScenePath, "\"" );
			}
			SetGlobalVar( "X64.StartupSmoke", 1 );
			SetGlobalVar( "X64.ReferenceScene", 1 );
			SetGlobalVar( "novideo", 1 );
		}
		else if ( szParams[i] == "-reference-resolution" && i + 2 < szParams.size() )
		{
			pCmdParams->nReferenceWidth = atoi( szParams[++i].c_str() );
			pCmdParams->nReferenceHeight = atoi( szParams[++i].c_str() );
		}
		else if ( szParams[i].compare(0, 4, "-mod") == 0 )
		{
			std::string szModDir = szParams[i].c_str() + 4;
			NStr::TrimBoth( szModDir, '"' );
			if ( !szModDir.empty() && szModDir[szModDir.size() - 1] != '\\' ) 
				szModDir += '\\';
			pCmdParams->szModName = szModDir;
		}
		else if ( szParams[i] == "-windowed" )
		{
			pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
			SetGlobalVar( "windowed", "1" );
			SetGlobalVar( "fullscreen", "0" );
		}
		else if ( szParams[i] == "-fullscreen" )
		{
			pCmdParams->eFullscreenMode = GFXFS_FULLSCREEN;
			SetGlobalVar( "fullscreen", "1" );
			SetGlobalVar( "windowed", "0" );
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
			NStr::TrimRight( szConnectParams, ':' );
			pCmdParams->szIPToGameSpyConnect = szConnectParams;

		}
		else if ( szParams[i].compare( 0, 5, "-host" ) == 0 )
		{
			std::string szConnectParams = realStr.c_str() + 5;
			if ( szConnectParams == "" )
				pCmdParams->nGameSpyHostPort = -1;
			else
				pCmdParams->nGameSpyHostPort = NStr::ToInt( szConnectParams );

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
		else if ( szParams[i].compare( 0, 7, "-cheats" ) == 0 )
		{
			SetGlobalVar( "EnableCheats", 1 );
		}
		else if ( szParams[i].compare( 0, 9, "-numsaves" ) == 0 )
		{
			SetGlobalVar( "NumSaves", 1 );
		}
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
	if ( pCmdParams->eFullscreenMode == GFXFS_WINDOWED )
		pCmdParams->nFreq = 0;
}
