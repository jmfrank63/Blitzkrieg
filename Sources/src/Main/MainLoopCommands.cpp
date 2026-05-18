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
#include "RandomMapHelper.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** save and load
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSave::Exec( IMainLoop *pML )
{
	// save history
	if ( GetGlobalVar("SaveHistoryFileName", (const char*)0) != 0 )
		GetSingleton<ICommandsHistory>()->Save();
	// mission saves in multiplayer & impossible difficulty are disabled

	if ( GetGlobalVar("MultiplayerGame", 0) != 0 ) // saves in multiplayer are not allowed
		return;

	// all exept autosaves in-mission are not allowed 
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
		// create and store file header (/w signature)
		{
			NSaveLoad::SFileHeader hdr;
			hdr.szTitleName = L"UNKNOWN Title";
			hdr.szChapterName = GetGlobalVar( "Chapter.Current.Name", "UNKNOWN Chapter" );
			hdr.szMissionName = GetGlobalVar( "Mission.Current.Name", "UNKNOWN Mission" );
			hdr.bRandomMission = ( GetGlobalVar( "AreWeInMission", (const char*)0 ) != 0 ) && 
				                   ( GetGlobalVar( ("Mission." + hdr.szMissionName + ".Random").c_str(), 0 ) != 0 );
			//
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
		// store structurized game context
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		pML->Serialize( pSS );
	}
	// report about save
	if ( pStream != 0 ) 
		ReportSaveLoad( "game_saved", szFileName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICLoad::Exec( IMainLoop *pML )
{
	if ( GetGlobalVar("MultiplayerGame", 0) != 0  )
		return;
	// check for such file exist
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
		return;
	}
	// load and check header
	{
		CStreamAccessor stream = pStream;
		DWORD dwSignature = 0;
		stream >> dwSignature;
		if ( dwSignature == NSaveLoad::SFileHeader::SIGNATURE ) 
		{
			NSaveLoad::SFileHeader hdr;
			stream >> hdr;
			if ( hdr.nVersion != NSaveLoad::SFileHeader::VERSION )
			{
				GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Invalid save file \"%s\" of version %d (current version = %d)", szFullFileName.c_str(), hdr.nVersion, NSaveLoad::SFileHeader::VERSION), 0xffff0000 );
				return;
			}
			// remove all interfaces!
			while ( pML->GetInterface() ) 
				pML->PopInterface();
			//
			if ( hdr.bRandomMission ) 
			{
				// read random header
				NSaveLoad::SRandomHeader rndhdr;
				stream >> rndhdr;
				// read random seed for the map generator
				CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
				pSeed->Restore( stream );
				//
				RestoreRandomMap( hdr.szMissionName, rndhdr, pSeed );
			}
			// 'trunk' stream to the new range
			pStream = new CStreamRangeAdaptor( pStream, pStream->GetPos(), pStream->GetSize() );
		}
		else
			pStream->Seek( -sizeof(dwSignature), STREAM_SEEK_CUR );
	}
	// load structurized game context
	{
		CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
		pProgress->Init( IMovieProgressHook::PT_LOAD );
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ, pProgress );
		pML->Serialize( pSS, pProgress );
		pProgress->Stop();
		GetSingleton<IUserProfile>()->RegisterLoad( GetSingleton<IScenarioTracker>()->GetCurrMissionGUID() );
	}
	// report about load
	//pML->FinishWaitLoadingMovie();
	ReportSaveLoad( "game_loaded", szFileName );
	//
	pML->EnableMessageProcessing( true );
	pML->Command( MAIN_COMMAND_CMD, NStr::Format("%d", CMD_LOAD_FINISHED) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** send command
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSendCommand::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	// config = "commandID : param";
	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ':' );
	if ( !szStrings.empty() )
		nCommand = NStr::ToInt( szStrings[0] );
	if ( szStrings.size() > 1 )
		nParam = NStr::ToInt( szStrings[1] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSendCommand::Exec( IMainLoop *pML )
{
	IInput *pInput = GetSingleton<IInput>();
	NI_ASSERT_T( pInput != 0, "ERROR - Can't send command - input is not registered in the singleton" );
	pInput->AddMessage( SGameMessage(nCommand, nParam) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** exit from game command
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICExitGame::Exec( IMainLoop *pML )
{
	GetSingleton<ISFX>()->StopStream();
	pML->ResetStack();
	pML->Command( MISSION_COMMAND_VIDEO, "demo\\exit;-1" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** change MOD command
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ClearMOD()
{
	GetSingleton<IDataStorage>()->RemoveStorage( "MOD" );
	RemoveGlobalVar( "MOD.Active" );
	RemoveGlobalVar( "MOD.Name" );
	RemoveGlobalVar( "MOD.Version" );
	GetSingleton<IUserProfile>()->SetMOD( "" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
			//
			std::string szMODName = "MyMOD", szMODVersion = "1.0";
			{
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
				saver.Add( "MODName", &szMODName );
				saver.Add( "MODVersion", &szMODVersion );
			}
			//
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
	//
	pML->ClearResources( true );
	GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAR_ALL );
	pML->ResetStack();
	//
	GetSingleton<IFilesInspector>()->Clear();
	GetSingleton<IFilesInspector>()->InspectStorage( GetSingleton<IDataStorage>() );
	//
	GetSingleton<IObjectsDB>()->LoadDB();
	GetSingleton<IGFX>()->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** pause/unpause game
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CICPauseGame::Exec( IMainLoop *pML )
{
	pML->Pause( bSetPause, nPauseReason );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
