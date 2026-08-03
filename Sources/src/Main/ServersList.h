#ifndef __SERVERS_LIST_H__
#define __SERVERS_LIST_H__
#pragma ONCE
#include "GameCreationInterfaces.h"
#include "ServerInfo.h"
#include "MessagesStore.h"

#include "..\Net\NetDriver.h"
interface INetNodeAddress;
interface IMultiplayerMessage;
class CServersList : public IServersList
{
	CPtr<INetDriver> pNetDriver;

	typedef std::list<SServerInfo> CServers;
	CServers servers;

	WORD wCurUniqueId;

	CMessagesStore messages;
	NTimer::STime lastServersCheck;

	void AddServer( INetNodeAddress *pAddress, const float fPing, const struct INetDriver::SGameInfo &gameInfo, const bool bSameVersion );
	void RefreshServerInfo( const SServerInfo &info, const bool bSameVersion );
	void RemoveServer( const SServerInfo &info );

	void RefreshServersList();

	const SServerInfo* FindServerByID( const WORD wServerID ) const;
protected:
	void Init( INetDriver *pNetDriver );
	void DestroyNetDriver();

	INetDriver* GetNetDriver() const { return pNetDriver; }

	virtual interface INetDriver* CreateInGameNetDriver( const int nPort ) = 0;
	virtual void CreateInGameChat( CPtr<IChat> *pChat, interface INetDriver *pNetDriver ) = 0;
public:
	CServersList() { }

	virtual IMultiplayerMessage* STDCALL GetMessage();
	virtual void STDCALL Segment();

	virtual bool STDCALL CanJoinToServerByID( const WORD wServerID );
	virtual bool STDCALL IsNeedPassword( const WORD wServerID ) const;
	virtual interface IGameCreation* STDCALL JoinToServerByID( const WORD wServerID, CPtr<IChat> *pChat, bool bPasswordRequired, const std::string &szPassword );

	virtual interface IGameCreation* STDCALL JoinToServerByAddress( INetNodeAddress *pAddress, CPtr<IChat> *pChat, const int nPort, bool bPasswordRequired, const std::string &szPassword );
	
	virtual void STDCALL Refresh();
	
	virtual interface INetDriver* STDCALL GetInGameNetDriver() const { return 0; }
};
class CLanServersList : public CServersList
{
	OBJECT_COMPLETE_METHODS( CLanServersList );
protected:
	virtual void CreateInGameChat( CPtr<IChat> *pChat, interface INetDriver *pNetDriver );
	virtual interface INetDriver* CreateInGameNetDriver( const int nPort );
public:
	CLanServersList() { }
	void Init();

	virtual interface IGameCreation* STDCALL CreateServer( const SGameInfo &gameInfo, const SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat );
};
class CGameSpyServersList : public CServersList
{
	OBJECT_COMPLETE_METHODS( CGameSpyServersList );
protected:
	virtual void CreateInGameChat( CPtr<IChat> *pChat, interface INetDriver *pNetDriver );
	virtual interface INetDriver* CreateInGameNetDriver( const int nPort );
public:
	CGameSpyServersList() { }
	void Init();

	virtual interface IGameCreation* STDCALL CreateServer( const struct SGameInfo &gameInfo, const struct SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat );
};
class CInternetServersList : public CServersList
{
	OBJECT_COMPLETE_METHODS( CInternetServersList );
protected:
	virtual void CreateInGameChat( CPtr<IChat> *pChat, interface INetDriver *pNetDriver );
	virtual interface INetDriver* CreateInGameNetDriver( const int nPort );
public:
	CInternetServersList() { }
	void Init();

	virtual interface IGameCreation* STDCALL CreateServer( const struct SGameInfo &gameInfo, const struct SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat );
};
#endif // __SERVERS_LIST_H__
