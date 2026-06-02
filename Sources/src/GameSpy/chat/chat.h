/*
GameSpy Chat SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com
*/

#ifndef _CHAT_H_
#define _CHAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/************
** DEFINES **
************/
#define CHAT_MESSAGE        0
#define CHAT_ACTION         1
#define CHAT_NOTICE         2
#define CHAT_UTM            3
#define CHAT_ATM            4

#define CHAT_NORMAL         0
#define CHAT_VOICE          1
#define CHAT_OP             2

#define CHAT_LEFT           0  // The user left the channel.
#define CHAT_QUIT           1  // The user quit the chat network.
#define CHAT_KICKED         2  // The user was kicked from the channel.
#define CHAT_KILLED         3  // The user was kicked off the chat network.

#define CHAT_IN_USE         0
#define CHAT_INVALID        1

/**********
** TYPES **
**********/
typedef enum { CHATFalse, CHATTrue } CHATBool;

typedef void * CHAT;

typedef struct CHATChannelMode
{
	CHATBool InviteOnly;
	CHATBool Private;
	CHATBool Secret;
	CHATBool Moderated;
	CHATBool NoExternalMessages;
	CHATBool OnlyOpsChangeTopic;
	int Limit;
} CHATChannelMode;

typedef enum
{
	CHATEnterSuccess,        // The channel was successfully entered.
	CHATBadChannelName,      // The channel name was invalid.
	CHATChannelIsFull,       // The channel is at its user limit.
	CHATInviteOnlyChannel,   // The channel is invite only.
	CHATBannedFromChannel,   // The local user is banned from this channel.
	CHATBadChannelPassword,  // The channel has a password, and a bad password (or none) was given.
	CHATTooManyChannels,     // The server won't allow this user in any more channels.
	CHATEnterTimedOut,       // The attempt to enter timed out.
	CHATBadChannelMask       // Not sure if any servers use this, or what it means! (ERR_BADCHANMASK)
} CHATEnterResult;

/**********************
** GLOBALS CALLBACKS **
**********************/
typedef void (* chatRaw)(CHAT chat,
						 const char * raw,
						 void * param);

typedef void (* chatDisconnected)(CHAT chat,
								  const char * reason,
								  void * param);

typedef void (* chatPrivateMessage)(CHAT chat,
									const char * user,
									const char * message,
									int type,  // See defined message types above.
									void * param);

typedef void (* chatInvited)(CHAT chat,
							 const char * channel,
							 const char * user,
							 void * param);

typedef struct chatGlobalCallbacks
{
	chatRaw raw;
	chatDisconnected disconnected;
	chatPrivateMessage privateMessage;
	chatInvited invited;
	void * param;
} chatGlobalCallbacks;

/**********************
** CHANNEL CALLBACKS **
**********************/
typedef void (* chatChannelMessage)(CHAT chat,
									const char * channel,
									const char * user,
									const char * message,
									int type,  // See defined message types above.
									void * param);

typedef void (* chatKicked)(CHAT chat,
							const char * channel,
							const char * user,
							const char * reason,
							void * param);

typedef void (* chatUserJoined)(CHAT chat,
								const char * channel,
								const char * user,
								int mode,    // See defined user modes above.
								void * param);

typedef void (* chatUserParted)(CHAT chat,
								const char * channel,
								const char * user,
								int why,    // See defined part reasons above.
								const char * reason,
								const char * kicker,
								void * param);

typedef void (* chatUserChangedNick)(CHAT chat,
									 const char * channel,
									 const char * oldNick,
									 const char * newNick,
									 void * param);

typedef void (* chatTopicChanged)(CHAT chat,
								  const char * channel,
								  const char * topic,
								  void * param);

typedef void (* chatChannelModeChanged)(CHAT chat,
										const char * channel,
										CHATChannelMode * mode,
										void * param);

typedef void (* chatUserModeChanged)(CHAT chat,
									 const char * channel,
									 const char * user,
									 int mode,  // See defined user modes above.
									 void * param);

typedef void (* chatUserListUpdated)(CHAT chat,
									 const char * channel,
									 void * param);

typedef void (* chatNewUserList)(CHAT chat,
								 const char * channel,
								 int num,
								 const char ** users,
								 int * modes,
								 void * param);

typedef void (* chatBroadcastKeyChanged)(CHAT chat,
										 const char * channel,
										 const char * user,
										 const char * key,
										 const char * value,
										 void * param);

typedef struct chatChannelCallbacks
{
	chatChannelMessage channelMessage;
	chatKicked kicked;
	chatUserJoined userJoined;
	chatUserParted userParted;
	chatUserChangedNick userChangedNick;
	chatTopicChanged topicChanged;
	chatChannelModeChanged channelModeChanged;
	chatUserModeChanged userModeChanged;
	chatUserListUpdated userListUpdated;
	chatNewUserList newUserList;
	chatBroadcastKeyChanged broadcastKeyChanged;
	void * param;
} chatChannelCallbacks;

/************
** GENERAL **
************/
typedef void (* chatConnectCallback)(CHAT chat,
									 CHATBool success,
									 void * param);
typedef void (* chatNickErrorCallback)(CHAT chat,
									   int type,  // CHAT_IN_USE, CHAT_INVALID
									   const char * nick,
									   void * param);
typedef void (* chatFillInUserCallback)(CHAT chat,
										unsigned int IP, // PANTS|08.21.00 - changed from unsigned long
										char user[128],
										void * param);
CHAT chatConnect(const char * serverAddress,
				 int port,
                 const char * nick,
				 const char * user,
				 const char * name,
				 chatGlobalCallbacks * callbacks,
				 chatNickErrorCallback nickErrorCallback,
                 chatConnectCallback connectCallback,
                 void * param,
                 CHATBool blocking);
CHAT chatConnectSpecial(const char * serverAddress,
				 int port,
                 const char * nick,
				 const char * name,
				 chatGlobalCallbacks * callbacks,
				 chatNickErrorCallback nickErrorCallback,
				 chatFillInUserCallback fillInUserCallback,
                 chatConnectCallback connectCallback,
                 void * param,
                 CHATBool blocking);
CHAT chatConnectSecure(const char * serverAddress,
				 int port,
                 const char * nick,
				 const char * name,
				 const char * gamename,
				 const char * secretKey,
				 chatGlobalCallbacks * callbacks,
				 chatNickErrorCallback nickErrorCallback,
				 chatFillInUserCallback fillInUserCallback,
                 chatConnectCallback connectCallback,
                 void * param,
                 CHATBool blocking);
void chatRetryWithNick(CHAT chat,
					   const char * nick);

void chatDisconnect(CHAT chat);

void chatThink(CHAT chat);

void chatSendRaw(CHAT chat,
				 const char * command);

typedef void (* chatChangeNickCallback)(CHAT chat,
										CHATBool success,
										const char * oldNick,
										const char * newNick,
										void * param);
void chatChangeNick(CHAT chat,
					const char * newNick,
					chatChangeNickCallback callback,
					void * param,
					CHATBool blocking);

char * chatGetNick(CHAT chat);

void chatFixNick(char * newNick,
				 const char * oldNick);

void chatSetQuietMode(CHAT chat,
					  CHATBool quiet);

/*************
** CHANNELS **
*************/
typedef void (* chatEnumChannelsCallbackEach)(CHAT chat,
											  CHATBool success,
											  int index,
											  const char * channel,
											  const char * topic,
											  int numUsers,
											  void * param);
typedef void (* chatEnumChannelsCallbackAll)(CHAT chat,
											 CHATBool success,
											 int numChannels,
											 const char ** channels,
											 const char ** topics,
											 int * numUsers,
											 void * param);
void chatEnumChannels(CHAT chat,
					  const char * filter,
					  chatEnumChannelsCallbackEach callbackEach,
					  chatEnumChannelsCallbackAll callbackAll,
					  void * param,
					  CHATBool blocking);

typedef void (* chatEnumJoinedChannelsCallback)(CHAT chat,
											  int index,
											  const char * channel,
											  void * param);

void chatEnumJoinedChannels(CHAT chat,
					  chatEnumJoinedChannelsCallback callback,
					  void * param);



typedef void (* chatEnterChannelCallback)(CHAT chat,
										  CHATBool success,
										  CHATEnterResult result,
										  const char * channel,
										  void * param);
void chatEnterChannel(CHAT chat,
					  const char * channel,
					  const char * password,
					  chatChannelCallbacks * callbacks,
					  chatEnterChannelCallback callback,
					  void * param,
					  CHATBool blocking);

void chatLeaveChannel(CHAT chat,
					  const char * channel,
					  const char * reason);  // PANTS|03.13.01

void chatSendChannelMessage(CHAT chat,
							const char * channel,
							const char * message,
							int type);

void chatSetChannelTopic(CHAT chat,
						 const char * channel,
						 const char * topic);

typedef void (* chatGetChannelTopicCallback)(CHAT chat,
											 CHATBool success,
											 const char * channel,
											 const char * topic,
											 void * param);
void chatGetChannelTopic(CHAT chat,
						 const char * channel,
						 chatGetChannelTopicCallback callback,
						 void * param,
						 CHATBool blocking);

void chatSetChannelMode(CHAT chat,
						const char * channel,
						CHATChannelMode * mode);

typedef void (* chatGetChannelModeCallback)(CHAT chat,
											CHATBool success,
											const char * channel,
											CHATChannelMode * mode,
											void * param);
void chatGetChannelMode(CHAT chat,
						const char * channel,
						chatGetChannelModeCallback callback,
						void * param,
						CHATBool blocking);

void chatSetChannelPassword(CHAT chat,
							const char * channel,
							CHATBool enable,
							const char * password);

typedef void (* chatGetChannelPasswordCallback)(CHAT chat,
												CHATBool success,
												const char * channel,
												CHATBool enabled,
												const char * password,
												void * param);
void chatGetChannelPassword(CHAT chat,
							const char * channel,
							chatGetChannelPasswordCallback callback,
							void * param,
							CHATBool blocking);

typedef void (* chatEnumChannelBansCallback)(CHAT chat,
											 CHATBool success,
											 const char * channel,
											 int numBans,
											 const char ** bans,
											 void * param);
void chatEnumChannelBans(CHAT chat,
						 const char * channel,
						 chatEnumChannelBansCallback callback,
						 void * param,
						 CHATBool blocking);

void chatAddChannelBan(CHAT chat,
					   const char * channel,
					   const char * ban);

void chatRemoveChannelBan(CHAT chat,
						  const char * channel,
						  const char * ban);

void chatSetChannelGroup(CHAT chat,
						 const char * channel,
						 const char * group);

int chatGetChannelNumUsers(CHAT chat,
						   const char * channel);

/**********
** USERS **
**********/
typedef void (* chatEnumUsersCallback)(CHAT chat,
									   CHATBool success,
									   const char * channel, //PANTS|02.11.00|added paramater
									   int numUsers,
									   const char ** users,
									   int * modes,
									   void * param);
void chatEnumUsers(CHAT chat,
				   const char * channel,
				   chatEnumUsersCallback callback,
				   void * param,
				   CHATBool blocking);

void chatSendUserMessage(CHAT chat,
						 const char * user,
						 const char * message,
						 int type);

typedef void (* chatGetUserInfoCallback)(CHAT chat,
										 CHATBool success,
										 const char * nick,  //PANTS|02.14.2000|added nick and user
										 const char * user,
										 const char * name,
										 const char * address,
										 int numChannels,
										 const char ** channels,
										 void * param);
void chatGetUserInfo(CHAT chat,
					 const char * user,
					 chatGetUserInfoCallback callback,
					 void * param,
					 CHATBool blocking);

typedef void (* chatGetBasicUserInfoCallback)(CHAT chat,
											  CHATBool success,
											  const char * nick,
											  const char * user,
											  const char * address,
											  void * param);

void chatGetBasicUserInfo(CHAT chat,
						  const char * user,
						  chatGetBasicUserInfoCallback callback,
						  void * param,
						  CHATBool blocking);

CHATBool chatGetBasicUserInfoNoWait(CHAT chat,
									const char * nick,
									const char ** user,
									const char ** address);

typedef void (* chatGetChannelBasicUserInfoCallback)(CHAT chat,
													 CHATBool success,
													 const char * channel,
													 const char * nick,
													 const char * user,
													 const char * address,
													 void * param);

void chatGetChannelBasicUserInfo(CHAT chat,
								 const char * channel,
								 chatGetChannelBasicUserInfoCallback callback,
								 void * param,
								 CHATBool blocking);

void chatInviteUser(CHAT chat,
					const char * channel,
					const char * user);

void chatKickUser(CHAT chat,
				  const char * channel,
				  const char * user,
				  const char * reason);

void chatBanUser(CHAT chat,
				 const char * channel,
				 const char * user);

void chatSetUserMode(CHAT chat,
					 const char * channel,
					 const char * user,
					 int mode);

typedef void (* chatGetUserModeCallback)(CHAT chat,
										 CHATBool success,
										 const char * channel,
										 const char * user,
										 int mode,
										 void * param);
void chatGetUserMode(CHAT chat,
					 const char * channel,
					 const char * user,
					 chatGetUserModeCallback callback,
					 void * param,
					 CHATBool blocking);

CHATBool chatGetUserModeNoWait(CHAT chat,
							   const char * channel,
							   const char * user,
							   int * mode);

/*********
** KEYS **
*********/
void chatSetGlobalKeys(CHAT chat,
					   int num,
					   const char ** keys,
					   const char ** values);

typedef void (* chatGetGlobalKeysCallback)(CHAT chat,
										   CHATBool success,
										   const char * user,
										   int num,
										   const char ** keys,
										   const char ** values,
										   void * param);
void chatGetGlobalKeys(CHAT chat,
					   const char * target,
					   int num,
					   const char ** keys,
					   chatGetGlobalKeysCallback callback,
					   void * param,
					   CHATBool blocking);

void chatSetChannelKeys(CHAT chat,
						const char * channel,
						const char * user,
						int num,
						const char ** keys,
						const char ** values);

typedef void (* chatGetChannelKeysCallback)(CHAT chat,
											CHATBool success,
											const char * channel,
											const char * user,
											int num,
											const char ** keys,
											const char ** values,
											void * param);
void chatGetChannelKeys(CHAT chat,
						const char * channel,
						const char * user,
						int num,
						const char ** keys,
						chatGetChannelKeysCallback callback,
						void * param,
						CHATBool blocking);

#ifdef __cplusplus
}
#endif

#endif