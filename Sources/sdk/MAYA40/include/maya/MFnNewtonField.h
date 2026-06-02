#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNewtonField
#define _MFnNewtonField

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MFnField.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnNewtonField : public MFnField
{

    declareDagMFn(MFnNewtonField, MFnField);

public:
    double       minDistance        ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setMinDistance     ( double distance );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNewtonField */
