#include "stdafx.h"
extern "C"
{
	void __cdecl CallAssert( int bCondition )
	{
		(void)bCondition;
		NPlatform::BreakIntoDebugger();
	}
}
