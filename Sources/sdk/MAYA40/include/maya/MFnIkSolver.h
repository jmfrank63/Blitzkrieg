#ifndef LINUX
#pragma once
#endif
#ifndef _MFnIkSolver
#define _MFnIkSolver

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>





/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnIkSolver : public MFnDependencyNode 
{
	declareMFn( MFnIkSolver, MFn::kIkSolver );

public:
	unsigned maxIterations( MStatus * ReturnStatus = NULL );
	MStatus setMaxIterations( unsigned maxIters );
	double tolerance( MStatus * ReturnStatus = NULL );
	MStatus setTolerance( double tolerance );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnIkSolver */
