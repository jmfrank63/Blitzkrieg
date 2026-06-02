#ifndef LINUX
#pragma once
#endif
#ifndef _MFnIkJoint
#define _MFnIkJoint

#if defined __cplusplus



#include <maya/MFnTransform.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MObject.h>





/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnIkJoint : public MFnTransform 
{
	declareDagMFn( MFnIkJoint, MFn::kJoint );

public:
	enum Axis {
		kXAxis,
		kYAxis,
		kZAxis,
		kNone };
	MObject create( MObject parent = MObject::kNullObj,
					MStatus * ReturnStatus = NULL );
	MStatus getOrientation( MQuaternion &quaternion ) const; 
	MStatus setOrientation( const MQuaternion &quaternion );
	MStatus getOrientation( MEulerRotation &rotation ) const; 
	MStatus setOrientation( const MEulerRotation &rotation );
	MStatus getOrientation( double rotation[3], 
							MTransformationMatrix::RotationOrder &order ) 
		                    const; 
	MStatus setOrientation( const double rotation[3],
							MTransformationMatrix::RotationOrder order );
	MStatus getScaleOrientation( MQuaternion &quaternion ) const; 
	MStatus setScaleOrientation( const MQuaternion &quaternion );
	MStatus getScaleOrientation( double rotation[3], 
								 MTransformationMatrix::RotationOrder &order ) 
		                         const; 
	MStatus setScaleOrientation( const double rotation[3],
								 MTransformationMatrix::RotationOrder order );
	MStatus getSegmentScale( double scale[3] ) const; 
	MStatus setSegmentScale( const double scale[3] );
	MStatus getStiffness( double stiffness[3] ) const; 
	MStatus setStiffness( const double stiffness[3] );
	MStatus getPreferedAngle( double rotation[3] ) const; 
	MStatus setPreferedAngle( const double rotation[3] );
	MStatus getDegreesOfFreedom( bool &freeInX, bool &freeInY, 
								 bool &freeInZ ) const;
	MStatus setDegreesOfFreedom( bool freeInX, bool freeInY,
								 bool freeInZ );
	double  minRotateDampXRange( MStatus * ReturnStatus = NULL ) const;
	double  minRotateDampYRange( MStatus * ReturnStatus = NULL ) const;
	double  minRotateDampZRange( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampXRange( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampYRange( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampZRange( MStatus * ReturnStatus = NULL ) const;
	MStatus setMinRotateDampXRange( double angle );
	MStatus setMinRotateDampYRange( double angle );
	MStatus setMinRotateDampZRange( double angle );
	MStatus setMaxRotateDampXRange( double angle );
	MStatus setMaxRotateDampYRange( double angle );
	MStatus setMaxRotateDampZRange( double angle );
	double  minRotateDampXStrength( MStatus * ReturnStatus = NULL ) const;
	double  minRotateDampYStrength( MStatus * ReturnStatus = NULL ) const;
	double  minRotateDampZStrength( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampXStrength( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampYStrength( MStatus * ReturnStatus = NULL ) const;
	double  maxRotateDampZStrength( MStatus * ReturnStatus = NULL ) const;
	MStatus setMinRotateDampXStrength( double angle );
	MStatus setMinRotateDampYStrength( double angle );
	MStatus setMinRotateDampZStrength( double angle );
	MStatus setMaxRotateDampXStrength( double angle );
	MStatus setMaxRotateDampYStrength( double angle );
	MStatus setMaxRotateDampZStrength( double angle );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnIkJoint */
