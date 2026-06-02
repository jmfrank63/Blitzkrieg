#ifndef __SAVE_COMMANDS_HISTORY_COMMAND_H__
#define __SAVE_COMMANDS_HISTORY_COMMAND_H__
#pragma ONCE
#include "Transceiver.h"
#include "CommandsHistoryInterface.h"
class CSaveCommandsHistoryCommand : public IBaseCommand
{
	OBJECT_MINIMAL_METHODS( CSaveCommandsHistoryCommand );

	CPtr<ICommandsHistory> pHistory;
public:
	CSaveCommandsHistoryCommand( ICommandsHistory *_pHistory ) : pHistory( _pHistory ) { }
	virtual void STDCALL Do() { pHistory->Save(); }
	virtual void STDCALL UnDo() {  }
	virtual bool STDCALL CanUnDo() { return false; }
};
#endif //__SAVE_COMMANDS_HISTORY_COMMAND_H__
