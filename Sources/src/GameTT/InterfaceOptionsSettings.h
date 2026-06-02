#ifndef __INTERFACEOPTIONSSETTINGS_H__
#define __INTERFACEOPTIONSSETTINGS_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class COptionsListWrapper;
class CInterfaceOptionsSettings : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceOptionsSettings );

	std::vector< CPtr<COptionsListWrapper> > optionsLists;

	int nActive;													// nurrent active division
	int nMaxDivision;											// total number of divisions.
	int nMinDifficulty;

	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );

	virtual ~CInterfaceOptionsSettings() {  }
	CInterfaceOptionsSettings() : CInterfaceInterMission( /*"InterMission"*/"Current" ), nActive( -1 ), nMaxDivision( 0 ) {  }

	virtual void SuspendAILogic( bool bSuspend );
	void OnChangeDivision( const int nDivision );
	void Close();
	virtual bool OpenCurtains();
public:
	virtual void STDCALL Done();
	virtual bool STDCALL Init();
	void Create();

};
class CICOptionsSettings: public CInterfaceCommandBase<CInterfaceOptionsSettings, MISSION_INTERFACE_OPTIONSSETTINGS>
{
	OBJECT_NORMAL_METHODS( CICOptionsSettings );
	
	virtual void PreCreate( IMainLoop *pML ) { }
	virtual void PostCreate( IMainLoop *pML, CInterfaceOptionsSettings *pEI )
	{
		pEI->Create();
		pML->PushInterface( pEI );
	}
	CICOptionsSettings() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig ) 
	{  
	}
};
#endif // __INTERFACEOPTIONSSETTINGS_H__
