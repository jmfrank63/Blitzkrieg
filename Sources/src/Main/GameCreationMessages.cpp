#include "StdAfx.h"

#include "GameCreationMessages.h"
#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\GameTT\MuliplayerToUIConsts.h"
void CConnectionFailed::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddCommandToUI( 
		SToUICommand( EMTUC_CONNECTION_FAILED, new SNotificationSimpleParam( eReason ) )
	);
/*
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = NStr::Format( "Connection failed by reason %d", eReason );
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
};
void CPlayerInfoRefreshed::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();

	pCommandManager->AddCommandToUI( 
		SToUICommand( EMTUC_UPDATE_PLAYER_INFO, 
			new SUIPlayerInfo( info.nLogicID, szSide.c_str(), info.bReady, info.fPing, reinterpret_cast<const WORD*>( info.szName.c_str() ), info.cMapLoadProgress )
		)
	);

/*
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = NStr::Format( "Player %d info refreshed", info.nLogicID );
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
}
void CPlayerDeleted::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	CPtr<SUIPlayerInfo> pInfo = new SUIPlayerInfo( nLogicID, "", false, -1, reinterpret_cast<const WORD*>( wszPlayerName.c_str() ), 100 );

	if ( eReason == ER_LEFT )
		pCommandManager->AddCommandToUI( SToUICommand( EMTUC_PLAYER_LEFT, pInfo ) );
	else
		pCommandManager->AddCommandToUI( SToUICommand( EMTUC_PLAYER_KICKED, pInfo ) );

/*
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = NStr::Format( "Player %d deleted by reason %d", nLogicID, eReason );
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
}
void CGameInfoReceived::SendToUI()
{
	CPtr<SUIStagingRoomConfigure> pInfo = new SUIStagingRoomConfigure();
	pInfo->szGameName = gameInfo.szGameName;
	if ( gameInfo.bMapLoaded )
		pInfo->szMapLocation = gameInfo.szMapName;
	else
		pInfo->szMapLocation = "";

	pInfo->nPlayersMax = gameInfo.nMaxPlayers;
	pInfo->bServer = bServer;
	pInfo->nLocalPlayerID = nOurID;
	pInfo->serverSettings = gameInfo.gameSettings;

	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddCommandToUI( SToUICommand( EMTUC_CONFIGURE_STAGING_ROOM, pInfo ) );

/*
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = "Game info received";
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
}
void CGameStarted::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_START_GAME, 0 ) );
}
void CWrongResources::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_WRONG_RESOURCES, 0 ) );
}
void CWrongMap::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_WRONG_MAP, 0 ) );
}
void CNoMap::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_NO_MAP, 0 ) );
}
void CWrongPassword::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_WRONG_PASSWORD, 0 ) );
}
void CGameIsAlreadyStarted::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_GAME_IS_ALREADY_STARTED, 0 ) );
}
void CCanStartGameState::SendToUI()
{
	CPtr<SNotificationSimpleParam> pInfo = new SNotificationSimpleParam( bCanStartGame );
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_ALLOW_START_GAME, pInfo ) );
}
void CGameSettingsChanged::SendToUI()
{
	CPtr<SServerNewSettings> pNewSettings = new SServerNewSettings( settings );
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_SERVER_SETTINGS_CHANGED, pNewSettings ) );
}
void CCreateStagingRoom::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_CREATE_STAGING_ROOM, 0 ) );
}
void CAIMKicked::SendToUI()
{
	GetSingleton<IMPToUICommandManager>()->AddCommandToUI( SToUICommand( EMTUC_AIM_KICKED, 0 ) );
}
