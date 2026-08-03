#include "StdAfx.h"

#include "AILogicCommand.h"
#include "CommandsHistory.h"
#include "Transceiver.h"
#include "ScenarioTracker.h"
#include "RandomMapHelper.h"

#include "..\AILogic\AILogic.h"
#include "..\Misc\FileUtils.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\GameTT\ReplayList.h"
#include "..\StreamIO\StreamIOTypes.h"
void CCommandsHistory::PrepareToStartMission()
{
	if ( !bLoadedFromCommandLine )
	{
		savingHistory.clear();

		if ( !bLoadedHistory )
		{
			startMapCheckSum = 0;
			loadedHistory.clear();
		}

		pStartRandomSeed = GetSingleton<IRandomGen>()->GetSeed();
	}

	bLoadedFromCommandLine = false;
	bLoadedHistory = false;
	pStartScenarioTracker = GetSingleton<IScenarioTracker>()->Duplicate();
}
bool CCommandsHistory::LoadCommandLineHistory()
{
	if ( !bLoadedFromCommandLine )
	{
		PrepareToStartMission();
		bLoadedFromCommandLine = true;

		const std::string szLoadedHistoryFileName = GetGlobalVar( "LoadHistoryFileName", "" );
		if ( szLoadedHistoryFileName != "" )
		{
			Load( szLoadedHistoryFileName.c_str() );
			RemoveGlobalVar( "History.Playing" );

			return true;
		}
		else
			return false;
	}
	else
		return false;
}
void CCommandsHistory::Clear()
{
	if ( bStored )
		GetSingleton<IMainLoop>()->RestoreScenarioTracker();
	bStored = false;
	RemoveGlobalVar( "History.Playing" );
	RemoveGlobalVar( "MultiplayerGame" );
}
bool CCommandsHistory::Load( const char *pszFileName )
{
	CPtr<IDataStream> pStream = CreateFileStream( pszFileName, STREAM_ACCESS_READ );
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );

	saver.Add( "History", &loadedHistory );
	saver.Add( "StartMapCheckSum", &startMapCheckSum );

	std::string szMissionName( "" );
	saver.Add( "MissionName", &szMissionName );
	SetGlobalVar( "Mission.Current.Name", szMissionName.c_str() );

	std::string szMapName( "" );
	saver.Add( "MapName", &szMapName );
	SetGlobalVar( "Map.Current.Name", szMapName.c_str() );

	std::string szChapterName( "" );
	saver.Add( "ChapterName", &szChapterName );
	SetGlobalVar( "Chapter.Current.Name", szChapterName.c_str() );
	bStored = true;
	GetSingleton<IMainLoop>()->StoreScenarioTracker();
	saver.Add( "ScenarioTracker", GetSingleton<IScenarioTracker>() );
	/*
	bool bRandomMission = false;
	saver.Add( "IsRandomMission", &bRandomMission );
	if ( bRandomMission ) 
	{
		NSaveLoad::SRandomHeader rndhdr;
		CPtr<IRandomGenSeed> pSeed;
		saver.Add( "RandomMapHeader", &rndhdr );
		saver.Add( "RandomMapSeed", &pSeed );

		RestoreRandomMap( szMissionName, rndhdr, pSeed );
	}
	saver.Add( "RandomSeed01", &pStartRandomSeed );
	if ( pStartRandomSeed != 0 )
		GetSingleton<IRandomGen>()->SetSeed( pStartRandomSeed );
	*/
	CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
	pSeed->InitByZeroSeed();
	GetSingleton<IRandomGen>()->SetSeed( pSeed );

	saver.Add( "Players", &players );

	unsigned long checkSumMap = GetGlobalVar( "Multiplayer.CheckSumMap", 0 );
	unsigned long checkSumRes = GetGlobalVar( "Multiplayer.CheckSumRes", 0 );

	int nMultiplayerGame = GetGlobalVar( "MultiplayerGame", 0 ) != 0;
	saver.Add( "Multiplayer", &nMultiplayerGame );
	if ( nMultiplayerGame == 1 )
	{
		GetSingleton<IGlobalVars>()->SerializeVarsByMatch( saver, "Multiplayer." );
		SetGlobalVar( "MultiplayerGame", 1 );
	}

	unsigned long savedCheckSumMap = GetGlobalVar( "Multiplayer.CheckSumMap", 0 );
	unsigned long savedCheckSumRes = GetGlobalVar( "Multiplayer.CheckSumRes", 0 );

	{
		std::string szMapName = GetGlobalVar( "Multiplayer.MapName", "" );
		CMapInfo fullMapInfo;
		if ( LoadTypedSuperLatestDataResource( "maps\\" + szMapName, ".bzm", 1, fullMapInfo ) )
		{
			fullMapInfo.GetCheckSums( &checkSumRes, &checkSumMap );

			SetGlobalVar( "Multiplayer.CheckSumMap", checkSumMap );
			SetGlobalVar( "Multiplayer.CheckSumRes", checkSumRes );
		}
	}

	saver.Add( "MODName", &szModName );
	saver.Add( "MODVersion", &szModVersion );

#if defined(_FINALRELEASE) || defined(_BETARELEASE)
	if ( checkSumRes != savedCheckSumRes || checkSumMap != savedCheckSumMap )
	{
		EReplayError eError = checkSumRes != savedCheckSumRes ? ERR_BAD_RESOURCES : ERR_BAD_MAP;
		
		SetGlobalVar( "ReplayError", eError );
		return false;
	}
	else
#endif // #if defined(_FINALRELEASE) || defined(_BETARELEASE)
	{
		SetGlobalVar( "History.Playing", 1 );
		bLoadedHistory = true;
		return true;
	}
}
void CCommandsHistory::Save( const char *pszFileName )
{
	std::string szFileName( pszFileName );
	if ( (szFileName == "") || szFileName.empty() )
		szFileName = GetGlobalVar( "SaveHistoryFileName", "" );

	if ( (szFileName != "") && !szFileName.empty() )
	{
		CPtr<IDataStream> pStream = CreateFileStream( szFileName, STREAM_ACCESS_WRITE );
		if ( pStream == 0 )
			return;
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );

		saver.Add( "StartMapCheckSum", &startMapCheckSum );
		saver.Add( "History", &savingHistory );
		/*
		if ( pStartRandomSeed == 0 )
			pStartRandomSeed = GetSingleton<IRandomGen>()->GetSeed();
		saver.Add( "RandomSeed01", &pStartRandomSeed );
		*/
		
		std::string szMissionName = GetGlobalVar( "Mission.Current.Name", "" );
		saver.Add( "MissionName", &szMissionName );

		std::string szMapName = GetGlobalVar( "Map.Current.Name", "" );
		saver.Add( "MapName", &szMapName );
		
		std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
		saver.Add( "ChapterName", &szChapterName );

		if ( pStartScenarioTracker == 0 )
			pStartScenarioTracker = GetSingleton<IScenarioTracker>()->Duplicate();
		saver.Add( "ScenarioTracker", &pStartScenarioTracker );
		/*
		bool bRandomMission = GetGlobalVar( ("Mission." + szMissionName + ".Random").c_str(), 0 ) != 0;
		saver.Add( "IsRandomMission", &bRandomMission );
		if ( bRandomMission ) 
		{
			NSaveLoad::SRandomHeader rndhdr;
			CPtr<IRandomGenSeed> pSeed;
			StoreRandomMap( szMissionName, &rndhdr, &pSeed );

			saver.Add( "RandomMapHeader", &rndhdr );
			saver.Add( "RandomMapSeed", &pSeed );
		}
		*/

		std::vector<SMPPlayerInfo> savingPlayers;
		for ( CPtr<IPlayerScenarioInfoIterator> pIter = pStartScenarioTracker->CreatePlayerScenarioInfoIterator(); !pIter->IsEnd(); pIter->Next() )
		{
			if ( pIter->Get()->GetDiplomacySide() != 2 )
			{
				savingPlayers.push_back( SMPPlayerInfo() );
				savingPlayers.back().nLogicID = pIter->GetID();
				savingPlayers.back().nSide = pIter->Get()->GetDiplomacySide();
			}
		}
		saver.Add( "Players", &savingPlayers );

		int nMultiplayerGame = GetGlobalVar( "MultiplayerGame", 0 ) != 0;
		saver.Add( "Multiplayer", &nMultiplayerGame );

		SetGlobalVar( "Multiplayer.GameSpeed", GetSingleton<IGameTimer>()->GetSpeed() );

		GetSingleton<IGlobalVars>()->SerializeVarsByMatch( saver, "Multiplayer." );
		
		std::string szCurModName = GetGlobalVar( "MOD.Name", "" );
		saver.Add( "MODName", &szCurModName );
		std::string szCurModVersion = GetGlobalVar( "MOD.Version", "" );
		saver.Add( "MODVersion", &szCurModVersion );
	}
}
void CCommandsHistory::AddCommand( const int nSegment, IAILogicCommand *pCmd )
{
	if ( pCmd->NeedToBeStored() )
		savingHistory[nSegment].push_back( pCmd );
}
void CCommandsHistory::ExecuteSegmentCommands( const int nSegment, ITransceiver *pTranceiver )
{
	if ( GetGlobalVar("HistoryClient", 0 ) == 0 )
	{
		if ( loadedHistory.find( nSegment ) != loadedHistory.end() )
		{
			for ( std::list< CPtr<IAILogicCommand> >::iterator iter = loadedHistory[nSegment].begin(); iter != loadedHistory[nSegment].end(); ++iter )
				pTranceiver->AddCommandToSend( *iter );
		}
	}
}
void CCommandsHistory::InvalidHistory( const char *pMessage )
{
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, pMessage, 0xffff0000, false );
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, pMessage, 0xffff0000, true );
}
void CCommandsHistory::CheckStartMapCheckSum( const int checkSum )
{
	const std::string szLoadedHistoryFileName = GetGlobalVar( "LoadHistoryFileName" );
	if ( szLoadedHistoryFileName.size() != 0 )
	{
		if ( checkSum != startMapCheckSum )
			InvalidHistory( "Invalid history: maps are different" );
	}

	startMapCheckSum = checkSum;
}
const int CCommandsHistory::GetMPPlayerLogicID( const int nPlayer ) const
{
	NI_ASSERT_T( nPlayer < GetNumPlayersInMPGame(), NStr::Format( "Number of player (%d) is to big, total number of players is %d", nPlayer, GetNumPlayersInMPGame() ) );
	return players[nPlayer].nLogicID;
}
const int CCommandsHistory::GetMPPlayerSide( const int nPlayer ) const
{
	NI_ASSERT_T( nPlayer < GetNumPlayersInMPGame(), NStr::Format( "Number of player (%d) is to big, total number of players is %d", nPlayer, GetNumPlayersInMPGame() ) );
	return players[nPlayer].nSide;
}
int CCommandsHistory::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &savingHistory );
	saver.Add( 2, &loadedHistory );
	saver.Add( 3, &startMapCheckSum );
	saver.Add( 5, &pStartRandomSeed );
	saver.Add( 6, &bLoadedFromCommandLine );
	saver.Add( 7, &pStartScenarioTracker );
	saver.Add( 8, &players );
	saver.Add( 9, &checkSumMap );
	saver.Add( 10, &checkSumRes );

	return 0;
}
