#ifndef __UI_STATE_H__
#define __UI_STATE_H__

class CInterfaceMainMenu;
interface IUIState
{
private:
	int nMenuID;
	std::string szTitleKey;
protected:
	CInterfaceMainMenu *pMainInterface;

public:
	IUIState( int nID, const char *pszTitleKey ) : pMainInterface( 0 ), nMenuID( nID ), szTitleKey( pszTitleKey ) {}
	virtual ~IUIState() {}
	void Init( CInterfaceMainMenu *pMainMenu ) { pMainInterface = pMainMenu; }
	CInterfaceMainMenu *GetMainInterface() { return pMainInterface; }

	virtual bool ProcessMessage( const SGameMessage &msg ) = 0;
	virtual void Show();
	virtual void Hide();
};
enum ECommands
{
	IMC_MULTIPLAYER			=	10003,
	IMC_LOAD_GAME				=	10004,
	IMC_SETTINGS				=	10005,
	IMC_EXIT_GAME				= 10006,
	IMC_SHOW_EXIT_GAME	= 8888,
	
	IMC_TUTORIAL				= 10003,
	IMC_SINGLE_MISSION	=	10004,
	IMC_PLAYER_PROFILE	=	10005,
	IMC_SCENARIO				= 10002,

	IMC_GERMAN_CAMPAIGN	= 10004,
	IMC_RUSSIAN_CAMPAIGN= 10003,
	IMC_ALLIES_CAMPAIGN	=	10002,
	
	IMC_OPTIONS					=	10003,
	IMC_VIDEO						= 10004,
	IMC_CREDITS					=	10005,
	IMC_MODS						= 10007,

	IMC_LAN							= 10003,
	IMC_INTERNET				= 10002,
	IMC_GAMESPY					= 10004,
	
	IMC_CUSTOM_CAMPAIGNS	= 10003,
	IMC_CUSTOM_CHAPTERS		= 10004,
	IMC_CUSTOM_MISSIONS		= 10002,

	IMC_LOAD_LOAD_GAME		=	10002,
	IMC_LOAD_LOAD_REPLAY	= 10003,

	IMC_DEMO_NEW_GAME			= 10002,
	IMC_DEMO_OPTIONS			= 10005,
	IMC_DEMO_LOAD_GAME		= 10004,
	IMC_DEMO_EXIT_GAME		= 10006,
};
class CUIMainMenuState : public IUIState
{
public:
	CUIMainMenuState() : IUIState( 2000, "imheader-mainmenu-header" ) {}

	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUINewGameState : public IUIState
{
public:
	CUINewGameState() : IUIState( 2001, "imheader-newgame-header" ) {}
	virtual void Show();
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUISelectCampaignState : public IUIState
{
public:
	CUISelectCampaignState() : IUIState( 2002, "imheader-selectcampaign-header" ) {}

	virtual void Show();
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUIOptionsState : public IUIState
{
public:
	CUIOptionsState() : IUIState( 2003, "imheader-options-header" ) {}

	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUIMultiplayerState : public IUIState
{
public:
	CUIMultiplayerState () : IUIState( 2004, "Textes\\UI\\Intermission\\MainMenu\\Multiplayer\\caption" ) {  }

	virtual void Show();
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUICustomGameState : public IUIState
{
public:
	CUICustomGameState () : IUIState( 2005, "Textes\\UI\\Intermission\\MainMenu\\CustomGame\\caption" ) {  }
	
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUILoadGameState : public IUIState
{
public:
	CUILoadGameState () : IUIState( 2006, "Textes\\UI\\Intermission\\MainMenu\\LoadGame\\caption" ) {  }
	
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUICreditsState : public IUIState
{
	bool bLeaveToMainMenu;
public:
	CUICreditsState () : IUIState( 2007, "imheader-gbutton-credits" ), bLeaveToMainMenu( false ) {  }
	
	virtual void Show();
	virtual void Hide();
	virtual bool ProcessMessage( const SGameMessage &msg );
};
class CUIDemoMainMenuState : public IUIState
{
public:
	CUIDemoMainMenuState() : IUIState( 2008, "imheader-mainmenu-header" ) {}

	virtual bool ProcessMessage( const SGameMessage &msg );
};
#endif		//__UI_STATE_H__
