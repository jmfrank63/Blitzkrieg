#ifndef __INTERFACEMPCHAT_H__
#define __INTERFACEMPCHAT_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "MultiplayerCommandManager.h"
#include "ListControlWrapper.h"
#include "ChatWrapper.h"
class CInterfaceMPChat : public CInterfaceMultiplayerScreen, public IWhisper
{
	OBJECT_NORMAL_METHODS( CInterfaceMPChat );

	NInput::CCommandRegistrator commandMsgs;
	CListControlWrapper<SUIChatPlayerInfo,std::wstring> playerList;

	CChatWrapper chat;

	CPtr<IUIDialog> pDialog;							// for editting friend info
	CPtr<SUIChatPlayerInfo> pCurEdittedInfo;	//current friend
	NTimer::STime timeLastAwayPressed;
	bool bAwayPressed;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	CInterfaceMPChat() : CInterfaceMultiplayerScreen ( "InterMission" ), timeLastAwayPressed( 0 ), bAwayPressed( false ) { }
	
	void SendTextFromEditBox( const bool bWhisper = false );
	bool ProcessMPCommand( SToUICommand & cmd );
	void UpdateButtons();
	
	void ShowPlayerInfo( SUIChatPlayerInfo * pInfo );
	void OnPlayerInfoOk();
	void AddPlayer( SUIChatPlayerInfo * pInfo );
	virtual bool STDCALL StepLocal( bool bAppActive );
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();

	std::u16string szDestinationName;
	const WORD * GetDestinationName();
};
class CICMultyplayerChat : public CInterfaceCommandBase<CInterfaceMPChat, MISSION_INTERFACE_MULTYPLAYER_CHAT>
{
	OBJECT_NORMAL_METHODS( CICMultyplayerChat );

	virtual void PreCreate( IMainLoop *pML ) 
	{ 
		pML->ResetStack(); 
	}
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceMPChat *pIMM )
	{
		pML->PushInterface( pIMM );
	}
};

#endif // __INTERFACEMPCHAT_H__
