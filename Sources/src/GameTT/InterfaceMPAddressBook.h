#ifndef __INTERFACEMPADDRESSBOOK_H__
#define __INTERFACEMPADDRESSBOOK_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Common\InterfaceScreenBase.h"
#include "..\Input\InputHelper.h"
#include "iMission.h"
#include "MapSettingsWrapper.h"
#include "InterMission.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterfaceMPAddressBook  : public CInterfaceMultiplayerScreen
{
	OBJECT_NORMAL_METHODS( CInterfaceMPAddressBook );
	//
	NInput::CCommandRegistrator msgs;
	typedef std::unordered_set<std::string> CServersList;
	
	CServersList szServers;
	bool bChanged;

	IUIDialog *pDialogAskAddress;
	IUIDialog *pDialogWaitForConnection;
	//
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceMPAddressBook() {}
	bool ProcessMPCommand( const SToUICommand &cmd );

	void CheckEnableButtons();
	std::string GetServer();
	void AddServer( const std::string &szServer );
	void AddServerInternal( const std::string &szServer );
	void DeleteServer();
	
	void Serialize( const bool bRead );
	void SaveServersList();
	void InitServersList();

protected:
	CInterfaceMPAddressBook() : CInterfaceMultiplayerScreen( "InterMission" ), pDialogAskAddress( 0 ), pDialogWaitForConnection( 0 ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	virtual void STDCALL StartInterface();
	virtual void STDCALL OnGetFocus( bool bFocus );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CICMPAddressBook : public CInterfaceCommandBase<CInterfaceMPAddressBook, MISSION_INTERFACE_ADDRESS_BOOK>
{
	OBJECT_NORMAL_METHODS( CICMPAddressBook );
	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	virtual void PostCreate( IMainLoop *pML, CInterfaceMPAddressBook *pInterface ) 
	{ 
		pML->PushInterface( pInterface ); 
	}
	//
	CICMPAddressBook() {  }

public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __INTERFACEMPADDRESSBOOK_H__
