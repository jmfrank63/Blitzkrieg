#ifndef __MISSION_H__
#define __MISSION_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
struct SMissionStats;
class CInterfaceAboutMission : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceAboutMission );
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceAboutMission();
	CInterfaceAboutMission() : CInterfaceInterMission( "InterMission" ), m_nActiveObjective( -1 ) {  }
	const SMissionStats *ReadMissionStats();
	void UpdateActiveObjectiveFlag( bool bShow );
	void ShowActiveObjective( bool bShow );

	int m_nActiveObjective;
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICAboutMission : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_ABOUT_MISSION>
{
	OBJECT_NORMAL_METHODS( CICAboutMission );

	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICAboutMission() {  }
};
#endif		//__MISSION_H__
