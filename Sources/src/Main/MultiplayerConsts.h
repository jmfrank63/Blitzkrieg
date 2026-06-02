#ifndef __MULTIPLAYER_CONSTS_H__
#define __MULTIPLAYER_CONSTS_H__
#pragma ONCE
struct SMultiplayerConsts
{
	enum
	{
		NET_PORT = 8889,
		GS_NET_PORT = 9089,
		TIME_TO_ASK_PLAYER_INFO = 1000,
		TIME_TO_LAG_PLAYER = 3000,
	};
};
#endif // __MULTIPLAYER_CONSTS_H__
