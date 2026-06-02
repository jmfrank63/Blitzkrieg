#ifndef __INTERFACEUNITPerformance_H__
#define __INTERFACEUNITPerformance_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CAfterMissionPopups;
class CInterfaceUnitPerformance : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceUnitPerformance );
	DECLARE_SERIALIZE;
	NInput::CCommandRegistrator commandMsgs;

	int nPlayerUnits;
	int nTotalNumUnits;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceUnitPerformance() {  }
	
	void PrepairShortcutBar();
	const char * GetUnitNameByWindowID( const int nWindowID );

protected:
	CInterfaceUnitPerformance() : CInterfaceInterMission( "InterMission" ),
		/*bDisableGetFocus( true ),*/ nPlayerUnits( 0 ), nTotalNumUnits( 0 ) {  }
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL OnGetFocus( bool bFocus );
};
class CICUnitPerformance : public CInterfaceCommandBase<CInterfaceUnitPerformance, MISSION_INTERFACE_UNIT_PERFORMANCE>
{
	OBJECT_NORMAL_METHODS( CICUnitPerformance );

	bool bDisableChange;

	virtual void PreCreate( IMainLoop *pML )
	{
	}

	virtual void PostCreate( IMainLoop *pML, CInterfaceUnitPerformance *pInterface ) 
	{ 
		pML->PushInterface( pInterface ); 
	}
	CICUnitPerformance() {  }

public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
	}
};
#endif // __INTERFACEUNITPerformance_H__
