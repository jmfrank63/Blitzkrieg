#ifndef __IM_UNITS_POOL_H__
#define __IM_UNITS_POOL_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceUnitsPool : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceUnitsPool );
	NInput::CCommandRegistrator commandMsgs;
	std::vector< std::vector<int> > units;
	std::vector< std::vector<int> > depot;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceUnitsPool();
	CInterfaceUnitsPool() : CInterfaceInterMission( "InterMission" ) {}

public:
	virtual bool STDCALL Init();
	void Create( int nNewUnits );
};
class CICUnitsPool : public CInterfaceCommandBase<CInterfaceUnitsPool, MISSION_INTERFACE_UNITS_POOL>
{
	OBJECT_NORMAL_METHODS( CICUnitsPool );

	int nNewUnits;
	
	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfaceUnitsPool *pIUP );
	CICUnitsPool() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif		//__IM_UNITS_POOL_H__
