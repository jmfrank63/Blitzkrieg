#ifndef __INTERFACECLOUDSHUTDOWN_H__
#define __INTERFACECLOUDSHUTDOWN_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "iMission.h"
#include <cstdint>
// The shutdown screen: what the player sees while the exit-time cloud sync
// runs. That sync used to run after the main loop had ended - nothing drew,
// the OS put its busy cursor over a frozen window, and a slow upload looked
// like a hung game. Now the exit command pushes this screen in place of the
// exit video. The main loop (GameMain.cpp, which owns the sync handle and
// the option checks) starts the sync and reports through CloudSync.ExitSync:
// -1 until it has looked, 1 while a run holds the handle, 0 once settled or
// when there was nothing to do. This screen shows the credits backdrop with
// a please-wait notice while that says 1, and plays the exit video - the
// old exit path, unchanged - once it says 0.
class CInterfaceCloudShutdown : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceCloudShutdown );

	bool bNoticeShown;
	bool bLeft;
	std::uint64_t nStartedMs;

	void ShowNotice( bool bShow );
	void Leave();

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg ) { return false; }
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual ~CInterfaceCloudShutdown() {}
protected:
	CInterfaceCloudShutdown() : CInterfaceScreenBase( "Current" ), bNoticeShown( false ), bLeft( false ), nStartedMs( 0 ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICCloudShutdown : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_CLOUD_SHUTDOWN>
{
	OBJECT_NORMAL_METHODS( CICCloudShutdown );

	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICCloudShutdown() {  }
};
#endif // __INTERFACECLOUDSHUTDOWN_H__
