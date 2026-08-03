#include "StdAfx.h"

#include "ServersListMessages.h"

#include "..\GameTT\MultiplayerCommandManager.h"
#include "..\GameTT\MuliplayerToUIConsts.h"
void CServerInfoRefreshed::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddCommandToUI
	(
		SToUICommand(
			EMTUC_UPDATE_SERVER_INFO,
			new SUIServerInfo(
					wUniqueServerID, ToWordString( szGameName ), szMapName.c_str(), nCurPlayers, nMaxPlayers, bPasswordRequired,
					eState == SServerInfo::ESS_OPEN, fPing,
					szModName.c_str(), szModVersion.c_str(), bSameVersion, eGameType, gameSettings
			)
		)
	);

/*	
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = NStr::Format( "info refreshed %s, %d", szGameName, wUniqueServerID );
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
}
void CServerRemoved::SendToUI()
{
	IMPToUICommandManager *pCommandManager = GetSingleton<IMPToUICommandManager>();
	pCommandManager->AddCommandToUI
	(
		SToUICommand(
			EMTUC_DELETE_SERVER,
			new SUIServerInfo( wUniqueServerID, ToWordString( std::wstring() ), "", -1, -1, false, false, -1, "", "", true, CMapInfo::TYPE_NONE, SMultiplayerGameSettings() )
		)
	);

/*
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	std::string szString = NStr::Format( "server removed %d", wUniqueServerID );
	pBuffer->WriteASCII( CONSOLE_STREAM_CHAT, szString.c_str(), 0xff00ff00 );
*/
}
