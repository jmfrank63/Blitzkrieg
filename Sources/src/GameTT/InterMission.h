#ifndef __INTERMISSION_H__
#define __INTERMISSION_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"

class CInterfaceInterMission : public CInterfaceScreenBase
{
	virtual void STDCALL DrawAdd();
protected:

	NInput::CCommandRegistrator intermissionMsgs;

	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	CInterfaceInterMission( const std::string &szInterfaceType ) : CInterfaceScreenBase( szInterfaceType ) {  }
public:
	virtual bool STDCALL Init();
};
interface IMPToUICommandManager;
class CInterfaceMultiplayerScreen : public CInterfaceInterMission 
{
protected:
	CPtr<IMPToUICommandManager> pCommandManager;	// singleton shortcut

	CInterfaceMultiplayerScreen( const std::string &szInterfaceType ) : CInterfaceInterMission( szInterfaceType ) {  }
	virtual bool STDCALL StepLocal( bool bAppActive );
public:
	virtual bool STDCALL Init();

};
#endif // __INTERMISSION_H__
