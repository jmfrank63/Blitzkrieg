#ifndef __SERVER_INFO_H__
#define __SERVER_INFO_H__
#pragma ONCE
#include "../RandomMapGen/MapInfo_Types.h"
#include "../GameTT/MultiplayerCommandManager.h"
#include "../Net/NetDriver.h"
#include "../zlib/zlib.h"
interface INetNodeAddress;
struct SServerInfo
{
	enum EServerState { ESS_OPEN, ESS_IN_GAME };
	EServerState eState;

	std::wstring szGameName;
	std::string szMapName;
	int nMaxPlayers;
	int nCurPlayers;
	float fPing;
	CPtr<INetNodeAddress> pAddress;
	int nHostPort;
	bool bPasswordRequired;

	WORD wUniqueServerId;
	bool bUpdated;

	CMapInfo::GAME_TYPE eGameType;
	SMultiplayerGameSettings gameSettings;

	std::string szModName;
	std::string szModVersion;
	
	SServerInfo() : wUniqueServerId( 0 ), bPasswordRequired( false ) { }
	SServerInfo( const struct SGameInfo &gameInfo );

	void Pack( struct INetDriver::SGameInfo *pGameInfo );
	void Unpack( const struct INetDriver::SGameInfo &gameInfo );

	bool operator==( const SServerInfo &info ) const;
	bool operator!=( const SServerInfo &info ) const { return !(*this == info ); }
};
struct SPlayerInfo
{
	int nClientID;
	int nLogicID;
	int nSide;
	bool bReady;
	float fPing;
	NTimer::STime lastTimeInfoAsked;
	BYTE cMapLoadProgress;

	std::wstring szName;

	enum EPlayerStates { EPS_INVALID, EPS_CONNECTED, EPS_VALID };
	EPlayerStates eState;

	SPlayerInfo() : 
		bReady( false ), fPing( -1.0f ), nSide( -1 ), nClientID( -1 ), nLogicID( -1 ), 
			szName( L"" ), lastTimeInfoAsked( 0 ), eState( EPS_INVALID ), cMapLoadProgress( 100 ) { }
	SPlayerInfo( const int _nCliendID, const int _nLogicID, const int _nSide, const bool _bReady, const	WORD *pszName, const EPlayerStates _eState, const BYTE _cMapLoadProgress )
		: nClientID( _nCliendID ), nLogicID( _nLogicID ), nSide( _nSide ), bReady( _bReady ), 
			szName( MakeWideStringFromWordString( pszName ) ), lastTimeInfoAsked( 0 ), eState( _eState ), cMapLoadProgress( _cMapLoadProgress ) { }
	
	void Pack( IDataStream *pDataSteam );
	void Unpack( IDataStream *pDataStream );
};
typedef std::vector<SPlayerInfo> CPlayers;
struct SGameInfo
{
	std::wstring szGameName;
	std::string szMapName;
	int nMaxPlayers;
	int nCurPlayers;
	SServerInfo::EServerState eState;

	bool bPasswordRequired;
	std::string szPassword;

	CMapInfo::GAME_TYPE eGameType;
	SMultiplayerGameSettings gameSettings;

	uLong checkSumMap;
	uLong checkSumRes;

	std::string szModName;
	std::string szModVersion;
	
	bool bMapLoaded;

	SGameInfo() : checkSumMap( 0L ), checkSumRes( 0L ), bPasswordRequired( false ), szPassword( "" ), bMapLoaded( true ) { }

	void Init( const WORD *pszGameName, const char *pszMapName, const int _nMaxPlayers, const int _nCurPlayers,
						 const SMultiplayerGameSettings &_gameSettings,
						 const CMapInfo::GAME_TYPE _eGameType, bool _bPasswordRequired, const std::string &_szPassword,
						 const std::string &_szModName, const std::string &_szModVersion, const bool _bMapLoaded )
	{
		szGameName = MakeWideStringFromWordString( pszGameName );
		szMapName = pszMapName;
		nMaxPlayers = _nMaxPlayers;
		nCurPlayers = _nCurPlayers;
		eState = SServerInfo::ESS_OPEN;

		gameSettings = _gameSettings;
		eGameType = _eGameType;

		bPasswordRequired = _bPasswordRequired;
		szPassword = _szPassword;

		szModName = _szModName;
		szModVersion = _szModVersion;

		bMapLoaded = _bMapLoaded;
	}

	void Pack( IDataStream *pDataStream );
	void Unpack( IDataStream *pDataStream );
};
#endif // __SERVER_INFO_H__
