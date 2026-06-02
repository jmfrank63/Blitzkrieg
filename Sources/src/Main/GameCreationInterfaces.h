#ifndef __GAME_CREATION_INTERFACES_H__
#define __GAME_CREATION_INTERFACES_H__
#pragma ONCE
#include "ServerInfo.h"
#include "Multiplayer.h"
interface IChat;
interface IServersList : public IRefCount
{
	virtual interface IGameCreation* STDCALL CreateServer( const struct SGameInfo &gameInfo, const struct SQuickLoadMapInfo &mapInfo, CPtr<IChat> *pChat ) = 0;
	virtual bool STDCALL CanJoinToServerByID( const WORD wServerID ) = 0;
	virtual bool STDCALL IsNeedPassword( const WORD wServerID ) const = 0;
	virtual interface IGameCreation* STDCALL JoinToServerByID( const WORD wServerID, CPtr<IChat> *pChat, bool bPasswordRequired, const std::string &szPassword ) = 0;
	virtual interface IGameCreation* STDCALL JoinToServerByAddress( interface INetNodeAddress *pAddress, CPtr<IChat> *pChat, const int nPort, bool bPasswordRequired, const std::string &szPassword ) = 0;

	virtual interface IMultiplayerMessage* STDCALL GetMessage() = 0;
	virtual void STDCALL Segment() = 0;
	virtual void STDCALL Refresh() = 0;
	
	virtual interface INetDriver* STDCALL GetInGameNetDriver() const = 0;
};
interface IGameCreation : public IRefCount
{
	enum EPlayerSettings { EPS_READY, EPS_SIDE, EPS_NAME, EPS_MAP_LOAD_PROGRESS };
	
	virtual void STDCALL LeftGame() = 0;
	virtual void STDCALL KickPlayer( const int nLogicID ) = 0;
	virtual void STDCALL ChangeGameSettings() = 0;
	virtual void STDCALL ChangePlayerSettings( const struct SPlayerInfo &info, const EPlayerSettings &eSettingsType ) = 0;

	virtual void STDCALL Launch() = 0;
	
	virtual interface IMultiplayerMessage* STDCALL GetMessage() = 0;
	virtual void STDCALL Segment() = 0;

	virtual bool STDCALL CanStartGame() const = 0;
	virtual bool STDCALL IsAllPlayersInOneParty() const = 0;
	virtual interface IGamePlaying* STDCALL CreateGamePlaying() = 0;

	virtual const bool STDCALL GetPlayerInfo( const WORD *pszPlayerName, SPlayerInfo *pInfo ) const = 0;
	virtual const bool STDCALL GetOurPlayerInfo( SPlayerInfo *pInfo ) const = 0;
	virtual void STDCALL SetNewGameSettings( const SMultiplayerGameSettings &settings ) = 0;
	virtual void STDCALL ModChanged() { }

	virtual interface INetDriver* STDCALL GetInGameNetDriver() const = 0;
};
interface IAILogicCommand;
interface IGamePlaying : public IRefCount
{
	virtual void STDCALL Init( interface INetDriver *pInGameNetDriver, interface INetDriver *pOutGameNetDriver, 
														 const CPlayers &players, bool bServer, const int nOurID,
														 const std::vector<BYTE> &diplomacies ) = 0;
	
	virtual IMultiplayer::CCommand* STDCALL GetCommand() = 0;
	virtual void STDCALL SendClientCommands( IDataStream *pPacket ) = 0;
	virtual void STDCALL LeftGame() = 0;
	
	virtual void STDCALL Segment() = 0;

	virtual const bool STDCALL GetPlayerInfo( const WORD *pszPlayerName, SPlayerInfo *pInfo ) const = 0;
	virtual const bool STDCALL GetOurPlayerInfo( SPlayerInfo *pInfo ) const = 0;

	virtual const int STDCALL GetNAllies() const = 0;
	virtual const SPlayerInfo& STDCALL GetAlly( const int n ) const = 0;
	
	virtual int STDCALL GetNumberOfPlayers() const = 0;
	
	virtual void STDCALL TogglePause() = 0;
	virtual void STDCALL GameSpeed( const int nChange ) = 0;
	virtual void STDCALL DropPlayer( const int nLogicID ) = 0;
	
	virtual void STDCALL CommandTimeOut( const bool bSet ) = 0;
	
	virtual void STDCALL SendAliveMessage() = 0;
	virtual void STDCALL FinishGame() = 0;

	virtual interface INetDriver* STDCALL GetInGameNetDriver() const = 0;
};
interface IChat : public IRefCount
{
	enum EUserMode
	{ 
		EUM_NONE, 
		EUM_AWAY, 
		EUM_NOT_AWAY, 
		EUM_IN_GS_CHAT, 
		EUM_IN_SERVERS_LIST, 
		EUM_IN_STAGING_ROOM,
		EUM_IN_GAME_PLAYING,
	};
	
	virtual void STDCALL InitGSChat( const WORD *pszUserName ) = 0;
	virtual void STDCALL InitInGameChat( INetDriver *pNetDriver ) = 0;
	virtual void STDCALL DestroyInGameChat() = 0;
	
	virtual void STDCALL SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer ) = 0;
	virtual void STDCALL SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer ) = 0;
	virtual void STDCALL SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper ) = 0;

	virtual void STDCALL Segment() = 0;

	virtual interface IMultiplayerMessage* STDCALL GetMessage() = 0;

	virtual void STDCALL UserModeChanged( const EUserMode eMode ) = 0;
};
#endif // __GAME_CREATION_INTERFACES_H__
