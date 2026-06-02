#ifndef LINUX
#pragma once
#endif
#ifndef _MFnIkEffector
#define _MFnIkEffector

#if defined __cplusplus



#include <maya/MFnTransform.h>
#include <maya/MObject.h>





/**
  Function set for inverse kinematics end effectors
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnIkEffector : public MFnTransform 
{
	declareDagMFn( MFnIkEffector, MFn::kEffector );

public:
	MObject create( MObject parent = MObject::kNullObj,
					MStatus * ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnIkEffector */
