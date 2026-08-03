#ifndef __MULTIPLAYER_INTERNAL_H__
#define __MULTIPLAYER_INTERNAL_H__
#pragma ONCE
#include "Multiplayer.h"

#include "../Net/NetDriver.h"
interface IServersList;
interface IGameCreation;
interface IChat;
interface IGamePlaying;
class CMultiplayer : public IMultiplayer
{
private:
	EMultiplayerStates eState;
	int nParam;
	
	CPtr<IServersList> pServersList;
	CPtr<IGameCreation> pGameCreation;
	CPtr<IChat> pChat;
	CPtr<IGamePlaying> pGamePlaying;
	bool bInGSChat;
	
	NTimer::STime finishGameTime;

	void ServersListSegment();
	void GameCreationSegment();
	void PlayingSegment();
	void CreateServer( const struct SGameInfo &gameInfo, const struct SQuickLoadMapInfo &mapInfo );
	void JoinToServer( const WORD wServerID, bool bPasswordRequired, const std::string &szPassword );
	void StartGame();
	void ProcessChat();
	void SendChatMessageToConsole( interface IMultiplayerMessage *pMessage );
protected:
	virtual IServersList* CreateServersList() = 0;
	void SetGameCreation( IGameCreation* pGameCreation );
	void SetServersList( IServersList *pServersList );
	void SetState( const EMultiplayerStates eState );
	void SetChat( IChat *pChat );
	IChat* GetChat();
public:
	CMultiplayer() : eState( EMS_NONE ), bInGSChat( false ) { }

	virtual void STDCALL InitServersList();
	virtual void STDCALL Segment();

	const EMultiplayerStates GetState() { return eState; }
	
	virtual CCommand* STDCALL GetCommand();
	virtual void STDCALL SendClientCommands( IDataStream *pPacket );
	virtual void STDCALL SendInGameChatMessage( const WORD *pszType, const WORD *pszMessage );
	
	virtual int STDCALL GetNumberOfPlayers() const;
	
	virtual void STDCALL TogglePause();
	virtual void STDCALL GameSpeed( const int nChange );
	virtual void STDCALL DropPlayer( const int nLogicID );
	
	virtual void STDCALL CommandTimeOut( const bool bSet );
	
	virtual void STDCALL SendAliveMessage();
	virtual void STDCALL FinishGame();
	
	virtual interface INetDriver* STDCALL GetInGameNetDriver() const;
};
class CLanMultiplayer : public CMultiplayer
{
	OBJECT_COMPLETE_METHODS( CLanMultiplayer );
protected:
	virtual IServersList* CreateServersList();
public:
	virtual void STDCALL Init() { }
	virtual bool STDCALL InitJoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword ) { return true; }
};
class CGameSpyMultiplayer : public CMultiplayer
{
	OBJECT_COMPLETE_METHODS( CGameSpyMultiplayer );
protected:
	virtual IServersList* CreateServersList();
public:
	CGameSpyMultiplayer();
	virtual void STDCALL Init();

	virtual void STDCALL InitServersList();
	virtual bool STDCALL InitJoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword );
};
class CInternetMultiplayer : public CMultiplayer
{
	OBJECT_COMPLETE_METHODS( CInternetMultiplayer );
protected:
	virtual IServersList* CreateServersList();
public:
	CInternetMultiplayer() { }
	virtual void STDCALL Init() { }
	virtual bool STDCALL InitJoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword ) { return true; }
};
#endif //__MULTIPLAYER_INTERNAL_H__
