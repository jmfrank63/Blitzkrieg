#include "stdafx.h"

#include "GSQueryReportingDriver.h"
#include "GSConsts.h"

#include "stdio.h"

#include "..\Main\MultiplayerConsts.h"
#include "..\GameTT\MultiplayerCommandManager.h"
using namespace NWin32Helper;
CGSQueryReportingDriver::CGSQueryReportingDriver()
: CThread( 50 ), bInitialized( false )
{
}
CGSQueryReportingDriver::~CGSQueryReportingDriver()
{
	if ( bInitialized )
	{
		gameInfo.eGameMode = INetDriver::ESGM_EXITING;
		qr_send_statechanged( gsHandler );
		bInitialized = false;
		qr_shutdown( gsHandler );
	}
	
	StopThread();
}
bool CGSQueryReportingDriver::Init( const APPLICATION_ID _nApplicationID, int _nGamePort, bool _bClientOnly )
{
	nGamePort = _nGamePort;

	std::string szSecretKey;
	szSecretKey.resize( 6 );

	szSecretKey[0] = 'f';
	szSecretKey[1] = 'Y';
	szSecretKey[2] = 'D';
	szSecretKey[3] = 'X';
	szSecretKey[4] = 'B';
	szSecretKey[5] = 'N';

	const int nResult = 
		qr_init(
			&gsHandler, 0, &nGamePort, GetGlobalVar("GameSpyGameName"), szSecretKey.c_str(),
			qr_basic_callback,
			qr_info_callback,
			qr_rules_callback,
			qr_players_callback,
			this
		);
	bInitialized = nResult == 0;

	qr_send_statechanged( gsHandler );
	RunThread();

	return bInitialized;
}
INetDriver::EState CGSQueryReportingDriver::GetState() const
{
	if ( bInitialized )
		return ACTIVE;
	else
		return INACTIVE;
}
INetDriver::EReject CGSQueryReportingDriver::GetRejectReason() const
{
	return NONE;
}
void CGSQueryReportingDriver::StartGame()
{
}
void CGSQueryReportingDriver::StartGameInfoSend( const SGameInfo &_gameInfo )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	if ( GetState() == ACTIVE )
	{
		gameInfo = _gameInfo;
		qr_send_statechanged( gsHandler );
	}
}
void CGSQueryReportingDriver::StopGameInfoSend()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
}
void CGSQueryReportingDriver::StartNewPlayerAccept()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	gameInfo.eGameMode = ESGM_OPENPLAYING;

	if ( GetState() == ACTIVE )
		qr_send_statechanged( gsHandler );
}
void CGSQueryReportingDriver::StopNewPlayerAccept()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	gameInfo.eGameMode = ESGM_CLOSEDPLAYING;

	if ( GetState() == ACTIVE )
		qr_send_statechanged( gsHandler );
}
void CGSQueryReportingDriver::Step()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	if ( GetState() == ACTIVE )
		qr_process_queries( gsHandler );
}
void CGSQueryReportingDriver::QRBasicCallBack( char *pszOutBuf, int nMaxLen )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	if ( GetSingleton<IGlobalVars>() )
		sprintf( pszOutBuf, "\\gamename\\%s\\gamever\\%d", GetGlobalVar("GameSpyGameName"), GetGlobalVar("NetGameVersion", 1) );
}
void CGSQueryReportingDriver::QRInfoCallBack( char *pszOutBuf, int nMaxLen )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	NStr::SetCodePage( GetACP() );
	std::string szServerName = NStr::ToAscii( gameInfo.wszServerName );

	std::string szGSMapName = NStr::ToAscii( gameInfo.wszMapName );
	for ( int i = 0; i < szGSMapName.size(); ++i )
	{
		if ( szGSMapName[i] == '\\' )
			szGSMapName[i] = '/';
	}

	std::string szGameMode = GetMode( gameInfo.eGameMode );

	std::string szGameType = gameInfo.szGameType;
	if ( gameInfo.szModName != "" || gameInfo.szModVersion != "" )
		szGameType = gameInfo.szModName + "/" + gameInfo.szModVersion + "/" + szGameType;

	std::string szFormatString =
		NStr::Format( "\\hostname\\%s\\hostport\\%d\\mapname\\%s\\gametype\\%s\\numplayers\\%d\\maxplayers\\%d\\gamemode\\%s",
		szServerName.c_str(), nGamePort, szGSMapName.c_str(), szGameType.c_str(),
		gameInfo.nCurPlayers, gameInfo.nMaxPlayers, szGameMode.c_str(),
		gameInfo.szModName.c_str(), gameInfo.szModVersion.c_str() );

	if ( gameInfo.bPasswordRequired )
		szFormatString += NStr::Format( "\\password\\1" );

	strcpy( pszOutBuf, szFormatString.c_str() );
}
void CGSQueryReportingDriver::QRRulesCallBack( char *pszOutBuf, int nMaxLen )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );

	if ( gameInfo.pGameSettings )
	{
		SMultiplayerGameSettings settings;
		gameInfo.pGameSettings->Seek( 0, STREAM_SEEK_SET );
		settings.Unpack( gameInfo.pGameSettings );

		const std::string szFormatString = 
			NStr::Format( "\\flagscorelimit\\%d\\killscorelimit\\%d\\timelimit\\%d\\timetocapture\\%d\\gamespeed\\%s",
										settings.nFlagScoreLimit, settings.nKillScoreLimit, settings.nTimeLimit, 
										settings.nTimeToCapture, settings.szGameSpeed );

		strcpy( pszOutBuf, szFormatString.c_str() );
	}
}
void CGSQueryReportingDriver::QRPlayersCallBack( char *pszOutBuf, int nMaxLen )
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
}
void CGSQueryReportingDriver::qr_basic_callback( char *pszOutBuf, int nMaxLen, void *pUserData )
{
	reinterpret_cast<CGSQueryReportingDriver*>(pUserData)->QRBasicCallBack( pszOutBuf, nMaxLen );
}
void CGSQueryReportingDriver::qr_info_callback( char *pszOutBuf, int nMaxLen, void *pUserData )
{
	reinterpret_cast<CGSQueryReportingDriver*>(pUserData)->QRInfoCallBack( pszOutBuf, nMaxLen );
}
void CGSQueryReportingDriver::qr_rules_callback( char *pszOutBuf, int nMaxLen, void *pUserData )
{
	reinterpret_cast<CGSQueryReportingDriver*>(pUserData)->QRRulesCallBack( pszOutBuf, nMaxLen );
}
void CGSQueryReportingDriver::qr_players_callback( char *pszOutBuf, int nMaxLen, void *pUserData )
{
	reinterpret_cast<CGSQueryReportingDriver*>(pUserData)->QRPlayersCallBack( pszOutBuf, nMaxLen );
}
