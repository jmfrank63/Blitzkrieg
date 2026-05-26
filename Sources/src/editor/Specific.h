// Stingray Objective Toolkit
#define SEC_NO_NAMESPACE_USING 1
#include "..\\Common\\StingrayCompat.h"
// Objective Toolkit toolbar/menubar migrated via StingrayCompat.h

//
#include "..\Formats\fmtTerrain.h"
inline bool IsShiftKeyDown()
{
	return ( GetKeyState( VK_SHIFT ) < 0 );
}

inline bool IsCtrlKeyDown()
{
	return ( GetKeyState( VK_CONTROL ) < 0 );
}


const int GAME_SIZE_X = 800;
const int GAME_SIZE_Y = 600;

extern const std::string szTGAFilter;
extern const std::string szTextFilter;
extern const std::string szLuaFilter;
extern const std::string szMusicFilter;
extern const std::string szMovieFilter;
extern const std::string szXMLFilter;
extern const std::string szMapFilter;
extern const std::string szSoundFilter;
extern const std::string szMODFilter;
extern const std::string szSANFilter;
extern const std::string szDDSFilter;


