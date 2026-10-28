#include "StdAfx.h"

#include <cstdio>
#include <cstdlib>

#include "AILogicCommand.h"
#include "CommandsHistory.h"
#include "Transceiver.h"
#include "ScenarioTracker.h"
#include "RandomMapHelper.h"
#include "Remastered.h"

#include "../AILogic/AILogic.h"
#include "../Misc/FileUtils.h"
#include "../Formats/fmtMapScriptPath.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../RandomMapGen/Resource_Types.h"
#include "../GameTT/ReplayList.h"
#include "../StreamIO/StreamIOTypes.h"
// BK_REPLAY_TRACE=1 prints every recorded and played command, every state hash
// and the start state to stderr: replay-game-reads-it compares a recording
// with its playbacks through these lines.
static bool ReplayTrace()
{
	static const bool bTrace = getenv( "BK_REPLAY_TRACE" ) != 0;
	return bTrace;
}
// The random generator's whole state, through IRandomGen::Store and Restore
// (four bytes in the port's generator, StreamIOZig/legacy_bridge.cpp).
static unsigned int GetRandomState()
{
	CPtr<IDataStream> pStream = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	GetSingleton<IRandomGen>()->Store( pStream );
	pStream->Seek( 0, STREAM_SEEK_SET );
	unsigned int nState = 0;
	pStream->Read( &nState, sizeof( nState ) );
	return nState;
}
static void SetRandomState( const unsigned int nState )
{
	CPtr<IDataStream> pStream = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	pStream->Write( &nState, sizeof( nState ) );
	pStream->Seek( 0, STREAM_SEEK_SET );
	GetSingleton<IRandomGen>()->Restore( pStream );
}
void CCommandsHistory::PrepareToStartMission()
{
	if ( !bLoadedFromCommandLine )
	{
		savingHistory.clear();
		stateHashes.clear();

		if ( !bLoadedHistory )
		{
			startMapCheckSum = 0;
			loadedHistory.clear();
			loadedStateHashes.clear();
			bLoadedStartState = false;
			// A mission after a playback runs at the player's own difficulty again.
			if ( nViewerDifficulty >= 0 )
			{
				GetSingleton<IAILogic>()->SetDifficultyLevel( nViewerDifficulty );
				nViewerDifficulty = -1;
			}
		}
		stateHashCheck.SetRecorded( loadedStateHashes );

		if ( bLoadedHistory )
			ApplyLoadedStartState();
		TakeStartState();
	}

	bLoadedFromCommandLine = false;
	bLoadedHistory = false;
	pStartScenarioTracker = GetSingleton<IScenarioTracker>()->Duplicate();
}
// A single-player replay starts from the random state and difficulty it was
// recorded with. A multiplayer one, and one written before the port stored
// them, keep the zero seed Load set and the viewer's difficulty.
void CCommandsHistory::ApplyLoadedStartState()
{
	if ( !bLoadedStartState )
		return;
	SetRandomState( nLoadedStartRandomState );
	if ( nLoadedDifficulty >= 0 )
	{
		if ( nViewerDifficulty < 0 )
			nViewerDifficulty = GetSingleton<IAILogic>()->GetChosenDifficultyLevel();
		GetSingleton<IAILogic>()->SetDifficultyLevel( nLoadedDifficulty );
	}
}
// The start of the recording: the random state the AI starts from and the
// difficulty it plays at, taken where the mission starts.
void CCommandsHistory::TakeStartState()
{
	pStartRandomSeed = GetSingleton<IRandomGen>()->GetSeed();
	nStartRandomState = GetRandomState();
	nStartDifficulty = GetSingleton<IAILogic>()->GetChosenDifficultyLevel();
	bStartStateKnown = true;
	// From the start state on, every draw a replay cannot repeat is one too many.
	GetSingleton<IRandomGen>()->ArmAudit( true );
	if ( ReplayTrace() )
		fprintf( stderr, "BK_REPLAY_TRACE: start random state %08x difficulty %d\n", nStartRandomState, nStartDifficulty );
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
			// The start PrepareToStartMission just took is this process's own;
			// the replay's replaces it at the same point of the mission start.
			ApplyLoadedStartState();
			TakeStartState();

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
	if ( nViewerDifficulty >= 0 )
	{
		GetSingleton<IAILogic>()->SetDifficultyLevel( nViewerDifficulty );
		nViewerDifficulty = -1;
	}
}
bool CCommandsHistory::Load( const char *pszFileName )
{
	CPtr<IDataStream> pStream = CreateFileStream( pszFileName, STREAM_ACCESS_READ );
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );

	saver.Add( "History", &loadedHistory );
	saver.Add( "StartMapCheckSum", &startMapCheckSum );

	// A replay written before the state hashes has none and checks nothing.
	std::string szStateHashes;
	saver.Add( "StateHashes", &szStateHashes );
	if ( !NReplay::DecodeStateHashes( szStateHashes, &loadedStateHashes ) )
		InvalidHistory( "Invalid history: its state hashes cannot be read" );
	stateHashCheck.SetRecorded( loadedStateHashes );

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
	// The zero seed stays for multiplayer replays and for those written before
	// StartRandomState; a single-player one sets its own at the mission start
	// (ApplyLoadedStartState), not here, so nothing that runs between the
	// replay list and the mission start can shift it.
	CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
	pSeed->InitByZeroSeed();
	GetSingleton<IRandomGen>()->SetSeed( pSeed );
	std::string szStartRandomState;
	saver.Add( "StartRandomState", &szStartRandomState );
	nLoadedDifficulty = -1;
	saver.Add( "Difficulty", &nLoadedDifficulty );

	saver.Add( "Players", &players );

	unsigned long checkSumMap = GetGlobalVar( "Multiplayer.CheckSumMap", 0 );
	unsigned long checkSumRes = GetGlobalVar( "Multiplayer.CheckSumRes", 0 );

	int nMultiplayerGame = GetGlobalVar( "MultiplayerGame", 0 ) != 0;
	saver.Add( "Multiplayer", &nMultiplayerGame );
	if ( nMultiplayerGame == 1 )
	{
		// A replay from before the host's Remastered option travelled has none:
		// not the last game's.
		RemoveGlobalVar( NRemastered::pszMultiplayerVar );
		GetSingleton<IGlobalVars>()->SerializeVarsByMatch( saver, "Multiplayer." );
		SetGlobalVar( "MultiplayerGame", 1 );
	}
	bLoadedStartState = nMultiplayerGame != 1 && NReplay::ParseRandomState( szStartRandomState, &nLoadedStartRandomState );
	if ( nLoadedDifficulty < 0 || nLoadedDifficulty > 2 )
		nLoadedDifficulty = -1;

	unsigned long savedCheckSumMap = GetGlobalVar( "Multiplayer.CheckSumMap", 0 );
	unsigned long savedCheckSumRes = GetGlobalVar( "Multiplayer.CheckSumRes", 0 );

	{
		std::string szMapName = GetGlobalVar( "Multiplayer.MapName", "" );
		CMapInfo fullMapInfo;
		if ( LoadTypedSuperLatestDataResource( "maps\\" + szMapName, ".bzm", 1, fullMapInfo ) )
		{
			// The same expansion the game's own load makes (GameCreation.cpp).
			fullMapInfo.szScriptFile = NMapScriptPath::ExpandOnLoad( fullMapInfo.szScriptFile, "maps\\" + szMapName );
			fullMapInfo.GetCheckSums( &checkSumRes, &checkSumMap );

			SetGlobalVar( "Multiplayer.CheckSumMap", checkSumMap );
			SetGlobalVar( "Multiplayer.CheckSumRes", checkSumRes );
		}
	}

	saver.Add( "MODName", &szModName );
	saver.Add( "MODVersion", &szModVersion );

	// The Remastered mode the replay was recorded with; a history written
	// before the option has none and plays back as the original game.
	int nRemastered = 0;
	saver.Add( "Remastered", &nRemastered );
	SetGlobalVar( NRemastered::pszReplayVar, nRemastered );

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
		std::string szStateHashes = NReplay::EncodeStateHashes( stateHashes );
		saver.Add( "StateHashes", &szStateHashes );
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

		// The start a single-player playback sets again (multiplayer starts from
		// the zero seed and the normal difficulty in every case). Unknown for a
		// history that came out of a save written before the port stored it.
		if ( nMultiplayerGame == 0 && bStartStateKnown )
		{
			std::string szStartRandomState = NReplay::FormatRandomState( nStartRandomState );
			saver.Add( "StartRandomState", &szStartRandomState );
			int nDifficulty = nStartDifficulty;
			saver.Add( "Difficulty", &nDifficulty );
		}

		SetGlobalVar( "Multiplayer.GameSpeed", GetSingleton<IGameTimer>()->GetSpeed() );

		GetSingleton<IGlobalVars>()->SerializeVarsByMatch( saver, "Multiplayer." );
		
		std::string szCurModName = GetGlobalVar( "MOD.Name", "" );
		saver.Add( "MODName", &szCurModName );
		std::string szCurModVersion = GetGlobalVar( "MOD.Version", "" );
		saver.Add( "MODVersion", &szCurModVersion );

		int nRemastered = NRemastered::Enabled() ? 1 : 0;
		saver.Add( "Remastered", &nRemastered );
	}
}
void CCommandsHistory::AddCommand( const int nSegment, IAILogicCommand *pCmd )
{
	if ( pCmd->NeedToBeStored() )
	{
		savingHistory[nSegment].push_back( pCmd );
		if ( ReplayTrace() )
			fprintf( stderr, "BK_REPLAY_TRACE: recorded command at segment %d\n", nSegment );
	}
}
void CCommandsHistory::ExecuteSegmentCommands( const int nSegment, ITransceiver *pTranceiver )
{
	if ( GetGlobalVar("HistoryClient", 0 ) == 0 )
	{
		if ( loadedHistory.find( nSegment ) != loadedHistory.end() )
		{
			for ( std::list< CPtr<IAILogicCommand> >::iterator iter = loadedHistory[nSegment].begin(); iter != loadedHistory[nSegment].end(); ++iter )
			{
				if ( ReplayTrace() )
					fprintf( stderr, "BK_REPLAY_TRACE: played command at segment %d\n", nSegment );
				pTranceiver->AddCommandToSend( *iter );
			}
		}
	}
}
void CCommandsHistory::AddStateHash( const int nSegment, const unsigned int nHash )
{
	NReplay::RecordStateHash( &stateHashes, nSegment, nHash );
	if ( ReplayTrace() )
		fprintf( stderr, "BK_REPLAY_TRACE: state hash at segment %d %08x\n", nSegment, nHash );
	if ( stateHashCheck.Check( nSegment, nHash ) == NReplay::CStateHashCheck::FIRST_MISMATCH )
	{
		const std::string szMessage = "Replay out of sync at " + NReplay::FormatSegmentTime( nSegment );
		InvalidHistory( szMessage.c_str() );
		NStr::DebugTrace( "%s (segment %d)\n", szMessage.c_str(), nSegment );
		if ( ReplayTrace() )
			fprintf( stderr, "BK_REPLAY_TRACE: out of sync at segment %d\n", nSegment );
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
	saver.Add( 11, &stateHashes );
	saver.Add( 12, &loadedStateHashes );
	if ( saver.IsReading() )
		stateHashCheck.SetRecorded( loadedStateHashes );
	// A save written before these has no chunk 15 and reads false: its
	// recording's start is unknown, and a replay written later omits it.
	saver.Add( 13, &nStartRandomState );
	saver.Add( 14, &nStartDifficulty );
	saver.Add( 15, &bStartStateKnown );

	return 0;
}
