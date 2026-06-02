#ifndef LINUX
#pragma once
#endif
#ifndef _MFnField
#define _MFnField

#if defined __cplusplus



#include <maya/MFnDagNode.h>



class MPointArray;
class MVectorArray;
class MDoubleArray;



/**

*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnField : public MFnDagNode
{

    declareDagMFn(MFnField, MFnDagNode);

public:
    MStatus getForceAtPoint(const MPointArray&   point, 
                            const MVectorArray&  velocity,
                            const MDoubleArray&  mass,
                            MVectorArray&        force,
							double deltaTime = 1.0 / 24.0 );
    MStatus getForceAtPoint(const MVectorArray&  point, 
                            const MVectorArray&  velocity,
                            const MDoubleArray&  mass,
                            MVectorArray&        force,
							double deltaTime = 1.0 / 24.0 );
    double       magnitude          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setMagnitude       ( double mag );
    double       attenuation        ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setAttenuation     ( double atten );
    double       maxDistance        ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setMaxDistance     ( double maxDist );
    bool         perVertex          ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setPerVertex       ( bool enable );
    bool         useMaxDistance     ( MStatus *ReturnStatus = NULL ) const;
    MStatus      setUseMaxDistance  ( bool enable );



protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnField */
