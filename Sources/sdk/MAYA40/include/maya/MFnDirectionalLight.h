#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDirectionalLight
#define _MFnDirectionalLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnNonExtendedLight.h>





/**
  Facilitate the creation and manipulation of directional light nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnDirectionalLight : public MFnNonExtendedLight 
{

	declareDagMFn(MFnDirectionalLight,MFnNonExtendedLight);

public:
	MObject     create( bool UIvisible = true, MStatus * ReturnStatus = NULL );
	MObject     create( const MObject& parent, bool UIvisible = true,
						MStatus * ReturnStatus = NULL );
	float       shadowAngle( MStatus * ReturnStatus = NULL ) const;
	MStatus     setShadowAngle( const float& shadow_angle );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDirectionalLight */



