#include "StdAfx.h"

#include "ChatWrapper.h"
#include "MultiplayerCommandManager.h"
#include "..\UI\UIMessages.h"

void CChatWrapper::AddMessageToChat( const struct SChatMessage *pChatMessage )
{
	std::wstring szName = pChatMessage->szPlayerName;
	if ( pChatMessage->bWhisper )
	{
		IText * pText = GetSingleton<ITextManager>()->GetDialog( "Textes\\UI\\Intermission\\Multiplayer\\chat_whisper" );	
		if ( pText )
			szName += MakeWideStringFromWordString( pText->GetString() );
	}
	szName += L":";
	pChatText->AppendMessage( reinterpret_cast<const WORD*>( szName.c_str() ), reinterpret_cast<const WORD*>( pChatMessage->szMessageText.c_str() ) );
}
void CChatWrapper::AddEditBoxText( const bool bWhisper )
{
	std::wstring wszTextTmp = MakeWideStringFromWordString( pChatEdit->GetWindowText( 0 ) );

	if ( !wszTextTmp.empty() )
	{
		const unsigned int nPos = wszTextTmp.find_last_not_of( L" " );
		wszTextTmp.resize( Min(wszTextTmp.size(), static_cast<size_t>(nPos) + 1 ) );
		
		const unsigned int nFirstNotSpace = wszTextTmp.find_first_not_of( L" " );
		
		std::wstring wszText = wszTextTmp.c_str() + Min(static_cast<size_t>(nFirstNotSpace),wszTextTmp.size());
		
		if ( !wszText.empty() )
		{
			CPtr<SChatMessage> pChatMessage = new SChatMessage( 
				reinterpret_cast<const WORD*>( wszText.c_str() ), ( bWhisper ? pWhisper->GetDestinationName(): reinterpret_cast<const WORD*>(L"") ), bWhisper );
			pCommandManager->AddChatMessageFromUI( pChatMessage );
			pChatEdit->SetWindowText( 0, reinterpret_cast<const WORD*>( L"" ) );
		}
	}
}		
void CChatWrapper::ClearEditBoxText()
{
	pChatEdit->SetWindowText( 0, reinterpret_cast<const WORD*>( L"" ) );
	pChatEdit->SetFocus( true );
}
void CChatWrapper::Init( IUIColorTextScroll * _pChatText,
							IUIEditBox * _pChatEdit,
							const int _nWhisperButton,
							IWhisper * _pWhisper )
{
	bEmptyChat = true;
	pChatEdit = _pChatEdit;
	pChatText = _pChatText;
	pCommandManager = GetSingleton<IMPToUICommandManager>();
	nWhisperButton = _nWhisperButton;
	pWhisper =_pWhisper;
}
void CChatWrapper::AddImportantText( const WORD *wszMessage )
{
	pChatText->AppendMessage( 0, wszMessage, IUIColorTextScroll::E_COLOR_IMPORTANT );
}
bool CChatWrapper::ProcessMessage( const SGameMessage &msg )
{
	if ( msg.nEventID == nWhisperButton )
	{
		AddEditBoxText( true );
		ClearEditBoxText();
	}
	else
	{
		switch( msg.nEventID )
		{
		case UI_NOTIFY_EDIT_BOX_RETURN:
			AddEditBoxText( false );
			ClearEditBoxText();
			pChatEdit->SetFocus( true );

			return true;
		case UI_NOTIFY_EDIT_BOX_ESCAPE:
			if ( MakeWideStringFromWordString( pChatEdit->GetWindowText( 0 ) ).empty() )
				return false;

			ClearEditBoxText();
			return true;
		}
	}
	return false;
}
