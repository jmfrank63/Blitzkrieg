#ifndef LINUX
#pragma once
#endif
#ifndef _MFnRadialField
#define _MFnRadialField

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MFnField.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnRadialField : public MFnField
{

    declareDagMFn(MFnRadialField, MFnField);

public:
    double       radialType        ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setType           ( double value );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnRadialField */
