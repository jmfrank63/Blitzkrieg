#ifndef __IM_TUTORIAL_H__
#define __IM_TUTORIAL_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceIMTutorial : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceIMTutorial );
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceIMTutorial();
	CInterfaceIMTutorial();
	
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICIMTutorial : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_IM_TUTORIAL>
{
	OBJECT_NORMAL_METHODS( CICIMTutorial );
	
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICIMTutorial() {  }
};
#endif		//__IM_TUTORIAL_H__
