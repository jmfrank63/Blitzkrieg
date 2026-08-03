#ifndef __MULTIPLAYERTRANSCEIVER_H__
#define __MULTIPLAYERTRANSCEIVER_H__
#pragma ONCE
#include "Transceiver.h"
#include "iMain.h"
#include "..\StreamIO\StreamIOHelper.h"
interface IMultiplayer;
interface IAILogic;
interface IAILogicCommand;
interface ICommandsHistory;
class CTimeOut
{
	CPtr<IMultiplayer> pMultiplayer;

	int nMyNumber;
	int nTimeOutPlayer;

	NTimer::STime timeOutTime;
	NTimer::STime timeBWTimeOuts;
	NTimer::STime lastTimeOutTime;

public:
	CTimeOut() : nMyNumber( -1 ), nTimeOutPlayer( -1 ) { }

	void Init( IMultiplayer *pMultiplayer );
	void InitGameStart( const int n );

	bool IsActive() const { return nTimeOutPlayer != -1; }
	const int GetTimeOutPlayer() const { return nTimeOutPlayer; }

	void SetTimeOut( const int nPlayer );
	void UnsetTimeOut( const bool bByClientCommand );
	void Segment();
};
class CMultiPlayerTransceiver : public CTRefCount<ITransceiver>
{
	OBJECT_SERVICE_METHODS( CMultiPlayerTransceiver );
	DECLARE_SERIALIZE;
	struct SPlayerInfo
	{
		std::wstring szName;								// player's name
		int nSide;													// player side
		int nLogicID;												// logical ID (for right sequencing)

		WORD totalLagTime;					// in seconds
		NTimer::STime lastLagUpdateTime;
	};
	static const int MAX_LATENCY;
	static const NTimer::STime TIME_TO_START_LAG_BY_NO_SEGMENT_DATA;
	CPtr<IAILogic> pAILogic;							// shortcut to AI logic
	CPtr<IMultiplayer> pMultiplayer;
	CStreamAccessor pktOutgoing;

	int nLatency;
	int nSegmentsPackSize;

	int nNumPlayers;											// количество игроков
	int nNumPlayersInMap;									// максимальнок количество игроков на этой карте
	int nSegment;													// номер текущего сегмента
	int nMyNumber;												// номер "нашего" игрока
	
	std::vector<WORD> segmFinished;
	std::vector<int> lastFinishedSegment;
	std::vector<int> timeOfLastSegmFinished;
	NTimer::STime lastTimeToCheckNoSegmDataLag;
	std::vector<int> noSegmDataLags;
	std::vector<int> loadingPlayers;
	int nLoadingPlayers;

	WORD wMask;

	typedef std::list< CPtr<IAILogicCommand> > CAILogicCommandsList;
	CArray2D<CAILogicCommandsList> cmds;
	CAILogicCommandsList receivedCmds;
	typedef std::list<SPlayerInfo> CPlayersList;
	CPlayersList players;

	CPtr<ICommandsHistory> pCmdsHistory;
	long nCommonSegment;

	bool bGameStarted;
	bool bHistoryPlaying;
	
	int nTimeToAllowDropByLag; // in sec

	bool bTotalOutOfSync;
	float fLastSentMultiplayerTime;
	
	bool bSpeedSet;

	CTimeOut timeOut;

	void SegmentFinished();
	const int GetPastSegmentNum( const int nLatency ) const;
	const int GetCommonPastSegmentNum() const;
	void ProcessMultiplayerCommands();
	void LoadGameSettings();
	void InitVariables();
	void SendChatMessages();
	void SetPlayerLag( const int nPlayer, const bool bSet );
	void SetPlayerLoading( const int nPlayer, const bool bSet );
	void SetNoSegmentDataLags();
	void RemovePlayer( const int nLogicID );

	void UpdateLags();
	void SetPlayerAlive( const int nPlayer );
	void SendIAmAlive();
public:
	virtual void STDCALL Init( ISingleton *pSingleton, const int nMultiplayerType );
	virtual void STDCALL PreMissionInit() { }
	virtual void STDCALL Done();
	virtual void SetLatency( int nSegments );
	virtual void STDCALL DoSegments();
	virtual int STDCALL CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen );
	virtual void STDCALL CommandUnregisterGroup( const WORD wGroup );
	virtual void STDCALL CommandGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue );
	virtual int STDCALL CommandUnitCommand( const struct SAIUnitCmd *pCommand );
	virtual void STDCALL CommandShowAreas( int nGroupID, int nAreaType, bool bShow );
	virtual void STDCALL AddCommandToSend( IAILogicCommand *pCommand );

	virtual int STDCALL GetNumberOfPlayers() const;
	
	virtual bool STDCALL JoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword );
	virtual void STDCALL CreateServer();
	virtual void STDCALL InitByCreateServersList();
	
	virtual void STDCALL CommandClientTogglePause();
	virtual void STDCALL CommandClientSpeed( const int nChange );
	virtual void STDCALL CommandClientDropPlayer( const WORD *pszPlayerNick );
	
	virtual void STDCALL CommandTimeOut( const bool bSet );
	
	virtual NTimer::STime STDCALL GetMultiplayerTime();

	virtual void STDCALL LoadAllGameParameters();
	
	virtual void STDCALL SetTotalOutOfSync();
	
	virtual void STDCALL GameFinished();

	virtual interface INetDriver* STDCALL GetInGameNetDriver() const;
};
class CChangeTransceiverCommand : public CTRefCount<IInterfaceCommand>
{
	OBJECT_SERVICE_METHODS( CChangeTransceiverCommand );
	int nTransceiverTypeID;								// type id of the transceiver to create
	int nMultiplayerType;									// LAN/Address Book/GameSpy
	bool bInitByServersList;
	NTimer::STime timeDelayed;
public:
	CChangeTransceiverCommand() : nTransceiverTypeID( -1 ), timeDelayed( 0 ) {  }
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( !pszConfig ) return;
		std::vector<std::string> szParams;
		NStr::SplitStringWithMultipleBrackets( pszConfig, szParams, ' ' );
		
		NI_ASSERT_TF( NStr::IsDecNumber(szParams[0]), NStr::Format("Can't create transceiver %s", pszConfig), return );
		nTransceiverTypeID = NStr::ToInt( szParams[0] );

		bInitByServersList = false;
		if ( szParams.size() >= 2 )
		{
			NI_ASSERT_TF( NStr::IsDecNumber(szParams[1]), NStr::Format("Can't create transceiver %s", pszConfig), return );
			nMultiplayerType = NStr::ToInt( szParams[1] );
			if ( szParams.size() == 3 )
				bInitByServersList = true;
		}
		else
			nMultiplayerType = -1;
	}
	virtual void STDCALL Exec( IMainLoop *pML )
	{
		NI_ASSERT_TF( nTransceiverTypeID != -1, "Can't create unknown transceiver", return );

		ITransceiver *pCurTransceiver = GetSingleton<ITransceiver>();
		if ( CMultiPlayerTransceiver *pMultiTransceiver = dynamic_cast<CMultiPlayerTransceiver*>( pCurTransceiver ) )
			pMultiTransceiver->Done();

		UnRegisterSingleton( ITransceiver::tidTypeID );
		ITransceiver *pTrans = CreateObject<ITransceiver>( nTransceiverTypeID );
		pTrans->Init( GetSingletonGlobal(), nMultiplayerType );
		if ( bInitByServersList )
			pTrans->InitByCreateServersList();
		RegisterSingleton( ITransceiver::tidTypeID, pTrans );
	}
	virtual void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	virtual NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	virtual int STDCALL operator&( IStructureSaver &ss ) 
	{ 
		CSaverAccessor saver = &ss; 
		saver.Add( 1, &nTransceiverTypeID ); 
		saver.Add( 2, &timeDelayed );
		return 0; 
	}
};
#endif // __MULTIPLAYERTRANSCEIVER_H__
