#include "stdafx.h"
extern "C"
{
	void __cdecl CallAssert( int bCondition )
	{
		DEBUG_BREAK;
	}
}
