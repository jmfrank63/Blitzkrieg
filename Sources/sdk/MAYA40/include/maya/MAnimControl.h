#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimControl
#define _MAnimControl

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTime.h>
#include <maya/MTypes.h>
#include <maya/MFnAnimCurve.h>





/**
 Retrieve and set animation parameters and control playback
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimControl  
{
public:
	enum PlaybackMode {
		kPlaybackOnce,
		kPlaybackLoop,
		kPlaybackOscillate,
	};

	enum PlaybackViewMode {
		kPlaybackViewAll,
		kPlaybackViewActive
	};


											MAnimControl();
	virtual									~MAnimControl();
	static MAnimControl::PlaybackMode		playbackMode();
	static MStatus							setPlaybackMode( PlaybackMode
															 newMode );
	static MAnimControl::PlaybackViewMode	viewMode();
	static MStatus							setViewMode( PlaybackViewMode
														 newMode );
	static double							playbackBy();
	static MStatus							setPlaybackBy( const double& );
	static MTime							minTime();
	static MTime							maxTime();
	static MStatus							setMinTime( MTime newMinTime );
	static MStatus							setMaxTime( MTime newMaxTime );
	static MStatus							setMinMaxTime( MTime min,
														   MTime max );
	static MTime							currentTime();
	static MStatus							setCurrentTime( const MTime&
															newTime );
	static double							playbackSpeed();
	static MStatus							setPlaybackSpeed( double speed );
	static MStatus							playForward();
	static MStatus							playBackward();
	static bool								isPlaying();
	static MStatus							stop();
	static bool			autoKeyMode ();
	static MStatus		setAutoKeyMode ( bool mode );
	static MFnAnimCurve::TangentType globalInTangentType (
												MStatus * ReturnStatus = NULL );
	static MStatus		setGlobalInTangentType (const MFnAnimCurve::TangentType
												&tangentType );
	static MFnAnimCurve::TangentType globalOutTangentType (
												MStatus * ReturnStatus = NULL );
	static MStatus		setGlobalOutTangentType (const MFnAnimCurve::TangentType
												 &tangentType );
	static bool			weightedTangents( MStatus * ReturnStatus = NULL );
	static MStatus		setWeightedTangents( bool weightState );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAnimControl */
