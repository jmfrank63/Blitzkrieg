#ifndef LINUX
#pragma once
#endif
#ifndef _MToolsInfo
#define _MToolsInfo

#if defined __cplusplus



#include <maya/MTypes.h>



class MStatus;
class MPxContext;


/**
MToolsInfo provides methods for keeping track of the 
state of the current tool property sheet.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32


class OPENMAYAUI_EXPORT MToolsInfo {
public:
	static void		setDirtyFlag(const MPxContext &context);
	static void		resetDirtyFlag();
	static bool		isDirty();

private:
	static const char *className();
};


#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MToolsInfo */
