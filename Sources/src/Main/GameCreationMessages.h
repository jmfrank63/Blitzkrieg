#ifndef __GAME_CREATION_MESSAGES_H__
#define __GAME_CREATION_MESSAGES_H__
#pragma ONCE
#include "Messages.h"
#include "ServerInfo.h"

#include "../Net/NetDriver.h"
class CConnectionFailed : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CConnectionFailed );

	INetDriver::EReject eReason;
public:
	CConnectionFailed() { }
	CConnectionFailed( const INetDriver::EReject &_eReason ) : eReason( _eReason ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return CONNECTION_FAILED; }
	virtual void SendToUI();
};
class CPlayerInfoRefreshed : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CPlayerInfoRefreshed );

	SPlayerInfo info;
	std::string szSide;
	int nLoadMapProgress;
public:
	CPlayerInfoRefreshed() { }
	CPlayerInfoRefreshed( const SPlayerInfo &_info, const char *pszSide )
		: info( _info ), szSide( pszSide ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return PLAYER_INFO_REFRESHED; }
	virtual void SendToUI();
};
class CPlayerDeleted : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CPlayerDeleted );
public:
	enum EReasons { ER_LEFT, ER_KICKED };
private:
	enum EReasons eReason;
	int nLogicID;
	std::wstring wszPlayerName;
public:
	CPlayerDeleted() { }
	CPlayerDeleted( const int _nLogicID, const std::wstring _wszPlayerName, const EReasons _eReason ) 
		: eReason( _eReason ), nLogicID( _nLogicID ), wszPlayerName( _wszPlayerName ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return PLAYER_DELETED; }
	virtual void SendToUI();
};
class CGameInfoReceived : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CGameInfoReceived );

	SGameInfo gameInfo;
	bool bServer;
	int nOurID;
public:
	CGameInfoReceived() { }
	CGameInfoReceived( const SGameInfo &_gameInfo, bool _bServer, const int _nOurID ) 
		: gameInfo( _gameInfo ), bServer( _bServer ), nOurID( _nOurID ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return GAME_INFO_RECEIVED; }
	virtual void SendToUI();
};
class CGameStarted : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CGameStarted );
public:
	CGameStarted() { }

	virtual const EMultiplayerMessages GetMessageID() const { return GAME_STARTED; }
	virtual void SendToUI();
};
class CWrongResources : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CWrongResources );
public:
	CWrongResources() { }

	virtual const EMultiplayerMessages GetMessageID() const { return WRONG_RESOURCES; }
	virtual void SendToUI();
};
class CWrongMap : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CWrongMap );
public:
	CWrongMap() { }

	virtual const EMultiplayerMessages GetMessageID() const { return WRONG_MAP; }
	virtual void SendToUI();
};
class CNoMap : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CNoMap );
public:
	CNoMap() { }

	virtual const EMultiplayerMessages GetMessageID() const { return NO_MAP; }
	virtual void SendToUI();
};
class CWrongPassword : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CWrongPassword );
public:
	CWrongPassword() { }

	virtual const EMultiplayerMessages GetMessageID() const { return WRONG_PASSWORD; }
	virtual void SendToUI();
};
class CGameIsAlreadyStarted : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CGameIsAlreadyStarted );
public:
	CGameIsAlreadyStarted() { }

	virtual const EMultiplayerMessages GetMessageID() const { return GAME_IS_ALREADY_STARTED; }
	virtual void SendToUI();
};
class CCanStartGameState : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CCanStartGameState );

	bool bCanStartGame;

	CCanStartGameState() { }
public:
	explicit CCanStartGameState( const bool _bCanStartGame ) : bCanStartGame( _bCanStartGame ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return CAN_START_GAME; }
	virtual void SendToUI();
};
class CGameSettingsChanged : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CGameSettingsChanged );

	SMultiplayerGameSettings settings;
public:
	CGameSettingsChanged() { }
	explicit CGameSettingsChanged( const SMultiplayerGameSettings &_settings ) : settings( _settings ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return GAME_INFO_CHANGED; }
	virtual void SendToUI();
};
class CCreateStagingRoom : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CCreateStagingRoom );
public:
	CCreateStagingRoom() { }

	virtual const EMultiplayerMessages GetMessageID() const { return CREATE_STAGING_ROOM; }
	virtual void SendToUI();
};
class CAIMKicked : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CAIMKicked );
public:
	CAIMKicked() { }

	virtual const EMultiplayerMessages GetMessageID() const { return AIM_KICKED; }
	virtual void SendToUI();
};
#endif // __GAME_CREATION_MESSAGES_H__
