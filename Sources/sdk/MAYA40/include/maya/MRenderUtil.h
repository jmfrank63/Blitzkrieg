#ifndef LINUX
#pragma once
#endif
#ifndef _MRenderUtil
#define _MRenderUtil

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MFloatVector;
class MFloatVectorArray;
class MIntArray;
class MFloatPoint;
class MFloatArray;
class MFloatPointArray;
class MFloatMatrix;



/**
*/
class OPENMAYARENDER_EXPORT MRenderUtil  
{
public:

	enum MRenderState {
		kNotRendering,
		kBatchRender,
		kInteractiveRender,
		kIprRender,
		kHardwareRender,
	};

	enum MRenderPass {
		kAll,
		kColorOnly,
		kShadowOnly,
		kAmbientOnly,
		kDiffuseOnly,
		kSpecularOnly,
	};

	static MRenderState	mayaRenderState();

	static MStatus	raytrace(
						const MFloatVector& rayOrigin,  // in camera space
						const MFloatVector& rayDirection,
						const int objectId,
						const int raySampler,
						const short rayDepth,


						MFloatVector& resultColor,
						MFloatVector& resultTransparency,

						const bool isReflectedRays = true
					);


	static MStatus	raytrace(
						const MFloatVectorArray& rayOrigins,  // in camera space
						const MFloatVectorArray& rayDirections,
						const int objectId,
						const int raySampler,
						const short rayDepth,


						MFloatVectorArray& resultColors,
						MFloatVectorArray& resultTransparencies,

						const bool isReflectedRays = true
					);	

	static MStatus	raytraceFirstGeometryIntersections(
						const MFloatVectorArray& rayOrigins,  // in camera space
						const MFloatVectorArray& rayDirections,
						const int objectId,
						const int raySampler,


						MFloatVectorArray& 	resultIntersections,
						MIntArray& 			resultIntersected
					);	


	static MStatus sampleShadingNetwork(

		MString             shadingNodeName,
		int                numSamples,
		bool				useShadowMaps,
		bool				reuseMaps,

		MFloatMatrix		&cameraMatrix,	// eye to world

		MFloatPointArray    *points,	// in world space
		MFloatArray         *uCoords,
		MFloatArray         *vCoords,
		MFloatVectorArray   *normals,	// in world space
		MFloatPointArray    *refPoints,	// in world space
		MFloatVectorArray   *tangentUs,	// in world space
		MFloatVectorArray   *tangentVs,	// in world space
		MFloatArray         *filterSizes,

		MFloatVectorArray   &resultColors,
		MFloatVectorArray   &resultTransparencies
	);

	static bool 	   generatingIprFile();

	static MRenderPass renderPass( void );

protected:
	static const char* className();
private:
    ~MRenderUtil();
#ifdef __GNUC__
	friend class shutUpAboutPrivateDestructors;
#endif
};

#endif /* __cplusplus */
#endif /* _MRenderUtil */
