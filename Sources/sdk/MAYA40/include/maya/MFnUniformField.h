#ifndef LINUX
#pragma once
#endif
#ifndef _MFnUniformField
#define _MFnUniformField

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MFnField.h>
#include <maya/MVector.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnUniformField : public MFnField
{

    declareDagMFn(MFnUniformField, MFnField);

public:
    MVector      direction          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setDirection       ( const MVector & uniformDirection );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnUniformField */
