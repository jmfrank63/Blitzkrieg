#include "StdAfx.h"

#include "ServerInfo.h"

#include "../Net/NetDriver.h"
#include "../StreamIO/StreamIOHelper.h"
#include "../StreamIO/StreamIOTypes.h"
#include "MultiplayerConsts.h"
SServerInfo::SServerInfo( const SGameInfo &gameInfo )
{
	szGameName = gameInfo.szGameName;
	szMapName = gameInfo.szMapName;
	eState = gameInfo.eState;
	nMaxPlayers = gameInfo.nMaxPlayers;
	nCurPlayers = gameInfo.nCurPlayers;
	eGameType = gameInfo.eGameType;
	gameSettings = gameInfo.gameSettings;
	bPasswordRequired = gameInfo.bPasswordRequired;
	szModName = gameInfo.szModName;
	szModVersion = gameInfo.szModVersion;
}
bool SServerInfo::operator==( const SServerInfo &info ) const
{
	if ( pAddress == 0 && info.pAddress == 0 )
		return true;
	else
		return
			pAddress->IsSameIP( info.pAddress )	&&
			eState == info.eState && szGameName == info.szGameName && nMaxPlayers == info.nMaxPlayers &&
			nCurPlayers == info.nCurPlayers && fPing == info.fPing && bPasswordRequired == info.bPasswordRequired &&
			szModName == info.szModName && szModVersion == info.szModVersion && gameSettings == info.gameSettings;
}
void SServerInfo::Pack( INetDriver::SGameInfo *pGameInfo )
{
	std::string szGameType;
	switch ( eGameType )
	{
		case CMapInfo::TYPE_FLAGCONTROL: 
			szGameType = "Flag control";
			break;
		case CMapInfo::TYPE_SABOTAGE:
			szGameType = "Sabotage";
			break;
		default:
			NI_ASSERT_T( false, "Unknown game type" );
	}

	INetDriver::EServerGameMode eGameMode;
	switch ( eState )
	{
		case ESS_OPEN:
			eGameMode = INetDriver::ESGM_OPENPLAYING;
			break;
		case ESS_IN_GAME:
			eGameMode = INetDriver::ESGM_CLOSEDPLAYING;
			break;
		default: 
			NI_ASSERT_T( false, "Unknown game state" );
	}

	pGameInfo->eGameMode = eGameMode;
	pGameInfo->nCurPlayers = nCurPlayers;
	pGameInfo->nHostPort = nHostPort;
	pGameInfo->nMaxPlayers = nMaxPlayers;
	pGameInfo->szGameType = szGameType;
	pGameInfo->wszMapName = NStr::ToUnicode( szMapName );
	pGameInfo->wszServerName = szGameName;
	pGameInfo->bPasswordRequired = bPasswordRequired;
	pGameInfo->szModName = szModName;
	pGameInfo->szModVersion = szModVersion;
	
	pGameInfo->pGameSettings = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	gameSettings.Pack( pGameInfo->pGameSettings );
}
void SServerInfo::Unpack( const INetDriver::SGameInfo &gameInfo )
{
	switch ( gameInfo.eGameMode )
	{
		case INetDriver::ESGM_OPENPLAYING:
			eState = ESS_OPEN;
			break;
		case INetDriver::ESGM_CLOSEDPLAYING:
			eState = ESS_IN_GAME;
			break;
	}

	if ( gameInfo.szGameType == "Flag control" )
		eGameType = CMapInfo::TYPE_FLAGCONTROL;
	else if ( gameInfo.szGameType == "Sabotage" )
		eGameType = CMapInfo::TYPE_SABOTAGE;
	else
	{
		eGameType = CMapInfo::TYPE_SABOTAGE;
	}

	nCurPlayers = gameInfo.nCurPlayers;
	nMaxPlayers = gameInfo.nMaxPlayers;
	NStr::SetCodePage( GetACP() );
	szMapName = NStr::ToAscii( gameInfo.wszMapName );
	szGameName = gameInfo.wszServerName;
	nHostPort = gameInfo.nHostPort;
	bPasswordRequired = gameInfo.bPasswordRequired;
	szModName = gameInfo.szModName;
	szModVersion = gameInfo.szModVersion;

	gameInfo.pGameSettings->Seek( 0, STREAM_SEEK_SET );
	gameSettings.Unpack( gameInfo.pGameSettings );
}
void SPlayerInfo::Pack( IDataStream *pDataStream )
{
	CStreamAccessor stream = pDataStream;
	stream << nLogicID << nSide << bReady << fPing << szName << cMapLoadProgress;
}
void SPlayerInfo::Unpack( IDataStream *pDataStream )
{
	CStreamAccessor stream = pDataStream;
	stream >> nLogicID >> nSide >> bReady >> fPing >> szName >> cMapLoadProgress;
}
void SGameInfo::Pack( IDataStream *pDataStream )
{
	CStreamAccessor stream = pDataStream;
	stream << szGameName << szMapName << nMaxPlayers << eGameType;
	gameSettings.Pack( pDataStream );
	stream << checkSumMap << checkSumRes << bPasswordRequired << szPassword << szModName << szModVersion;
}
void SGameInfo::Unpack( IDataStream *pDataStream )
{
	CStreamAccessor stream = pDataStream;
	stream >> szGameName >> szMapName >> nMaxPlayers >> eGameType;
	gameSettings.Unpack( pDataStream );
	stream >> checkSumMap >> checkSumRes >> bPasswordRequired >> szPassword >> szModName >> szModVersion;
}
