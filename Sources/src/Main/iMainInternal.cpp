#include "StdAfx.h"

#include <mmsystem.h>

#include "iMainInternal.h"
#include "iMainCommands.h"
#include "SaveCommandsHistoryCommand.h"
#include "EmergencySave.h"
#include "..\Misc\FileUtils.h"
#include "..\Misc\Win32Random.h"
#include "..\SFX\SFX.h"
#include "..\GameTT\iMission.h"
#include "..\Common\PauseGame.h"
#include "..\GameTT\CommonID.h"
#include "..\Main\GameStats.h"
#include "..\Scene\PFX.h"
#include "..\Input\InputTypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const NInput::SRegisterCommandEntry stdCommands[] = 
{
	{ "save"			,	CMD_SAVE				},
	{ "load"			,	CMD_LOAD				},
	{ "screenshot", CMD_SCREENSHOT	},
#ifndef _FINALRELEASE
	{ "exit"			,	CMD_EXIT				},
	{ "save_input", CMD_SAVE_INPUT	},
	{ "play_input", CMD_PLAY_INPUT	},
	{ "wireframe"	,	CMD_WIREFRAME		},
	{ "memdump"		, CMD_DUMP_MEMORY },
#endif // _FINALRELEASE
	//
	{ "game_speed_inc", CMD_GAME_SPEED_INC_SEND	},
	{ "game_speed_dec", CMD_GAME_SPEED_DEC_SEND	},
	{ "game_pause"		, CMD_GAME_PAUSE_SEND	},
	//double clicks
	{ "mouse0_dblclk"	,	CMD_MOUSE0_DBLCLK		},
	// actions
	{ "begin_action1"	,	CMD_BEGIN_ACTION1		},
	{ "end_action1"		,	CMD_END_ACTION1			},
	{ "begin_action2"	,	CMD_BEGIN_ACTION2		},
	{ "end_action2"		,	CMD_END_ACTION2			},
	//
	{ "mouse_button0_down", CMD_MOUSE_BUTTON0_DOWN },
	{ "mouse_button0_up"	, CMD_MOUSE_BUTTON0_UP	 },
	{ "mouse_button1_down", CMD_MOUSE_BUTTON1_DOWN },
	{ "mouse_button1_up"	, CMD_MOUSE_BUTTON1_UP	 },
	{ "mouse_button2_down", CMD_MOUSE_BUTTON2_DOWN },
	{ "mouse_button2_up"	, CMD_MOUSE_BUTTON2_UP	 },
	{ "numpad_enter"			,	CMD_NUMPAD_ENTER			 },
	//
	{ "switch_input"			, CMD_SWITCH_INPUT			 },
	//
	{ 0,						0								}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeConfig( const bool bRead, const DWORD dwSerialize )
{
	if ( !bRead && !GetSingleton<IUserProfile>()->IsChanged() ) 
		return true;

	const std::string szConfigName = GetGlobalVar( "ConfigName", "config.cfg" );
	const std::string szConfigFileName = ".\\" + szConfigName;
	const std::string szDefaultValuesName = GetGlobalVar( "DefaultConfigName", "defconf.cfg" );
	const std::string szDefaultValuesFileName = ".\\" + szDefaultValuesName;
	
	if ( bRead && !NFile::IsFileExist(szConfigFileName.c_str()) &&
								!NFile::IsFileExist(szDefaultValuesName.c_str())	) 
		return false;
	//

	CPtr<IDataStream> pStream = OpenFileStream(szConfigFileName.c_str(), bRead ? STREAM_ACCESS_READ : STREAM_ACCESS_WRITE);
	// to repair from
	CPtr<IDataStream> pStreamToRepair;
	CPtr<IDataTree> pTreeToRepair;
	if( bRead )
	{
		pStreamToRepair = OpenFileStream( szDefaultValuesFileName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT_T( pStreamToRepair != 0, NStr::Format( "NO CONFIG FILE %s", szDefaultValuesFileName.c_str() ) );
		pTreeToRepair = CreateDataTreeSaver(pStreamToRepair, IDataTree::READ );
	}

	CPtr<IDataTree> pTree;
	if ( pStream && (!bRead || 0 != pStream->GetSize()) )
		pTree = CreateDataTreeSaver(pStream, bRead ? IDataTree::READ : IDataTree::WRITE);

	if ( pTree ) 
		GetSingleton<IUserProfile>()->SerializeConfig( pTree );
	if ( pTreeToRepair ) 
		GetSingleton<IUserProfile>()->Repair( pTreeToRepair, false );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** main loop 
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IMainLoop* STDCALL CreateMainLoop()
{
	IBaseCommand *pCommand = new CSaveCommandsHistoryCommand( GetSingleton<ICommandsHistory>() );
	NBugSlayer::AddEmergencyCommand( pCommand );
	
	IMainLoop *pML = new CMainLoop();
	//
	CPtr<IDataStream> pStream = CreateFileStream( ".\\saves\\emergency.sav", STREAM_ACCESS_WRITE );
	if ( pStream )
	{
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		if ( pSS )
		{
			IBaseCommand *pCommand = new CEmergencySave( pML, pSS );
			NBugSlayer::AddEmergencyCommand( pCommand );
		}
	}
	//
	return pML;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMainLoop::CMainLoop()
: bAppIsActive( false ), bWireFrame( false ), bPaused( false )
{ 
	bDisableMessageProcessing = false;
	nAutoSavePeriod = GetGlobalVar( "autosave", 0 ) * 1000;
	timeLastAutoSave = 0;
	// base dir
	{
		char buff[2048];
		GetCurrentDirectory( 2048, buff );
		szBaseDir = buff;
		if ( szBaseDir.empty() )
			szBaseDir = ".\\";
		else if ( szBaseDir[szBaseDir.size() - 1] != '\\' )
			szBaseDir += '\\';
	}
	//
	nNetAppID = -1;
	nNetPort = 8889;
	// acquire all singletons
	pGFX = GetSingleton<IGFX>();
	pInput = GetSingleton<IInput>();
	pScene = GetSingleton<IScene>();
	pCamera = GetSingleton<ICamera>();
	pCursor = GetSingleton<ICursor>();
	//
	standardMsgs.Init( pInput, stdCommands );
	// retrieve all shared data managers
	{
		int nNumObjects = 0;
		IRefCount **pObjects = 0;
		GetSingletonGlobal()->GetAllObjects( &pObjects, &nNumObjects );
		NI_ASSERT_T( pObjects != 0, "Can't get objects from the signleton" );
		for ( int i=0; i<nNumObjects; ++i )
		{
			if ( ISharedManager *pSM = dynamic_cast<ISharedManager*>( pObjects[i] ) )
				managers.push_back( pSM );
		}
	}

	// AI part
	pAILogic = GetSingleton<IAILogic>();
	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
	pGameTimer->GetGameSegmentTimer()->SetSegmentTime( SAIConsts::AI_SEGMENT_DURATION );
	pGameTimer->GetSyncSegmentTimer()->SetSegmentTime( SAIConsts::AI_SEGMENT_DURATION );
	// end of AI part

	// guaranee FPS
	nGuaranteeFPS = GetGlobalVar( "GuaranteeFPS", -1 );
	nGuaranteeFPSTime = 0;
	//hThread = 0;
	//hFinishedLoading = CreateEvent( 0, true, false, 0 );
	//hThreadFinished = CreateEvent( 0, true, false, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMainLoop::~CMainLoop()
{
	while ( !interfaces.empty() ) 
	{
		interfaces.back()->Done();
		interfaces.pop_back();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::Serialize( IStructureSaver *pSS, IProgressHook *pHook )
{
	CSaverAccessor saver = pSS;
	if ( pHook ) 
		pHook->SetNumSteps( 6 );
	// serialize shared managers. shared managers IDs begin from the 100
	for ( CManagersList::iterator i = managers.begin(); i != managers.end(); ++i )
	{
		(*i)->operator&( *pSS );
	}
	// step hook
	if ( pHook ) 
	{
		pHook->Recover();
		pHook->Step(); // 0
	}
	// serialize commands
	saver.Add( 1, &cmds );
	// serialize interfaces
	saver.Add( 2, &interfaces );
	// step hook
	if ( pHook ) 
		pHook->Step(); // 1
	//
	if ( saver.IsReading() )
	{
		// init interfaces
		for ( CInterfacesList::iterator i = interfaces.begin(); i != interfaces.end(); ++i )
			(*i)->Init();
	}
	// serialize global objects
	// scene
	saver.Add( 3, pScene.GetPtr() );
	// step hook
	if ( pHook ) 
		pHook->Step(); // 2
	// camera and cursor
	saver.Add( 4, pCamera.GetPtr() );
	saver.Add( 5, pCursor.GetPtr() );
	// game timer
	saver.Add( 6, GetSingleton<IGameTimer>() );
	// serialize data
	saver.Add( 7, &bWireFrame );
	// AI variables	
	saver.Add( 8, pAILogic.GetPtr() );
	// 
	// step hook
	if ( pHook ) 
		pHook->Step(); // 3
	// CRAP{ resore font
	saver.Add( 9, pGFX.GetPtr() );
	// CRAP}
	// global vars system serialization
	saver.Add( 10, GetSingleton<IGlobalVars>() );
	// serialize sounds
	saver.Add( 11, GetSingleton<ISFX>() );
	// serialize transceiver
	saver.Add( 12, GetSingleton<ITransceiver>() );
	//
	// ack manager
	saver.Add( 13, GetSingleton<IClientAckManager>() );
	// step hook
	if ( pHook ) 
		pHook->Step(); // 4
	// random gen
	if ( g_pGlobalRandomGen ) 
		saver.Add( 14, g_pGlobalRandomGen );
	// input
	saver.Add( 15, GetSingleton<IInput>() );
	//
	saver.Add( 16, &nAutoSavePeriod );
	saver.Add( 17, &timeLastAutoSave );
	// scenario tracker
	saver.Add( 18, GetSingleton<IScenarioTracker>() );
	// enabble/disable message processing
	saver.Add( 19, &bDisableMessageProcessing );
//	// timeout counter
//	saver.Add( 21, &timeout );
	//
	saver.Add( 22, &pStoredScenarioTracker );
	//
	saver.Add( 23, GetSingleton<ICommandsHistory>() );
	//
//	saver.Add( 24, GetSingleton<IFilesInspector>() );
	// serialize added missions in chapter stats
	{
		const std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
		const SChapterStats *pChapter = static_cast<const SChapterStats*>( GetSingleton<IObjectsDB>()->GetGameStats(szChapterName.c_str(), IObjectsDB::CHAPTER) );
		if ( pChapter ) 
		{
			std::vector<SChapterStats::SMission> missions;
			if ( saver.IsReading() ) 
			{
				SChapterStats *pNCChapter = const_cast<SChapterStats*>( pChapter );
				// read stored template missions
				saver.Add( 25, &missions );
				// restore template missions
				pNCChapter->RemoveTemplateMissions();
				for ( int i = 0; i < missions.size(); ++i )
					pNCChapter->AddMission( missions[i] );
				// write resulting stats
				IDataStorage *pStorage = GetSingleton<IDataStorage>();
				const std::string szChapterFileName = pStorage->GetName() + szChapterName + ".xml";
				CPtr<IDataStream> pStream = CreateFileStream( szChapterFileName.c_str(), STREAM_ACCESS_WRITE );
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
				saver.Add( "RPG", pNCChapter );
			}
			else
			{
				// form template missions
				for ( int i = 0; i < pChapter->missions.size(); ++i )
				{
					if ( pChapter->missions[i].pMission && pChapter->missions[i].pMission->IsTemplate() ) 
						missions.push_back( pChapter->missions[i] );
				}
				// store template missions list
				saver.Add( 25, &missions );
			}
		}
	}
	if ( saver.IsReading() )
	{
		// pause game after loading
		bPaused = false;
		Pause( bPaused = !bPaused, PAUSE_TYPE_USER_PAUSE );
		// restore wireframe state
		pGFX->SetWireframe( bWireFrame );		
	}
	// step hook
	if ( pHook ) 
		pHook->Step(); // 5
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::SerializeConfig( const bool bRead, const DWORD dwSerialize )
{
	::SerializeConfig( bRead, dwSerialize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::Pause( const bool _bPause, const int _nPauseReason )
{
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
	pTimer->PauseGame( _bPause, _nPauseReason );
	const bool bPauseSFXes = pTimer->HasPause( PAUSE_TYPE_USER_PAUSE ) ||
		                       pTimer->HasPause( PAUSE_TYPE_MENU ) ||
		                       pTimer->HasPause( PAUSE_TYPE_MP_TIMEOUT );
	const bool bPauseALL = pTimer->HasPause( PAUSE_TYPE_INACTIVE );
	//
	if ( bPauseALL || bPauseSFXes ) 
		GetSingleton<ISFX>()->Pause( true );
	else
		GetSingleton<ISFX>()->Pause( false );
	//
	if ( bPauseALL ) 
		GetSingleton<ISFX>()->PauseStreaming( true );
	else
		GetSingleton<ISFX>()->PauseStreaming( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::ClearResources( const bool bClearAll )
{
	if ( bClearAll ) 
	{
		GetSingleton<IGFX>()->Clear();
		GetSingleton<IParticleManager>()->Clear( ISharedManager::CLEAR_ALL );
		GetSingleton<IAnimationManager>()->Clear( ISharedManager::CLEAR_ALL );
		GetSingleton<ISoundManager>()->Clear( ISharedManager::CLEAR_ALL );
		GetSingleton<IFontManager>()->Clear( ISharedManager::CLEAR_ALL );
		GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAR_ALL );
		GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAR_ALL );
		// we can't clear all text data because it's a inter-interface data!
		GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		// restore system font
		GetSingleton<IGFX>()->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
	}
	else
	{
		GetSingleton<IParticleManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<IAnimationManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<ISoundManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<IFontManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
		GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::ResetStack()
{
	while ( !interfaces.empty() ) 
	{
		interfaces.back()->OnGetFocus( false );
		interfaces.back()->Done();
		interfaces.pop_back();
	}
	// clear all managers data
	GetSingleton<IScene>()->Clear();
	GetSingleton<ICursor>()->Clear();
}
void CMainLoop::SetInterface( IInterfaceBase *pNewInterface )
{
	NI_ASSERT_T( pNewInterface->IsValid(), NStr::Format("Invalid Interface of class \"%s\"", typeid(*pNewInterface).name()) );
	ResetStack();
	interfaces.push_back( pNewInterface );
	pNewInterface->OnGetFocus( true );
}
void CMainLoop::PushInterface( IInterfaceBase *pNewInterface )
{
	NI_ASSERT_T( pNewInterface->IsValid(), NStr::Format("Invalid Interface of class \"%s\"", typeid(*pNewInterface).name()) );
	if ( !interfaces.empty() ) 
		interfaces.back()->OnGetFocus( false );
	interfaces.push_back( pNewInterface );
	pNewInterface->OnGetFocus( true );
}
void CMainLoop::PopInterface()
{
	if ( !interfaces.empty() ) 
	{
		interfaces.back()->OnGetFocus( false );
		interfaces.back()->Done();
		interfaces.pop_back();
	}
	//
	if ( !interfaces.empty() )
		interfaces.back()->OnGetFocus( true );
	// clear unreferenced resources
	ClearResources( false );
}
IInterfaceBase* CMainLoop::GetInterface() const
{
  if ( interfaces.empty() )
    return 0;
  return interfaces.back();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int nShotIndex = 0;
static bool bUseInput = true;
static int nSaveNum = 30;
void CMainLoop::ProcessStandardMsgs( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case CMD_WIREFRAME:
			pGFX->SetWireframe( bWireFrame = !bWireFrame );
			break;
		case CMD_SCREENSHOT:
			{
				CTRect<long> rcRect = pGFX->GetScreenRect();
				CPtr<IImage> pImage = GetImageProcessor()->CreateImage( rcRect.Width(), rcRect.Height() );
				if ( pGFX->TakeScreenShot(pImage) )
				{
					while ( NFile::IsFileExist(NStr::Format("%sscreenshots\\shot%.4d.tga", szBaseDir.c_str(), nShotIndex)) ) 
						++nShotIndex;
					//
					CPtr<IDataStream> pStream = CreateFileStream( NStr::Format("%sscreenshots\\shot%.4d.tga", szBaseDir.c_str(), nShotIndex), STREAM_ACCESS_WRITE );
					GetImageProcessor()->SaveImageAsTGA( pStream, pImage );
					//
					if ( IText *pText = GetSingleton<ITextManager>()->GetDialog("textes\\strings\\screenshot") )
					{
						const std::wstring wszShotName = std::wstring(reinterpret_cast<const wchar_t*>(pText->GetString())) + NStr::ToUnicode( NStr::Format(" screenshots\\shot%.4d.tga", nShotIndex) );
						GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, wszShotName.c_str(), 0xff00ff00 );
					}
					//
					++nShotIndex;
				}
			}
			break;
		case CMD_SAVE:
			{
#ifndef _FINALRELEASE		
				if ( GetGlobalVar( "NumSaves", 0 ) == 1 )
				{
					nSaveNum = ( nSaveNum + 1 ) % 30;					
					Command( MAIN_COMMAND_SAVE, NStr::Format( "quick%d.sav", nSaveNum ) );
				}
				else
#endif // _FINALRELEASE
					Command( MAIN_COMMAND_SAVE, "quick.sav" );
			}
			break;
		case CMD_LOAD:
			{
#ifndef _FINALRELEASE						
				if ( GetGlobalVar( "NumSaves", 0 ) == 1 )
					Command( MAIN_COMMAND_LOAD, NStr::Format( "quick%d.sav", nSaveNum ) );
				else
#endif // _FINALRELEASE
					Command( MAIN_COMMAND_LOAD, "quick.sav" );
			}

			break;
		case CMD_GAME_PAUSE_SEND:
			GetSingleton<ITransceiver>()->CommandClientTogglePause();
			break;
		case CMD_GAME_PAUSE:
			bPaused = !bPaused;
			Pause( bPaused, PAUSE_TYPE_USER_PAUSE );
			break;
		case CMD_DUMP_MEMORY:
			NBugSlayer::MemSystemDumpStats();
			break;
		case CMD_EXIT:
			Command( MAIN_COMMAND_EXIT_GAME, 0 );
			break;
		case CMD_MP_PLAYER_STATE_CHANGED:
			OnMultiplayerStateCommand( msg );
			break;
		case CMD_LOAD_FINISHED:
			{
				GetSingleton<IScene>()->Reposition();
				const RECT rcRect = GetSingleton<IGFX>()->GetScreenRect();
				GetSingleton<ICursor>()->SetBounds( rcRect.left, rcRect.top, rcRect.right, rcRect.bottom );
			}
			break;
		case CMD_SWITCH_INPUT:
			bUseInput = !bUseInput;
			if ( bUseInput ) 
			{
				pInput->SetDeviceEmulationStatus( DEVICE_TYPE_MOUSE, false );
				GetSingleton<ICursor>()->SetUpdateMode( ICursor::UPDATE_MODE_INPUT );
			}
			else
			{
				pInput->SetDeviceEmulationStatus( DEVICE_TYPE_MOUSE, true );
				GetSingleton<ICursor>()->SetUpdateMode( ICursor::UPDATE_MODE_WINDOWS );
			}
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::OnMultiplayerStateCommand( const SGameMessage &msg )
{
	const int nPlayer = ( msg.nParam & 0xff00 ) >> 8;	// player's id 
	const int nEvent = ( msg.nParam & 0x00ff );
	const int nTime = ( msg.nParam & 0xffff0000 ) >> 16;
	
	IScenarioTracker * pTracker = GetSingleton<IScenarioTracker>();
	ITransceiver * pTransceiver = GetSingleton<ITransceiver>();
	
	// 0 - remove, 1 - out of sync, 2 - lag started, 3 - lag finished
	switch( nEvent )
	{
		case 0:	// remove
			{
				ITextManager * pTM = GetSingleton<ITextManager>();
				IPlayerScenarioInfo * pPlayerInfo = pTracker->GetPlayer( nPlayer );
				IConsoleBuffer * pBuffer = GetSingleton<IConsoleBuffer>();

				IText * pPlayer = pTM->GetString( "Textes\\UI\\MultiplayerMessages\\player" );
				NI_ASSERT_T( pPlayer != 0, "Textes\\UI\\MultiplayerMessages\\player not found" );
				IText * pLeftTheGame = pTM->GetString( "Textes\\UI\\MultiplayerMessages\\LeftTheGame" );
				NI_ASSERT_T( pLeftTheGame != 0, "Textes\\UI\\MultiplayerMessages\\LeftTheGame not found" );

				if ( pPlayer && pPlayerInfo && pLeftTheGame )
				{
					std::wstring szOutput = pPlayer->GetString();
					szOutput += NStr::ToUnicode( " " );
					szOutput += pPlayerInfo->GetName();
					szOutput += NStr::ToUnicode( " " );
					szOutput += pLeftTheGame->GetString();
					pBuffer->Write( CONSOLE_STREAM_CHAT, szOutput.c_str(), 0xffff0000 );
				}
				// if player was lagged, it is not lagged anymore
				pInput->AddMessage( SGameMessage(MC_MP_LAG_FINISHED, nPlayer) );
				pInput->AddMessage( SGameMessage(MC_MP_PLAYER_LOAD_FINISHED, nPlayer) );
				GetSingleton<IScene>()->AddSound( "Int_information", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
			}
			
			break;
		case 1: // out of sync
			{
				ITextManager * pTM = GetSingleton<ITextManager>();
				IPlayerScenarioInfo * pPlayerInfo = pTracker->GetPlayer( nPlayer );
				IConsoleBuffer * pBuffer = GetSingleton<IConsoleBuffer>();

				IText * pPlayer = pTM->GetString( "Textes\\UI\\MultiplayerMessages\\player" );
				NI_ASSERT_T( pPlayer != 0, "Textes\\UI\\MultiplayerMessages\\player not found" );
				IText * pOutOfSync = pTM->GetString( "Textes\\UI\\MultiplayerMessages\\OutOfSync" );
				NI_ASSERT_T( pOutOfSync != 0, "Textes\\UI\\MultiplayerMessages\\OutOfSync not found" );

				if ( pPlayer && pPlayerInfo && pOutOfSync )
				{
					std::wstring szOutput = pPlayer->GetString();
					szOutput += NStr::ToUnicode( " " );
					szOutput += pPlayerInfo->GetName();
					szOutput += NStr::ToUnicode( " " );
					szOutput += pOutOfSync->GetString();
					
					pBuffer->Write( CONSOLE_STREAM_CHAT, szOutput.c_str(), 0xffff0000 );
				}
				GetSingleton<IScene>()->AddSound( "Int_information", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
				pTransceiver->CommandClientDropPlayer( pPlayerInfo->GetName().c_str() );
			}
			
			break;
		case 2: // lag started
			pInput->AddMessage( SGameMessage(MC_MP_LAG_STARTED, (nPlayer) | (nTime<<16)) );

			break;
		case 3:	// lag finished 
			pInput->AddMessage( SGameMessage(MC_MP_LAG_FINISHED, nPlayer) );

			break;
		case 4: // loading started
			pInput->AddMessage( SGameMessage(MC_MP_PLAYER_LOAD_STARTED, nPlayer ) );

			break;
		case 5: // load finished
			pInput->AddMessage( SGameMessage(MC_MP_PLAYER_LOAD_FINISHED, nPlayer ) );

			break;
		case 6: // local player is out of sync from all
			pInput->AddMessage( SGameMessage(MC_LOCAL_PLAYER_OUT_OF_SYNC) );
			break;

		case 7: // some players are out of sync from all
			pInput->AddMessage( SGameMessage(MC_LOCAL_PLAYER_OUT_OF_SYNC) );
			break;
			
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::Command( IInterfaceCommand *pCmd )
{
	cmds.push_back( pCmd );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::Command( int nCommandID, const char *pszConfiguration )
{
	if ( nCommandID != -1 ) 
	{
		IInterfaceCommand *pCmd = CreateObject<IInterfaceCommand>( nCommandID );
		NI_ASSERT_TF( pCmd != 0, NStr::Format("Can't create command 0x%x", nCommandID), return );
		pCmd->Configure( pszConfiguration );
		Command( pCmd );
	}
	else
		Command( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetFileVersionString( const std::string &szFileName, std::string *pVersion )
{
	VS_FIXEDFILEINFO version;
	if ( GetFileVersion(szFileName, &version) != false )
	{
		*pVersion = NStr::Format( "%d.%d.%d.%d", (version.dwProductVersionMS >> 16) & 0xffff, 
			                        version.dwProductVersionMS & 0xffff, (version.dwProductVersionLS >> 16) & 0xffff, 
															version.dwProductVersionLS & 0xffff );
	}
	else
		*pVersion = "\"UNKNOWN\"";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ReportDesc( const SModuleDescriptor *pDesc, IConsoleBuffer *pCB )
{
	const std::string szModuleFileName = NMain::GetModuleFileNameByDesc( pDesc );
	std::string szVersion;
	GetFileVersionString( szModuleFileName, &szVersion );
	pCB->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Module \"%s\"of version %s", pDesc->pszName, szVersion.c_str()), 0xffffffff, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ReportMainModuleVersion( IConsoleBuffer *pCB )
{
	char buffer[2048];
	GetModuleFileName( 0, buffer, 2048 );
	std::string szVersion;
	GetFileVersionString( buffer, &szVersion );
	pCB->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Main Module Version: %s", szVersion.c_str()), 0xffffffff, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ParseWorldStreamCommands()
{
	IConsoleBuffer *pCB = GetSingleton<IConsoleBuffer>();
	std::list<std::string> returncommands;
	while ( const char *pszCommand = pCB->ReadASCII(CONSOLE_STREAM_WORLD) )
	{
		std::string szCommand = pszCommand;
		std::vector<std::string> szStrings;
		NStr::SplitString( szCommand, szStrings, ' ' );
		if ( szStrings[0] == "version" ) 
		{
			if ( const SModuleDescriptor *pDesc = NMain::GetFirstModuleDesc() )
			{
				pCB->WriteASCII( CONSOLE_STREAM_CHAT, "Loaded Modules:", 0xffffffff, true );
				ReportDesc( pDesc, pCB );
				while ( const SModuleDescriptor *pDesc = NMain::GetNextModuleDesc() ) 
					ReportDesc( pDesc, pCB );
			}
			// main build version
			ReportMainModuleVersion( pCB );
		}
		else if ( szStrings[0] == "dumpvars" ) 
		{
			GetSingleton<IGlobalVars>()->DumpVars( szStrings[1].c_str() );
		}
		else
			returncommands.push_back( pszCommand );
	}
	// add unprocessed commands
	for ( std::list<std::string>::const_iterator it = returncommands.begin(); it != returncommands.end(); ++it )
		pCB->WriteASCII( CONSOLE_STREAM_WORLD, it->c_str() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CMainLoop::ProcessTimeoutMsg( const SGameMessage &msg )
{
	switch ( msg.nEventID ) 
	{
		case CMD_GAME_TIMEOUT:
			if ( const NTimer::STime timeTimeout = GetGlobalVar("Multiplayer.Game.Timeout", 30000) ) 
				timeout.Init( timeTimeout );
			break;
		case CMD_GAME_UNTIMEOUT:
			timeout.Done();
			break;
	}
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainLoop::StepApp( bool bActive )
{
	bAppIsActive = bActive;
	// do autosave
#ifndef _FINALRELEASE	
	if ( nAutoSavePeriod > 0 ) 
	{
		const NTimer::STime timeCurrTime = GetSingleton<IGameTimer>()->GetGameTime();
		if ( timeCurrTime > timeLastAutoSave + nAutoSavePeriod ) 
		{
			timeLastAutoSave = timeCurrTime;
			Command( MAIN_COMMAND_SAVE, "auto.sav" );
		}
	}
#endif // _FINALRELEASE
	// execute interface (overlord) commands
	CInterfaceCommandsList delayedCommands;
	const NTimer::STime timeAbs = timeGetTime();
	while ( !cmds.empty() )
	{
		CPtr<IInterfaceCommand> pCmd = cmds.front();
		cmds.pop_front();
		if ( (pCmd == 0) || !pCmd->IsValid() )
		{
			NBugSlayer::RemoveAllEmergencyCommands();
			ResetStack();
			return false;
		}
		if ( pCmd->GetDelayedTime() <= timeAbs ) 
			pCmd->Exec( this );
		else
			delayedCommands.push_back( pCmd );
	}
	// re-store delayed commands
	for ( CInterfaceCommandsList::iterator it = delayedCommands.begin(); it != delayedCommands.end(); ++it )
		cmds.push_back( *it );
	// check for empty interfaces stack
	if ( interfaces.empty() )
	{
		NI_ASSERT_T( !interfaces.empty(), "Can't perform execution more: empty interfaces stack... leaving..." );
		NBugSlayer::RemoveAllEmergencyCommands();
		return false;
	}
	NI_ASSERT_T( interfaces.back()->IsValid(), NStr::Format("Invalid Interface of class \"%s\"", typeid(*interfaces.back()).name()) );
	// parse WORLD commands from console
	ParseWorldStreamCommands();
	// update game timer
	if ( nGuaranteeFPS == -1 ) 
	{
		NHPTimer::STime hptime;
		NHPTimer::GetTime( &hptime );
		GetSingleton<IGameTimer>()->Update( DWORD(NHPTimer::GetSeconds(hptime) * 1000.0f) );
	}
	else
	{
		nGuaranteeFPSTime += nGuaranteeFPS;
		GetSingleton<IGameTimer>()->Update( nGuaranteeFPSTime );
	}
	// do all game segments
	GetSingleton<ITransceiver>()->DoSegments();

	// commands processing
	if ( !bDisableMessageProcessing ) 
	{
		// process text messages
		STextMessage textMessage;
		while ( pInput->GetTextMessage( &textMessage ) )
			interfaces.back()->ProcessTextMessage( textMessage );
		// process main messages
		SGameMessage msg;
		while ( pInput->GetMessage( &msg ) )
		{
			//ProcessTimeoutMsg( msg );
			interfaces.back()->ProcessUIMessage( msg );
			while ( interfaces.back()->GetMessage(&msg) )
				ProcessStandardMsgs( msg );
		}
		// make UI self-generated messages processing
		msg.nEventID = -1;
		if ( interfaces.back()->ProcessUIMessage(msg) )
		{
			while ( interfaces.back()->GetMessage(&msg) )
				ProcessStandardMsgs( msg );
		}
	}
	else
	{
		// clear text messages
		STextMessage textMessage;
		while ( pInput->GetTextMessage( &textMessage ) );
		// clear main messages
		SGameMessage msg;
//		while ( pInput->GetMessage( &msg ) )
//			ProcessTimeoutMsg( msg );
		while ( pInput->GetMessage( &msg ) );
	}
	/*
	// check for timeout update
	if ( timeout.IsActive() ) 
	{
		const NTimer::STime timeRest = timeout.Update();
		if ( !interfaces.empty() ) 
			interfaces.back()->ProcessUIMessage( SGameMessage(CMD_GAME_TIMEOUT_UPDATE, timeRest) );
		if ( timeRest == 0 )
			GetSingleton<ITransceiver>()->CommandTimeOut( false );
	}
	*/
	// do step for all interfaces
	for ( CInterfacesList::reverse_iterator it = interfaces.rbegin(); it != interfaces.rend(); ++it )
		(*it)->Step( bActive );
	// save movie frame (if it is)
	if ( bActive ) 
	{
		if ( const char *pszMovieDir = GetGlobalVar("MovieDir", (const char*)0) )
		{
			if ( pScreenShotImage == 0 ) 
			{
				CTRect<long> rcScreen = pGFX->GetScreenRect();
				pScreenShotImage = GetImageProcessor()->CreateImage( rcScreen.Width(), rcScreen.Height() );
			}
			static int nFrameCounter = 0;
			if ( pGFX->TakeScreenShot(pScreenShotImage) )
			{
				CPtr<IDataStream> pStream = CreateFileStream( NStr::Format("%sshot%.4d.tga", pszMovieDir, nFrameCounter), STREAM_ACCESS_WRITE );
				GetImageProcessor()->SaveImageAsTGA( pStream, pScreenShotImage );
				++nFrameCounter;
			}
		}
	}
//	GetSingleton<ITransceiver>()->SegmentFinished();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::EnableMessageProcessing( const bool bEnable ) 
{ 
	bDisableMessageProcessing = !bEnable; 
	GetSingleton<ICursor>()->LockPos( !bEnable );
	GetSingleton<ICursor>()->SetMode( bEnable ? USER_ACTION_UNKNOWN : USER_ACTION_HOURGLASS );
	GetSingleton<IInput>()->AddMessage( SGameMessage(TUTORIAL_TRY_SHOW_IF_NOT_SHOWN) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainLoop::StoreScenarioTracker()
{
	NI_ASSERT_T( pStoredScenarioTracker == 0, "Can't store new scenario tracker - restore previous one before" );
	if ( pStoredScenarioTracker == 0 ) 
		pStoredScenarioTracker = GetSingleton<IScenarioTracker>();
}
void CMainLoop::RestoreScenarioTracker()
{
	NI_ASSERT_T( pStoredScenarioTracker != 0, "Can't restore scenario tracker - store one before" );
	if ( pStoredScenarioTracker != 0 ) 
	{
		UnRegisterSingleton( IScenarioTracker::tidTypeID );
		RegisterSingleton( IScenarioTracker::tidTypeID, pStoredScenarioTracker );
		pStoredScenarioTracker = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** progress screen
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Init( EProgressType nType )
{
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( "movies\\progress\\progress.xml", STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, "Unable to open: movies\\progress\\progress.xml" );
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	std::vector< SProgressMovieInfo > vMovies;
	switch ( nType )
	{
		case PT_MAPGEN:
			tree.Add( "MapGen", &vMovies );
			break;
		case PT_NEWMISSION:
			tree.Add( "NewMission", &vMovies );
			break;
		case PT_MINIMAP:
			tree.Add( "Minimap", &vMovies );
			break;
		case PT_TOTAL_ENCYCLOPEDIA_LOAD:
			tree.Add( "EncyclopediaLoad", &vMovies );
			break;
		case PT_CONNECTING_TO_SERVER:
			tree.Add( "Connecting", &vMovies );
			break;
		case PT_LOAD:
		default:
			tree.Add( "Load", &vMovies );
			break;
	}
	NI_ASSERT_T( vMovies.size() > 0, "No movies defined!" );
	const int i = vMovies.size() == 1 ? 0 : NWin32Random::Random( vMovies.size() - 1 );
	Init( "movies\\progress\\" + vMovies[i].szMovieName );
	SetText( &(vMovies[i]) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Init( const std::string &szMovieName )
{
	pGFX = GetSingleton<IGFX>();
//	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER | GFXCLEAR_STENCIL, 0 );
	pVP = CreateObject<IVideoPlayer>( SCENE_VIDEO_PLAYER );
	pVP->Play( szMovieName.c_str(), IVideoPlayer::PLAY_FROM_MEMORY, pGFX, GetSingleton<ISFX>() );
	nNumFrames = pVP->GetNumFrames();
	CTRect<long> rcScreenRect = pGFX->GetScreenRect();
	pVP->SetDstRect( rcScreenRect, false );
	nCurrFrame = 0;
	GetSingleton<ICursor>()->Show( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Stop()
{
	pVP->Stop();
	GetSingleton<ICursor>()->Show( true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::SetNumSteps( const int nRange, const float fPercentage )
{
	if ( nNumFrames == 0 ) 
	{
		nNumSteps = nRange > 1 ? nRange : 1;
		nCurrentStep = 1;
		NI_ASSERT_SLOW_T( false, "Can't set num steps for progress screen - init it first!" );
	}
	else if ( nNumSteps == 0 ) 
	{
		nNumSteps = nRange > 1 ? nRange : 1;
		nCurrentStep = 1;
		nMaxFrame = nNumFrames * fPercentage;
	}
	else
	{
		nMaxFrame = nNumFrames * fPercentage;
		const float fCurrPercentage = float( nCurrFrame ) / float( nNumFrames );
		nNumSteps = nRange;
		nCurrentStep = fCurrPercentage * nRange;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Step()
{
	if ( nCurrentStep != nNumSteps )
		++nCurrentStep;
	Draw();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::SetCurrPos( const int nPos )
{
	nCurrentStep = Clamp( nPos, 1, nNumSteps );
	Draw();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CProgressScreen::GetCurrPos() const
{
	return nCurrentStep;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Draw()
{
	if ( pVP != 0 )
	{
		int nNextFrame = Clamp( int( ( nCurrentStep / float(nNumSteps) ) * nMaxFrame + 1 ), 1, nMaxFrame );
		if ( nNextFrame > nCurrFrame )
		{
			nCurrFrame = nNextFrame;
			pVP->SetCurrentFrame( nCurrFrame );
			pGFX->Clear( 0, 0, GFXCLEAR_ALL, 0 );
			pGFX->BeginScene();
			pGFX->SetupDirectTransform();
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			pVP->Draw( pGFX );
			if ( pGFXText )
			{
				CTRect<float> currRect = wndRect;
				CTRect<long> rcScreenRect = pGFX->GetScreenRect();
				currRect.x1 = currRect.x1 * ( rcScreenRect.x2 - rcScreenRect.x1 ) + rcScreenRect.x1;
				currRect.x2 = currRect.x2 * ( rcScreenRect.x2 - rcScreenRect.x1 ) + rcScreenRect.x1;
				currRect.y1 = currRect.y1 * ( rcScreenRect.y2 - rcScreenRect.y1 ) + rcScreenRect.y1;
				currRect.y2 = currRect.y2 * ( rcScreenRect.y2 - rcScreenRect.y1 ) + rcScreenRect.y1;
				pGFX->SetShadingEffect( 3 );
				pGFXText->SetColor( 0xff000000 );
				currRect.x1 += 2;
				currRect.x2 += 2;
				pGFX->DrawText( pGFXText, currRect, 2, nTextAlign );
				pGFXText->SetColor( dwTextColor );
				currRect.x1 -= 2;
				currRect.x2 -= 2;
				pGFX->DrawText( pGFXText, currRect, 0, nTextAlign );
			}
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );
			pGFX->RestoreTransform();
			pGFX->EndScene();
			pGFX->Flip();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::SetText( const SProgressMovieInfo *pInfo )
{
	if ( CPtr<IText> pText = GetSingleton<ITextManager>()->GetString(pInfo->szTextSource.c_str()) )
	{
		pGFXText = CreateObject<IGFXText>( GFX_TEXT );
		pGFXText->SetText( pText );
		pGFXText->EnableRedLine( false );
		switch ( pInfo->nFontSize )
		{
			case 0:
				pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\small" ) );
				break;
			case 1:
				pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
				break;
			case 2:
			default:
				pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\large" ) );
				break;
		}
		nFontSize = pInfo->nFontSize;
		dwTextColor = pInfo->dwTextColor;
		wndRect = CTRect<float>( pInfo->vTextTop.x, pInfo->vTextTop.y, pInfo->vTextBottom.x, pInfo->vTextBottom.y );
		switch ( pInfo->nTextAlign )
		{
			case 0:
				nTextAlign = FNT_FORMAT_LEFT;
				break;
			case 1:
				nTextAlign = FNT_FORMAT_CENTER;
				break;
			case 2:
			default:
				nTextAlign = FNT_FORMAT_RIGHT;
				break;
		}
	}
	else
		pGFXText = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressScreen::Recover()
{
	switch ( nFontSize )
	{
		case 0:
			pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\small" ) );
			break;
		case 1:
			pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
			break;
		case 2:
		default:
			pGFXText->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\large" ) );
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
