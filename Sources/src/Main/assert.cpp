#include "stdafx.h"
extern "C"
{
	void BK_CDECL CallAssert( int bCondition )
	{
		(void)bCondition;
		NPlatform::BreakIntoDebugger();
	}
}
