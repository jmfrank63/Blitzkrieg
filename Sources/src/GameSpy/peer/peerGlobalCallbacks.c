/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com
*/

/*************
** INCLUDES **
*************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "peer.h"
#include "peerMain.h"
#include "peerGlobalCallbacks.h"
#include "peerRooms.h"
#include "peerPlayers.h"
#include "peerCallbacks.h"
#include "peerOperations.h"
#include "peerPing.h"
#include "peerMangle.h"
#include "peerKeys.h"

/************
** DEFINES **
************/
#define PI_UTM_MATCH(utm)       (strncmp(piUTMCommand, utm, strlen(utm) - 1) == 0)

#define PI_UTM_COMMAND_LEN      8
#define PI_UTM_PARAMATERS_LEN   512

/************
** GLOBALS **
************/
static char piUTMCommand[PI_UTM_COMMAND_LEN];
static char piUTMParameters[PI_UTM_PARAMATERS_LEN];

/**************
** FUNCTIONS **
**************/

/* Chat.
*******/
static PEERBool piIsOldUTM
(
	const char * message
)
{
	// Check for no message.
	////////////////////////
	assert(message);
	if(!message)
		return PEERFalse;

	// Check for too short for prefix + 1 char.
	///////////////////////////////////////////
	if(strlen(message) < 4)
		return PEERFalse;

	// Check for no prefix.
	///////////////////////
	if((message[0] != '@') ||
		(message[1] != '@') ||
		(message[2] != '@') ||
		(message[3] == ' '))
	{
		return PEERFalse;
	}

	return PEERTrue;
}



static PEERBool piParseUTM
(
	const char * message
)
{
	int len;

	// Check for no message.
	////////////////////////
	assert(message);
	if(!message)
		return PEERFalse;

	// Find the end of the command.
	///////////////////////////////
	len = strcspn(message, "/ ");
	if(len >= PI_UTM_COMMAND_LEN)
		return PEERFalse;
	memcpy(piUTMCommand, message, len);
	piUTMCommand[len] = '\0';

	// Copy off the parameters.
	///////////////////////////
	message += len;
	if(message[0])
	{
		message++;
		if(strlen(message) >= PI_UTM_PARAMATERS_LEN)
			return PEERFalse;
		strcpy(piUTMParameters, message);
	}
	else
	{
		piUTMParameters[0] = '\0';
	}

	return PEERTrue;
}

static void piProcessUTM
(
	PEER peer,
	piPlayer * player,
	PEERBool inRoom,
	RoomType roomType
)
{
	char * params = piUTMParameters;

	PEER_CONNECTION;

	assert(piUTMCommand[0]);
	assert(player);

	if(PI_UTM_MATCH(PI_UTM_LAUNCH))
	{
#ifdef _DEBUG
		assert(connection->inRoom[StagingRoom]);
		if(inRoom)
			assert(roomType == StagingRoom);
		else
			assert(player->inRoom[StagingRoom]);
#endif
		if(!connection->inRoom[StagingRoom])
			return;
		if(inRoom && (roomType != StagingRoom))
			return;
		if(!inRoom && !player->inRoom[StagingRoom])
			return;

		// Ignore if we're hosting.
		///////////////////////////
		if(connection->hosting)
			return;

		// Only accept launch from ops.
		///////////////////////////////
		if(!(player->flags[roomType] & PEER_FLAG_OP))
			return;

		// We're playing.
		/////////////////
		connection->playing = PEERTrue;

		// Set the flags.
		/////////////////
		piSetLocalFlags(peer);

		// Add the callback.
		////////////////////
		piAddGameStartedCallback(peer, connection->serverIP, params);
	}
	else if(PI_UTM_MATCH(PI_UTM_XPING))
	{
		piPlayer * other;
		int ping;
		unsigned int IP;

#ifdef _DEBUG


#endif
		if(inRoom && !connection->xpingRoom[roomType])
			return;

		// Check for no params.
		///////////////////////
		if(!params[0])
			return;

		// Get the IP.
		//////////////
		IP = piDemangleIP(params);

		// Get the ping.
		////////////////
		params += 11;
		ping = atoi(params);

		// Figure out who this ping is to.
		//////////////////////////////////
		other = piFindPlayerByIP(peer, IP);
		if(!other)
			return;
		if(strcasecmp(player->nick, other->nick) == 0)
			return;
		if(inRoom && !player->inRoom[roomType])
			return;
		if(!inRoom)
		{
			int i;
			PEERBool success = PEERFalse;

			// Check that the three of us are in a room with xping enabled.
			///////////////////////////////////////////////////////////////
			for(i = 0 ; i < NumRooms ; i++)
			{
				if(connection->xpingRoom[i] && connection->inRoom[i] && player->inRoom[i] && other->inRoom[i])
					success = PEERTrue;
			}
			if(!success)
				return;
		}

		// Update.
		//////////
		piUpdateXping(peer, player->nick, other->nick, ping);

		// Add a xping callback.
		////////////////////////
		piAddCrossPingCallback(peer, player->nick, other->nick, ping);
	}
}

void piChatDisconnected
(
	CHAT chat,
	const char * reason,
	PEER peer
)
{
	PEER_CONNECTION;

	// We're disconnected.
	//////////////////////
	connection->disconnect = PEERTrue;

	// Add the disconnected callback.
	/////////////////////////////////
	piAddDisconnectedCallback(peer, reason);
}

static void piHandleOldNFO
(
	PEER peer,
	piPlayer * player,
	const char * message
)
{
	// Ignore old NFOs from new clients.
	////////////////////////////////////
	if(strncmp(message + strlen(message) - 2, "X\\", 2) != 0)
	{
		const char * str;

		if(!player->inRoom[StagingRoom])
			return;

		str = strstr(message, "\\$flags$\\");
		if(str)
		{
			PEERBool ready = PEERFalse;
			str += 9;
			while(*str && (*str != '\\'))
			{
				if(*str++ == 'r')
				{
					ready = PEERTrue;
					break;
				}
			}

			if(ready)
				player->flags[StagingRoom] |= PEER_FLAG_READY;
			else
				player->flags[StagingRoom] &= ~PEER_FLAG_READY;
		}
	}
}

void piChatPrivateMessage
(
	CHAT chat,
	const char * user,
	const char * message,
	int type,
	PEER peer
)
{
	assert(message);

	if(!user || !user[0])
		return;

	// Check for old-style UTMs.
	////////////////////////////
	if(piIsOldUTM(message))
	{
		// Check for ready.
		///////////////////
		if(strncasecmp(message, "@@@NFO", 6) == 0)
		{
			piPlayer * player;

			player = piGetPlayer(peer, user);
			if(player)
				piHandleOldNFO(peer, player, message);
		}

		return;
	}

	// Check if it's a UTM.
	///////////////////////
	if((type == CHAT_UTM) || (type == CHAT_ATM))
	{
		if(piParseUTM(message))
		{
			piPlayer * player;

			// Get the player it's from.
			////////////////////////////
			player = piGetPlayer(peer, user);
			if(player)
			{
				// Process it.
				//////////////
				piProcessUTM(peer, player, PEERFalse, 0);
			}

			// Pass it along.
			/////////////////
			piAddPlayerUTMCallback(peer, user, piUTMCommand, piUTMParameters, type == CHAT_ATM);
		}

		return;
	}

	// It's a regular message, deliver it.
	//////////////////////////////////////
	piAddPlayerMessageCallback(peer, user, message, (MessageType)type);
}

void piChannelMessage
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * message,
	int type,
	PEER peer
)
{
	piPlayer * player;
	RoomType roomType;

	PEER_CONNECTION;

	assert(message);

	// Check the room type.
	///////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Get the player.
	//////////////////
	player = piGetPlayer(peer, user);

	// Check for old-style UTMs.
	////////////////////////////
	if(player && piIsOldUTM(message))
	{
		// Only take stuff in staging rooms.
		////////////////////////////////////
		if(roomType != StagingRoom)
			return;

		// Check for a launch.
		//////////////////////
		if(strncasecmp(message, "@@@GML", 6) == 0)
		{
			// Ignore old launches from new clients.
			////////////////////////////////////////
			if(strncmp(message + strlen(message) - 4, "/OLD", 4) == 0)
				return;

			// Convert this into its modern equivalent.
			///////////////////////////////////////////
			type = CHAT_UTM;
			message = "GML";
		}
		// Check for ready.
		///////////////////
		else if(strncasecmp(message, "@@@NFO", 6) == 0)
		{
			piHandleOldNFO(peer, player, message);

			return;
		}
		else
		{
			return;
		}
	}

	// Check if it's a UTM.
	///////////////////////
	if((type == CHAT_UTM) || (type == CHAT_ATM))
	{
		if(piParseUTM(message))
		{
			// Process it.
			//////////////
			if(player)
				piProcessUTM(peer, player, PEERTrue, roomType);

			// Pass it along.
			/////////////////
			piAddRoomUTMCallback(peer, roomType, user, piUTMCommand, piUTMParameters, type == CHAT_ATM);
		}

		return;
	}

	// Add the callback.
	////////////////////
	piAddRoomMessageCallback(peer, roomType, user, message, (MessageType)type);
}

void piChannelKicked
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * reason,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Leave the room.
	//////////////////
	piLeaveRoom(peer, roomType, NULL);

	// Add the callback.
	////////////////////
	piAddKickedCallback(peer, roomType, user, reason);
}

void piChannelUserJoined
(
	CHAT chat,
	const char * channel,
	const char * user,
	int mode,
	PEER peer
)
{
	RoomType roomType;
	piPlayer * player;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Add this player to the room.
	///////////////////////////////
	player = piPlayerJoinedRoom(peer, user, roomType, mode);
	if(!player)
		return;

	// Get IP and profile ID if we don't already have it.
	/////////////////////////////////////////////////////
	if(!player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int IP;
		int profileID;

		if(chatGetBasicUserInfoNoWait(connection->chat, user, &info, NULL) && piDemangleUser(info, &IP, &profileID))
		{
			piSetPlayerIPAndProfileID(peer, user, IP, profileID);
		}
	}

	// Refresh this player's watch keys.
	////////////////////////////////////
	piKeyCacheRefreshPlayer(peer, roomType, user);

	// Add the callback.
	////////////////////
	piAddPlayerJoinedCallback(peer, roomType, user);

#if 1
	// If this is the staging room, send our ready state.
	/////////////////////////////////////////////////////
	if((roomType == StagingRoom) && connection->ready)
		peerMessagePlayer(peer, user, "@@@NFO \\$flags$\\rX\\", NormalMessage);
#endif
}

void piChannelUserParted
(
	CHAT chat,
	const char * channel,
	const char * user,
	int why,
	const char * reason,
	const char * kicker,
	PEER peer
)
{
	RoomType roomType;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Remove this player from the room.
	////////////////////////////////////
	piPlayerLeftRoom(peer, user, roomType);

	if((why == CHAT_KICKED) || (why == CHAT_KILLED))
		reason = "Kicked";
	else if(!reason)
		reason = "";

	// Add the callback.
	////////////////////
	piAddPlayerLeftCallback(peer, roomType, user, reason);
}

void piChannelUserChangedNick
(
	CHAT chat,
	const char * channel,
	const char * oldNick,
	const char * newNick,
	PEER peer
)
{
	RoomType roomType;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Change the nick locally.
	///////////////////////////
	piPlayerChangedNick(peer, oldNick, newNick);

	// Add the callback.
	////////////////////
	piAddPlayerChangedNickCallback(peer, roomType, oldNick, newNick);
}

void piChannelTopicChanged
(
	CHAT chat,
	const char * channel,
	const char * topic,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;
	
	// Don't allow blank names.
	///////////////////////////
	if(!topic[0])
		return;

	// Is it the same as the old name?
	//////////////////////////////////
	if(strcmp(NAME, topic) == 0)
		return;

	// Copy the new name.
	/////////////////////
	strzcpy(NAME, topic, PI_NAME_MAX_LEN);

	// Add a callback.
	//////////////////
	if(IN_ROOM)
		piAddRoomNameChangedCallback(peer, roomType);
}

void piChannelNewUserList
(
	CHAT chat,
	const char * channel,
	int num,
	const char ** users,
	int * modes,
	PEER peer
)
{
	int i;
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Clear all the players out of this room.
	//////////////////////////////////////////
	piClearRoomPlayers(peer, roomType);

	// Add all the new ones.
	////////////////////////
	for(i = 0 ; i < num ; i++)
		piPlayerJoinedRoom(peer, users[i], roomType, modes[i]);

	// Refresh keys.
	////////////////
	piKeyCacheRefreshRoom(peer, roomType);

	// Call the callback.
	/////////////////////
	piAddNewPlayerListCallback(peer, roomType);	
}

void piBroadcastKeyChanged
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * key,
	const char * value,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Update the watch keys.
	/////////////////////////
	piRoomKeyChanged(peer, roomType, user, key, value);

	// Add the callback.
	////////////////////
	piAddRoomKeyChangedCallback(peer, roomType, user, key, value);
}

void piUserModeChanged
(
	CHAT chat,
	const char * channel,
	const char * user,
	int mode,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Set the user mode.
	/////////////////////
	piSetPlayerModeFlags(peer, user, roomType, mode);
}

void piChannelModeChanged
(
	CHAT chat,
	const char * channel,
	CHATChannelMode * mode,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	// Figure out the room type.
	////////////////////////////
	if(!piRoomToType(peer, channel, &roomType))
		return;

	// Add the callback.
	////////////////////
	piAddRoomModeChangedCallback(peer, roomType, mode);
}

void piSetChannelCallbacks
(
	PEER peer,
	chatChannelCallbacks * channelCallbacks
)
{
	memset(channelCallbacks, 0, sizeof(chatChannelCallbacks));
	channelCallbacks->param = peer;
	channelCallbacks->channelMessage = piChannelMessage;
	channelCallbacks->kicked = piChannelKicked;
	channelCallbacks->userJoined = piChannelUserJoined;
	channelCallbacks->userParted = piChannelUserParted;
	channelCallbacks->userChangedNick = piChannelUserChangedNick;
	channelCallbacks->topicChanged = piChannelTopicChanged;
	channelCallbacks->newUserList = piChannelNewUserList;
	channelCallbacks->broadcastKeyChanged = piBroadcastKeyChanged;
	channelCallbacks->userModeChanged = piUserModeChanged;
	channelCallbacks->channelModeChanged = piChannelModeChanged;
}

/* GOA.
******/
#define PI_ADD_STR_KEYVALUE(key, value)      piGOAAddKeyValue(outbuf, maxlen, &pos, key, value)
static char intStringBuffer[16];
#define PI_ADD_INT_KEYVALUE(key, value)      {sprintf(intStringBuffer, "%d", value);\
                                             PI_ADD_STR_KEYVALUE(key, intStringBuffer);}
#define PI_CALL_PROGRAM_CALLBACK(callback)   if(callback)\
                                               (callback)(peer, connection->playing, outbuf + pos, maxlen + pos, connection->callbacks.param)

static void piGOAAddKeyValue
(
	char * outbuf,
	int maxlen,
	int * posPtr,
	char * key,
	char * value
)
{
	int pos;
	int len;

	assert(outbuf);
	assert(posPtr);
	assert(key);
	assert(key[0]);
	assert(value);

	pos = *posPtr;

	// How long will this be?
	/////////////////////////
	len = (strlen(key) + strlen(value) + 3);
	assert((len + pos) < maxlen);
	if((len + pos) >= maxlen)
		return;

	// Write it.
	////////////
	sprintf(outbuf + pos, "\\%s\\%s", key, value);

	// Get the new length of the update.
	////////////////////////////////////
	len = strlen(outbuf + pos);

	// Update the pos.
	//////////////////
	*posPtr += len;
	assert(*posPtr < maxlen);
}

/*
\gamename\
\gamever\
\location\
*/
void piGOABasicCallback
(
	char * outbuf,
	int maxlen,
	PEER peer
)
{
	int pos = 0;

	PEER_CONNECTION;

	assert(connection->hosting);

	// gamename.
	////////////
	PI_ADD_STR_KEYVALUE("gamename", connection->gamename);

	// Call the program's callback.
	///////////////////////////////
	PI_CALL_PROGRAM_CALLBACK(connection->callbacks.GOABasic);
}

/*
\hostname\
\hostport\
\mapname\
\gametype\
\numplayers\
\maxplayers\
\gamemode\
\password\
\groupid\
*/
void piGOAInfoCallback
(
	char * outbuf,
	int maxlen,
	PEER peer
)
{
	int pos = 0;

	PEER_CONNECTION;

	assert(connection->hosting);

	if(connection->inRoom[StagingRoom] && (!connection->playing || (connection->reportingOptions & PEER_REPORT_INFO)))
	{
		// hostname.
		////////////
		PI_ADD_STR_KEYVALUE("hostname", NAMES[StagingRoom]);

		// numplayers.
		//////////////
		PI_ADD_INT_KEYVALUE("numplayers", connection->numPlayers[StagingRoom]);

		// maxplayers.
		//////////////
		if(connection->maxPlayers)
			PI_ADD_INT_KEYVALUE("maxplayers", connection->maxPlayers);

		// gamemode.
		////////////
		if(!connection->playing)
			PI_ADD_STR_KEYVALUE("gamemode", "openstaging");

		// password.
		////////////
		if(connection->password[0])
			PI_ADD_INT_KEYVALUE("password", 1);
	}

	// groupid.
	///////////
	if(connection->reportingGroupID)
		PI_ADD_INT_KEYVALUE("groupid", connection->reportingGroupID);

	// Call the program's callback.
	///////////////////////////////
	PI_CALL_PROGRAM_CALLBACK(connection->callbacks.GOAInfo);

	// Was no gamemode set?
	///////////////////////
	if(!strstr(outbuf, "gamemode"))
		PI_ADD_STR_KEYVALUE("gamemode", "openplaying");
}

/*
\timelimit\
\fraglimit\
\teamplay\
\rankedserver\
*/
void piGOARulesCallback
(
	char * outbuf,
	int maxlen,
	PEER peer
)
{
	int pos = 0;

	PEER_CONNECTION;

	assert(connection->hosting);

	// Call the program's callback.
	///////////////////////////////
	PI_CALL_PROGRAM_CALLBACK(connection->callbacks.GOARules);
}

/*
\player_N\
\frags_N\
\deaths_N\
\skill_N\
\ping_N\
\team_N\
*/
#define PI_ADD_PLAYER_STR_KEYVALUE(key, value)      piGOAAddPlayerKeyValue(data, key, value, index)
#define PI_ADD_PLAYER_INT_KEYVALUE(key, value)      {sprintf(intStringBuffer, "%d", value);\
                                                    PI_ADD_PLAYER_STR_KEYVALUE(key, intStringBuffer);}

typedef struct piGOAPlayersCallbackData
{
	char * outbuf;
	int maxlen;
	int pos;
} piGOAPlayersCallbackData;

static void piGOAAddPlayerKeyValue
(
	piGOAPlayersCallbackData * data,
	char * key,
	char * value,
	int playerNum
)
{
	char keyN[32];

	assert((strlen(key) + 4) < sizeof(keyN));

	sprintf(keyN, "%s_%d", key, playerNum);

	piGOAAddKeyValue(data->outbuf, data->maxlen, &data->pos, keyN, value);
}

void piGOAPlayersCallbackEnumPlayers
(
	PEER peer,
	RoomType roomType,
	piPlayer * player,
	int index,
	void *param
)
{
	if(player)
	{
		piGOAPlayersCallbackData * data = (piGOAPlayersCallbackData *)param;
		int ping;

		PI_ADD_PLAYER_STR_KEYVALUE("player", player->nick);

		if(player->local)
			ping = 0;
		else if(player->numPings)
			ping = player->pingAverage;
		else
			ping = 9999;
		PI_ADD_PLAYER_INT_KEYVALUE("ping", ping)
	}
}

void piGOAPlayersCallback
(
	char * outbuf,
	int maxlen,
	PEER peer
)
{
	piGOAPlayersCallbackData data;
	int pos = 0;

	PEER_CONNECTION;

	assert(connection->hosting);

	if(connection->inRoom[StagingRoom] && (!connection->playing || (connection->reportingOptions & PEER_REPORT_PLAYERS)))
	{
		// Go through the list of players.
		//////////////////////////////////
		data.outbuf = outbuf;
		data.maxlen = maxlen;
		data.pos = pos;

		piEnumRoomPlayers(peer, StagingRoom, piGOAPlayersCallbackEnumPlayers, &data);

		outbuf = data.outbuf;
		pos = data.pos;
	}

	// Call the program's callback.
	///////////////////////////////
	PI_CALL_PROGRAM_CALLBACK(connection->callbacks.GOAPlayers);
}