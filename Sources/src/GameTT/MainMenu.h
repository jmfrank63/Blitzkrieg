#ifndef __SELECT_MAIN_MENU_H__
#define __SELECT_MAIN_MENU_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "UIState.h"
class CInterfaceMainMenu : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceMainMenu );
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual ~CInterfaceMainMenu();
	CInterfaceMainMenu();

	// The cloud sync indicator (element 21001): what it currently shows,
	// the state seen last frame (a settled->running edge means a new run),
	// and whether the player clicked skip while a sync was still running.
	std::string szCloudShownKey;
	int nCloudLastState;
	bool bCloudSkipRequested;
	void RefreshCloudIndicator();

	CUIMainMenuState mainMenuState;
	CUINewGameState newGameState;
	CUISelectCampaignState selectCampaignState;
	CUIOptionsState optionsState;
	CUIMultiplayerState multiplayerState;
	CUICustomGameState customGameState;
	CUILoadGameState loadGameState;
	CUICreditsState creditsState;
	CUIDemoMainMenuState demoMainMenu;
	int nActiveState;
	std::vector<IUIState *> states;
public:
	virtual bool STDCALL Init();
	static void PlayIntermissionSound();

	virtual void STDCALL OnGetFocus( bool bFocus );

	enum EUIState
	{
		E_MAIN_MENU													= 0,
		E_NEW_GAME													= 1,
		E_SELECT_CAMPAIGN										= 2,
		E_OPTIONS														= 3,
		E_MULTIPLAYER												= 4,
		E_CUSTOM_GAME												= 5,
		E_LOAD_GAME													= 6,
		E_CREDITS														= 7,
		E_DEMOVERSION_MAIN_MENU							= 8,
	};

	void SetActiveState( int nState );
	IUIScreen *GetUIScreen() { return pUIScreen; }
	void Create( int nState );
	void RefreshCursor();
};
class CICMainMenu : public CInterfaceCommandBase<CInterfaceMainMenu, MISSION_INTERFACE_MAIN_MENU>
{
	OBJECT_NORMAL_METHODS( CICMainMenu );
	DECLARE_SERIALIZE;
	int nState;
	int nNextIC;
	std::string szNextICConfig;

	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	virtual void PostCreate( IMainLoop *pML, CInterfaceMainMenu *pIMM );
	CICMainMenu() : nState( 0 ), nNextIC( -1 ) {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif		//__SELECT_MAIN_MENU_H__
