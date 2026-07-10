#include "stdafx.h"

#include "GameSpyChat.h"
#include "LanChat.h"

CGameSpyChat::CGameSpyChat()
{
}

CGameSpyChat::~CGameSpyChat()
{
	DestroyInGameChat();
}

void CGameSpyChat::InitGSChat( const WORD *pszUserName )
{
	GetSingleton<IConsoleBuffer>()->WriteASCII(
		CONSOLE_STREAM_CONSOLE,
		"Open internet chat backend is not implemented yet.",
		0xffffff00,
		true
	);
}

void CGameSpyChat::InitInGameChat( INetDriver *pNetDriver )
{
	pInGameChat = new CLanChat();
	static_cast_ptr<CLanChat*>(pInGameChat)->InitInGameChat( pNetDriver );
}

void CGameSpyChat::DestroyInGameChat()
{
	pInGameChat = 0;
}

void CGameSpyChat::SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer )
{
	if ( pInGameChat )
		pInGameChat->SendMessage( pszMessage, ourPlayer );
}

void CGameSpyChat::SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer )
{
	if ( pInGameChat )
		pInGameChat->SendWhisperMessage( pszMessage, toPlayer, ourPlayer );
}

void CGameSpyChat::SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper )
{
	if ( pInGameChat )
		pInGameChat->SendMessage( pszMessage, wszToPlayer, bWhisper );
}

void CGameSpyChat::Segment()
{
	if ( pInGameChat )
		pInGameChat->Segment();
}

IMultiplayerMessage* CGameSpyChat::GetMessage()
{
	return pInGameChat ? pInGameChat->GetMessage() : 0;
}

void CGameSpyChat::UserModeChanged( const EUserMode eMode )
{
	if ( pInGameChat )
		pInGameChat->UserModeChanged( eMode );
}
