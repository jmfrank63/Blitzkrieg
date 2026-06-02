#ifndef __INTERFACENEWDEPOTUPGRADES_H__
#define __INTERFACENEWDEPOTUPGRADES_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceNewDepotUpgrades : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceNewDepotUpgrades );
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceNewDepotUpgrades();
	CInterfaceNewDepotUpgrades();
	
public:
	virtual void STDCALL StartInterface();
};
class CICNewDepotUpgrades : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_NEW_DEPOTUPGRADES>
{
	OBJECT_NORMAL_METHODS( CICNewDepotUpgrades );
	
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICNewDepotUpgrades() {  }
};
#endif // __INTERFACENEWDEPOTUPGRADES_H__
