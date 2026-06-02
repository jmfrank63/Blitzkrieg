#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPhongShader
#define _MFnPhongShader

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnReflectShader.h>


class MFnReflectShader;



/**
  Facilitate the creation and manipulation of Phong shaders.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnPhongShader : public MFnReflectShader
{

	declareMFn( MFnPhongShader, MFnReflectShader );

public:
	MObject 	create( bool UIvisible = true, MStatus * ReturnStatus = NULL );	
	float       cosPower( MStatus * ReturnStatus = NULL ) const;
	MStatus     setCosPower( const float& cos_power );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPhongShader */
