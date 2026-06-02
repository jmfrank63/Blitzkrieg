#ifndef __SERVERS_LIST_MESSAGES_H__
#define __SERVERS_LIST_MESSAGES_H__
#pragma ONCE
#include "Messages.h"
#include "ServerInfo.h"
class CServerInfoRefreshed : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CServerInfoRefreshed );

	WORD wUniqueServerID;
	std::wstring szGameName;
	std::string szMapName;
	float fPing;
	SServerInfo::EServerState eState;
	int nMaxPlayers;
	int nCurPlayers;
	
	bool bPasswordRequired;
	
	bool bSameVersion;
	
	std::string szModName;								// eg "MyMod"
	std::string szModVersion;							// eg "v. 1.1"

	CMapInfo::GAME_TYPE eGameType;
	SMultiplayerGameSettings gameSettings;
public:
	CServerInfoRefreshed() { }
	CServerInfoRefreshed( const int _wUniqueServerID, const std::wstring &_szGameName,
												const float _fPing, const SServerInfo::EServerState _eState,
												const int _nCurPlayers, const int _nMaxPlayers, const std::string &_szMapName,
												bool _bPasswordRequired,
												const std::string &_szModName, const std::string &_szModVersion, const bool _bSameVersion,
												const CMapInfo::GAME_TYPE _eGameType, const SMultiplayerGameSettings &_gameSettings
											)
		: wUniqueServerID( _wUniqueServerID ),
			fPing( _fPing ), eState( _eState ), nMaxPlayers( _nMaxPlayers ), nCurPlayers( _nCurPlayers ),
			szGameName( _szGameName ), szMapName( _szMapName ), bPasswordRequired( _bPasswordRequired ),
			szModName( _szModName ), szModVersion( _szModVersion ), bSameVersion( _bSameVersion ),
			eGameType( _eGameType ), gameSettings( _gameSettings ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return SERVER_INFO_REFRESHED; }
	virtual void SendToUI();
};
class CServerRemoved : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CServerRemoved );

	WORD wUniqueServerID;
public:
	CServerRemoved() { }
	CServerRemoved( const int _wUniqueServerID ) : wUniqueServerID( _wUniqueServerID ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return SERVER_REMOVED; }
	virtual void SendToUI();
};
#endif // __SERVERS_LIST_MESSAGES_H__
