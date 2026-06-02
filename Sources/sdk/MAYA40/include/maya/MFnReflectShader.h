#ifndef LINUX
#pragma once
#endif
#ifndef _MFnReflectShader
#define _MFnReflectShader

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnLambertShader.h>



class MFnLambertShader;


class MColor;



/**
  Facilitate the creation and manipulation of reflective surface shaders.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnReflectShader : public MFnLambertShader
{

	declareMFn( MFnReflectShader, MFn::kReflect );

public:
	short       reflectedRayDepthLimit( MStatus * ReturnStatus = NULL ) const;
	MStatus     setReflectedRayDepthLimit( const short& new_limit );
	MColor      specularColor( MStatus * ReturnStatus = NULL ) const;
	MStatus     setSpecularColor( const MColor& specular_color );
	float       reflectivity( MStatus * ReturnStatus = NULL ) const;
	MStatus     setReflectivity( const float& reflectivity );
	MColor      reflectedColor( MStatus * ReturnStatus = NULL ) const;
	MStatus     setReflectedColor( const MColor& reflected_color );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnReflectShader */
