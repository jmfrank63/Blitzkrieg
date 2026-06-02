#ifndef LINUX
#pragma once
#endif
#ifndef _MSelectionMask
#define _MSelectionMask

#if defined __cplusplus




#include <maya/MObject.h>
#include <maya/MStatus.h>



class MPoint;
class MString;
class MTime;



/**
  Selection masks provide a way to control what is selectable in Maya.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MSelectionMask
{
public:
	enum SelectionType {
		kSelectHandles,
		kSelectLocalAxis,
		
		kSelectIkHandles,
		kSelectIkEndEffectors,
		kSelectJoints,
		
		kSelectLights,
		kSelectCameras,
		
		kSelectLattices,
		kSelectClusters,
		kSelectSculpts,
		
		kSelectNurbsCurves,
		kSelectNurbsSurfaces,
		kSelectMeshes,
		kSelectSubdiv,
		kSelectSketchPlanes,
		
		kSelectParticleShapes,
		kSelectEmitters,
		kSelectFields,
		kSelectSprings,
		kSelectRigidBodies,
		kSelectRigidConstraints,
		kSelectCollisionModels,
		
		kSelectXYZLocators,
		kSelectOrientationLocators,
		kSelectUVLocators,
		
		kSelectTextures,

		kSelectCurves,
		kSelectSurfaces,
		kSelectLocators,
		kSelectObjectsMask,
		
		
		kSelectCVs,
		kSelectHulls,
		kSelectEditPoints,
		
		kSelectMeshVerts,
		kSelectMeshEdges,
		kSelectMeshFreeEdges,
		kSelectMeshFaces,
		kSelectSubdivMeshPoints,
		kSelectSubdivMeshEdges,
		kSelectSubdivMeshFaces,
		kSelectMeshUVs,
		
		kSelectVertices,
		kSelectEdges,
		kSelectFacets,
		kSelectMeshLines,
		kSelectMeshComponents,
		
		kSelectCurveParmPoints,
		kSelectCurveKnots,
		kSelectSurfaceParmPoints,
		kSelectSurfaceKnots,
		kSelectSurfaceRange,
		kSelectSurfaceEdge,
		kSelectIsoparms,
		kSelectCurvesOnSurfaces,
		kSelectPPStrokes,
		
		kSelectLatticePoints,
		
		kSelectParticles,

		kSelectJointPivots,
		kSelectScalePivots,
		kSelectRotatePivots,
		
		kSelectPivots,
		
		kSelectSelectHandles,
		
		kSelectComponentsMask,
		
		kSelectAnimCurves,
		kSelectAnimKeyframes,
		kSelectAnimInTangents,
		kSelectAnimOutTangents,
		
		kSelectAnimMask,
		kSelectAnimAny,
		
		kSelectTemplates,
		kSelectManipulators,
		kSelectGuideLines,
		kSelectPointsForGravity,
		kSelectPointsOnCurvesForGravity,
		kSelectPointsOnSurfacesForGravity,
		kSelectObjectGroups,
		kSelectSubdivMeshMaps
	};

	MSelectionMask();
	MSelectionMask( SelectionType selType );
	MSelectionMask( const MSelectionMask& in );

	virtual ~MSelectionMask();

	MStatus		setMask( SelectionType selType );
	MStatus		setMask( MSelectionMask& mask );
	MStatus		addMask( SelectionType selType );

	bool		intersects( SelectionType selType,
							MStatus * ReturnStatus = NULL ) const;
	bool		intersects( MSelectionMask& mask,
							MStatus * ReturnStatus = NULL ) const;

	MSelectionMask	operator| ( SelectionType selType );
	MSelectionMask	operator| ( MSelectionMask& mask );
	MSelectionMask&	operator =( MSelectionMask& other );

protected:

private:


	MSelectionMask( const void *, bool );
	const void * data;
	bool fOwn;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MSelectionMask */
