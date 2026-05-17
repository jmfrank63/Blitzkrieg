#ifndef __GAME_PLAYING_H__
#define __GAME_PLAYING_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GameCreationInterfaces.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface INetDriver;
class IMultiplayer::CCommand;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGamePlaying : public IGamePlaying
{
	OBJECT_COMPLETE_METHODS( CGamePlaying );

	CPtr<INetDriver> pInGameNetDriver;
	CPtr<INetDriver> pOutGameNetDriver;

	std::list< CPtr<IMultiplayer::CCommand> > commands;
	CPtr<IMultiplayer::CCommand> pTakenCommand;
	
	CPlayers players;
	int nOurID;
	std::vector<bool> lags;

	std::unordered_map<int, int> clientID2LogicID;

	std::vector<BYTE> diplomacies;
	bool bStartGameReceived;

	//
	void RemoveClient( const int nClientID );
	void ProcessPacket( const int nClientID, IDataStream *pPkt );
	void UpdatePlayersInfo();
	void ProcessNewClient( const int nClientID );
public:
	CGamePlaying() { }
	virtual void STDCALL Init( INetDriver *pInGameNetDriver, INetDriver *pOutGameNetDriver, const CPlayers &players, bool bServer, const int nOurID, const std::vector<BYTE> &diplomacies );
	
	virtual IMultiplayer::CCommand* STDCALL GetCommand();
	virtual void STDCALL SendClientCommands( IDataStream *pPacket );

	virtual void STDCALL LeftGame();
	
	virtual void STDCALL Segment();

	virtual const bool STDCALL GetPlayerInfo( const WORD *pszPlayerName, SPlayerInfo *pInfo ) const;
	virtual const bool STDCALL GetOurPlayerInfo( SPlayerInfo *pInfo ) const;
	
	virtual const int STDCALL GetNAllies() const;
	virtual const SPlayerInfo& STDCALL GetAlly( const int n ) const;
	
	virtual int STDCALL GetNumberOfPlayers() const;
	
	// client commands
	virtual void STDCALL TogglePause();
	virtual void STDCALL GameSpeed( const int nChange );
	virtual void STDCALL DropPlayer( const int nLogicID );
	
	virtual void STDCALL CommandTimeOut( const bool bSet );
	
	virtual void STDCALL SendAliveMessage();
	virtual void STDCALL FinishGame();

	virtual interface INetDriver* STDCALL GetInGameNetDriver() const { return pInGameNetDriver; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GAME_PLAYING_H__
