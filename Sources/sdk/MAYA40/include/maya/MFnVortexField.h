#ifndef LINUX
#pragma once
#endif
#ifndef _MFnVortexField
#define _MFnVortexField

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MFnField.h>
#include <maya/MVector.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnVortexField : public MFnField
{

    declareDagMFn(MFnVortexField, MFnField);

public:
    MVector      axis               ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setAxis            ( const MVector & axisVector );


protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnVortexField */
