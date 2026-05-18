#ifndef _MULTIPLAYER_COMMAND_MANAGER_
#define _MULTIPLAYER_COMMAND_MANAGER_
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMission.h"
#include "MuliplayerToUIConsts.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\StreamIO\StreamIOHelper.h"

inline std::wstring MakeWideStringFromWordString( const WORD *pszText )
{
	std::wstring szText;

	if ( pszText == 0 )
		return szText;

	while ( *pszText != 0 )
	{
		szText += static_cast<wchar_t>( *pszText );
		++pszText;
	}

	return szText;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//player state in chat
enum EPlayerChatState
{
	EPCS_IN_CHAT,
	EPCS_AWAY,
	EPCS_IN_SERVERSLIST,
	EPCS_IN_GAME,
	EPCS_IN_STAGINGROOM,
	EPCS_ISNT_CHANGED,
	EPCS_NONE,
};
// relation to player
enum EPlayerRelation
{
	EPR_NORMAL,
	EPR_FRIEND,
	EPR_IGNORED,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMultiplayerGameSettings
{
	//server settings
	//FC settings
	int nFlagScoreLimit;
	int nKillScoreLimit;
	//

	//sabotage settings
	int nTimeToCapture;										// in seconds

	// common settings
	int nTimeLimit;												// in seconds

	std::string szGameSpeed;							// speed is set by server.

	void Pack( IDataStream *pDataStream ) const
	{
		CStreamAccessor stream = pDataStream;
		stream << nFlagScoreLimit << nKillScoreLimit << nTimeToCapture << nTimeLimit << szGameSpeed;
	}

	void Unpack( IDataStream *pDataStream )
	{
		CStreamAccessor stream = pDataStream;
		stream >> nFlagScoreLimit >> nKillScoreLimit >> nTimeToCapture >> nTimeLimit >> szGameSpeed;
	}

	bool operator==( const SMultiplayerGameSettings &settings ) const
	{
		return nFlagScoreLimit == settings.nFlagScoreLimit && nKillScoreLimit == settings.nKillScoreLimit &&
					 nTimeToCapture == settings.nTimeToCapture && nTimeLimit == settings.nTimeLimit &&
					 szGameSpeed == settings.szGameSpeed;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 									notifications from UI												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFromUINotification
{
	EUIToMultiplayerNotifications eNotifyID;
	CPtr<IRefCount> pCommandParams;

	void Clear() { pCommandParams = 0; eNotifyID = EUTMN_UNITIALIZED; }

	SFromUINotification() :eNotifyID(EUTMN_UNITIALIZED) { }
	SFromUINotification( const EUIToMultiplayerNotifications _eNotifyID, IRefCount * _pCommandParams )
	: pCommandParams( _pCommandParams ), eNotifyID( _eNotifyID )
	{
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNotificationStringParam : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SNotificationStringParam );
public:
	std::string szParam;
	SNotificationStringParam() {  }
	SNotificationStringParam( const std::string &_szParam ) : szParam( _szParam )  {  }
	SNotificationStringParam( const char *pszParam ) : szParam( pszParam == 0 ? "" : pszParam )  {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNotificationSimpleParam : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SNotificationSimpleParam );
public:
	int nParam;

	SNotificationSimpleParam() : nParam( -1 ) { }
	SNotificationSimpleParam( const int _nParam ) : nParam( _nParam ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPassword : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SPassword );
public:
	std::string szPassword;
	bool bCancelFlag;											// this is not a password, but cancel of connection

	SPassword() : szPassword( "" ), bCancelFlag( true ) { }
	SPassword( const std::string &_szPassword, const bool _bCancelFlag = false ) : szPassword( _szPassword ), bCancelFlag( _bCancelFlag ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SServerNewSettings : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SServerNewSettings );
public:
	SMultiplayerGameSettings settings;

	SServerNewSettings() {  }
	SServerNewSettings( const SMultiplayerGameSettings &_settings )
		:settings ( _settings )
	{
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNewMapInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SNewMapInfo );
public:
	SQuickLoadMapInfo mapInfo;
	std::string szMapName;

	bool bPasswordRequired;
	std::string szPassword;

	SMultiplayerGameSettings settings;

	SNewMapInfo() { }
	SNewMapInfo( const char *pszMapName, const SQuickLoadMapInfo &_mapInfo, const SMultiplayerGameSettings & _settings, const std::string _szPassword )
		: szMapName( pszMapName ), mapInfo( _mapInfo ), settings( _settings ), bPasswordRequired( !_szPassword.empty() ), szPassword( _szPassword ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 										commands to UI														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SToUICommand
{
	EMultiplayerToUICommands eCommandID;
	CPtr<IRefCount> pCommandParams;

	SToUICommand() { }
	SToUICommand( const EMultiplayerToUICommands _eCommandID, IRefCount *_pCommandParams ) : eCommandID( _eCommandID ), pCommandParams( _pCommandParams ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// info to confugure staging room
struct SUIStagingRoomConfigure : public IRefCount
{
	OBJECT_COMPLETE_METHODS(SUIStagingRoomConfigure);
public:
	bool bServer;													//are 
	std::string szMapLocation;						// full path to the map
	std::wstring szGameName;							// name of the game
	int nPlayersMax;											// max numbe of players in the game
	int nLocalPlayerID;										// id of local player
	SMultiplayerGameSettings serverSettings;		// settings of server

	virtual int STDCALL operator&( interface IStructureSaver &ss ) 
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &bServer );
		saver.Add( 2, &szMapLocation );
		saver.Add( 3, &szGameName );
		saver.Add( 4, &nPlayersMax );
		saver.Add( 5, &nLocalPlayerID );
		saver.Add( 6, & serverSettings );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SChatMessage : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SChatMessage );
public:
	bool bWhisper;												// if true then message only to specific user
	std::wstring szPlayerName;						// when whisper
	std::wstring szMessageText;

	SChatMessage() : szPlayerName( L"" ) { }
	SChatMessage( const	WORD *pszMessageText, bool _bWhisper ) : szMessageText( MakeWideStringFromWordString( pszMessageText ) ), bWhisper( _bWhisper ) { }
	SChatMessage( const	WORD *pszMessageText, const WORD *pszPlayerName, bool _bWhisper ) : szMessageText( MakeWideStringFromWordString( pszMessageText ) ), szPlayerName( MakeWideStringFromWordString( pszPlayerName ) ), bWhisper( _bWhisper ) { }

	virtual int STDCALL operator&( interface IStructureSaver &ss ) 
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &bWhisper );
		saver.Add( 4, &szMessageText );
		saver.Add( 5, &szPlayerName );

		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// info about server in multiplayer interface screen
struct SUIServerInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS(SUIServerInfo);
public:
	WORD wServerID;												//
	std::wstring szName;									// game name
	std::string szMapName;								// map name
	int nPlayers;													// current number of players
	int nPlayersMax;											// maximum players for this game
	bool bPassword;												//
	bool bCanJoin;												// true if we can connect to this server
	float fPing;													// in seconds.
	bool bSamePatch;											// the same version of Game.exe on  server
	
	// for MOD support
	std::string szModName;								// eg "MyMod"
	std::string szModVersion;							// eg "v. 1.1"

	//server settings
	CMapInfo::GAME_TYPE eGameType;						// map type of this server.
	SMultiplayerGameSettings settings;
		
	const int GetID() const { return wServerID; }
	SUIServerInfo() { }

	virtual int STDCALL operator&( interface IStructureSaver &ss ) 
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &wServerID );
		saver.Add( 2, &szName );
		saver.Add( 3, &szMapName );
		saver.Add( 4, &nPlayers );
		saver.Add( 5, &nPlayersMax );
		saver.Add( 6, &bPassword );
		saver.Add( 7, &bCanJoin );												
		saver.Add( 8, &fPing );
		saver.Add( 9, &eGameType );

		saver.Add( 14, &settings );
		saver.Add( 16, &szModName );
		saver.Add( 17, &szModVersion );
		saver.Add( 18, &bSamePatch );

		return 0;
	}

	SUIServerInfo( const WORD _wServerID, const WORD *pszName, const char *pszMapName,
		const int _nPlayers, const int _nPlayersMax,
		const bool _bPassword, const bool _bCanJoin, const float _fPing,
		const char *_pszModName, const char *_pszModVersion, const bool _bSamePatch, 
		const CMapInfo::GAME_TYPE _eGameType, const SMultiplayerGameSettings &_gameSettings )
		: wServerID( _wServerID ), szName( MakeWideStringFromWordString( pszName ) ), szMapName( pszMapName ), 
			nPlayers( _nPlayers ), nPlayersMax( _nPlayersMax ), 
			bPassword( _bPassword ), bCanJoin( _bCanJoin ), fPing( _fPing ), 
			szModName( _pszModName ), szModVersion( _pszModVersion ), bSamePatch( _bSamePatch ),
			eGameType( _eGameType ), settings( _gameSettings ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUIRelationNotify : public IRefCount
{
	OBJECT_COMPLETE_METHODS(SUIRelationNotify);
public:
	EPlayerRelation eRelation;
	std::wstring szName;									// this player

	virtual int STDCALL operator&( interface IStructureSaver &ss ) 
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &eRelation );
		saver.Add( 2, &szName );
		return 0;
	}
	SUIRelationNotify( ) {  }

	SUIRelationNotify( const WORD * pszName, const EPlayerRelation _eRelation )
		: szName( MakeWideStringFromWordString( pszName ) ), eRelation( _eRelation)
	{  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//player's info fot chat
struct SUIChatPlayerInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SUIChatPlayerInfo );
public:
	EPlayerRelation eRelation;
	EPlayerChatState eState;

	std::wstring szName;
	std::wstring szAwayReason;

	const std::wstring GetID() const { return szName; }

	SUIChatPlayerInfo() {  }
	SUIChatPlayerInfo( const WORD * pszName ) 
		: eRelation( EPR_NORMAL ), eState( EPCS_IN_CHAT ), szName( MakeWideStringFromWordString( pszName ) ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUIChatPlayerChangedNick : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SUIChatPlayerChangedNick );

	std::wstring wszOldNick;
	std::wstring wszNewNick;
public:
	SUIChatPlayerChangedNick() : wszOldNick( L"" ), wszNewNick( L"" ) { }
	SUIChatPlayerChangedNick( const WORD *pwszOldNick, const WORD *pwszNewNick )
		: wszOldNick( MakeWideStringFromWordString( pwszOldNick ) ), wszNewNick( MakeWideStringFromWordString( pwszNewNick ) ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// to notify UI that local player changes side
struct SUISideInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SUISideInfo );
public:
	std::string szPartyName;							// local player is of this side

	SUISideInfo() {  }
	SUISideInfo( const char *pszParty ) : szPartyName( pszParty ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUIPlayerInfo : public IRefCount
{
	OBJECT_COMPLETE_METHODS( SUIPlayerInfo );
public:
	int nID;
	std::string szSide;
	bool bReady;
	float fPing;
	int nDownloadCount;											// download progress (in percent)

	std::wstring szName;

	const int GetID() const { return nID; }
	
	SUIPlayerInfo() { }
	SUIPlayerInfo( const int _nID, const char *pszSide, const bool _bReady, const float _fPing, const WORD *pszName, const int _nDownloadCount )
		: nID( _nID ), szSide( pszSide ), bReady( _bReady ), fPing( _fPing ), szName( MakeWideStringFromWordString( pszName ) ), nDownloadCount( _nDownloadCount ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMultiplayerConnectionType
{
	EMCT_LAN,
	EMCT_INTERNET,
	EMCT_GAMESPY,
	EMCT_NONE,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IMPToUICommandManager : public IRefCount
{
	enum { tidTypeID = GAMETT_MULTIPLAYER_TO_UI_COMMANDS };

	//adding
	virtual void STDCALL AddCommandToUI( SToUICommand &cmd ) = 0;
	virtual void STDCALL AddNotificationFromUI( SFromUINotification &notify ) = 0;

	//recieveing. return true if put command by ptr. if ptr == 0 or no more commands
	// returns false;
	virtual bool STDCALL GetCommandToUI( SToUICommand *pCmd ) = 0;
	virtual bool STDCALL GetNotificationFromUI( SFromUINotification *pNotify ) = 0;
	
	virtual bool STDCALL PeekCommandToUI( SToUICommand *pCmd ) = 0;
	virtual bool STDCALL PeekNotificationFromUI( SFromUINotification *pNotify ) = 0;

	// chat
	virtual SChatMessage* STDCALL GetChatMessageFromUI() = 0;
	virtual SChatMessage* STDCALL GetChatMessageToUI() = 0;
	virtual SChatMessage* STDCALL PeekChatMessageToUI() = 0;

	virtual void STDCALL AddChatMessageToUI( SChatMessage *pMessage ) = 0;
	virtual void STDCALL AddChatMessageFromUI( SChatMessage *pMessage ) = 0;

	//to init from UI side.
	virtual void STDCALL InitUISide() = 0;
	
	// manipulation with connection type
	virtual void STDCALL SetConnectionType( const enum EMultiplayerConnectionType ) = 0;
	virtual enum EMultiplayerConnectionType STDCALL GetConnectionType() const = 0;
	
	// delayed notifications to MP
	virtual void STDCALL DelayedNotification( SFromUINotification &notify ) = 0;
	virtual void STDCALL SendDelayedNotification() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //_MULTIPLAYER_COMMAND_MANAGER_
