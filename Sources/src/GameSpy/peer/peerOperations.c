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
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "peerOperations.h"
#include "peerCallbacks.h"
#include "peerGlobalCallbacks.h"
#include "peerMangle.h"
#include "peerRooms.h"
#include "peerPlayers.h"
#include "peerCEngine.h"
#include "peerKeys.h"
#if defined(applec) || defined(THINK_C) || defined(__MWERKS__) && !defined(__KATANA__) && !defined(__mips64)
	#include "::md5.h"
#else
	#include "../md5.h"
#endif

/************
** DEFINES **
************/
#define PI_CHAT_SERVER_ADDRESS    "peerchat.gamespy.com"
#define PI_CHAT_SERVER_PORT       6667

#define PEER_CONNECTION_OP        piConnection * connection;\
                                  PEER peer;\
                                  assert(operation);\
								  assert(operation->peer);\
								  peer = operation->peer;\
                                  connection = (piConnection *)peer;
#if 0
PEER peer; // for Visual Assist
#endif

/**********
** TYPES **
**********/
typedef struct piOperationContainer
{
	piOperation * operation;
} piOperationContainer;

/**************
** FUNCTIONS **
**************/
static void piOperationsListFree(void *elem1)
{
	piOperationContainer * container = (piOperationContainer *)elem1;
	piOperation * operation;

	assert(container);
	assert(container->operation);

	operation = container->operation;

	// gsifree the name.
	/////////////////
	gsifree(operation->name);

	// gsifree the user data.
	//////////////////////
	gsifree(operation->data);

	// gsifree the operation.
	//////////////////////
	gsifree(operation);
}

PEERBool piOperationsInit
(
	PEER peer
)
{
	PEER_CONNECTION;

	// Init data.
	/////////////
	connection->operationsStarted = 0;
	connection->operationsFinished = 0;

	// Init the list.
	/////////////////
	connection->operationList = ArrayNew(sizeof(piOperationContainer), 0, piOperationsListFree);
	if(!connection->operationList)
		return PEERFalse;

	return PEERTrue;
}

void piOperationsCleanup
(
	PEER peer
)
{
	PEER_CONNECTION;

	// Cleanup the list.
	////////////////////
	if(connection->operationList)
		ArrayFree(connection->operationList);
}

static piOperation * piAddOperation
(
	PEER peer,
	piOperationType type,
	void * data,
	void * callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	piOperationContainer container;

	PEER_CONNECTION;

	assert(type >= 0);
	assert(type < PI_NUM_OPERATION_TYPES);

	// Alloc the operaiton.
	///////////////////////
	operation = (piOperation *)gsimalloc(sizeof(piOperation));
	if(!operation)
		return NULL;

	// Fill in the operation.
	/////////////////////////
	memset(operation, 0, sizeof(piOperation));
	operation->peer = peer;
	operation->type = type;
	operation->data = data;
	operation->ID = opID;
	operation->callback = callback;
	operation->callbackParam = callbackParam;
	operation->name = NULL;
	operation->cancel = PEERFalse;

	// Add the operation to the list.
	/////////////////////////////////
	container.operation = operation;
	ArrayAppend(connection->operationList, &container);

	// One more op.
	///////////////
	connection->operationsStarted++;

	return operation;
}

static int piRemoveCompareCallback
(
 const void *elem1,
 const void *elem2
)
{
	piOperationContainer * container1 = (piOperationContainer *)elem1;
	piOperationContainer * container2 = (piOperationContainer *)elem2;
	assert(container1);
	assert(container2);

	return (container1->operation - container2->operation);
}

void piRemoveOperation
(
	PEER peer,
	piOperation * operation
)
{
	piOperationContainer container;
	int index;

	PEER_CONNECTION;

	// Find this operation.
	///////////////////////
	container.operation = operation;
	index = ArraySearch(connection->operationList, &container, piRemoveCompareCallback, 0, 0);
	if(index == NOT_FOUND)
		return;

	// Remove it.
	/////////////
	ArrayDeleteAt(connection->operationList, index);

	// One more finished.
	/////////////////////
	connection->operationsFinished++;
}

static int piIsOperationFinishedCompareCallback
(
	const void *elem1,
	const void *elem2
)
{
	piOperationContainer * container1 = (piOperationContainer *)elem1;
	piOperationContainer * container2 = (piOperationContainer *)elem2;
	assert(container1);
	assert(container2);
	assert(container1->operation);
	assert(container2->operation);

	return (container1->operation->ID - container2->operation->ID);
}

PEERBool piIsOperationFinished
(
	PEER peer,
	int opID
)
{
	piOperationContainer container;
	piOperation operation;
	int index;

	PEER_CONNECTION;

	// Find this operation.
	///////////////////////
	container.operation = &operation;
	operation.ID = opID;
	index = ArraySearch(connection->operationList, &container, piIsOperationFinishedCompareCallback, 0, 0);

	// Did we find it?
	//////////////////
	return (index == NOT_FOUND);
}

/***************
** OPERATIONS **
***************/

/* Connect.
**********/
static void piConnectConnectCallback
(
	CHAT chat,
	CHATBool success,
	void *param
)
{
	piOperation * operation = (piOperation *)param;
	PEER_CONNECTION_OP;
	
	// If successful, try and do the connect title stuff.
	/////////////////////////////////////////////////////
	if(success)
	{
		if(!piConnectTitle(peer))
		{
			piDisconnectTitle(peer);
			success = CHATFalse;
		}
	}

	// Connection attempt finished.
	///////////////////////////////
	connection->connecting = PEERFalse;
	connection->connected = (PEERBool)success;

	if(success)
	{
		const char * nick;

		// Setup server pinging.
		////////////////////////
		connection->lastChatPing = current_time();
		
		// Check the nick.
		//////////////////
		nick = chatGetNick(chat);
		if(strcasecmp(connection->nick, nick) != 0)
			strcpy(connection->nick, nick);
	}
	else
	{
		// Set the disconnect flag.
		///////////////////////////
		connection->disconnect = PEERTrue;
	}

	// Add the callback.
	////////////////////
	piAddConnectCallback(peer, success, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	piRemoveOperation(peer, operation);
}

static void piConnectNickErrorCallback
(
	CHAT chat,
	int type,
	const char * nick,
	void *param
)
{
	piOperation * operation = (piOperation *)param;
	PEER_CONNECTION_OP;

	// Add the peer callback.
	/////////////////////////
	piAddNickErrorCallback(peer, type, nick, operation->callbackParam, operation->ID);
}

static void piConnectFillInUserCallback
(
	CHAT chat,
	unsigned int IP,
	char user[128],
	void * param
)
{
	piOperation * operation = (piOperation *)param;
	PEER_CONNECTION_OP;

	connection->IP = IP;
	piMangleUser(user, connection->IP, connection->profileID);
}

PEERBool piNewConnectOperation
(
	PEER peer,
	const char * nick,
	peerConnectCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatGlobalCallbacks globalCallbacks;
	const char * uniqueID;
	char encodedUniqueID[33];

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);
	assert(callback);

	// Add an operation.
	////////////////////
	operation = piAddOperation(peer, PI_CONNECT_OPERATION, NULL, callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;

	// Setup the global callbacks.
	//////////////////////////////
	memset(&globalCallbacks, 0, sizeof(chatGlobalCallbacks));
	globalCallbacks.disconnected = piChatDisconnected;
	globalCallbacks.privateMessage = piChatPrivateMessage;
	globalCallbacks.param = peer;

	// Encode the unique ID.
	////////////////////////
	uniqueID = GOAGetUniqueID();
	MD5Digest((unsigned char *)uniqueID, strlen(uniqueID), encodedUniqueID);

	// Connect to chat.
	///////////////////
	connection->chat = chatConnectSecure(
		PI_CHAT_SERVER_ADDRESS,
		PI_CHAT_SERVER_PORT,
		nick,
		encodedUniqueID, //"me@fake-email.com",
		connection->engineName,
		connection->engineSecretKey,
		&globalCallbacks,
		connection->nickErrorCallback ? piConnectNickErrorCallback : NULL,
		piConnectFillInUserCallback,
		piConnectConnectCallback,
		operation,
		CHATFalse);
	if(!connection->chat)
	{
		piRemoveOperation(peer, operation);
		return PEERFalse;
	}

	return PEERTrue;
}

/* Create Staging Room.
**********************/
static PEERJoinResult piEnterResultToJoinResult
(
	CHATEnterResult result
)
{
	switch(result)
	{
	case CHATEnterSuccess:
		return PEERJoinSuccess;
	case CHATChannelIsFull:
		return PEERFullRoom;
	case CHATInviteOnlyChannel:
		return PEERInviteOnlyRoom;
	case CHATBannedFromChannel:
		return PEERBannedFromRoom;
	case CHATBadChannelPassword:
		return PEERBadPassword;
	}

	return PEERJoinFailed;
}

static void piCreateStagingRoomEnumUsersCallback
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	int numUsers,
	const char ** users,
	int * modes,
	void *param
)
{
	piOperation * operation = (piOperation *)param;
	PEERBool callCallback = PEERTrue;

	PEER_CONNECTION_OP;

	// Check for cancel.
	////////////////////
	if(connection->cancelJoinRoom[StagingRoom])
	{
		// Failed.
		//////////
		success = CHATFalse;
		callCallback = connection->cancelJoinRoomError[StagingRoom];
		
		// Clear the flag.
		//////////////////
		connection->cancelJoinRoom[StagingRoom] = PEERFalse;
		connection->cancelJoinRoomError[StagingRoom] = PEERFalse;
	}

	// Check for ops.
	/////////////////
//	if(success && !(modes[0] & CHAT_OP))
//		success = CHATFalse;

	// Setup GOA.
	/////////////
	if(success && !piStartHosting(peer, operation->socket))
		success = CHATFalse;

	// Do stuff based on success.
	/////////////////////////////
	if(success)
	{
		int i;

		// Done entering.
		/////////////////
		piFinishedEnteringRoom(peer, StagingRoom, operation->name);

		// Add everyone to the room.
		////////////////////////////
		for(i = 0 ; i < numUsers ; i++)
			piPlayerJoinedRoom(peer, users[i], StagingRoom, modes[i]);

		// Set the name.
		////////////////
		chatSetChannelTopic(connection->chat, channel, operation->name);

		// Set the password.
		////////////////////
		if(connection->password[0])
			chatSetChannelPassword(connection->chat, channel, CHATTrue, connection->password);
	}
	else
	{
		// Leave the room.
		//////////////////
		piLeaveRoom(peer, StagingRoom, NULL);
	}

	// Add the callback.
	////////////////////
	if(callCallback)
		piAddJoinRoomCallback(peer, (PEERBool)success, success?PEERJoinSuccess:PEERJoinFailed, StagingRoom, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	piRemoveOperation(peer, operation);
}

static void piCreateStagingRoomEnterChannelCallback
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const char * channel,
	void *param
)
{
	piOperation * operation = (piOperation *)param;
	PEERBool callCallback = PEERTrue;
	
	PEER_CONNECTION_OP;

	assert(channel);
	assert(channel[0]);

	// Check for cancel.
	////////////////////
	if(connection->cancelJoinRoom[StagingRoom])
	{
		// Failed.
		//////////
		success = CHATFalse;
		callCallback = connection->cancelJoinRoomError[StagingRoom];
		
		// Clear the flag.
		//////////////////
		connection->cancelJoinRoom[StagingRoom] = PEERFalse;
		connection->cancelJoinRoomError[StagingRoom] = PEERFalse;
	}

	if(success)
	{
		// How many users?
		//////////////////
		chatEnumUsers(chat, channel, piCreateStagingRoomEnumUsersCallback, operation, CHATFalse);
	}

	if(!success)
	{
		PEERJoinResult joinResult;

		// Not entering.
		////////////////
		piLeaveRoom(peer, StagingRoom, NULL);

		// Get the peer result.
		///////////////////////
		if(result == CHATEnterSuccess)
			joinResult = PEERJoinFailed;  // We might have succeeded in entering, then failed in here.
		else
			joinResult = piEnterResultToJoinResult(result);

		// Add the callback.
		////////////////////
		if(callCallback)
			piAddJoinRoomCallback(peer, PEERFalse, joinResult, StagingRoom, operation->callback, operation->callbackParam, operation->ID);

		// Remove the operation.
		////////////////////////
		piRemoveOperation(peer, operation);
	}
}

PEERBool piNewCreateStagingRoomOperation
(
	PEER peer,
	const char * name,
	SOCKET socket,
	peerJoinRoomCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatChannelCallbacks channelCallbacks;
	char room[PI_ROOM_MAX_LEN];

	PEER_CONNECTION;

	assert(name);
	if(!name)
		name = "";

	assert(callback);
	if(!callback)
		return PEERFalse;

	// Get the room name.
	/////////////////////
	piMangleStagingRoom(room, connection->title, connection->IP);
	assert(room[0]);

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_CREATE_ROOM_OPERATION, NULL, callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;
	operation->name = _strdup(name);
	if(!operation->name)
	{
		piRemoveOperation(peer, operation);
		return PEERFalse;
	}
	operation->socket = socket;

	// Set the callbacks.
	/////////////////////
	piSetChannelCallbacks(peer, &channelCallbacks);

	// Create the room.
	///////////////////
	piStartedEnteringRoom(peer, StagingRoom, room);
	chatEnterChannel(connection->chat, room, NULL, &channelCallbacks, piCreateStagingRoomEnterChannelCallback, operation, CHATFalse);

	return PEERTrue;
}

/* Join Room.
************/
static char * piGetGroupRoomName
(
	PEER peer
)
{
	int i;
	int count;
	GServer server;
	int id;
	static char * noName = "(No Name)";

	PEER_CONNECTION;

	// Go through the list of groups until we find the right one.
	/////////////////////////////////////////////////////////////
	count = ServerListCount(connection->groupList);
	for(i = 0 ; i < count ; i++)
	{
		server = ServerListGetServer(connection->groupList, i);
		id = ServerGetIntValue(server, "groupid", -1);
		if(id == connection->groupID)
			return ServerGetStringValue(server, "hostname", "noName");
	}

	return noName;
}

static void piJoinRoomEnumUsersCallback
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	int numUsers,
	const char ** users,
	int * modes,
	void * param
)
{
	char * name;
	piOperation * operation = (piOperation *)param;

	PEERBool callCallback = PEERTrue;
	
	PEER_CONNECTION_OP;

	// Check for cancel.
	////////////////////
	if(connection->cancelJoinRoom[operation->roomType])
	{
		// Failed.
		//////////
		success = CHATFalse;
		callCallback = connection->cancelJoinRoomError[operation->roomType];
		
		// Clear the flag.
		//////////////////
		connection->cancelJoinRoom[operation->roomType] = PEERFalse;
		connection->cancelJoinRoomError[operation->roomType] = PEERFalse;
	}
	
	// Check for success.
	/////////////////////
	if(success)
	{
		int i;

		// Get the name.
		////////////////
		if(operation->roomType == GroupRoom)
			name = piGetGroupRoomName(peer);
		else
			name = "";

		// Finished entering the room.
		//////////////////////////////
		piFinishedEnteringRoom(peer, operation->roomType, name);

		// Add all these people to the room.
		////////////////////////////////////
		for(i = 0 ; i < numUsers ; i++)
			piPlayerJoinedRoom(peer, users[i], operation->roomType, modes[i]);
	}
	else
	{
		// Leave the room.
		//////////////////
		piLeaveRoom(peer, operation->roomType, NULL);
	}

	// Add the callback.
	////////////////////
	if(callCallback)
		piAddJoinRoomCallback(peer, (PEERBool)success, success?PEERJoinSuccess:PEERJoinFailed, operation->roomType, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	piRemoveOperation(peer, operation);
}

static void piJoinRoomEnterChannelCallback
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const char * channel,
	void *param
)
{
	piOperation * operation = (piOperation *)param;
	PEERBool callCallback = PEERTrue;
	
	PEER_CONNECTION_OP;

	assert(channel);
	assert(channel[0]);

	// Check for cancel.
	////////////////////
	if(connection->cancelJoinRoom[operation->roomType])
	{
		// Failed.
		//////////
		success = CHATFalse;
		callCallback = connection->cancelJoinRoomError[operation->roomType];
		
		// Clear the flag.
		//////////////////
		connection->cancelJoinRoom[operation->roomType] = PEERFalse;
		connection->cancelJoinRoomError[operation->roomType] = PEERFalse;
	}

	if(success)
	{
		// How many users?
		//////////////////
		chatEnumUsers(chat, channel, piJoinRoomEnumUsersCallback, operation, CHATFalse);
	}
	else
	{
		PEERJoinResult joinResult;

		// Not entering.
		////////////////
		piLeaveRoom(peer, operation->roomType, NULL);

		// Get the peer result.
		///////////////////////
		if(result == CHATEnterSuccess)
			joinResult = PEERJoinFailed;  // We might have succeeded in entering, then failed in here.
		else
			joinResult = piEnterResultToJoinResult(result);

		// Add the callback.
		////////////////////
		if(callCallback)
			piAddJoinRoomCallback(peer, PEERFalse, joinResult, operation->roomType, operation->callback, operation->callbackParam, operation->ID);

		// Remove the operation.
		////////////////////////
		piRemoveOperation(peer, operation);
	}
}

PEERBool piNewJoinRoomOperation
(
	PEER peer,
	RoomType roomType,
	const char * channel,
	peerJoinRoomCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatChannelCallbacks channelCallbacks;
	const char * password;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(callback);

	// Check the name.
	//////////////////
	assert(channel);
	assert(channel[0]);
	if(!channel || !channel[0])
		return PEERFalse;

	// Check that the name isn't too long.
	//////////////////////////////////////
	assert(strlen(channel) < PI_ROOM_MAX_LEN);
	if(strlen(channel) >= PI_ROOM_MAX_LEN)
		return PEERFalse;

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_JOIN_ROOM_OPERATION, NULL, callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;
	operation->roomType = roomType;

	// Set the callbacks.
	/////////////////////
	piSetChannelCallbacks(peer, &channelCallbacks);

	// Set the password for staging rooms.
	//////////////////////////////////////
	if((roomType == StagingRoom) && connection->password[0])
		password = connection->password;
	else
		password = NULL;

	// Join the room.
	/////////////////
	piStartedEnteringRoom(peer, roomType, channel);
	chatEnterChannel(connection->chat, channel, password, &channelCallbacks, piJoinRoomEnterChannelCallback, operation, CHATFalse);

	return PEERTrue;
}

/* List Group Rooms.
*******************/
PEERBool piNewListGroupRoomsOperation
(
	PEER peer,
	peerListGroupRoomsCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(callback);

	// Add the operation.
	/////////////////////
	operation = connection->listingGroupsOperation = piAddOperation(peer, PI_LIST_GROUP_ROOMS_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;

	// Start the listing.
	/////////////////////
	return piCEngineStartListingGroups(peer);
}

/* ListingGames.
***************/
static void piListingGamesEnterChannelCallback
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const char * channel,
	void * param
)
{
	piOperation * operation = (piOperation *)param;
	PEER_CONNECTION_OP;

	// Check for a cancelled operation.
	///////////////////////////////////
	if(operation->cancel)
	{
		piRemoveOperation(peer, operation);
		return;
	}

	// Start listing the servers.
	/////////////////////////////
	if(success && !piCEngineStartListingGames(peer, operation->name))
		success = CHATFalse;

	// Check for success.
	/////////////////////
	if(success)
	{
		// We're listing games.
		///////////////////////
		connection->listingGames = PEERTrue;

		// Clear the list.
		//////////////////
		piAddListingGamesCallback(peer, PEERTrue, NULL, NULL, PEERFalse, PEER_CLEAR, 0, operation->callback, operation->callbackParam);
	}
	else
	{
		// Leave the channel.
		/////////////////////
		chatLeaveChannel(connection->chat, channel, NULL);

		// Add the callback.
		////////////////////
		piAddListingGamesCallback(peer, PEERFalse, NULL, NULL, PEERFalse, 0, 0, operation->callback, operation->callbackParam);

		// Clear the operation.
		///////////////////////
		piRemoveOperation(peer, operation);
		connection->listingGamesOperation = NULL;
	}
}

PEERBool piNewListingGamesOperation
(
	PEER peer,
	const char * filter,
	peerListingGamesCallback callback,
	void * param
)
{
	piOperation * operation;
	chatChannelCallbacks callbacks;
	int groupID;
	char * filterCopy;

	PEER_CONNECTION;

	assert(callback);
	assert(!connection->listingGamesOperation);

	// Copy off the filter.
	///////////////////////
	filterCopy = _strdup(filter);
	if(filter && !filterCopy)
		return PEERFalse;

	// Add the operation.
	/////////////////////
	operation = connection->listingGamesOperation = piAddOperation(peer, PI_LIST_STAGING_ROOMS_OPERATION, NULL, callback, param, -1);
	if(!operation)
	{
		gsifree(filterCopy);
		return PEERFalse;
	}

	// Store the filter with the operation.
	///////////////////////////////////////
	operation->name = filterCopy;

	// Set the channel callbacks.
	/////////////////////////////
	memset(&callbacks, 0, sizeof(chatChannelCallbacks));
	callbacks.param = peer;
	callbacks.channelMessage = piListingGamesChannelMessage;
	callbacks.kicked = piListingGamesChannelKicked;

	// Join the updates room.
	/////////////////////////
	if(connection->inRoom[GroupRoom])
		groupID = connection->groupID;
	else
		groupID = 0;
	if(!connection->updatesRoom[0])
		piMangleUpdatesRoom(connection->updatesRoom, connection->title, groupID);
	chatEnterChannel(connection->chat, connection->updatesRoom, "", &callbacks, piListingGamesEnterChannelCallback, operation, CHATFalse);

	return PEERTrue;
}

/* Get Player Info.
*******************/
void piGetPlayerInfoCallback
(
	CHAT chat,
	CHATBool success,
	const char * nick,
	const char * user,
	const char * address,
	void * param
)
{
	piOperation * operation = (piOperation *)param;
	int profileID = 0;
	unsigned int IP = 0;

	PEER_CONNECTION_OP;

	assert(nick);
	assert(nick[0]);

	// Check for success.
	/////////////////////
	if(success)
	{
		assert(user);
		assert(user[0]);

		// Get the info.
		////////////////
		if(!piDemangleUser(user, &IP, &profileID))
			success = CHATFalse;

		// Cache the info.
		//////////////////
		if(success)
			piSetPlayerIPAndProfileID(peer, nick, IP, profileID);
	}
	if(!success)
	{
		profileID = 0;
		IP = 0;
	}

	// Add the callback.
	////////////////////
	if(operation->callback)
	{
		if(operation->type == PI_GET_PROFILE_ID_OPERATION)
			piAddGetPlayerProfileIDCallback(peer, success, nick, profileID, operation->callback, operation->callbackParam, operation->ID);
		else if(operation->type == PI_GET_IP_OPERATION)
			piAddGetPlayerIPCallback(peer, success, nick, IP, operation->callback, operation->callbackParam, operation->ID);
		else
			assert(0);
	}

	// Remove the operation.
	////////////////////////
	piRemoveOperation(peer, operation);
}

PEERBool piNewGetProfileIDOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerProfileIDCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_GET_PROFILE_ID_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;

	// Get the user's info.
	///////////////////////
	chatGetBasicUserInfo(connection->chat, nick, piGetPlayerInfoCallback, operation, CHATFalse);

	return PEERTrue;
}

PEERBool piNewGetIPOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerIPCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_GET_IP_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;

	// Get the user's info.
	///////////////////////
	chatGetBasicUserInfo(connection->chat, nick, piGetPlayerInfoCallback, operation, CHATFalse);

	return PEERTrue;
}

/* Change Nick.
**************/
void piChangeNickCallback
(
	CHAT chat,
	CHATBool success,
	const char * oldNick,
	const char * newNick,
	void * param
)
{
	piOperation * operation = (piOperation *)param;

	PEER_CONNECTION_OP;

	// Check for success.
	/////////////////////
	if(success)
	{
		// Update the nick locally.
		///////////////////////////
		strcpy(connection->nick, newNick);
	}

	// Add the callback.
	////////////////////
	if(operation->callback)
		piAddChangeNickCallback(peer, success, oldNick, newNick, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	piRemoveOperation(peer, operation);
}

PEERBool piNewChangeNickOperation
(
	PEER peer,
	const char * newNick,
	peerChangeNickCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(newNick);
	assert(newNick[0]);

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_CHANGE_NICK_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;

	// Change the nick.
	///////////////////
	chatChangeNick(connection->chat, newNick, piChangeNickCallback, operation, CHATFalse);

	return PEERTrue;
}

/* Get Global Keys.
******************/
static void piGetGlobalKeysCallback
(
	CHAT chat,
	CHATBool success,
	const char * user,
	int num,
	const char ** keys,
	const char ** values,
	void * param
)
{
	int i;
	piOperation * operation = (piOperation *)param;

	PEER_CONNECTION_OP;

	// Update the watch keys.
	/////////////////////////
	if(success && user)
	{
		for(i = 0 ; i < num ; i++)
			piGlobalKeyChanged(peer, user, keys[i], values[i]);
	}

	// Add the callback.
	////////////////////
	if(operation->callback)
		piAddGetGlobalKeysCallback(peer, success, user, num, keys, values, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	if(!success || !user || !operation->num)
		piRemoveOperation(peer, operation);
}

PEERBool piNewGetGlobalKeysOperation
(
	PEER peer,
	const char * target,
	int num,
	const char ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(target);
	assert(num > 0);
	assert(keys);

	if(!target || !target[0])
		return PEERFalse;
	if(num <= 0)
		return PEERFalse;

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_GET_GLOBAL_KEYS_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;

	// Use num as a flag for getting a whole channel.
	/////////////////////////////////////////////////
	operation->num = (target[0] == '#');

	// Get the keys.
	////////////////
	chatGetGlobalKeys(connection->chat, target, num, keys, piGetGlobalKeysCallback, operation, CHATFalse);

	return PEERTrue;
}

/* Get Room Keys.
****************/
static void piGetChannelKeysCallback
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	const char * user,
	int num,
	const char ** keys,
	const char ** values,
	void * param
)
{
	int i;
	piOperation * operation = (piOperation *)param;

	PEER_CONNECTION_OP;

	// Update the watch keys.
	/////////////////////////
	if(success && user)
	{
		for(i = 0 ; i < num ; i++)
			piRoomKeyChanged(peer, operation->roomType, user, keys[i], values[i]);
	}

	// Add the callback.
	////////////////////
	if(operation->callback)
		piAddGetRoomKeysCallback(peer, success, operation->roomType, user, num, keys, values, operation->callback, operation->callbackParam, operation->ID);

	// Remove the operation.
	////////////////////////
	if(!success || !user || !operation->num)
		piRemoveOperation(peer, operation);
}

PEERBool piNewGetRoomKeysOperation
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	int num,
	const char ** keys,
	peerGetRoomKeysCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(num >= 0);
	assert(keys);

	if(num < 0)
		return PEERFalse;

	if(!ENTERING_ROOM && !IN_ROOM)
		return PEERFalse;

	// Add the operation.
	/////////////////////
	operation = piAddOperation(peer, PI_GET_ROOM_KEYS_OPERATION, NULL, callback, param, opID);
	if(!operation)
		return PEERFalse;
	operation->roomType = roomType;

	// Use num as a flag for getting a whole channel.
	/////////////////////////////////////////////////
	if(nick)
		operation->num = (strcmp(nick, "*") == 0);
	else
		operation->num = 0;

	// Get the keys.
	////////////////
	chatGetChannelKeys(connection->chat, ROOM, nick, num, keys, piGetChannelKeysCallback, operation, CHATFalse);

	return PEERTrue;
}
