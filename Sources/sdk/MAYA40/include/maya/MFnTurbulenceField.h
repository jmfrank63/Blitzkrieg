#ifndef LINUX
#pragma once
#endif
#ifndef _MFnTurbulenceField
#define _MFnTurbulenceField

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MFnField.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnTurbulenceField : public MFnField
{

    declareDagMFn(MFnTurbulenceField, MFnField);

public:
    double       frequency          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setFrequency       ( double value );
    double       phase              ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setPhase           ( double value );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnTurbulenceField */
