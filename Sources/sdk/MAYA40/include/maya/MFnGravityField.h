#ifndef LINUX
#pragma once
#endif
#ifndef _MFnGravityField
#define _MFnGravityField

#if defined __cplusplus



#include <maya/MFnField.h>
#include <maya/MVector.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnGravityField : public MFnField
{

    declareDagMFn(MFnGravityField, MFnField);

public:
    MVector      direction          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setDirection       ( const MVector & gravityDirection );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnGravityField */
