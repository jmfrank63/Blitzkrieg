#include "stdafx.h"

#include "GameSpyPeerChat.h"
#include "LanChat.h"

CGameSpyPeerChat::CGameSpyPeerChat()
{
}

CGameSpyPeerChat::~CGameSpyPeerChat()
{
	DestroyInGameChat();
}

void CGameSpyPeerChat::InitGSChat( const WORD *pszUserName )
{
	GetSingleton<IConsoleBuffer>()->WriteASCII(
		CONSOLE_STREAM_CONSOLE,
		"Open internet lobby backend is not implemented yet.",
		0xffffff00,
		true
	);
}

void CGameSpyPeerChat::InitInGameChat( INetDriver *pNetDriver )
{
	pInGameChat = new CLanChat();
	static_cast_ptr<CLanChat*>(pInGameChat)->InitInGameChat( pNetDriver );
}

void CGameSpyPeerChat::DestroyInGameChat()
{
	pInGameChat = 0;
}

void CGameSpyPeerChat::SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer )
{
	if ( pInGameChat )
		pInGameChat->SendMessage( pszMessage, ourPlayer );
}

void CGameSpyPeerChat::SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer )
{
	if ( pInGameChat )
		pInGameChat->SendWhisperMessage( pszMessage, toPlayer, ourPlayer );
}

void CGameSpyPeerChat::SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper )
{
	if ( pInGameChat )
		pInGameChat->SendMessage( pszMessage, wszToPlayer, bWhisper );
}

void CGameSpyPeerChat::Segment()
{
	if ( pInGameChat )
		pInGameChat->Segment();
}

IMultiplayerMessage* CGameSpyPeerChat::GetMessage()
{
	return pInGameChat ? pInGameChat->GetMessage() : 0;
}

void CGameSpyPeerChat::UserModeChanged( const EUserMode eMode )
{
	if ( pInGameChat )
		pInGameChat->UserModeChanged( eMode );
}
