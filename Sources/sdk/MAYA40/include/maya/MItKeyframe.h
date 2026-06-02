#ifndef LINUX
#pragma once
#endif
#ifndef _MItKeyframe
#define _MItKeyframe

#if defined __cplusplus



#include <maya/MFn.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>



class MObject;
class MPtrBase;



/**

Iterate over the keyframes of a particular Anim Curve Node, and query
and edit the keyframe to which the iterator points.

Determine the time and value of the keyframe, as well as the x,y values
and type of the tangent to the curve entering (in tangent) and leaving
(out tangent) the keyframe.

Set the time and value of the keyframe, and the type of the tangents.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MItKeyframe { 
public:
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
	MItKeyframe( MObject & animCurveNode, MStatus * ReturnStatus = NULL );
	~MItKeyframe();
	MStatus     reset( MObject & animCurveNode );
	MStatus     reset();
	MStatus     next();
	bool        isDone( MStatus * ReturnStatus = NULL );
	MTime       time( MStatus * ReturnStatus = NULL ); 
	MStatus     setTime( MTime time );
	double      value( MStatus * ReturnStatus = NULL );
	MStatus     setValue( double value );
	TangentType inTangentType( MStatus * ReturnStatus = NULL );
	TangentType outTangentType( MStatus * ReturnStatus = NULL );
	MStatus     setInTangentType( TangentType tangentType );
	MStatus     setOutTangentType( TangentType tangentType );
	MStatus     getTangentOut( float &x, float &y );
	MStatus     getTangentIn( float &x, float &y );
	bool		tangentsLocked( MStatus * ReturnStatus = NULL ) const;
	MStatus		setTangentsLocked( bool locked );

protected:

private:
	static const char* className();
    MPtrBase *        f_ptr;
	unsigned          f_index;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItKeyframe */
