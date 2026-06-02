#ifndef LINUX
#pragma once
#endif
#ifndef _MFnAmbientLight
#define _MFnAmbientLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnLight.h>



class MPoint;
class MColor;
class MDagPath;



/**
  Facilitate the creation and manipulation of ambient light nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnAmbientLight : public MFnLight 
{

	declareDagMFn(MFnAmbientLight,MFnLight);
public:
	MObject     create( bool UIvisible = true, MStatus * ReturnStatus = NULL );
	MObject     create( const MObject& parent, bool UIvisible = true,
											   MStatus * ReturnStatus = NULL );
	float       ambientShade( MStatus * ReturnStatus = NULL ) const;
	MStatus     setAmbientShade( const float& ambient_shade );
	bool        castSoftShadows( MStatus * ReturnStatus = NULL ) const;
	MStatus     setCastSoftShadows( const bool& cast_soft_shadows );
	float       shadowRadius( MStatus * ReturnStatus = NULL ) const;
	MStatus     setShadowRadius( const float& shadow_radius );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnAmbientLight */
