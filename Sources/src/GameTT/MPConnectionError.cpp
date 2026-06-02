#include "StdAfx.h"

#include "MPConnectionError.h"
#include "..\main\IMain.h"
#include "iMission.h"
#include "MuliplayerToUIConsts.h"
bool CMPConnectionError::DisplayError( const enum EMultiplayerToUICommands eErrorID )
{
	std::string szMessage;
	switch( eErrorID )
	{
	case EMTUC_WRONG_RESOURCES:
	 szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_wronresource";
	 break;

	case EMTUC_WRONG_MAP:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_wrongmap";
		break;
		
	case EMTUC_AIM_KICKED:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_im_kicked";
		break;
	
	case EMTUC_NO_MAP:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_nomap";
		break;

	case EMTUC_GAME_IS_ALREADY_STARTED:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_game_started"; 
		
		break;
	case EMTUC_CONNECTION_FAILED:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_cannotconnect";

		break;
	case EMTUC_WRONG_PASSWORD:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_wrong_password";

		break;
	case EMTUC_WRONG_GAMEEXE_VERSION:
		szMessage = "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\message_connection_error_wrong_gameexe";

		break;
	}
	if ( !szMessage.empty() )
	{

		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
									NStr::Format( "%s;%s;0;", "Textes\\UI\\Intermission\\Multiplayer\\GamesList\\caption_connection_error",
													 szMessage.c_str() ) );
	}

	return true;
}
