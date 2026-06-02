#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLightDataAttribute
#define _MFnLightDataAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>




class MString;


/**
  Function set for light data attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnLightDataAttribute : public MFnAttribute 
{

	declareMFn(MFnLightDataAttribute, MFnAttribute);

public:
	MObject    create( const MString& fullName,
					   const MString& briefName,
					   const MObject & direction,
					   const MObject & intensity,
					   const MObject & ambient,
					   const MObject & diffuse,
					   const MObject & specular,
					   const MObject & shadowFraciton,
					   const MObject & preShadowIntensity,
					   const MObject & blindData,
					   MStatus* ReturnStatus = NULL );

	MStatus     setDefault( float defDirectionX, 
							float defDirectionY, 
							float defDirectionZ,
							float defIntensityR,
							float defIntensityG, 
							float defIntensityB,
							bool  defAmbient, 
							bool  defDiffuse, 
							bool  defSpecular,
							float defShadowFraction,
							float defPreShadowIntensity,
							unsigned int defBlindData);

	MObject     child( unsigned index, MStatus* returnStatus );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLightDataAttribute */



