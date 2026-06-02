#ifndef __LAN_CHAT_H__
#define __LAN_CHAT_H__
#pragma ONCE
#include "GameCreationInterfaces.h"
#include "MessagesStore.h"
interface INetDriver;
class CLanChat : public IChat
{
	OBJECT_COMPLETE_METHODS( CLanChat );

	CPtr<INetDriver> pNetDriver;
	CMessagesStore messages;
public:
	virtual void STDCALL InitGSChat( const WORD *pszUserName ) { }
	virtual void STDCALL InitInGameChat( INetDriver *pNetDriver );
	virtual void STDCALL DestroyInGameChat() { }

	virtual void STDCALL SendMessage( const WORD *pszMessage, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendWhisperMessage( const WORD *pszMessage, const SPlayerInfo &toPlayer, const SPlayerInfo &ourPlayer );
	virtual void STDCALL SendMessage( const WORD *pszMessage, const WORD *wszToPlayer, const bool bWhisper ) { }
	virtual void STDCALL Segment();

	virtual IMultiplayerMessage* STDCALL GetMessage();
	
	virtual void STDCALL UserModeChanged( const EUserMode eMode ) { };
};
#endif // __LAN_CHAT_H__
