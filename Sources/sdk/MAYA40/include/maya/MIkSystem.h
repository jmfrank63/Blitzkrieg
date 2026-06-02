#ifndef LINUX
#pragma once
#endif
#ifndef _MIkSystem
#define _MIkSystem

#if defined __cplusplus




#include <maya/MObject.h>
#include <maya/MStringArray.h>
#include <maya/MStatus.h>





/**
 
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MIkSystem  
{

public:
	static MObject	findSolver( MString name, MStatus * ReturnStatus = NULL );
	static MStatus	getSolvers( MStringArray & names );
	static bool		isGlobalSnap( MStatus * ReturnStatus = NULL );
	static MStatus	setGlobalSnap( bool isSnap );
	static bool		isGlobalSolve( MStatus * ReturnStatus = NULL );
	static MStatus	setGlobalSolve( bool isSnap );
protected:
	static const char* className();

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MIkSystem */
