#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLambertShader
#define _MFnLambertShader

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnDependencyNode.h>


class MFnDependencyNode;
class MFltVector;
class MColor;



/**
  Facilitate the creation and manipulation of Lambert shaders.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnLambertShader : public MFnDependencyNode
{

	declareMFn( MFnLambertShader, MFnDependencyNode );

public:
	MObject     create( bool UIvisible = true, MStatus * ReturnStatus = NULL );
	short       refractedRayDepthLimit( MStatus * ReturnStatus = NULL ) const;
	MStatus     setRefractedRayDepthLimit( const short& new_limit );
	float       refractiveIndex( MStatus * ReturnStatus = NULL ) const;
	MStatus     setRefractiveIndex( const float& refractive_index );
	bool        rtRefractedColor( MStatus * ReturnStatus = NULL ) const;
	MStatus     setRtRefractedColor( const bool& rt_refracted_color );
	float       diffuseCoeff( MStatus * ReturnStatus = NULL ) const;
	MStatus     setDiffuseCoeff( const float& diffuse_coeff );
	MColor      color( MStatus * ReturnStatus = NULL ) const;
	MStatus     setColor( const MColor & col );
	MColor      transparency( MStatus * ReturnStatus = NULL ) const;
	MStatus     setTransparency( const MColor & transp );
	MColor      ambientColor( MStatus * ReturnStatus = NULL ) const;
	MStatus     setAmbientColor( const MColor & ambient_color );
	MColor      incandescence( MStatus * ReturnStatus = NULL ) const;
	MStatus     setIncandescence( const MColor & incand );
	float       translucenceCoeff( MStatus * ReturnStatus = NULL ) const;
	MStatus     setTranslucenceCoeff( const float& translucence_coeff );
	float       glowIntensity( MStatus * ReturnStatus = NULL ) const;
	MStatus     setGlowIntensity( const float& glow_intensity );
	bool        hideSource( MStatus * ReturnStatus = NULL ) const;
	MStatus     setHideSource( const bool& hide_source );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLambertShader */
