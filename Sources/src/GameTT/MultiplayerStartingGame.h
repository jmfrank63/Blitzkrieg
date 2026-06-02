#ifndef __MULTIPLAYERSTARTINGGAME_H__
#define __MULTIPLAYERSTARTINGGAME_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "MultiplayerCommandManager.h"
#include "ListControlWrapper.h"
#include "ChatWrapper.h"

#include "..\Main\Transceiver.h"
#include "..\StreamIO\ProgressHook.h"
class CMapSettingsWrapper;
class CInterfaceMPStartingGame : public CInterfaceMultiplayerScreen, public IWhisper
{
	OBJECT_NORMAL_METHODS( CInterfaceMPStartingGame );

	CPtr<CMapSettingsWrapper> pMapSettingsWrapper;
	CListControlWrapper<SUIPlayerInfo,int> playerList;
	CChatWrapper chat;
	bool bFirstConfiguration;							// first recieve of configuration parameters.

	CPtr<SUIStagingRoomConfigure>  pConfiguration;	// configuration of this room
	NInput::CCommandRegistrator commandMsgs;
	std::string szMapName;
	bool bStarted;												// game launched

	CInterfaceMPStartingGame() : CInterfaceMultiplayerScreen ( "InterMission" ) { }
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	
	bool ProcessMPCommand( const SToUICommand &cmd );
	void UpdateButtons();
	void AddOrUpdatePlayer( SUIPlayerInfo * pPlayerInfo );
	void ConfigureStagingRoom( SUIStagingRoomConfigure *pInfo );
	void OnNewServerSettings( const SMultiplayerGameSettings &serverSettings, const bool bVisialNotify );

	void PlayerLeft( const SUIPlayerInfo * pInfo );
	void PlayerKicked( const SUIPlayerInfo * pInfo );
	void DeletePlayer( const SUIPlayerInfo * pPlayerInfo );
	
	void AddMessageToChat( const SChatMessage *pChatMessage );

	virtual void STDCALL OnGetFocus( bool bFocus );
	void OnStart( const bool bForced );
	void NotifyOptionsChanged();
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL SetParams( const char * pszParams );
	virtual void STDCALL Done();
	const WORD * GetDestinationName();
};
class CICMultyplayerStartingGame : public CInterfaceCommandBase<CInterfaceMPStartingGame, MISSION_INTERFACE_MULTIPLAYER_STARTINGGAME>
{
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS( CICMultyplayerStartingGame );

	std::string szParams;

	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceMPStartingGame *pIMM )
	{
		pIMM->SetParams( szParams.c_str() );
		pML->PushInterface( pIMM );
	}
public:
	virtual void STDCALL Configure( const char *pszConfig ) 
	{ 
		if( pszConfig )
			szParams = pszConfig;
	}
};
class CICGameSpyClientConnect : public CInterfaceCommandBase<CInterfaceMPStartingGame, MISSION_INTERFACE_MULTIPLAYER_STARTINGGAME>
{
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS( CICGameSpyClientConnect );

	std::string szIPAdress;
	bool bPasswordRequired;
	std::string szPassword;

	virtual void PreCreate( IMainLoop *pML ) 
	{ 
		GetSingleton<IMPToUICommandManager>()->SetConnectionType( EMCT_GAMESPY );
		pML->ResetStack(); 
	}
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceMPStartingGame *pIMM )
	{
		if ( !GetSingleton<ITransceiver>()->JoinToServer( szIPAdress.c_str(), -1, bPasswordRequired, szPassword.c_str() ) )
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
		pIMM->SetParams( "1" );
		pML->PushInterface( pIMM );
	}

public:
	virtual void STDCALL Configure( const char *pszConfig ) 
	{  
		if ( !pszConfig ) return;
		std::vector<std::string> szParams;
		NStr::SplitString( pszConfig, szParams, '"' );

		NI_ASSERT_T( szParams.size() > 0, "Wrong gamespy commandline params" );

		szIPAdress = szParams[0];
	
		if ( szParams.size() > 1 )
		{
			bPasswordRequired = true;
			szPassword = szParams[1];
		}
		else
		{
			bPasswordRequired = false;
			szPassword = "";
		}

		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CHANGE_TRANSCEIVER, NStr::Format("%d %d", MAIN_MP_TRANSCEIVER, EMCT_GAMESPY ) );
	}
};
#endif // __MULTIPLAYERSTARTINGGAME_H__
