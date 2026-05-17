#ifndef __COMMANDS_HISTORY_H__
#define __COMMANDS_HISTORY_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "CommandsHistoryInterface.h"

#include "..\zlib\zlib.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IAILogicCommand;
interface IRandomGenSeed;
interface IScenarioTracker;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

	std::string szModName;
	std::string szModVersion;
	//
	void InvalidHistory( const char *pMessage );
public:
	CCommandsHistory() : startMapCheckSum( 0 ), bLoadedFromCommandLine( false ), bLoadedHistory( false ), bStored( false ) { }

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
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif __COMMANDS_HISTORY_H__
