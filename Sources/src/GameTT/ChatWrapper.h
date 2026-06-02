#ifndef __CHATMANAGER_H__
#define __CHATMANAGER_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
interface IWhisper
{
	virtual const WORD * GetDestinationName() = 0;
};
class CChatWrapper
{

	CPtr<IUIColorTextScroll> pChatText;
	CPtr<IUIEditBox> pChatEdit;
	CPtr<IMPToUICommandManager> pCommandManager;
	int nWhisperButton;
	IWhisper * pWhisper;

	bool bEmptyChat;

	void AddEditBoxText( const bool bWhisper );
	void ClearEditBoxText();

public:
	void Init( IUIColorTextScroll * _pChatText,
								IUIEditBox * _pChatEdit,
								const int _nWhisperButton,
								IWhisper * _pWhisper );

	void AddImportantText( const WORD * wszMessage );
	bool ProcessMessage( const SGameMessage &msg );
	void AddMessageToChat( const struct SChatMessage *pChatMessage );
};
#endif // __CHATMANAGER_H__
