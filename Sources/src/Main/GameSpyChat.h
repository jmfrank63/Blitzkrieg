#ifndef __GAME_SPY_CHAT_H__
#define __GAME_SPY_CHAT_H__
#pragma ONCE


class CGameSpyChat : public IChat
{
	OBJECT_NORMAL_METHODS( CGameSpyChat );

	CPtr<IChat> pInGameChat;

public:
	CGameSpyChat();
	virtual ~CGameSpyChat();

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
#endif // __GAME_SPY_CHAT_H__
