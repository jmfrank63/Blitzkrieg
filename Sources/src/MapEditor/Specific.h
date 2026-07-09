
#define SEC_NO_NAMESPACE_USING 1
#include "..\\Common\\LegacyUiCompat.h"

#include "..\Formats\fmtTerrain.h"
inline bool IsShiftKeyDown()
{
	return ( GetKeyState( VK_SHIFT ) < 0 );
}

inline bool IsCtrlKeyDown()
{
	return ( GetKeyState( VK_CONTROL ) < 0 );
}

