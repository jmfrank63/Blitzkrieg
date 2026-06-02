#ifndef __PLAYER_GAIN_LEVEL_H__
#define __PLAYER_GAIN_LEVEL_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfacePlayerGainLevel : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfacePlayerGainLevel );
	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfacePlayerGainLevel();
	CInterfacePlayerGainLevel() : CInterfaceInterMission( "Current" ) {  }
	
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICPlayerGainLevel : public CInterfaceCommandBase<CInterfacePlayerGainLevel, MISSION_INTERFACE_PLAYER_GAIN_LEVEL>
{
	OBJECT_NORMAL_METHODS( CICPlayerGainLevel );
	
	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfacePlayerGainLevel *pISM );
	CICPlayerGainLevel() {}
};
#endif		//__PLAYER_GAIN_LEVEL_H__
