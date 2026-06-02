#ifndef LINUX
#pragma once
#endif
#ifndef _MFnIkHandle
#define _MFnIkHandle

#if defined __cplusplus



#include <maya/MFnTransform.h>
#include <maya/MObject.h>


 
class MObjectArray;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnIkHandle : public MFnTransform 
{
	declareDagMFn( MFnIkHandle, MFn::kHandle );

public:
	enum Stickiness {
		kStickyOff,
		kStickyOn,
		kSuperSticky,
    };
	MObject   create( MDagPath& startJoint,
	                  MDagPath& effector,
					  MStatus * ReturnStatus = NULL );
	MStatus   getStartJoint( MDagPath &jointPath );
	MStatus   setStartJoint( const MDagPath &jointPath );
	MStatus   getEffector( MDagPath &effectorPath );
	MStatus   setEffector( const MDagPath &effectorPath );
	MStatus   setStartJointAndEffector( const MDagPath &jointPath,
										const MDagPath &effectorPath );
	unsigned  priority( MStatus * ReturnStatus = NULL );
	MStatus   setPriority( unsigned priority ); 
	Stickiness stickiness( MStatus * ReturnStatus = NULL );
	MStatus   setStickiness( Stickiness stickiness );
	double    weight( MStatus * ReturnStatus = NULL );
	MStatus   setWeight( double weight );
	double    poWeight( MStatus * ReturnStatus = NULL );
	MStatus   setPOWeight( double weight );
	MObject   solver( MStatus * ReturnStatus = NULL );
	MStatus   setSolver( const MObject &solver );
	MStatus   setSolver( const MString & solverName );

protected:

private:

}; 

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnIkHandle */
