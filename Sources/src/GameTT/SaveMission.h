#ifndef __SAVE_MISSION_H__
#define __SAVE_MISSION_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
class CInterfaceSaveMission : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceSaveMission );
	std::vector<std::string> szSaves;
	std::string szProspecitveSave;
	NInput::CCommandRegistrator savemissionMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual void STDCALL DrawAdd();
	virtual ~CInterfaceSaveMission();

	void OnSave();
protected:
	CInterfaceSaveMission() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL OnGetFocus( bool bFocus );

	void Configure( const int nMode );
};
class CICSaveMission : public CInterfaceCommandBase<CInterfaceSaveMission, MISSION_INTERFACE_SAVE_MISSION>
{
	OBJECT_NORMAL_METHODS( CICSaveMission );

	int nType;

	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfaceSaveMission *pInterface ) 
	{ 
		pInterface->Configure( nType );
		pML->PushInterface( pInterface ); 
	}
	CICSaveMission() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( !pszConfig ) 
			nType = 0;
		else
			nType = NStr::ToInt( pszConfig );
	}
};
#endif // __SAVE_MISSION_H__
