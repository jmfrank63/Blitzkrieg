#ifndef __COMMANDS_HISTORY_H__
#define __COMMANDS_HISTORY_H__
#pragma ONCE
#include "CommandsHistoryInterface.h"
#include "ReplayCore.h"

#include "../zlib/zlib.h"
interface IAILogicCommand;
interface IRandomGenSeed;
interface IScenarioTracker;
class CCommandsHistory : public ICommandsHistory
{
	OBJECT_COMPLETE_METHODS( CCommandsHistory );
	DECLARE_SERIALIZE;

	typedef std::unordered_map<int, std::list< CPtr<IAILogicCommand> > > CHistory;
	CHistory savingHistory;
	CHistory loadedHistory;

	uLong startMapCheckSum;
	
	CPtr<IRandomGenSeed> pStartRandomSeed;
	CPtr<IScenarioTracker> pStartScenarioTracker;

	bool bLoadedFromCommandLine;
	bool bLoadedHistory;
	bool bStored;													// scenario tracker stored
	
	uLong checkSumMap;
	uLong checkSumRes;

	struct SMPPlayerInfo
	{
		int nLogicID; 
		int nSide; 

		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "LogicID", &nLogicID );
			saver.Add( "Side", &nSide );
			return 0;
		}
	};
	std::vector<SMPPlayerInfo> players;

	// The state hashes this run took, and those of the replay it plays back.
	std::vector<NReplay::SStateHash> stateHashes;
	std::vector<NReplay::SStateHash> loadedStateHashes;
	NReplay::CStateHashCheck stateHashCheck;

	// The recording's start: the random generator's state and the difficulty
	// (0..2) when the mission started, written as StartRandomState and
	// Difficulty; unknown for a history read from a save older than them.
	unsigned int nStartRandomState;
	int nStartDifficulty;
	bool bStartStateKnown;
	// The start of the replay being played, if it is a single-player one that
	// has it, and the viewer's own difficulty to go back to afterwards (-1: none).
	bool bLoadedStartState;
	unsigned int nLoadedStartRandomState;
	int nLoadedDifficulty;
	int nViewerDifficulty;

	std::string szModName;
	std::string szModVersion;
	void InvalidHistory( const char *pMessage );
	void ApplyLoadedStartState();
	void TakeStartState();
public:
	CCommandsHistory() : startMapCheckSum( 0 ), bLoadedFromCommandLine( false ), bLoadedHistory( false ), bStored( false ),
		nStartRandomState( 0 ), nStartDifficulty( -1 ), bStartStateKnown( false ),
		bLoadedStartState( false ), nLoadedStartRandomState( 0 ), nLoadedDifficulty( -1 ), nViewerDifficulty( -1 ) { }

	virtual void STDCALL PrepareToStartMission();
	virtual bool STDCALL LoadCommandLineHistory();
	virtual bool STDCALL Load( const char *pszFileName );
	virtual void STDCALL Save( const char *pszFileName );
	virtual void STDCALL Clear();

	virtual void STDCALL AddCommand( const int nSegment, interface IAILogicCommand *pCmd );
	virtual void STDCALL ExecuteSegmentCommands( const int nSegment, interface ITransceiver *pTranceiver );
	virtual void STDCALL CheckStartMapCheckSum( const int nCheckSum );

	virtual const int GetNumPlayersInMPGame() const { return players.size(); }
	virtual const int GetMPPlayerLogicID( const int nPlayer ) const;
	virtual const int GetMPPlayerSide( const int nPlayer ) const;

	virtual const char* STDCALL GetModName() const { return szModName.c_str(); }
	virtual const char* STDCALL GetModVersion() const { return szModVersion.c_str(); }

	virtual void STDCALL AddStateHash( const int nSegment, const unsigned int nHash );
};
#endif // __COMMANDS_HISTORY_H__
