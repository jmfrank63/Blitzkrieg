#ifndef __QUIT_MISSION_H__
#define __QUIT_MISSION_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
class CInterfaceQuitMission : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceQuitMission );
	NInput::CCommandRegistrator quitmissionMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual void STDCALL DrawAdd();
	virtual ~CInterfaceQuitMission();
protected:
	CInterfaceQuitMission() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICQuitMission : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_QUIT_MISSION>
{
	OBJECT_NORMAL_METHODS( CICQuitMission );

	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICQuitMission() {  }
};
#endif // __QUIT_MISSION_H__
