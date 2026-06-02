
#if !defined(AFX_GAMESLIST_H__17B9FF2C_3D75_462D_8FAD_C68FAA9A9191__INCLUDED_)
#define AFX_GAMESLIST_H__17B9FF2C_3D75_462D_8FAD_C68FAA9A9191__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InterMission.h"
#include "iMission.h"
#include "MultiplayerCommandManager.h"
#include "ListControlWrapper.h"
#include "OptionEntryWrapper.h"
class CInterfaceMPGamesList : public CInterfaceMultiplayerScreen
{
	OBJECT_NORMAL_METHODS( CInterfaceMPGamesList );

	NInput::CCommandRegistrator commandMsgs;
	CListControlWrapper<SUIServerInfo,int> serversList;
	CPtr<IUIDialog> pDialog;							// for editting SETTINGS
	CPtr<IUIDialog> pDialogError;					// for error dialog
	CPtr<IUIDialog> pDialogEnterPassword;
	CPtr<IUIDialog> pDialogWaitForConnection;
	
	CPtr<COptionsListWrapper> pOptions;

	CInterfaceMPGamesList();
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );

	void EnableButtonsByServerInfo( const SUIServerInfo *pInfo = 0 );
	void EnableButton( const int nButtonID, bool bEnable );

	void UpdateServerInfo( SUIServerInfo * pServerInfo );
	void DeleteServer( SUIServerInfo * pServerInfo );
	void PrepareMapsList();
	bool ProcessMPCommand( const SToUICommand &cmd );
	
	void ShowErrorMessage( const EMultiplayerToUICommands eCmdId );
	
	void ShowLocalSettings();
	void OnLocalSettingsOK();
	void AskPassword();

	void TryJoin();
	void ShowServerInfo( SUIServerInfo * pServerInfo );
public:
	virtual void STDCALL Done();
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	void Configure( const WORD wServerID );
};
class CICMultyplayerGamesList: public CInterfaceCommandBase<CInterfaceMPGamesList, MISSION_INTERFACE_MULTIPLAYER_GAMESLIST>
{
	OBJECT_NORMAL_METHODS( CICMultyplayerGamesList );

	bool bAutoConnect;
	WORD wServerID;												// auto connect to server

	virtual void PreCreate( IMainLoop *pML ) 
		{ pML->ResetStack(); }
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceMPGamesList *pIMM )
	{
		if ( bAutoConnect )
			pIMM->Configure( wServerID );

		pML->PushInterface( pIMM );
	}
public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( pszConfig )
		{
			bAutoConnect = true;
			wServerID = NStr::ToInt( pszConfig );
		}
		else
			bAutoConnect = false;
	}
};
#endif // !defined(AFX_GAMESLIST_H__17B9FF2C_3D75_462D_8FAD_C68FAA9A9191__INCLUDED_)
