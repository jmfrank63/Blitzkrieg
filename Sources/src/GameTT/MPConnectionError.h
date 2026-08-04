#ifndef __MPCONNECTIONERROR_H__
#define __MPCONNECTIONERROR_H__
#pragma ONCE
#include "MuliplayerToUIConsts.h"
class CMPConnectionError
{
public:
	static bool DisplayError( const enum EMultiplayerToUICommands eErrorID );
};
#endif // __MPCONNECTIONERROR_H__
