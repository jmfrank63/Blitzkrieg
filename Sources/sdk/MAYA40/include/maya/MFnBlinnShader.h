#ifndef LINUX
#pragma once
#endif
#ifndef _MFnBlinnShader
#define _MFnBlinnShader

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnReflectShader.h>


class MFnReflectShader;



/**
  Facilitate the creation and manipulation of Blinn shaders.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnBlinnShader : public MFnReflectShader
{

	declareMFn( MFnBlinnShader, MFnReflectShader );

public:
	MObject 	create( bool UIvisible = true, MStatus * ReturnStatus = NULL );	
	float       eccentricity( MStatus * ReturnStatus = NULL ) const;
	MStatus     setEccentricity( const float& eccentricity );
	float       specularRollOff( MStatus * ReturnStatus = NULL ) const;
	MStatus     setSpecularRollOff( const float& specular_rolloff );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnBlinnShader */
