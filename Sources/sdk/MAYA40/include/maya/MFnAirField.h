#ifndef LINUX
#pragma once
#endif
#ifndef _MFnAirField
#define _MFnAirField

#if defined __cplusplus



#include <maya/MFnField.h>
#include <maya/MVector.h>





/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnAirField : public MFnField
{

    declareDagMFn(MFnAirField, MFnField);

public:
    MVector      direction          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setDirection       ( const MVector & airDirection );
    double       speed              ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setSpeed           ( double value );
    double       inheritVelocity    ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setInheritVelocity ( double velocity );
    bool         inheritRotation    ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setInheritRotation ( bool enable );
    bool         componentOnly      ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setComponentOnly   ( bool enable );
    double       spread             ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setSpread          ( double value );
    bool         enableSpread       ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setEnableSpread    ( bool enable );


protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnAirField */
