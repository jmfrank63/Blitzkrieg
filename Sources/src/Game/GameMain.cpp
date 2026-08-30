#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "GameMain.h"

#if defined(_WIN32) || defined(_WIN64)
#include <shellapi.h>
#include <crtdbg.h>
#include "WinFrame.h"
#else
#include "GameFrame.h"
#endif
#include "SysKeys.h"

#include "../GFX/GFX.H"
#include "../Image/Image.h"
#include "../SFX/SFX.h"
#include "../Input/Input.h"
#include "../Input/InputTypes.h"
#include "../Scene/Scene.h"
#include "../GameTT/iMission.h"
#include "../Misc/FileUtils.h"

#include "../Net/NetDriver.h"

#include "../StreamIO/OptionSystem.h"
#include "../StreamIO/ProfilePaths.h"
#include "../StreamIO/RandomGen.h"
#include <fstream>
#include <filesystem>
#include "../StreamIO/OptionSystem.h"

#include "../Main/CloudSyncFacade.h"
#include "../Main/iMain.h"
#include "../Main/GameDB.h"
#include "../Main/GameStats.h"
#include "../Main/GameTimer.h"
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
#include "../Platform/DynamicLibrary.h"
#include "../Platform/Event.h"

#if !defined(_WIN32) && !defined(_WIN64)
namespace NWinFrame
{
static NGame::GameFrame game_frame;

HINSTANCE GetHInstance() { return nullptr; }
HWND GetHWnd() { return game_frame.BorrowWindow().value; }
void *GetSDLWindow() { return game_frame.BorrowWindow().value; }
bool InitApplication( HINSTANCE, const char *pszAppName, const char *, int nWidth, int nHeight )
{
	if ( !game_frame.Initialize( pszAppName, nWidth, nHeight ) ) return false;
	// The engine draws its own cursor, so hide the system pointer the way
	// WinFrame's SetCursor( 0 ) does; otherwise two pointers are on screen.
	game_frame.SetCursorVisible( false );
	// The app icon - macOS Dock/cmd-tab and the Linux window/taskbar; a bare
	// executable otherwise shows the generic one. The image is the original
	// Game.exe icon (main.ico); Linux loads the icon.bmp sibling because SDL
	// decodes only BMP on its own.
	std::string szIconPath = NPlatform::Paths::ModuleRoot();
	if ( !szIconPath.empty() && szIconPath.back() != '/' && szIconPath.back() != '\\' ) szIconPath += '/';
	szIconPath += "Data/icon.png";
	game_frame.SetAppIcon( szIconPath.c_str() );
	return true;
}
void ShowAppWindow( bool bShow ) { if ( bShow ) game_frame.Show(); else game_frame.Hide(); }
void ShowSplashScreen( HINSTANCE, bool bShow )
{
	// The Windows build shows the Blitzkrieg logo between launch and the
	// first video, from a topmost dialog in WinFrame.cpp blitting the
	// IDB_SPLASH resource. Same picture here, from the staged Data - SDL
	// decodes only BMP on its own, so the logo ships as Data/splash.bmp
	// (the resource bitmap, unchanged). 600x352 mirrors the Win32 splash's
	// SPLASH_SCREEN_SIZE_X/Y.
	if ( bShow )
	{
		std::string szSplashPath = NPlatform::Paths::ModuleRoot();
		if ( !szSplashPath.empty() && szSplashPath.back() != '/' && szSplashPath.back() != '\\' ) szSplashPath += '/';
		szSplashPath += "Data/splash.bmp";
		game_frame.ShowSplash( szSplashPath.c_str(), 600, 352 );
	}
	else
		game_frame.HideSplash();
}
// GameFrame only queues the translated SDL events; nothing else drains that
// queue, so without this the events accumulated forever and the game received
// no mouse or keyboard input at all. Mirror the dispatch WinFrame performs.
void PumpMessages()
{
	game_frame.PumpMessages();
	NPlatform::PlatformEvent event;
	while ( game_frame.PollEvent( event ) )
	{
		if ( !NMain::IsInitialized() ) continue;
		IInput *pInput = GetSingleton<IInput>();
		switch ( event.type )
		{
			case NPlatform::EventType::keyDown:
			case NPlatform::EventType::keyUp:
				if ( event.key == static_cast<int>( NPlatform::PlatformKey::escape ) ||
					event.key == static_cast<int>( NPlatform::PlatformKey::space ) ||
					event.key == static_cast<int>( NPlatform::PlatformKey::returnKey ) )
					pInput->AddMessage( SGameMessage( MC_MOVIE_SKIP_SEQUENCE, 0 ) );
				pInput->ConsumePlatformEvent( event );
				break;
			case NPlatform::EventType::mouseWheel:
				// A wheel event's x/y are the scroll deltas, not a position (the
				// position rides in data1/data2), so the present transform below
				// must not touch them - it turned the delta into garbage whenever
				// the presentation had an offset (fullscreen letterboxing).
				pInput->ConsumePlatformEvent( event );
				break;
			case NPlatform::EventType::mouseMotion:
			case NPlatform::EventType::mouseButtonDown:
			case NPlatform::EventType::mouseButtonUp:
				// The scene may be presented centered (1:1) or aspect-fit
				// scaled inside the window; mouse events arrive in window
				// coordinates and map into game coordinates via the
				// transform the GFX engine publishes each frame.
				event.x = int( ( event.x - GetGlobalVar( "GFX.Present.OffsetX", 0.0f ) ) * GetGlobalVar( "GFX.Present.ScaleX", 1.0f ) + 0.5f );
				event.y = int( ( event.y - GetGlobalVar( "GFX.Present.OffsetY", 0.0f ) ) * GetGlobalVar( "GFX.Present.ScaleY", 1.0f ) + 0.5f );
				if ( event.type == NPlatform::EventType::mouseMotion )
					GetSingleton<ICursor>()->SetPos( event.x, event.y );
				pInput->ConsumePlatformEvent( event );
				break;
			case NPlatform::EventType::windowDisplayChanged:
				// The OS moved the window to another display (a drag, an
				// arrangement change, an unplug). The game's monitor selection
				// follows it; CInterfaceScreenBase::Step picks the flag up and,
				// in fullscreen, re-resolves the mode against the new display.
				SetGlobalVar( "GFX.Monitor.Index", event.data1 );
				SetGlobalVar( "GFX.DisplayChanged", 1 );
				break;
			case NPlatform::EventType::textInput:
			case NPlatform::EventType::focusLost:
			// focusGained is what sets CInputAPI::bFocusCaptured, and the input
			// pump is gated on that flag. Without it the module received every
			// mouse event and discarded them all, so clicks never reached the UI.
			case NPlatform::EventType::focusGained:
				pInput->ConsumePlatformEvent( event );
				break;
			default: break;
		}
	}
}
void CaptureMouse() { game_frame.CaptureMouse(); }
void ReleaseMouse() { game_frame.ReleaseMouse(); }
bool IsActive() { return game_frame.IsActive(); }
bool IsExit() { return game_frame.IsExit(); }
void ResetExit() { game_frame.ResetExit(); }
}
#endif
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
	std::string szProfileName;						// player profile to activate (-profile=Name)

	SCmdParams() : nGameSpyHostPort( 0 ), bGameSpyPasswordRequired( false ), bStartupSmoke( false ), bReferenceScene( false ), nReferenceWidth( 0 ), nReferenceHeight( 0 ) { }
};
static void ArmAllModulesLeakOnExit()
{
	NRefCount::LeakObjectsOnExit() = true;
#if defined(_WIN32) || defined(_WIN64)
	const char *const modules[] = { "AILogic.dll", "GameTT.dll", "UI.dll", "Scene.dll" };
#elif defined(__APPLE__)
	const char *const modules[] = { "libAILogic.dylib", "libGameTT.dylib", "libUI.dylib", "libScene.dylib" };
#else
	const char *const modules[] = { "libAILogic.so", "libGameTT.so", "libUI.so", "libScene.so" };
#endif
	const std::string root = NPlatform::Paths::ModuleRoot();
	for ( const char *module : modules )
	{
		std::string path = root;
		if ( !path.empty() && path.back() != '/' && path.back() != '\\' ) path += '/';
		path += module;
		NPlatform::DynamicLibrary library( path.c_str() );
		if ( !library.IsLoaded() ) continue;
		typedef void (*ArmFunc)();
		if ( ArmFunc arm = reinterpret_cast<ArmFunc>( library.GetFunction( "ArmRefCountLeakOnExit" ) ) ) arm();
	}
}
void ProcessCommandLine( const char *lpCmdLine, SCmdParams *pCmdParams );
void ReadAndSetSunlight( CTableAccessor &table, const std::string &szSeason );
static std::string szLaunchDirectory;

// The active cloud sync's handle - startup pull, post-save push or exit
// push, one at a time - polled by the main loop until it settles; the sync
// indicator packet will own presenting its state.
static int g_nCloudStartupSync = -1;
// Post-save coalescing: the save counter last observed, and when the
// pending push is allowed to start (0 = nothing pending). Every further
// save pushes the due time out, so a burst of autosaves is one sync.
static int g_nCloudSavesSeen = 0;
static std::uint64_t g_nCloudSyncDueMs = 0;

// A Cloud.* option's value through the live option system - available once
// the config has been read, unlike the raw-scan path the startup window
// needs. Empty when unset.
static std::string CloudSyncOptionValue( const char *pszKey )
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( pszKey, &var ) )
		return std::string();
	return std::string( (const char*)bstr_t( var ) );
}
static bool CloudSyncOptionOn( const char *pszKey )
{
	return CloudSyncOptionValue( pszKey ) == "ON";
}

// A Cloud.* option's value in the profile's config - by a minimal scan of
// the raw XML, because this is asked before the option system has
// initialised (the whole point of the startup window is that the config has
// not been read yet). Inside an item the live <Var> comes first and the
// <Default> block - with its own inner <Var> - sits between it and
// <KeyName>, so the scan must take the FIRST <Var> after the enclosing
// item's start; the nearest one before the key is always the default.
// Anything missing or malformed is empty.
static std::string CloudOptionValue( const std::string &szConfigPath, const char *pszKey )
{
	std::ifstream file( szConfigPath, std::ios::binary );
	if ( !file )
		return std::string();
	std::string szContent( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );

	const std::string szNeedle = std::string( "<KeyName>" ) + pszKey + "</KeyName>";
	const std::string::size_type nKeyAt = szContent.find( szNeedle );
	if ( nKeyAt == std::string::npos )
		return std::string();
	const std::string::size_type nItemAt = szContent.rfind( "<item", nKeyAt );
	if ( nItemAt == std::string::npos )
		return std::string();
	const std::string::size_type nVarAt = szContent.find( "<Var>", nItemAt );
	if ( nVarAt == std::string::npos || nVarAt > nKeyAt )
		return std::string();
	const std::string::size_type nVarEnd = szContent.find( "</Var>", nVarAt );
	if ( nVarEnd == std::string::npos || nVarEnd > nKeyAt )
		return std::string();
	return szContent.substr( nVarAt + 5, nVarEnd - nVarAt - 5 );
}
static bool CloudOptionIsOn( const std::string &szConfigPath, const char *pszKey )
{
	return CloudOptionValue( szConfigPath, pszKey ) == "ON";
}

// Cloud.Provider is the switch. "Off" - and the pre-row "ON"/"OFF" values a
// profile written before it may still carry - mean cloud sync is off;
// anything else is the rclone backend id the profile syncs with.
static bool CloudProviderSelected( const std::string &szProvider )
{
	return !szProvider.empty() &&
		NStr::CompareAsciiNoCase( szProvider.c_str(), "Off" ) != 0 &&
		NStr::CompareAsciiNoCase( szProvider.c_str(), "On" ) != 0;
}
// The saved credentials must name the chosen backend. The row is changed
// casually (an arrow key steps it), the document only by a deliberate save
// in Config..., and a sync must never run against a service whose setup was
// never saved. Loads the library, never probes for rclone.
static bool CloudCredentialsMatch( const std::string &szProvider )
{
	char szBackend[256];
	const int nLength = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	return nLength > 0 && nLength < (int)sizeof szBackend && szProvider == szBackend;
}
// The indicator's "chosen but not set up" line. No job exists to publish
// from, so the state is written directly; the menu maps the error text to
// textes\ui\cloudsync\unconfigured.
static void PublishCloudUnconfigured()
{
	SetGlobalVar( "CloudSync.State", (int)NCloudSync::STATE_FAILED );
	SetGlobalVar( "CloudSync.Outcome", (int)NCloudSync::OUTCOME_FAILED );
	SetGlobalVar( "CloudSync.Error", "unconfigured" );
	NStr::DebugTrace( "cloud sync: provider chosen but not set up\n" );
}
int RunGame( const BkGameLaunchInfo &launch )
{
	const NPlatform::Arguments &arguments = launch.arguments;
	const int command_line_exit = NGame::CommandLineExitCode( launch.options );
	if ( command_line_exit >= 0 )
	{
		// Ahead of everything else in this function - no window, no engine
		// module, nothing loaded yet - so an invalid -mode (or -help) never
		// has to be torn back down, just reported.
		NGame::ReportCommandLine( launch.options );
		return command_line_exit;
	}
	CTimeMeter<> timeMeter;
	#if defined(_WIN32) || defined(_WIN64)
	SetErrorMode( SEM_FAILCRITICALERRORS );
	#endif
	if ( !NMain::CanLaunch() )
		return 0xDEAD;
	if ( !NPlatform::Paths::Initialize() )
		return 0xDEAD;
	BK_STARTUP_MARKER("before LoadAllModules");
	if ( NMain::LoadAllModules( NPlatform::Paths::ModuleRoot().c_str() ) <= 0 )
	{
		NPlatform::ShowError( "ERROR", "Can't load game modules..." );
		return 0xDEAD;
	}
	BK_STARTUP_MARKER("after LoadAllModules");
	// NOT armed here. LeakObjectsOnExit makes every Release() skip delete and
	// DestroyContents from that moment on; arming it at startup (44c802b47, to
	// beat Finalize() unloading the modules) silently turned the whole run into
	// a leak: no CSound/CSubstSound was ever destroyed (a downed plane's siren
	// played on after the explosion), no unit map object, path or collision was
	// ever freed, and IsValid() never went false. It is armed once the main
	// loop is over instead - see the end of the loop - still before Finalize().
	NWinFrame::ShowSplashScreen( NWinFrame::GetHInstance(), true );
	// no _CRTDBG_LEAK_CHECK_DF: refcounted objects still alive when process
	// teardown begins are leaked on purpose (see NRefCount::LeakObjectsOnExit),
	// so an exit-time leak dump would only flood the debugger output
	#if defined(_WIN32) || defined(_WIN64)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	#endif
	// The module flags were armed immediately after LoadAllModules above, before
	// any normal shutdown path can unload a module.
	// CRT assert/abort dialogs open behind the fullscreen game window; route
	// asserts to stderr and let abort() raise a fail-fast exception so an
	// attached debugger breaks instead of the process exiting with code 3.
	#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( _CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
	#endif
	
	int nLeakId = -1;
	#if defined(_WIN32) || defined(_WIN64)
	_CrtSetBreakAlloc( nLeakId );
	#else
	(void)nLeakId;
	#endif
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
	{
		// The active player profile - the owner of saves, screenshots and
		// config.cfg under profiles\<name>\. It must be settled before the
		// config is read. -profile= beats the name remembered in
		// profiles\active.cfg, which beats the default "Player".
		std::string szProfile = cmdp.szProfileName;
		const bool bFirstProfileRun = !std::filesystem::exists( "profiles/active.cfg" );
		if ( szProfile.empty() && !bFirstProfileRun )
		{
			std::ifstream file( "profiles/active.cfg" );
			if ( file )
				std::getline( file, szProfile );
		}
		szProfile = NProfile::Sanitize( szProfile );
		SetGlobalVar( "Profile.Name", szProfile.c_str() );
		std::error_code pathError;
		std::filesystem::create_directories( "profiles/" + szProfile + "/saves", pathError );
		std::filesystem::create_directories( "profiles/" + szProfile + "/screenshots", pathError );
		std::ofstream active( "profiles/active.cfg", std::ios::trunc );
		if ( active )
			active << szProfile;
		if ( bFirstProfileRun )
		{
			// One-time migration: the pre-profile layout kept saves and
			// screenshots directly in the game directory. They become the
			// first profile's data, otherwise they would vanish from the
			// load dialog. config.cfg migrates by itself - the profile-less
			// copy is the read fallback and the next write lands in the
			// profile.
			const char *legacy[][2] = { { "saves", "saves" }, { "screenshots", "screenshots" } };
			for ( int i = 0; i < 2; ++i )
			{
				const std::filesystem::path from( legacy[i][0] );
				const std::filesystem::path to( "profiles/" + szProfile + "/" + legacy[i][1] );
				if ( !std::filesystem::is_directory( from, pathError ) )
					continue;
				for ( const auto &entry : std::filesystem::directory_iterator( from, pathError ) )
					std::filesystem::rename( entry.path(), to / entry.path().filename(), pathError );
			}
		}
	}
	{
		// Cloud sync's one startup window: the active profile is settled but
		// its config has not been read yet (SerializeConfig below), so a
		// staged restore applied here is exactly what that read will see.
		// This is the only point where no SerializeConfig can have run,
		// which is why restores stage instead of writing (P04-M03). The
		// apply is unconditional and purely local: a restore the player
		// already downloaded finishes even with cloud sync off or rclone
		// missing, and "nothing staged" is the ordinary cheap answer.
		const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
		if ( NCloudSync::ApplyPendingRestore( szProfile.c_str() ) == 1 )
			NStr::DebugTrace( "cloud sync: applied a staged config restore for \"%s\"\n", szProfile.c_str() );

		// Cloud.Sync.OnStartup lives in that same unread config, so the
		// option system cannot answer yet - it has not initialised. A
		// minimal scan of the raw file for the Cloud keys is deliberate,
		// and anything missing or unparsable means disabled: a first
		// launch, a corrupt config, and a profile that predates the
		// feature must all do nothing and add no startup latency. The
		// option checks run before Available(), because Available() is
		// what probes for rclone. Cloud.Provider is read the same way, and
		// a chosen provider whose credentials are missing or name another
		// backend publishes the unconfigured indicator instead of syncing.
		const std::string szConfigPath = "profiles/" + szProfile + "/config.cfg";
		const std::string szProvider = CloudOptionValue( szConfigPath, "Cloud.Provider" );
		if ( CloudProviderSelected( szProvider ) )
		{
			if ( !CloudCredentialsMatch( szProvider ) )
				PublishCloudUnconfigured();
			else if ( CloudOptionIsOn( szConfigPath, "Cloud.Sync.OnStartup" ) && NCloudSync::Available() )
			{
				// Begin only enqueues - the daemon spawn (reaping any orphan
				// from a crashed run first, the P00-M03 identity-checked path)
				// and the run itself happen on the library's worker, and the
				// main loop polls. A slow link can never stall the first frame.
				g_nCloudStartupSync = NCloudSync::Begin( szProfile.c_str(),
					CloudOptionIsOn( szConfigPath, "Cloud.Config.Backup" ) );
				if ( g_nCloudStartupSync >= 0 )
					NStr::DebugTrace( "cloud sync: startup sync begun for \"%s\"\n", szProfile.c_str() );
				else
					NStr::DebugTrace( "cloud sync: startup sync refused: %s\n", NCloudSync::LastError() );
			}
		}
	}
	if ( cmdp.bReferenceScene )
		SetGlobalVar( "fixrandom", 1 );
	GetSingleton<IRandomGen>()->Init();
	timeMeter.Sample( "random & cmd line" );
	BK_STARTUP_MARKER("before InitApplication");
	if ( !NWinFrame::InitApplication( NWinFrame::GetHInstance(), " Blitzkrieg Game", "A7_ENGINE", cmdp.nScreenSizeX, cmdp.nScreenSizeY ) )
		return 0xDEAD;
	if ( cmdp.bStartupSmoke ) SetGlobalVar( "X64.StartupSmoke.MainMenu", 1 );
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
		if ( cmdp.bStartupSmoke ) SetGlobalVar( "X64.StartupSmoke.MainMenu", 1 );
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
	// The config is loaded before the first SetMode (it used to follow it) so
	// the window can come up in the configured mode right away instead of
	// flipping once the first interface screen applies the options. Loading
	// only fills the option values - actions run later in Init() - so the two
	// mode-defining options are peeked here by hand. The command line still
	// wins: -windowed/-fullscreen/-mode left a marker global behind.
	SerializeConfig( true, SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_HELPCALLS );
	{
		// Seed the mode-size globals from the GFX.Mode option so the first
		// SetMode already uses the configured resolution (an explicit size is
		// no longer corrected by window-size adoption - the renderer honors
		// it and centers the picture). "Auto" (or anything unparsable) is
		// 0x0, which SetMode resolves to the desktop of the target display.
		// -mode left its normalized value behind in GFX.Mode.CmdLine.Value;
		// that wins over the config option, same as -windowed/-fullscreen
		// win over GFX.FullScreen below.
		std::string szMode;
		if ( GetGlobalVar( "GFX.Mode.CmdLine", -1 ) >= 0 )
		{
			szMode = GetGlobalVar( "GFX.Mode.CmdLine.Value", "" );
		}
		else
		{
			variant_t modeVar;
			if ( GetSingleton<IOptionSystem>()->Get( "GFX.Mode", &modeVar ) )
				szMode = (const char*)bstr_t( modeVar );
		}
		if ( !szMode.empty() )
		{
			int nModeX = 0, nModeY = 0, nModeBPP = 32;
			if ( sscanf( szMode.c_str(), "%dx%dx%d", &nModeX, &nModeY, &nModeBPP ) < 2 || nModeX <= 0 || nModeY <= 0 )
				nModeX = nModeY = 0;
			SetGlobalVar( "GFX.Mode.Mission.SizeX", nModeX );
			SetGlobalVar( "GFX.Mode.Mission.SizeY", nModeY );
			SetGlobalVar( "GFX.Mode.InterMission.SizeX", nModeX );
			SetGlobalVar( "GFX.Mode.InterMission.SizeY", nModeY );
		}
	}
	// Seed the monitor choice from the profile's config the same way, so the
	// window comes up on the remembered display instead of appearing on the
	// primary one and hopping over once the first interface screen applies the
	// options. Without a profile config the option is its own default,
	// Monitor1. A -monitorN command line already left GFX.Monitor.CmdLine
	// behind and wins for the session; a -monitor="name" match goes through
	// GFX.Monitor.Name instead. A remembered display that is not connected
	// falls back to the first one - see SelectedDisplay in GraphicsEngineGpu.
	if ( GetGlobalVar( "GFX.Monitor.CmdLine", -1 ) < 0 && std::string( GetGlobalVar( "GFX.Monitor.Name", "" ) ).empty() )
	{
		variant_t var;
		if ( GetSingleton<IOptionSystem>()->Get( "GFX.Monitor", &var ) )
		{
			const std::string szMonitor = (const char*)bstr_t( var );
			if ( szMonitor.compare( 0, 7, "Monitor" ) == 0 )
				SetGlobalVar( "GFX.Monitor.Index", Max( 0, NStr::ToInt( szMonitor.substr( 7 ) ) - 1 ) );
		}
	}
	if ( GetGlobalVar( "GFX.FullScreen.CmdLine", -1 ) < 0 )
	{
		variant_t var;
		if ( GetSingleton<IOptionSystem>()->Get( "GFX.FullScreen", &var ) )
		{
			const std::string szValue = (const char*)bstr_t( var );
			cmdp.eFullscreenMode = szValue == "OFF" ? GFXFS_WINDOWED : GFXFS_FULLSCREEN;
			SetGlobalVar( "GFX.Mode.Mission.FullScreen", int(cmdp.eFullscreenMode) );
			SetGlobalVar( "GFX.Mode.InterMission.FullScreen", int(cmdp.eFullscreenMode) );
			SetGlobalVar( "GFX.Mode.Current.FullScreen", int(cmdp.eFullscreenMode) );
			SetGlobalVar( "windowed", cmdp.eFullscreenMode == GFXFS_WINDOWED ? "1" : "0" );
			SetGlobalVar( "fullscreen", cmdp.eFullscreenMode == GFXFS_FULLSCREEN ? "1" : "0" );
		}
	}
	// Frame pacing, peeked out of the config the same way as the mode above and
	// left in globals: GraphicsEngineGpu reads GFX.Present.Mode when it
	// configures the swapchain, the main loop below reads GFX.Present.MaxFPS.
	// Both have an env override so a headless A/B run can pick a mode without
	// Set()ing anything, which would persist into the player's profile.
	{
		variant_t var;
		std::string szPresentMode = "VSync";
		if ( GetSingleton<IOptionSystem>()->Get( "GFX.Present.Mode", &var ) )
			szPresentMode = (const char*)bstr_t( var );
		if ( const char *pszPresentMode = getenv( "BK_PRESENT_MODE" ) )
			szPresentMode = pszPresentMode;
		SetGlobalVar( "GFX.Present.Mode", szPresentMode.c_str() );
		int nMaxFPS = 0;
		if ( GetSingleton<IOptionSystem>()->Get( "GFX.Present.MaxFPS", &var ) )
			nMaxFPS = int( var );
		if ( const char *pszMaxFPS = getenv( "BK_MAX_FPS" ) )
			nMaxFPS = atoi( pszMaxFPS );
		SetGlobalVar( "GFX.Present.MaxFPS", Max( 0, nMaxFPS ) );
	}
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
	{
		const int nOldVideoCard = GetSingleton<IUserProfile>()->GetVar( "Autodetect.VideoCard", GFXVC_DEFAULT );
		const int nNewVideoCard = GetSingleton<IGFX>()->GetVideoCard();
		if ( nOldVideoCard != nNewVideoCard ) 
		{
			GetSingleton<IUserProfile>()->AddVar( "Autodetect.VideoCard", nNewVideoCard );
			if ( (nNewVideoCard == GFXVC_RADEON9500) || (nNewVideoCard == GFXVC_RADEON9700) ) 
			{
			GetSingleton<IOptionSystem>()->Set( "GFX.OptBuffers", variant_t( "ON" ) );
			}
		}
	}
	{
		std::string szGameSpyServer = GetGlobalVar( "Options.Multiplayer.GameSpyServerName", "" );
		if ( !szGameSpyServer.empty() )
		{
			#if defined(_WIN32) || defined(_WIN64)
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerName", variant_t( szGameSpyServer.c_str() ) );
			#else
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerName", variant_t( szGameSpyServer ) );
			#endif
		}

		if ( cmdp.bGameSpyPasswordRequired )
		{
			#if defined(_WIN32) || defined(_WIN64)
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerPassword", variant_t( cmdp.szGameSpyPassword.c_str() ) );
			#else
			GetSingleton<IOptionSystem>()->Set( "Multiplayer.ServerPassword", variant_t( cmdp.szGameSpyPassword ) );
			#endif
		}
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
		// Init() re-applies every option's action, which includes the config's
		// monitor choice - overwriting a -monitorN / -monitor="name" given on
		// the command line moments earlier. The command line wins: a numeric
		// choice is pushed into the option itself, so the settings screen
		// shows the display the game is actually on and OK confirms it rather
		// than reverting to the config. A name match has no option
		// representation, so only its globals are put back.
		//
		// Without a command line the monitor choice comes from the profile's
		// config: Init() just re-ran the GFX.Monitor action, so the profile's
		// remembered display is what the first ChangeResolution honors. (This
		// includes OS drags from the previous session - the game follows drags
		// into the option, and the profile keeps whatever display the game was
		// last on. That is the wanted behavior: settings follow the profile.)
		//
		// A command line overrides the SESSION without editing the profile's
		// stored settings: the globals below are what the engine actually
		// targets, while the options keep the profile's own values, so the
		// settings screen still shows them and the config still saves them.
		// (Confirming that screen therefore re-applies the profile's choice
		// over the command line's - the option value is what OK applies.)
		const std::string szCmdMonitorName = GetGlobalVar( "GFX.Monitor.Name", "" );
		const int nCmdMonitorIndex = GetGlobalVar( "GFX.Monitor.CmdLine", -1 );
		IOptionSystem * pOptionSystem = GetSingleton<IOptionSystem>();
		pOptionSystem->Init();
		// Init() re-ran the GFX.Monitor action, which overwrote both globals
		// from the option (and cleared the name) - reassert the command line.
		if ( nCmdMonitorIndex >= 0 )
			SetGlobalVar( "GFX.Monitor.Index", nCmdMonitorIndex );
		if ( !szCmdMonitorName.empty() )
			SetGlobalVar( "GFX.Monitor.Name", szCmdMonitorName.c_str() );
		// Same for an explicit -windowed / -fullscreen: write the desired-mode
		// globals the way the option's own action would (GFXFS_FULLSCREEN=1,
		// GFXFS_WINDOWED=2), leaving GFX.FullScreen itself at the profile's value.
		const int nCmdFullscreen = GetGlobalVar( "GFX.FullScreen.CmdLine", -1 );
		if ( nCmdFullscreen >= 0 )
		{
			const int nMode = nCmdFullscreen != 0 ? int( GFXFS_FULLSCREEN ) : int( GFXFS_WINDOWED );
			SetGlobalVar( "GFX.Mode.Mission.FullScreen", nMode );
			SetGlobalVar( "GFX.Mode.InterMission.FullScreen", nMode );
			SetGlobalVar( "windowed", nCmdFullscreen != 0 ? "0" : "1" );
			SetGlobalVar( "fullscreen", nCmdFullscreen != 0 ? "1" : "0" );
		}
		// Same for an explicit -mode: Init() re-applied the config's GFX.Mode,
		// so put the command line's normalized size back into the size globals
		// ("Auto" and anything unparsable is 0x0, which SetMode resolves to the
		// target display's desktop).
		const std::string szCmdMode = GetGlobalVar( "GFX.Mode.CmdLine", -1 ) >= 0 ? GetGlobalVar( "GFX.Mode.CmdLine.Value", "" ) : "";
		if ( !szCmdMode.empty() )
		{
			int nModeX = 0, nModeY = 0, nModeBPP = 32;
			if ( sscanf( szCmdMode.c_str(), "%dx%dx%d", &nModeX, &nModeY, &nModeBPP ) < 2 || nModeX <= 0 || nModeY <= 0 )
				nModeX = nModeY = 0;
			SetGlobalVar( "GFX.Mode.Mission.SizeX", nModeX );
			SetGlobalVar( "GFX.Mode.Mission.SizeY", nModeY );
			SetGlobalVar( "GFX.Mode.InterMission.SizeX", nModeX );
			SetGlobalVar( "GFX.Mode.InterMission.SizeY", nModeY );
		}
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
		// An explicitly named map wins over startup smoke. -reference-scene implies
		// smoke, which parked at the main menu and made it impossible to capture an
		// in-game frame without a human driving the menus.
		else if ( !cmdp.szMapName.empty() && !cmdp.bMultiplayer )
		{
			// MISSION_COMMAND_MISSION takes a map under maps\ - every menu path
			// sends a mission's FinalMap. A mission XML given on the command line
			// (scenarios\...\1.xml, tutorial\...) is stats, not a map, so resolve
			// it through the GDB to the FinalMap it names - the way the demo menu
			// cold-starts its mission - or the command bounces off the missing
			// maps\ entry into the main menu.
			std::string szLaunchMap = cmdp.szMapName;
			NStr::ToLower( szLaunchMap );
			for ( char &c : szLaunchMap )
				if ( c == '/' ) c = '\\';
			const std::string szStatsKey = szLaunchMap.substr( 0, szLaunchMap.rfind( '.' ) );
			const std::string szTerrainName = std::string( "maps\\" ) + szStatsKey;
			IDataStorage *pStorage = GetSingleton<IDataStorage>();
			if ( !pStorage->IsStreamExist( (szTerrainName + ".xml").c_str() ) &&
				 !pStorage->IsStreamExist( (szTerrainName + ".bzm").c_str() ) )
			{
				const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szStatsKey.c_str(), IObjectsDB::MISSION );
				if ( pStats != 0 && !pStats->szFinalMap.empty() )
				{
					SetGlobalVar( "Chapter.Current.Name", "custom_mission" );
					SetGlobalVar( "Mission.Current.Name", szStatsKey.c_str() );
					szLaunchMap = pStats->szFinalMap + ".xml";
				}
			}
			GetSingleton<IScenarioTracker>()->StartCampaign( "custom_mission", CAMPAIGN_TYPE_CUSTOM_MISSION );
			GetSingleton<IScenarioTracker>()->StartChapter( "custom_mission" );
			pMainLoop->Command( MISSION_COMMAND_MISSION, NStr::Format("%s;%d", szLaunchMap.c_str(), cmdp.bCycledLaunch) );
		}
		else if ( cmdp.bStartupSmoke || getenv( "BK_AUTO_UI" ) != 0 )	// BK_AUTO_UI drives the UI headless; the intro just costs frames
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
		std::uint64_t nFrameDeadline = 0;		// frame pacing target, ns on the monotonic clock
		for (;;)
		{
			if ( !cmdp.szMovieDir.empty() ) 
				SetGlobalVar( "MovieDir", cmdp.szMovieDir.c_str() );
			NWinFrame::PumpMessages();
			bool bActive = NWinFrame::IsActive();
			// The active cloud sync, observed rather than awaited: Poll is a
			// futex and a struct copy, no I/O. The menu's indicator (element
			// 21001) renders from the CloudSync.* global vars published here,
			// and its skip-to-offline click lands as a global var because the
			// handle lives here. The traces stay: they are the headless
			// evidence channel.
			if ( GetGlobalVar( "CloudSync.SkipToOffline", 0 ) )
			{
				// Consumed even with no handle - a click racing the settle
				// must not cancel a future sync.
				RemoveGlobalVar( "CloudSync.SkipToOffline" );
				if ( g_nCloudStartupSync >= 0 )
				{
					NCloudSync::Cancel( g_nCloudStartupSync );
					NStr::DebugTrace( "cloud sync: skip to offline requested\n" );
				}
			}
			if ( GetGlobalVar( "CloudSync.Recheck", 0 ) )
			{
				// The settings screen closed: re-evaluate "chosen but not set
				// up" for the indicator. Only while no run holds the handle - a
				// run's own settle owns the state until it lands.
				RemoveGlobalVar( "CloudSync.Recheck" );
				if ( g_nCloudStartupSync < 0 )
				{
					const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
					if ( CloudProviderSelected( szProvider ) && !CloudCredentialsMatch( szProvider ) )
						PublishCloudUnconfigured();
					else if ( std::string( GetGlobalVar( "CloudSync.Error", "" ) ) == "unconfigured" )
					{
						SetGlobalVar( "CloudSync.State", (int)NCloudSync::STATE_IDLE );
						SetGlobalVar( "CloudSync.Error", "" );
					}
				}
			}
			if ( g_nCloudStartupSync >= 0 )
			{
				const NCloudSync::EState eCloudState = NCloudSync::Poll( g_nCloudStartupSync );
				SetGlobalVar( "CloudSync.State", (int)eCloudState );
				if ( eCloudState == NCloudSync::STATE_DONE || eCloudState == NCloudSync::STATE_FAILED )
				{
					SetGlobalVar( "CloudSync.Outcome", (int)NCloudSync::Outcome( g_nCloudStartupSync ) );
					SetGlobalVar( "CloudSync.Error",
						eCloudState == NCloudSync::STATE_FAILED ? NCloudSync::Error( g_nCloudStartupSync ) : "" );
					if ( eCloudState == NCloudSync::STATE_FAILED )
						NStr::DebugTrace( "cloud sync: sync failed: %s\n", NCloudSync::Error( g_nCloudStartupSync ) );
					else
						NStr::DebugTrace( "cloud sync: sync finished (%s)\n",
							NCloudSync::Outcome( g_nCloudStartupSync ) == NCloudSync::OUTCOME_PAIRED ? "paired" : "synced" );
					NCloudSync::Release( g_nCloudStartupSync );
					g_nCloudStartupSync = -1;
				}
			}
			// Post-save push. Saves bump a counter (CMainLoop::Command); every
			// bump pushes the due time out five seconds, so a burst of
			// autosaves coalesces into one sync. The push waits for a quiet
			// moment: never mid-mission - a network stall must not touch
			// frame pacing during play - and never while another run holds
			// the handle. The option checks come before Available(), which
			// is the one that probes for rclone.
			{
				const int nSavesSeen = GetGlobalVar( "CloudSync.SavesSeen", 0 );
				if ( nSavesSeen != g_nCloudSavesSeen )
				{
					g_nCloudSavesSeen = nSavesSeen;
					g_nCloudSyncDueMs = NPlatform::MonotonicMilliseconds() + 5000;
				}
				if ( g_nCloudSyncDueMs != 0 && g_nCloudStartupSync < 0 &&
						 NPlatform::MonotonicMilliseconds() >= g_nCloudSyncDueMs &&
						 !GetGlobalVar( "AreWeInMission", 0 ) )
				{
					g_nCloudSyncDueMs = 0;
					const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
					if ( CloudProviderSelected( szProvider ) && CloudCredentialsMatch( szProvider ) && CloudSyncOptionOn( "Cloud.Sync.OnSave" ) &&
							 NCloudSync::Available() )
					{
						const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
						g_nCloudStartupSync = NCloudSync::Begin( szProfile.c_str(), CloudSyncOptionOn( "Cloud.Config.Backup" ) );
						if ( g_nCloudStartupSync >= 0 )
							NStr::DebugTrace( "cloud sync: post-save sync begun for \"%s\"\n", szProfile.c_str() );
					}
				}
			}
			// BK_AUTO_UI="frame:action,..." drives the UI without a human, the way
			// BK_GFX_TRACE watches the mode changes. Actions: settings | ok |
			// cancel | shot (raw RGBA dump of the frame) | exit | msg=<id> |
			// cmd=<id> | click=<x>x<y> | key=<UP|DOWN|LEFT|RIGHT|TAB|ENTER|ESC|SPACE>
			// (a real key press through the bind chain) | text=<utf8> (typed into
			// the focused edit box through the platform text path; no commas or
			// colons - they are the schedule's separators) | wheel=<delta> (a wheel
			// notch, positive up) | set=<option>=<value> | var=<name>=<value>.
			// It also reports frame timing every 120 frames, which is how the menu
			// frame rate is measured headless.
			static const char *pszAutoUI = getenv( "BK_AUTO_UI" );
			if ( pszAutoUI != 0 )
			{
				bActive = true;		// keep stepping while the window is unfocused
				static int nAutoUIFrame = 0;
				static int nAutoUIRelease = 0;
				static int vAutoUIReleasePos[2] = { 0, 0 };
				static int nAutoUIRelease2 = 0;		// pending right-button release (rclick=)
				static int vAutoUIRelease2Pos[2] = { 0, 0 };
				// Pending key releases: one slot per key, so two overlapping key=
				// actions (a key pressed one frame, another the next) each get
				// their own release. A single slot let the second press overwrite
				// the first release, leaving the first key stuck down forever -
				// which looked exactly like the game losing a key-up.
				static std::vector< std::pair<int, int> > autoUIKeyReleases;	// (code, frame)
				++nAutoUIFrame;
				for ( int nRel = 0; nRel < int( autoUIKeyReleases.size() ); )
				{
					if ( nAutoUIFrame >= autoUIKeyReleases[nRel].second )
					{
						pInput->EmulateInput( DEVICE_TYPE_KEYBOARD, autoUIKeyReleases[nRel].first, 0, DWORD( NPlatform::MonotonicMilliseconds() ), 0 );
						autoUIKeyReleases.erase( autoUIKeyReleases.begin() + nRel );
					}
					else
						++nRel;
				}
				if ( ( nAutoUIFrame % 120 ) == 0 )
				{
					// Both clocks, because they are the pair a frame-pacing change
					// must not decouple: game time is driven by the real delta
					// (CSingleTimer::Update), so capping frames may only change how
					// many frames cover a second - never how much game time one
					// second buys.
					const unsigned nGameTime = GetSingleton<IGameTimer>() ? unsigned( GetSingleton<IGameTimer>()->GetGameTime() ) : 0;
					fprintf( stderr, "BK_AUTO_UI: frame %d at %u ms game %u ms\n", nAutoUIFrame, unsigned( NPlatform::MonotonicMilliseconds() ), nGameTime );
				}
				if ( nAutoUIRelease != 0 && nAutoUIFrame >= nAutoUIRelease )
				{
					const int nPacked = ( vAutoUIReleasePos[0] & 0x7fff ) | ( ( vAutoUIReleasePos[1] & 0x7fff ) << 15 ) | 0x40000000;
					pInput->AddMessage( SGameMessage( 0x00100022 /*CMD_END_ACTION1*/, nPacked ) );
					nAutoUIRelease = 0;
				}
				if ( nAutoUIRelease2 != 0 && nAutoUIFrame >= nAutoUIRelease2 )
				{
					const int nPacked = ( vAutoUIRelease2Pos[0] & 0x7fff ) | ( ( vAutoUIRelease2Pos[1] & 0x7fff ) << 15 ) | 0x40000000;
					pInput->AddMessage( SGameMessage( 0x00100024 /*CMD_END_ACTION2*/, nPacked ) );
					nAutoUIRelease2 = 0;
				}
				const std::string szSchedule = pszAutoUI;
				for ( size_t nPos = 0; nPos < szSchedule.size(); )
				{
					size_t nEnd = szSchedule.find( ',', nPos );
					if ( nEnd == std::string::npos ) nEnd = szSchedule.size();
					const std::string szEntry = szSchedule.substr( nPos, nEnd - nPos );
					nPos = nEnd + 1;
					const size_t nColon = szEntry.find( ':' );
					if ( nColon == std::string::npos || atoi( szEntry.substr( 0, nColon ).c_str() ) != nAutoUIFrame )
						continue;
					const std::string szAction = szEntry.substr( nColon + 1 );
					fprintf( stderr, "BK_AUTO_UI: frame %d action %s\n", nAutoUIFrame, szAction.c_str() );
					if ( szAction == "settings" ) pInput->AddMessage( SGameMessage( 10005 ) );		// IMC_SETTINGS
					else if ( szAction == "ok" ) pInput->AddMessage( SGameMessage( 10002 ) );			// IMC_OK
					else if ( szAction == "cancel" ) pInput->AddMessage( SGameMessage( 10001 ) );	// IMC_CANCEL
					else if ( szAction.compare( 0, 4, "msg=" ) == 0 ) pInput->AddMessage( SGameMessage( (int)strtol( szAction.c_str() + 4, 0, 0 ) ) );
					else if ( szAction.compare( 0, 4, "cmd=" ) == 0 ) pMainLoop->Command( (int)strtol( szAction.c_str() + 4, 0, 0 ), "" );
					else if ( szAction.compare( 0, 5, "text=" ) == 0 )
					{
						// Types into the focused edit box through the real text
						// path: a synthetic platform textInput event feeds the
						// same UTF-8 queue the SDL pump fills. Click the box
						// first - focus is what routes the characters.
						NPlatform::PlatformEvent event;
						event.type = NPlatform::EventType::textInput;
						strncpy( event.text, szAction.c_str() + 5, sizeof( event.text ) - 1 );
						pInput->ConsumePlatformEvent( event );
					}
					else if ( szAction.compare( 0, 6, "click=" ) == 0 )
					{
						int nClickX = 0, nClickY = 0;
						if ( sscanf( szAction.c_str() + 6, "%dx%d", &nClickX, &nClickY ) == 2 )
						{
							// The UI receives clicks as CMD_BEGIN/END_ACTION1 carrying
							// the position packed the way CInputAPI packs it.
							const int nPacked = ( nClickX & 0x7fff ) | ( ( nClickY & 0x7fff ) << 15 ) | 0x40000000;
							GetSingleton<ICursor>()->SetPos( nClickX, nClickY );
							pInput->AddMessage( SGameMessage( 0x00100021 /*CMD_BEGIN_ACTION1*/, nPacked ) );
							nAutoUIRelease = nAutoUIFrame + 2;
							vAutoUIReleasePos[0] = nClickX; vAutoUIReleasePos[1] = nClickY;
						}
					}
					else if ( szAction.compare( 0, 7, "rclick=" ) == 0 )
					{
						// Right button: the order/forced-action button in a mission (a left
						// click with nothing selected cancels a pending forced action).
						int nClickX = 0, nClickY = 0;
						if ( sscanf( szAction.c_str() + 7, "%dx%d", &nClickX, &nClickY ) == 2 )
						{
							const int nPacked = ( nClickX & 0x7fff ) | ( ( nClickY & 0x7fff ) << 15 ) | 0x40000000;
							GetSingleton<ICursor>()->SetPos( nClickX, nClickY );
							pInput->AddMessage( SGameMessage( 0x00100023 /*CMD_BEGIN_ACTION2*/, nPacked ) );
							nAutoUIRelease2 = nAutoUIFrame + 2;
							vAutoUIRelease2Pos[0] = nClickX; vAutoUIRelease2Pos[1] = nClickY;
						}
					}
					else if ( szAction.compare( 0, 7, "camera=" ) == 0 )
					{
						// Move the view: "camera=XxY" in world coordinates (the numbers
						// BK_REFERENCE_SCENE prints; 'x' separates them because the schedule
						// itself is comma-separated), so a capture can look at a spot the
						// action moved to - a plane at altitude is drawn far up-screen
						// from its ground position.
						float fAnchorX = 0.0f, fAnchorY = 0.0f;
						if ( sscanf( szAction.c_str() + 7, "%fx%f", &fAnchorX, &fAnchorY ) == 2 )
							if ( ICamera *pCamera = GetSingleton<ICamera>() )
								pCamera->SetAnchor( CVec3( fAnchorX, fAnchorY, 0.0f ) );
					}
					else if ( szAction == "shot" )
					{
						IGFX *pGFXShot = GetSingleton<IGFX>();
						const CTRect<long> rcShot = pGFXShot->GetScreenRect();
						CPtr<IImage> shotImage = GetImageProcessor()->CreateImage( rcShot.Width(), rcShot.Height() );
						pGFXShot->Flip();
						if ( pGFXShot->TakeScreenShot( shotImage ) )
						{
							FILE *pShotFile = fopen( NStr::Format( "autoshot_%d_%dx%d.rgba", nAutoUIFrame, shotImage->GetSizeX(), shotImage->GetSizeY() ), "wb" );
							if ( pShotFile )
							{
								const SColor *pPixels = shotImage->GetLFB();
								for ( int n = 0; n < shotImage->GetSizeX() * shotImage->GetSizeY(); ++n )
								{
									const BYTE rgba[4] = { pPixels[n].r, pPixels[n].g, pPixels[n].b, 255 };
									fwrite( rgba, 4, 1, pShotFile );
								}
								fclose( pShotFile );
								fprintf( stderr, "BK_AUTO_UI: shot written\n" );
							}
						}
						else
							fprintf( stderr, "BK_AUTO_UI: shot failed\n" );
					}
					else if ( szAction.compare( 0, 4, "key=" ) == 0 )
					{
						// Emulates a real key press through the input device layer, so
						// the whole bind chain (bind section, command registration) is
						// what a test exercises - unlike msg=, which injects past it.
						// The codes are the DIK-compatible config ABI (InputCodes.cpp).
						static const struct { const char *pszName; int nCode; } autoUIKeys[] =
						{
							{ "UP", 0xc8 }, { "DOWN", 0xd0 }, { "LEFT", 0xcb }, { "RIGHT", 0xcd },
							{ "TAB", 0x0f }, { "ENTER", 0x1c }, { "ESC", 0x01 }, { "SPACE", 0x39 },
						};
						const std::string szKey = szAction.substr( 4 );
						for ( int nKey = 0; nKey < int( sizeof(autoUIKeys) / sizeof(autoUIKeys[0]) ); ++nKey )
						{
							if ( szKey == autoUIKeys[nKey].pszName )
							{
								pInput->EmulateInput( DEVICE_TYPE_KEYBOARD, autoUIKeys[nKey].nCode, 0x80, DWORD( NPlatform::MonotonicMilliseconds() ), 0 );
								autoUIKeyReleases.push_back( std::make_pair( autoUIKeys[nKey].nCode, nAutoUIFrame + 2 ) );
								break;
							}
						}
					}
					else if ( szAction.compare( 0, 6, "wheel=" ) == 0 )
					{
						// Wheel notches through the device path, positive is wheel-up;
						// scaled to the legacy WHEEL_DELTA units the same way
						// SDLApplication scales a real wheel event.
						pInput->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_Z, atoi( szAction.c_str() + 6 ) * 120, DWORD( NPlatform::MonotonicMilliseconds() ), 0 );
					}
					else if ( szAction == "exit" ) pMainLoop->Command( MAIN_COMMAND_EXIT_GAME, 0 );
					else if ( szAction.compare( 0, 4, "var=" ) == 0 )
					{
						const std::string szPair = szAction.substr( 4 );
						const size_t nEq = szPair.find( '=' );
						if ( nEq != std::string::npos )
							SetGlobalVar( szPair.substr( 0, nEq ).c_str(), szPair.substr( nEq + 1 ).c_str() );
					}
					else if ( szAction.compare( 0, 4, "set=" ) == 0 )
					{
						const std::string szPair = szAction.substr( 4 );
						const size_t nEq = szPair.find( '=' );
						if ( nEq != std::string::npos )
							GetSingleton<IOptionSystem>()->Set( szPair.substr( 0, nEq ), variant_t( szPair.substr( nEq + 1 ).c_str() ) );
					}
				}
			}
			pInput->PumpMessages( bActive );
			if ( NWinFrame::IsExit() )
			{
				NWinFrame::ResetExit();
				pMainLoop->Command( MAIN_COMMAND_EXIT_GAME, 0 );				// generate 'EXIT' command
			}
			// BK_REFERENCE_CAMERA="x,y" pins the camera anchor so a capture can be
			// taken of somewhere other than wherever the mission happens to start.
			// A headless run has nobody to scroll the view, so without this the
			// only part of a map a renderer regression can be checked against is
			// the opening screenful.
			static const char *pszCameraAnchor = getenv( "BK_REFERENCE_CAMERA" );
			if ( cmdp.bReferenceScene && pszCameraAnchor != 0 )
			{
				float fAnchorX = 0.0f, fAnchorY = 0.0f;
				if ( sscanf( pszCameraAnchor, "%f,%f", &fAnchorX, &fAnchorY ) == 2 )
				{
					if ( ICamera *pCamera = GetSingleton<ICamera>() )
						pCamera->SetAnchor( CVec3( fAnchorX, fAnchorY, 0.0f ) );
				}
			}
			if ( !pMainLoop->StepApp( bActive ) )
				break;
			// BK_REFERENCE_DELAY overrides the 5-frame main-menu capture point so a
			// capture can be taken once a mission has loaded, which is what makes
			// automated renderer verification possible without a human at the keyboard.
			static const char *pszCaptureDelay = getenv( "BK_REFERENCE_DELAY" );
			static const int nCaptureDelay = pszCaptureDelay != 0 ? atoi( pszCaptureDelay ) : 5;
			const bool bCaptureGate = pszCaptureDelay != 0 || GetGlobalVar( "X64.StartupSmoke.MainMenu", 0 ) != 0;
			if ( cmdp.bReferenceScene && bCaptureGate && ++nReferenceCaptureDelay >= nCaptureDelay )
			{
				IGFX *pGFX = GetSingleton<IGFX>();
				// Printed in the form BK_REFERENCE_CAMERA takes, so a capture can be
				// repeated somewhere else on the map without guessing coordinates.
				if ( ICamera *pCamera = GetSingleton<ICamera>() )
					NPlatform::DebugWrite( NStr::Format( "BK_REFERENCE_SCENE: camera anchor %.0f,%.0f\n", pCamera->GetAnchor().x, pCamera->GetAnchor().y ) );
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
			if ( cmdp.bStartupSmoke && !cmdp.bReferenceScene )
			{
					NPlatform::DebugWrite( "BK_STARTUP: C6 main menu smoke checkpoint passed\n" );
				break;
			}
			if ( !bActive )
				NPlatform::SleepMilliseconds( 40 );
			// Frame pacing. GFX.Present.MaxFPS caps the loop (0 = unlimited);
			// menus are held to 60 whatever it says, because an interface screen
			// redraws an almost static picture and running it faster only burns
			// the GPU. AreWeInMission is the same flag the options screen uses to
			// tell the two apart.
			//
			// The wait runs against a rolling deadline rather than a fixed sleep
			// per frame: WaitAndAcquireGPUSwapchainTexture inside BeginScene
			// already blocks for most of a present interval, so a sleep added on
			// top of it would stack with that block and deliver half the rate
			// asked for.
			//
			// This does not change game speed. CSingleTimer::Update is handed an
			// absolute clock and advances by the real delta, and the simulation
			// runs a whole number of 50 ms segments out of that delta
			// (CSinglePlayerTransceiver::DoSegments), so a longer frame simply
			// covers more segments. -fps is the exception - it feeds the timer a
			// synthetic clock of nGuaranteeFPS ms per iteration, making game time
			// the frame count itself - so the cap stays out of its way.
			if ( GetGlobalVar( "GuaranteeFPS", -1 ) == -1 )
			{
				int nMaxFPS = GetGlobalVar( "GFX.Present.MaxFPS", 0 );
				if ( GetGlobalVar( "AreWeInMission", 0 ) == 0 )
					nMaxFPS = nMaxFPS > 0 ? Min( nMaxFPS, 60 ) : 60;
				if ( nMaxFPS <= 0 )
					nFrameDeadline = 0;
				else
				{
					const std::uint64_t nPeriod = 1000000000ULL / std::uint64_t( nMaxFPS );
					const std::uint64_t nNow = NPlatform::MonotonicNanoseconds();
					// A frame that overran by more than one period restarts the
					// schedule instead of catching up: mission loading blocks the
					// loop for seconds, and repaying that debt would let a burst of
					// uncapped frames through afterwards.
					if ( nFrameDeadline == 0 || nNow > nFrameDeadline + nPeriod )
						nFrameDeadline = nNow + nPeriod;
					else
					{
						// Wait out the rest of the frame, then hold the deadline
						// exactly. SleepPreciseNanoseconds lands within tens of
						// microseconds of it, so the loop below finds the deadline
						// already reached and the wait costs no CPU at all; measured
						// on an M1 at 60 FPS it is a fifth of what the millisecond
						// sleep below costs, with no more frame-to-frame jitter.
						//
						// Where there is no high-resolution timer it waits for
						// nothing and says so, and the old strategy stands:
						// SleepMilliseconds overshoots by a scheduler quantum, which
						// at 60 Hz is most of a frame, so sleep to a millisecond
						// short of the deadline and spin out the remainder. Spinning
						// is what keeps the pacing tight - dropping it in favour of
						// one coarse sleep costs more in jitter than it saves in CPU
						// - so it stays as the backstop on both paths.
						if ( nFrameDeadline > nNow &&
							 !NPlatform::SleepPreciseNanoseconds( nFrameDeadline - nNow ) &&
							 nFrameDeadline > nNow + 1000000ULL )
							NPlatform::SleepMilliseconds( std::uint32_t( ( nFrameDeadline - nNow - 1000000ULL ) / 1000000ULL ) );
						while ( NPlatform::MonotonicNanoseconds() < nFrameDeadline )
							;
						nFrameDeadline += nPeriod;
					}
				}
			}
		}
		// Catch-all: any exit path that bypassed CICExitGame (e.g. smoke-test
		// break) still tears the world down here. Leak refcounted objects from
		// now on - in this module and in every loaded one; they are all still
		// loaded here, Finalize() below is what unloads them.
		ArmAllModulesLeakOnExit();
		pMainLoop->ResetStack();
		UnRegisterSingleton( IMainLoop::tidTypeID );
		SerializeConfig( false, SERIALIZE_CONFIG_OPTIONS | SERIALIZE_CONFIG_BINDS | SERIALIZE_CONFIG_HELPCALLS );
		{
			// The exit push, after the config write so a backup snapshots the
			// final settings and the last saves are on disk. Honours
			// Cloud.Sync.OnExit under a chosen and set-up provider, and also
			// flushes a post-save push that was still coalescing rather than
			// dropping it. The wait is bounded: on timeout the run is
			// abandoned - the profile simply stays ahead of the cloud and the
			// next startup pull converges - and Shutdown() runs on this path
			// regardless, so the daemon dies with the game (a crash skips all
			// of this; the Windows job object and the next launch's
			// identity-checked reap cover it).
			const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
			const bool bExitSyncWanted = CloudProviderSelected( szProvider ) && CloudCredentialsMatch( szProvider ) &&
				( CloudSyncOptionOn( "Cloud.Sync.OnExit" ) || g_nCloudSyncDueMs != 0 ) &&
				NCloudSync::Available();
			if ( g_nCloudStartupSync < 0 && bExitSyncWanted )
			{
				const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
				g_nCloudStartupSync = NCloudSync::Begin( szProfile.c_str(), CloudSyncOptionOn( "Cloud.Config.Backup" ) );
			}
			if ( g_nCloudStartupSync >= 0 )
			{
				NStr::DebugTrace( "cloud sync: finishing before exit\n" );
				const std::uint64_t nDeadlineMs = NPlatform::MonotonicMilliseconds() + 15000;
				NCloudSync::EState eCloudState = NCloudSync::Poll( g_nCloudStartupSync );
				while ( eCloudState != NCloudSync::STATE_DONE && eCloudState != NCloudSync::STATE_FAILED &&
								NPlatform::MonotonicMilliseconds() < nDeadlineMs )
				{
					NPlatform::SleepMilliseconds( 100 );
					eCloudState = NCloudSync::Poll( g_nCloudStartupSync );
				}
				if ( eCloudState == NCloudSync::STATE_DONE )
					NStr::DebugTrace( "cloud sync: exit sync finished\n" );
				else if ( eCloudState == NCloudSync::STATE_FAILED )
					NStr::DebugTrace( "cloud sync: exit sync failed: %s\n", NCloudSync::Error( g_nCloudStartupSync ) );
				else
					NStr::DebugTrace( "cloud sync: exit sync timed out; the next startup pull converges\n" );
				NCloudSync::Release( g_nCloudStartupSync );
				g_nCloudStartupSync = -1;
			}
			NCloudSync::Shutdown();
		}
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

int GameMain( const NPlatform::Arguments &arguments )
{
	BkGameLaunchInfo launch;
	launch.arguments = arguments;
	launch.options = NGame::ParseCommandLine( arguments );
	return RunGame( launch );
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
		else if ( szParams[i] == "-x64-startup-smoke" || szParams[i] == "-startup-smoke" )
		{
			pCmdParams->bStartupSmoke = true;
			SetGlobalVar( "X64.StartupSmoke", 1 );
			SetGlobalVar( "X64.StartupSmoke.MainMenu", 1 );
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
		else if ( szParams[i] == "-mode" || szParams[i].compare(0, 6, "-mode=") == 0 )
		{
			// -mode=WxH / -mode=WxHxBPP (BPP defaults to 32) / -mode=auto:
			// sets the resolution exactly like picking it in the options
			// screen. Exact "-mode" or "-mode=..." only, not a bare prefix
			// match: "-mode" is also a prefix of "-mod<dir>" (e.g.
			// -modExpansion), and a loose compare(0,5,"-mode") here would
			// swallow that mod argument before the -mod branch ever saw it,
			// silently dropping the mod load. Garbage is tolerated here (the
			// flag is dropped and the config, or the option's own default,
			// keeps driving the mode) rather than rejected: main.cpp's
			// ParseCommandLine/CommandLineExitCode already reject it before
			// RunGame - the only caller of this function - ever gets this
			// far, so this is a defensive fallback, not the enforcement
			// point.
			std::string szMode = szParams[i].c_str() + 5;
			if ( !szMode.empty() && szMode[0] == '=' )
				szMode = szMode.substr( 1 );
			NStr::TrimBoth( szMode, "\"" );
			int nModeX = 0, nModeY = 0, nModeBPP = 32;
			const bool bAuto = szMode == "auto";
			if ( bAuto || ( sscanf( szMode.c_str(), "%dx%dx%d", &nModeX, &nModeY, &nModeBPP ) >= 2 && nModeX > 0 && nModeY > 0 ) )
			{
				const std::string szNormalized = bAuto ? "Auto" : NStr::Format( "%dx%dx%d", nModeX, nModeY, nModeBPP );
				SetGlobalVar( "GFX.Mode.CmdLine", 1 );
				SetGlobalVar( "GFX.Mode.CmdLine.Value", szNormalized.c_str() );
			}
		}
		else if ( szParams[i].compare(0, 4, "-mod") == 0 )
		{
			std::string szModDir = szParams[i].c_str() + 4;
			NStr::TrimBoth( szModDir, '"' );
			if ( !szModDir.empty() && szModDir[szModDir.size() - 1] != '\\' )
				szModDir += '\\';
			pCmdParams->szModName = szModDir;
		}
		else if ( szParams[i].compare( 0, 8, "-profile" ) == 0 )
		{
			// -profile=Name (or -profileName): play with this player profile
			// for the session and remember it as the active one.
			std::string szName = realStr.substr( 8 );
			if ( !szName.empty() && szName[0] == '=' )
				szName = szName.substr( 1 );
			NStr::TrimBoth( szName, "\"" );
			pCmdParams->szProfileName = szName;
		}
		else if ( szParams[i] == "-windowed" )
		{
			pCmdParams->eFullscreenMode = GFXFS_WINDOWED;
			SetGlobalVar( "windowed", "1" );
			SetGlobalVar( "fullscreen", "0" );
			SetGlobalVar( "GFX.FullScreen.CmdLine", 0 );
		}
		else if ( szParams[i] == "-fullscreen" )
		{
			pCmdParams->eFullscreenMode = GFXFS_FULLSCREEN;
			SetGlobalVar( "fullscreen", "1" );
			SetGlobalVar( "windowed", "0" );
			SetGlobalVar( "GFX.FullScreen.CmdLine", 1 );
		}
		else if ( szParams[i].compare( 0, 8, "-monitor" ) == 0 )
		{
			// Accepts either a 1-based ordinal (-monitor1 is the primary display,
			// -monitor2 the second, matching the "Monitor2" naming the options
			// screen uses) or part of the display's name (-monitor="Mi monitor"),
			// because the index order changes as monitors are plugged in and a
			// name survives that. GFX.Monitor.Index itself stays 0-based.
			std::string szMonitor = realStr.substr( 8 );
			if ( !szMonitor.empty() && szMonitor[0] == '=' )
				szMonitor = szMonitor.substr( 1 );
			NStr::TrimBoth( szMonitor, "\"" );
			bool bNumeric = !szMonitor.empty();
			for ( int n = 0; n < szMonitor.size() && bNumeric; ++n )
				bNumeric = isdigit( (unsigned char)szMonitor[n] ) != 0;
			// The marker distinguishes "the command line asked for this display"
			// from the profile's own remembered choice, which is seeded into
			// GFX.Monitor.Index further down. A command line overrides the
			// session only: the option - what the settings screen shows and
			// what the profile saves - is deliberately left alone.
			if ( bNumeric )
			{
				SetGlobalVar( "GFX.Monitor.Index", Max( 0, atoi( szMonitor.c_str() ) - 1 ) );
				SetGlobalVar( "GFX.Monitor.CmdLine", Max( 0, atoi( szMonitor.c_str() ) - 1 ) );
			}
			else if ( !szMonitor.empty() )
				SetGlobalVar( "GFX.Monitor.Name", szMonitor.c_str() );
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
			SetGlobalVar( "Options.Multiplayer.GameSpyPlayerName", NPlatform::WordStringData( NPlatform::WordStringFromWide( szWideNick.c_str() ) ) );
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
