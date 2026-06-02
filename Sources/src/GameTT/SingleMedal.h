#ifndef __IM_SINGLE_MEDAL_H__
#define __IM_SINGLE_MEDAL_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceSingleMedal : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceSingleMedal );
	std::string szMedalName;
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceSingleMedal();
	CInterfaceSingleMedal() : CInterfaceInterMission( "Current" ) {  }

public:
	virtual bool STDCALL Init();
	void Create( const char *pszName );
};
class CICSingleMedal : public CInterfaceCommandBase<CInterfaceSingleMedal, MISSION_INTERFACE_SINGLE_MEDAL>
{
	OBJECT_NORMAL_METHODS( CICSingleMedal );
	std::string szName;

	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfaceSingleMedal *pISM );
	CICSingleMedal() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif		//__IM_SINGLE_MEDAL_H__
