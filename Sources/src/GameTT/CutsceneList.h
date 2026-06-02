#ifndef __IM_CUTSCENE_LIST_H__
#define __IM_CUTSCENE_LIST_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "InterMission.h"
#include "iMission.h"
class CInterfaceCutsceneList : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceCutsceneList );
	NInput::CCommandRegistrator commandMsgs;
	std::vector<std::string> cutscenesList;		//список видеофайлов
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceCutsceneList();
	CInterfaceCutsceneList() : CInterfaceInterMission( "InterMission" ) {  }
protected:

public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CCutsceneList : public CInterfaceCommandBase<CInterfaceCutsceneList, MISSION_INTERFACE_CUTSCENE_LIST>
{
	OBJECT_NORMAL_METHODS( CCutsceneList );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceCutsceneList *pILM );
	CCutsceneList() {}
};
#endif // __IM_CUTSCENE_LIST_H__
