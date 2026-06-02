#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNonExtendedLight
#define _MFnNonExtendedLight

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnNonAmbientLight.h>





/**
  Facilitate the creation and manipulation of non-extended light nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnNonExtendedLight : public MFnNonAmbientLight 
{

	declareDagMFn(MFnNonExtendedLight,MFnNonAmbientLight);
public:
	float         shadowRadius( MStatus * ReturnStatus = NULL ) const;
	MStatus       setShadowRadius( const float& shadow_radius );
	bool          castSoftShadows( MStatus * ReturnStatus = NULL ) const;
	MStatus       setCastSoftShadows( const bool& cast_soft_shadows );
	bool          useDepthMapShadows( MStatus * ReturnStatus = NULL ) const;
	MStatus       setUseDepthMapShadows( const bool& use_depth_map );
	short         depthMapFilterSize( MStatus * ReturnStatus ) const;
	MStatus       setDepthMapFilterSize( const short& depth_map_filter_size );
	short         depthMapResolution( MStatus * ReturnStatus ) const;
	MStatus       setDepthMapResolution( const short& depth_map_resolution );
	float         depthMapBias( MStatus * ReturnStatus ) const;
	MStatus       setDepthMapBias( const float& depth_map_bias );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNonExtendedLight */



