#include "StdAfx.h"

#include "MainLoopCommands.h"

#include "..\Input\Input.h"
#include "..\SFX\SFX.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\GameTT\iMission.h"
#include "..\Formats\fmtSaveLoad.h"
#include "..\StreamIO\RandomGen.h"
#include "..\StreamIO\StreamIOTypes.h"
#include "..\StreamIO\StreamAdaptor.h"
#include "..\StreamIO\ProgressHook.h"
#include "TextSystem.h"
#include "CommandsHistoryInterface.h"
#include "ScenarioTracker.h"
#include "iMainClassIDs.h"
#include "iMainCommands.h"
#include "iMainInternal.h"
#include "RandomMapHelper.h"
#include "..\Platform\Clock.h"
#include "..\Platform\Debug.h"
static void TraceLoadProgress( const char *pszBaseDir, const char *pszMessage )
{
	if ( pszBaseDir == 0 || pszMessage == 0 )
		return;
	const std::string szTraceFileName = std::string( pszBaseDir ) + "load_trace.log";
	FILE *pFile = fopen( szTraceFileName.c_str(), "ab" );
	if ( pFile )
	{
		fprintf( pFile, "%u %s\n", NPlatform::MonotonicMilliseconds(), pszMessage );
		fclose( pFile );
	}
	NPlatform::DebugWriteFormat( "LOADTRACE: %s\n", pszMessage );
}
void ReportSaveLoad( const char *pszKey, const std::string &szFileName )
{
	if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetString(pszKey) )
	{
		if ( pText->GetString() != 0 ) 
		{
			std::wstring szString( reinterpret_cast<const wchar_t*>( pText->GetString() ) );
			szString += L" " + NStr::ToUnicode( szFileName );
			GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
		}
	}
}
void CICSave::Exec( IMainLoop *pML )
{
	if ( GetGlobalVar("SaveHistoryFileName", (const char*)0) != 0 )
		GetSingleton<ICommandsHistory>()->Save();

	if ( GetGlobalVar("MultiplayerGame", 0) != 0 ) // saves in multiplayer are not allowed
		return;

	if ( !bAutoSave && GetGlobalVar( "AreWeInMission", 0 ) && GetGlobalVar("Options.MissionSave.Disabled", 0) != 0 )
		return;

	std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	if ( !szModname.empty() )
	{
		szModname = "mods\\" + szModname;
	}
	const std::string szFullFileName = std::string( pML->GetBaseDir() )  + szModname + "saves\\"+ szFileName;
	CPtr<IDataStream> pStream = CreateFileStream( szFullFileName.c_str(), STREAM_ACCESS_WRITE );
	if ( pStream )
	{
		pML->ClearResources( false );
		{
			NSaveLoad::SFileHeader hdr;
			hdr.szTitleName = L"UNKNOWN Title";
			hdr.szChapterName = GetGlobalVar( "Chapter.Current.Name", "UNKNOWN Chapter" );
			hdr.szMissionName = GetGlobalVar( "Mission.Current.Name", "UNKNOWN Mission" );
			hdr.bRandomMission = ( GetGlobalVar( "AreWeInMission", (const char*)0 ) != 0 ) && 
				                   ( GetGlobalVar( ("Mission." + hdr.szMissionName + ".Random").c_str(), 0 ) != 0 );
			const DWORD dwSignature = NSaveLoad::SFileHeader::SIGNATURE;
			CStreamAccessor stream = pStream;
			stream << dwSignature;
			stream << hdr;
			if ( hdr.bRandomMission ) 
			{
				NSaveLoad::SRandomHeader rndhdr;
				CPtr<IRandomGenSeed> pSeed = 0;
				StoreRandomMap( hdr.szMissionName, &rndhdr, &pSeed );
				stream << rndhdr;
				pSeed->Store( stream );
			}
		}
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		pML->Serialize( pSS );
	}
	if ( pStream != 0 ) 
		ReportSaveLoad( "game_saved", szFileName );
}
void CICLoad::Exec( IMainLoop *pML )
{
	TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec begin" );
	if ( GetGlobalVar("MultiplayerGame", 0) != 0  )
	{
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec skipped multiplayer" );
		return;
	}
	std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	if ( !szModname.empty() )
	{
		szModname = "mods\\" + szModname;
	}
	const std::string szFullFileName = std::string( pML->GetBaseDir() ) + szModname + "saves\\" + szFileName;
	CPtr<IDataStream> pStream = OpenFileStream( szFullFileName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Can't find file \"%s\" to load - skipping...", szFileName.c_str()), 0xffff0000 );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec open failed" );
		return;
	}
	TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec stream opened" );
	{
		CStreamAccessor stream = pStream;
		DWORD dwSignature = 0;
		stream >> dwSignature;
		if ( dwSignature == NSaveLoad::SFileHeader::SIGNATURE ) 
		{
			TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec header signature" );
			NSaveLoad::SFileHeader hdr;
			stream >> hdr;
			if ( hdr.nVersion != NSaveLoad::SFileHeader::VERSION )
			{
				GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Invalid save file \"%s\" of version %d (current version = %d)", szFullFileName.c_str(), hdr.nVersion, NSaveLoad::SFileHeader::VERSION), 0xffff0000 );
				TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec version mismatch" );
				return;
			}
			// Pause streaming before interface teardown: PopInterface destroys the
			// entire world (hundreds of refcounted objects, texture/mesh releases)
			// which is a massive allocation burst that starves the audio callback.
			GetSingleton<ISFX>()->PauseStreaming( true );
			// Lower main thread priority during load: the audio mixer thread is
			// realtime, but the main thread's allocation storm monopolizes CPU.
			// Dropping to BELOW_NORMAL ensures the audio callback always gets
			// scheduled even during the heaviest load bursts.
			SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
			TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec streaming paused, thread priority lowered" );
			TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec popping interfaces" );
			// Keep the shared managers populated while the old world dies:
			// their SDSM_MERGE deserialize then reuses every same-name
			// resident resource (a reload of the running mission needs no
			// disk I/O at all). One purge runs after Serialize instead.
			static_cast<CMainLoop*>( pML )->SetDeferResourcePurge( true );
			while ( pML->GetInterface() )
				pML->PopInterface();
			TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec interfaces popped" );
			if ( hdr.bRandomMission ) 
			{
				TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec restore random map begin" );
				NSaveLoad::SRandomHeader rndhdr;
				stream >> rndhdr;
				CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
				pSeed->Restore( stream );
				RestoreRandomMap( hdr.szMissionName, rndhdr, pSeed );
				TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec restore random map end" );
			}
			pStream = new CStreamRangeAdaptor( pStream, pStream->GetPos(), pStream->GetSize() );
			TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec range stream ready" );
		}
		else
			pStream->Seek( -sizeof(dwSignature), STREAM_SEEK_CUR );
	}
	{
		// Streaming already paused before interface teardown above.
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec progress init begin" );
		CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
		pProgress->Init( IMovieProgressHook::PT_LOAD );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec progress init end" );
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ, pProgress );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec serialize begin" );
		pML->Serialize( pSS, pProgress );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec serialize end" );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec progress stop begin" );
		pProgress->Stop();
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec progress stop end" );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec deferred purge begin" );
		static_cast<CMainLoop*>( pML )->SetDeferResourcePurge( false );
		pML->ClearResources( false );		// drop what the loaded world doesn't reference
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec deferred purge end" );
		// Resume a few frames after CMD_LOAD_FINISHED so first-frame world init
		// and lazy uploads do not immediately starve streaming again.
		static_cast<CMainLoop*>( pML )->SetResumeStreamingAfterSteps( GetGlobalVar( "Sound.LoadResumeStreamingSteps", 6 ) );
		// Restore main thread priority now that the load storm is over.
		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
		GetSingleton<IUserProfile>()->RegisterLoad( GetSingleton<IScenarioTracker>()->GetCurrMissionGUID() );
		TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec register load end" );
	}
	ReportSaveLoad( "game_loaded", szFileName );
	pML->EnableMessageProcessing( true );
	pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_LOAD_FINISHED) );
	TraceLoadProgress( pML->GetBaseDir(), "CICLoad::Exec end" );
}
void CICSendCommand::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ':' );
	if ( !szStrings.empty() )
		nCommand = NStr::ToInt( szStrings[0] );
	if ( szStrings.size() > 1 )
		nParam = NStr::ToInt( szStrings[1] );
}
void CICSendCommand::Exec( IMainLoop *pML )
{
	IInput *pInput = GetSingleton<IInput>();
	NI_ASSERT_T( pInput != 0, "ERROR - Can't send command - input is not registered in the singleton" );
	pInput->AddMessage( SGameMessage(nCommand, nParam) );
}
// The whole game is quitting. Arm the leak-on-exit flag in EVERY module
// before ResetStack destroys the world: the tank/turret/etc. refcount cycles
// are torn down here during the main loop, while the per-module static-ctor
// arming (DllMain/atexit) hasn't fired yet — without this, ~CTank cascades
// into a re-entrant double-free. The bool lives per-module (inline static in
// Basic.h), so each DLL's exported ArmRefCountLeakOnExit must be called.
static void ArmAllModulesLeakOnExit()
{
	NRefCount::LeakObjectsOnExit() = true;
	const char *const modules[] = { "AILogic.dll", "GameTT.dll", "UI.dll", "Scene.dll" };
	for ( const char *mod : modules )
	{
		HMODULE h = ::GetModuleHandleA( mod );
		typedef void (*ArmFn)();
		ArmFn fn = h ? reinterpret_cast<ArmFn>( ::GetProcAddress( h, "ArmRefCountLeakOnExit" ) ) : 0;
		if ( fn )
			fn();
	}
}
void CICExitGame::Exec( IMainLoop *pML )
{
	ArmAllModulesLeakOnExit();
	GetSingleton<ISFX>()->StopStream();
	pML->ResetStack();
	pML->Command( MISSION_COMMAND_VIDEO, "demo\\exit;-1" );
}
void ClearMOD()
{
	GetSingleton<IDataStorage>()->RemoveStorage( "MOD" );
	RemoveGlobalVar( "MOD.Active" );
	RemoveGlobalVar( "MOD.Name" );
	RemoveGlobalVar( "MOD.Version" );
	GetSingleton<IUserProfile>()->SetMOD( "" );
}
void CICChangeMOD::Configure( const char *pszConfig ) 
{ 
	szMOD.clear();
	if ( pszConfig ) 
	{
		szMOD = pszConfig;
		NStr::ToLower( szMOD );
		if ( !szMOD.empty() && (szMOD[szMOD.size() - 1] != '\\') ) 
			szMOD += '\\';
	}
}
void CICChangeMOD::Exec( IMainLoop *pML )
{
	const std::string szMODPath = std::string( pML->GetBaseDir() ) + "mods\\" + szMOD + "data\\";
	if ( szMOD.empty() ) 
		ClearMOD();
	else if ( CPtr<IDataStorage> pMOD = OpenStorage((szMODPath + "*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON) )
	{
		if ( CPtr<IDataStream> pStream = pMOD->OpenStream("mod.xml", STREAM_ACCESS_READ) )
		{
			GetSingleton<IDataStorage>()->RemoveStorage( "MOD" );
			GetSingleton<IDataStorage>()->AddStorage( pMOD, "MOD" );
			std::string szMODName = "MyMOD", szMODVersion = "1.0";
			{
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
				saver.Add( "MODName", &szMODName );
				saver.Add( "MODVersion", &szMODVersion );
			}
			SetGlobalVar( "MOD.Active", 1 );
			SetGlobalVar( "MOD.Name", szMODName.c_str() );
			SetGlobalVar( "MOD.Version", szMODVersion.c_str() );			
			GetSingleton<IUserProfile>()->SetMOD( szMOD );
		}
		else
			ClearMOD();
	}
	else
		ClearMOD();
	pML->ClearResources( true );
	GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAR_ALL );
	pML->ResetStack();
	GetSingleton<IFilesInspector>()->Clear();
	GetSingleton<IFilesInspector>()->InspectStorage( GetSingleton<IDataStorage>() );
	GetSingleton<IObjectsDB>()->LoadDB();
	GetSingleton<IGFX>()->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
}
void CICPauseGame::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	std::vector<std::string> strings;
	NStr::SplitString( pszConfig, strings, ';' );
	NI_ASSERT_SLOW_T( strings.size() == 2, NStr::Format("Can't retrieve pause mode and pause reason from \"%s\"", pszConfig) );
	if ( strings.size() == 2 ) 
	{
		bSetPause = strings[0][0] == '0' ? false : true;
		nPauseReason = NStr::ToInt( strings[1] );
	}
	else
	{
		bSetPause = false;
		nPauseReason = -1;
	}
}
void CICPauseGame::Exec( IMainLoop *pML )
{
	pML->Pause( bSetPause, nPauseReason );
}
