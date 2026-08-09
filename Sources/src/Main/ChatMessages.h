#ifndef __CHAT_MESSAGES_H__
#define __CHAT_MESSAGES_H__
#pragma ONCE
#include "Messages.h"
#include "GameCreationInterfaces.h"
#include "../Platform/LegacyText.h"
class CChatMessage : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CChatMessage );
public:
	std::wstring szPlayerName;
	std::wstring szMessage;
	bool bWhisper;

	CChatMessage() { }
	CChatMessage( const WORD *pszMessage, const WORD *pszPlayerName, bool _bWhisper )
		: szPlayerName( MakeWideStringFromWordString( pszPlayerName ) ), szMessage( MakeWideStringFromWordString( pszMessage ) ), bWhisper( _bWhisper ) { }
	CChatMessage( const char *pszMessage, const char *pszPlayerName, bool _bWhisper );
	
	virtual const EMultiplayerMessages GetMessageID() const { return E_CHAT_MESSAGE; }
	virtual void SendToUI();

	// The name is stored wide, and wchar_t is 32 bits off Windows, so it has to
	// be converted rather than reinterpreted. The cache gives the caller a
	// pointer that outlives the call.
	const WORD* GetPlayerNick() const
	{
		szPlayerNickUtf16 = NPlatform::WordStringFromWide( szPlayerName );
		return NPlatform::WordStringData( szPlayerNickUtf16 );
	}
private:
	mutable std::u16string szPlayerNickUtf16;
};
class CSimpleChatMessage : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CSimpleChatMessage );
public:
	enum EParams { EP_NONE, EP_FAILED_TO_CONNECT, EP_DISCONNECTED, EP_KICKED };
private:
	EParams eParam;
public:
	CSimpleChatMessage() : eParam( EP_NONE ) { }
	explicit CSimpleChatMessage( const EParams &_eParam ) : eParam( _eParam ) { }

	virtual const EMultiplayerMessages GetMessageID() const { return SIMPLE_CHAT_MESSAGE; }
	virtual void SendToUI();
};
class CChatUserChanged : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CChatUserChanged );
public:
	enum EUserState { EUS_NONE, EUS_JOINED, EUS_PARTED, EUS_MODE };
private:
	EUserState eState;

	std::wstring wszUserNick;
	IChat::EUserMode eMode;
public:
	CChatUserChanged() : wszUserNick( L"" ), eState( EUS_NONE ) { }
	CChatUserChanged( const EUserState &_eState, const char *pszUserNick, const IChat::EUserMode &_eMode );

	virtual const EMultiplayerMessages GetMessageID() const { return CHAT_USER_CHANGED; }
	virtual void SendToUI();
};
class CChatUserChangedNick : public IMultiplayerMessage
{
	OBJECT_COMPLETE_METHODS( CChatUserChangedNick );

	std::wstring wszOldNick, wszNewNick;
public:
	CChatUserChangedNick() : wszNewNick( L"" ) { }
	CChatUserChangedNick( const std::wstring _wszOldNick, const std::wstring _wszNewNick )
		: wszOldNick( _wszOldNick ), wszNewNick( _wszNewNick ) { }
	CChatUserChangedNick( const char *pszOldNick, const char *pszNewNick );

	virtual const EMultiplayerMessages GetMessageID() const { return CHAT_USER_CHANGED_NICK; }
	virtual void SendToUI();
};
#endif // __CHAT_MESSAGES__
