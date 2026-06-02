#ifndef LINUX
#pragma once
#endif
#ifndef _MFnMotionPath
#define _MFnMotionPath

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
class MDagPath;
class MDagPathArray;
class MDGModifier;
class MTime;



/**
  Construct and manipulate motion paths
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnMotionPath : public MFnDependencyNode 
{

	declareMFn(MFnMotionPath, MFnDependencyNode);

public:
	enum Axis {
		kXaxis,
		kYaxis, 
		kZaxis
	};
	MObject		create( const MDagPath & pathObject,
						const MDagPath & objectToAnimate,
						MTime & timeStart, MTime & timeEnd,
						MDGModifier * modifier = NULL,
						MStatus * ReturnStatus = NULL );
	MStatus		setPathObject( const MDagPath & pathObject,
								 MDGModifier * modifier = NULL );
	MDagPath	pathObject( MStatus * ReturnStatus = NULL );
	MStatus		addAnimatedObject( const MDagPath & objectToAnimate,
									MDGModifier * modifier = NULL );
	MStatus		getAnimatedObjects( MDagPathArray & array );
	MStatus		setFollow( bool on, MDGModifier * modifier = NULL );
	bool		follow( MStatus * ReturnStatus = NULL ) const;
	MStatus		setFollowAxis( Axis axis );
	Axis		followAxis( MStatus * ReturnStatus = NULL );
	MStatus		setUpAxis( Axis axis );
	Axis		upAxis( MStatus * ReturnStatus = NULL );
	MStatus		setBank( bool bank );
	bool		bank( MStatus * ReturnStatus  = NULL) const;
	MStatus		setBankScale( double bankScale );
	double		bankScale( MStatus * ReturnStatus = NULL );
	MStatus		setBankThreshold( double bankThreshold );
	double		bankThreshold( MStatus * ReturnStatus = NULL );
	MStatus		setUseNormal( bool use );
	bool		useNormal( MStatus * ReturnStatus = NULL );
	MStatus		setInverseNormal( bool invert );
	bool		inverseNormal( MStatus * ReturnStatus = NULL );
	MStatus		setUStart( double start );
	MStatus		setUEnd( double end );

	double		uStart( MStatus * ReturnStatus = NULL );
	double		uEnd( MStatus * ReturnStatus = NULL );
	MStatus		setUTimeStart( MTime & start );
	MStatus		setUTimeEnd( MTime & end );
	MTime		uTimeStart( MStatus * ReturnStatus = NULL );
	MTime		uTimeEnd( MStatus * ReturnStatus = NULL );
	unsigned	numPositionMarkers( MStatus * ReturnStatus = NULL );
	MObject		getPositionMarker( unsigned, MStatus * ReturnStatus = NULL );
	unsigned	numOrientationMarkers( MStatus * ReturnStatus = NULL );
	MObject		getOrientationMarker( unsigned, MStatus * ReturnStatus = NULL );

protected:

private:
    void *         getCurve();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnMotionPath */
