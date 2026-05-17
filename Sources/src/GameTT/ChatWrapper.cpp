#include "StdAfx.h"

#include "ChatWrapper.h"
#include "MultiplayerCommandManager.h"
#include "..\UI\UIMessages.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CChatWrapper::AddMessageToChat( const struct SChatMessage *pChatMessage )
{
	// with default text color
	std::wstring szName = pChatMessage->szPlayerName;
	if ( pChatMessage->bWhisper )
	{
		IText * pText = GetSingleton<ITextManager>()->GetDialog( "Textes\\UI\\Intermission\\Multiplayer\\chat_whisper" );	
		if ( pText )
			szName += pText->GetString();
	}
	szName += L":";
	pChatText->AppendMessage( szName.c_str(), pChatMessage->szMessageText.c_str() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CChatWrapper::AddEditBoxText( const bool bWhisper )
{
	std::wstring wszTextTmp = pChatEdit->GetWindowText( 0 );

	if ( !wszTextTmp.empty() )
	{
		const unsigned int nPos = wszTextTmp.find_last_not_of( L" " );
		wszTextTmp.resize( Min(wszTextTmp.size(), nPos + 1 ) );
		
		const unsigned int nFirstNotSpace = wszTextTmp.find_first_not_of( L" " );
		
		std::wstring wszText = wszTextTmp.c_str() + Min(nFirstNotSpace,wszTextTmp.size());
		
		if ( !wszText.empty() )
		{
			// add editbox text to chat
			CPtr<SChatMessage> pChatMessage = new SChatMessage( 
				wszText.c_str(), ( bWhisper ? pWhisper->GetDestinationName(): reinterpret_cast<const WORD*>(L"") ), bWhisper );
			pCommandManager->AddChatMessageFromUI( pChatMessage );
			pChatEdit->SetWindowText( 0, L"" );
		}
	}
}		
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CChatWrapper::ClearEditBoxText()
{
		// clear editbox text
	pChatEdit->SetWindowText( 0, L"" );
	pChatEdit->SetFocus( true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CChatWrapper::AddImportantText( const WORD *wszMessage )
{
	pChatText->AppendMessage( 0, wszMessage, IUIColorTextScroll::E_COLOR_IMPORTANT );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
			// if text is empty, do not process this message.
			if ( 0 == wcslen( pChatEdit->GetWindowText( 0 ) ) )
				return false;

			ClearEditBoxText();
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
