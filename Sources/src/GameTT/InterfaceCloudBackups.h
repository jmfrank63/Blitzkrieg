#ifndef __INTERFACECLOUDBACKUPS_H__
#define __INTERFACECLOUDBACKUPS_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
// The cloud backup browser (P07-M02): the config history the backup engine
// has been quietly accumulating, one row per snapshot - host, time, size -
// grouped by host, newest first. The listing is a network call observed
// through the facade's poll, never awaited.
class CInterfaceCloudBackups : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceCloudBackups );
	NInput::CCommandRegistrator msgs;

	// Poll handle of the running listing, -1 when settled.
	int nListHandle;
	// Row order after sorting; rows carry an index into this via user data.
	// P07-M03's restore needs the entry id the engine gave us back.
	std::vector<std::string> entryIDs;

	void FillList();
	void SetStatus( const char *pszTextKey, const std::wstring &szSuffix );

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual ~CInterfaceCloudBackups() {}

protected:
	CInterfaceCloudBackups() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL Done();
};
class CICCloudBackups : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_CLOUD_BACKUPS>
{
	OBJECT_NORMAL_METHODS( CICCloudBackups );

	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICCloudBackups() {  }
};
#endif // __INTERFACECLOUDBACKUPS_H__
