#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLight
#define _MFnLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>



class MPoint;
class MFloatVector;
class MColor;
class MDagPath;



/**
  MFnLight allows manipulation of dependency graph nodes representing lights.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnLight : public MFnDagNode 
{

	declareDagMFn(MFnLight,MFnDagNode);
public:
	MColor       color( MStatus * ReturnStatus = NULL ) const;
	MStatus      setColor( const MColor &col );
	float        intensity( MStatus * ReturnStatus = NULL ) const;
	MStatus      setIntensity( const float& intens );
	bool         useRayTraceShadows( MStatus * ReturnStatus = NULL ) const;
	MStatus      setUseRayTraceShadows( const bool& useRayTraceShadows );
	MColor       shadowColor( MStatus * ReturnStatus = NULL ) const;
    MStatus      setShadowColor( const MColor& shadow_color );
	double       centerOfIllumination( MStatus * ReturnStatus = NULL ) const;
	MStatus      setCenterOfIllumination( const double& dist );
	short        numShadowSamples( MStatus * ReturnStatus = NULL ) const;
	MStatus      setNumShadowSamples( const short& num_shadow_samples );
	short        rayDepthLimit( MStatus * ReturnStatus = NULL ) const;
	MStatus      setRayDepthLimit( const short& rayDepthLimit );
    MColor       opticalFXvisibility( MStatus * ReturnStatus = NULL ) const;
	MStatus      setOpticalFXvisibility( const MColor& visibility );
	MColor       lightIntensity( MStatus * ReturnStatus = NULL ) const;
	MFloatVector lightDirection( int instance, MSpace::Space space = MSpace::kWorld, MStatus * ReturnStatus = NULL ) const;
	MFloatVector lightDirection( MStatus * ReturnStatus = NULL ) const;
	bool         lightAmbient( MStatus * ReturnStatus = NULL ) const;
	bool         lightDiffuse( MStatus * ReturnStatus = NULL ) const;
	bool         lightSpecular( MStatus * ReturnStatus = NULL ) const;



protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLight */
