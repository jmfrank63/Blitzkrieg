#ifndef __LOAD_MISSION_H__
#define __LOAD_MISSION_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
class CInterfaceLoadMission : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceLoadMission );
	std::vector<std::string> szSaves;
	NInput::CCommandRegistrator loadmissionMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual void STDCALL DrawAdd();
	virtual ~CInterfaceLoadMission();
protected:
	CInterfaceLoadMission() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICLoadMission : public CInterfaceCommandBase<CInterfaceLoadMission, MISSION_INTERFACE_LOAD_MISSION>
{
	OBJECT_NORMAL_METHODS( CICLoadMission );

	virtual void PostCreate( IMainLoop *pML, CInterfaceLoadMission *pILM );
	CICLoadMission() {  }
public:
};
#endif // __LOAD_MISSION_H__
