#ifndef LINUX
#pragma once
#endif
#ifndef _MFnAnimCurve
#define _MFnAnimCurve

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MTime.h>
#include <maya/MAngle.h>
#include <maya/MDoubleArray.h>
#include <maya/MTimeArray.h>



class MAnimCurveChange;
class MPlug;
class MDGModifier;



/**

Create, query and edit Anim Curve Nodes and the keys internal to
those Nodes.

Create an Anim Curve Node and connect its output to any animatable
attribute on another Node.

Evaluate an Anim Curve at a particular time.

Determine the number of keys held by an Anim Curve.

Determine the time and value of individual keys, as well as the
(angle,weight) or (x,y) values and type of key tangents.  The in-tangent
refers to the tangent entering the key.  The out-tangent refers to the tangent
leaving the key.

Find the index of the key at, or closest to a particular time.

Set the time and value of individual keys, as well as the type of
the tangent to the curve entering (in-tangent) and leaving
(out-tangent) the key, specify the tangent as either (angle,weight) or (x,y).

Add and remove keys from an Anim Curve.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnAnimCurve : public MFnDependencyNode 
{
	declareMFn(MFnAnimCurve, MFnDependencyNode);
public:
	MFnAnimCurve (const MPlug &plug, MStatus *ReturnStatus = NULL);

public:
	enum AnimCurveType {
		kAnimCurveTA = 0,
		kAnimCurveTL,
		kAnimCurveTT,
		kAnimCurveTU,
		kAnimCurveUA,
		kAnimCurveUL,
		kAnimCurveUT,
		kAnimCurveUU,
		kAnimCurveUnknown
	};
	enum TangentType {
		kTangentGlobal = 0,
		kTangentFixed,
		kTangentLinear,
		kTangentFlat,
		kTangentSmooth,
		kTangentStep,
		kTangentSlow,
		kTangentFast,
		kTangentClamped
    };
    enum InfinityType {
		kConstant = 0,
		kLinear = 1,
		kCycle = 3,
		kCycleRelative = 4,
		kOscillate = 5
	};
	MObject     create( const MObject & node,
						const MObject & attribute,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );
	MObject     create( const MObject & node,
						const MObject & attribute,
						AnimCurveType animCurveType,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );
	MObject     create( MPlug & plug,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );
	MObject     create( MPlug & plug,
						AnimCurveType animCurveType,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );
	MObject		create( AnimCurveType animCurveType,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );

	AnimCurveType	animCurveType (MStatus *ReturnStatus = NULL) const;

	AnimCurveType   timedAnimCurveTypeForPlug( MPlug& plug,
											   MStatus *ReturnStatus = NULL) const;

	AnimCurveType   unitlessAnimCurveTypeForPlug( MPlug& plug,
											   MStatus *ReturnStatus = NULL) const;

	double      evaluate( const MTime &atTime,
						  MStatus * ReturnStatus = NULL ) const;
	MStatus		evaluate( const MTime &atTime, double &value ) const;
	MStatus		evaluate( const MTime &atTime, MTime &timeValue ) const;
	MStatus		evaluate( const double &atUnitlessInput, double &value ) const;
	MStatus		evaluate( const double &atUnitlessInput,
						  MTime &timeValue ) const;
	bool		isStatic( MStatus * ReturnStatus = NULL ) const;
	unsigned    numKeyframes( MStatus * ReturnStatus = NULL ) const;
	unsigned	numKeys( MStatus * ReturnStatus = NULL ) const;
	MStatus     remove( unsigned index, MAnimCurveChange * change = NULL );
	MStatus     addKeyframe( MTime time, double value, 
							 MAnimCurveChange * change = NULL );
	MStatus     addKeyframe( MTime time, double value,
							 TangentType tangentInType,
							 TangentType tangentOutType, 
							 MAnimCurveChange * change = NULL );
	unsigned	addKey( MTime time, double value,
						TangentType tangentInType = kTangentGlobal,
						TangentType tangentOutType = kTangentGlobal,
						MAnimCurveChange * change = NULL,
					 	MStatus * ReturnStatus = NULL );
	unsigned	addKey( MTime timeInput, MTime timeValue,
						TangentType tangentInType = kTangentGlobal,
						TangentType tangentOutType = kTangentGlobal,
						MAnimCurveChange * change = NULL,
					 	MStatus * ReturnStatus = NULL );
	unsigned	addKey( double unitlessInput, double value,
						TangentType tangentInType = kTangentGlobal,
						TangentType tangentOutType = kTangentGlobal,
						MAnimCurveChange * change = NULL,
					 	MStatus * ReturnStatus = NULL );
	unsigned	addKey( double unitlessInput, MTime time,
						TangentType tangentInType = kTangentGlobal,
						TangentType tangentOutType = kTangentGlobal,
						MAnimCurveChange * change = NULL,
					 	MStatus * ReturnStatus = NULL );
	MStatus		addKeys( MTimeArray * timeArray,
						 MDoubleArray * valueArray,
						 TangentType tangentInType = kTangentGlobal,
						 TangentType tangentOutType = kTangentGlobal,
						 bool keepExistingKeys = false,
							 MAnimCurveChange * change = NULL );

	bool        find( MTime time, unsigned &index, 
					         MStatus * ReturnStatus = NULL ) const;
	bool		find( double unitlessInput, unsigned & index,
					  MStatus * ReturnStatus = NULL ) const;
	unsigned    findClosest( MTime time, MStatus * ReturnStatus = NULL ) const;
	unsigned	findClosest( double unitlessInput,
							 MStatus * ReturnStatus = NULL ) const;
	MTime       time( unsigned index, MStatus * ReturnStatus = NULL ) const;
	double      value( unsigned index, MStatus * ReturnStatus = NULL ) const;
	double		unitlessInput( unsigned index,
							   MStatus * ReturnStatus = NULL ) const;
	MStatus     setValue( unsigned index, double value,
					   	  MAnimCurveChange * change = NULL );
	MStatus     setTime( unsigned index, MTime time,
						 MAnimCurveChange * change = NULL );
    MStatus		setUnitlessInput( unsigned index, double unitlessInput,
								  MAnimCurveChange * change = NULL );
	bool		isTimeInput( MStatus * ReturnStatus = NULL ) const;
	bool		isUnitlessInput( MStatus * ReturnStatus = NULL ) const;
	TangentType inTangentType( unsigned index,
							   MStatus * ReturnStatus = NULL ) const;
	TangentType outTangentType( unsigned index, 
								MStatus * ReturnStatus = NULL ) const;
	MStatus     setInTangentType( unsigned index, TangentType tangentType,
								  MAnimCurveChange * change = NULL );
	MStatus     setOutTangentType( unsigned index, TangentType tangentType,
								   MAnimCurveChange * change = NULL );
	MStatus     getTangent( unsigned index, float &x, float &y,
							bool inTangent ) const;
	MStatus     getTangent( unsigned index, MAngle &angle, double &weight,
							bool inTangent ) const;
    MStatus		setTangent( unsigned index, float x, float y, bool inTangent,
						   	MAnimCurveChange * change = NULL );
    MStatus		setTangent( unsigned index, MAngle angle, double weight,
							bool inTangent, MAnimCurveChange * change = NULL );
	MStatus		setAngle( unsigned index, MAngle angle, bool inTangent,
						  MAnimCurveChange * change = NULL );
	MStatus		setWeight( unsigned index, double weight, bool inTangent,
						   MAnimCurveChange * change = NULL );
	bool		weightsLocked( unsigned index,
							   MStatus * ReturnStatus = NULL ) const;
	bool		tangentsLocked( unsigned index,
								MStatus * ReturnStatus = NULL ) const;
	MStatus		setWeightsLocked( unsigned index, bool locked,
								  MAnimCurveChange * change = NULL );
	MStatus		setTangentsLocked( unsigned index, bool locked,
								   MAnimCurveChange * change = NULL );
	InfinityType	preInfinityType( MStatus * ReturnStatus = NULL ) const;
	InfinityType	postInfinityType( MStatus * ReturnStatus = NULL ) const;
	MStatus		setPreInfinityType( InfinityType infinityType,
									MAnimCurveChange * change = NULL );
	MStatus		setPostInfinityType( InfinityType infinityType,
									 MAnimCurveChange * change = NULL );
	bool		isWeighted (MStatus *ReturnStatus = NULL) const;
	MStatus		setIsWeighted (bool isWeighted,
							   MAnimCurveChange *change = NULL);
	bool		isBreakdown( unsigned index,
							 MStatus * ReturnStatus = NULL ) const;
	MStatus		setIsBreakdown( unsigned index,
								bool isBreakdown,
								MAnimCurveChange *change = NULL);

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnAnimCurve */
