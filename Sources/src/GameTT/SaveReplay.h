#ifndef __SAVE_REPLAY_H__
#define __SAVE_REPLAY_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceSaveReplay : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceSaveReplay );
	NInput::CCommandRegistrator commandMsgs;
	std::string szSaveReplayFile;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceSaveReplay();
	CInterfaceSaveReplay() : CInterfaceInterMission( "InterMission" ) {  }
	void CheckEnableOk() const;

	void OnSave();
	virtual void STDCALL OnGetFocus( bool bFocus );
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICSaveReplay : public CInterfaceCommandBase<CInterfaceSaveReplay, MISSION_INTERFACE_SAVE_REPLAY>
{
	OBJECT_NORMAL_METHODS( CICSaveReplay );
	
	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfaceSaveReplay *pISR ) { pML->PushInterface( pISR ); }
	CICSaveReplay() {}
};
#endif		//__SAVE_REPLAY_H__
