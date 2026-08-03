#include "StdAfx.h"

#include "LanChat.h"
#include "NetMessages.h"
#include "ChatMessages.h"

#include "..\Net\NetDriver.h"

#include "..\StreamIO\StreamIOHelper.h"
#include "..\StreamIO\StreamIOTypes.h"
void CLanChat::InitInGameChat( INetDriver *_pNetDriver )
{
	pNetDriver = _pNetDriver;
	std::unordered_set<BYTE> messages;
	messages.insert( BYTE( NGM_CHAT_MESSAGE ) );

	pNetDriver->AddChannel( 1, messages );
}
void CLanChat::SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer )
{
	BYTE msgID = NGM_CHAT_MESSAGE;
	std::wstring szMessage = MakeWideStringFromWordString( pszMessage );

	CStreamAccessor info = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );	
	info << msgID << szMessage << ourPlayer.szName;

	pNetDriver->SendBroadcast( info );

	if ( ourPlayer.fPing != -1.0f && ourPlayer.nSide != -1 )
		messages.AddMessage( new CChatMessage( pszMessage, ToWordString( ourPlayer.szName ), false ) );
}
void CLanChat::SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer )
{
	BYTE msgID = NGM_CHAT_MESSAGE;
	std::wstring szMessage = MakeWideStringFromWordString( pszMessage );
	
	if ( toPlayer.nClientID != -1 )
	{
		CStreamAccessor info = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );	
		info << msgID << szMessage << ourPlayer.szName;

		pNetDriver->SendDirect( toPlayer.nClientID, info );
	}

	if ( ourPlayer.fPing != -1.0f && ourPlayer.nSide != -1 )	
		messages.AddMessage( new CChatMessage( pszMessage, ToWordString( ourPlayer.szName ), true ) );
}
void CLanChat::Segment()
{
	INetDriver::EMessage eMsgID;
	int nClientID;
	int received[128];
	CStreamAccessor pkt = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );

	while ( pNetDriver->GetChannelMessage( &eMsgID, &nClientID, received, pkt, 1 ) )
	{
		BYTE msg;
		pkt >> msg;
		NI_ASSERT_T( msg == NGM_CHAT_MESSAGE, "Wrong message type in chat" );

		std::wstring szMessage;
		pkt >> szMessage;

		std::wstring szPlayer;
		pkt >> szPlayer;

		switch ( eMsgID )
		{
			case INetDriver::DIRECT:
				messages.AddMessage( new CChatMessage( ToWordString( szMessage ), ToWordString( szPlayer ), true ) );
				break;
			case INetDriver::BROADCAST:
				messages.AddMessage( new CChatMessage( ToWordString( szMessage ), ToWordString( szPlayer ), false ) );
				break;
		}
	}
}
IMultiplayerMessage* CLanChat::GetMessage()
{
	return messages.GetMessage();
}
