#ifndef __PEER_CHAT_H__
#define __PEER_CHAT_H__
#pragma ONCE
#include "GameCreationInterfaces.h"
#include "MessagesStore.h"

class CGameSpyPeerChat : public IChat
{
	OBJECT_NORMAL_METHODS( CGameSpyPeerChat );

	CPtr<IChat> pInGameChat;

public:
	CGameSpyPeerChat();
	virtual ~CGameSpyPeerChat();

	virtual void STDCALL InitGSChat( const WORD *pszUserName );
	virtual void STDCALL InitInGameChat( INetDriver *pNetDriver );
	virtual void STDCALL DestroyInGameChat();
	
	virtual void STDCALL SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper );

	virtual void STDCALL Segment();
	
	virtual interface IMultiplayerMessage* STDCALL GetMessage();

	virtual void STDCALL UserModeChanged( const EUserMode eMode );
};
#endif // __PEER_CHAT_H__
