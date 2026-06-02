/*
GameSpy Ping SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com
*/

#ifndef _PINGER_H_
#define _PINGER_H_

#ifdef __cplusplus
extern "C" {
#endif

/************
** DEFINES **
************/
#ifndef PINGER_UDP_PING_SIZE
#define PINGER_UDP_PING_SIZE     32
#endif

#define PINGER_TIMEOUT           -1

/**********
** TYPES **
**********/
typedef enum { PINGERFalse, PINGERTrue } PINGERBool;

/**************
** CALLBACKS **
**************/
typedef void (* pingerGotPing)(unsigned int IP,
							   unsigned short port,
							   int ping,
							   const char * data,
							   int len,
							   void * param);

typedef void (* pingerSetData)(unsigned int IP,
							   unsigned short port,
							   char * data,
							   int len,
							   void * param);

/**************
** FUNCTIONS **
**************/

PINGERBool pingerInit(const char * localAddress,
					  unsigned short localPort,
					  pingerGotPing pinged,
					  void * pingedParam,
					  pingerSetData setData,
					  void * setDataParam);

void pingerShutdown(void);

void pingerThink(void);

void pingerPing(unsigned int IP,
				unsigned short port,
				pingerGotPing reply,
				void * replyParam,
				PINGERBool blocking,
				unsigned long timeout);
	
#ifdef __cplusplus
}
#endif

#endif