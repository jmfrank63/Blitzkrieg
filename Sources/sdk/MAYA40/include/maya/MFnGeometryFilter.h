#ifndef LINUX
#pragma once
#endif
#ifndef _MFnGeometryFilter
#define _MFnGeometryFilter

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>

class MObject;
class MDagPath;
class MIntArray;
class MFloatArray;
class MObjectArray;
class MSelectionList;
class MString;



/**
MFnGeometryFilter is the function set for deformers.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnGeometryFilter : public MFnDependencyNode
{
	declareMFn(MFnGeometryFilter, MFnDependencyNode);

public:
	MStatus		getInputGeometry(MObjectArray &objects) const;
	MStatus		getOutputGeometry(MObjectArray &objects) const;
	MObject		inputShapeAtIndex(unsigned index,
								  MStatus *ReturnStatus = NULL) const;
	MObject		outputShapeAtIndex(unsigned index,
								   MStatus *ReturnStatus = NULL) const;
	unsigned	indexForOutputShape(const MObject &shape, 
									MStatus *ReturnStatus = NULL) const;
	MStatus		getPathAtIndex(unsigned index, 
							   MDagPath &path) const;
	unsigned	indexForGroupId(unsigned groupId, 
								MStatus *ReturnStatus = NULL) const;
	unsigned	groupIdAtIndex(unsigned index, 
							   MStatus *ReturnStatus = NULL) const;
	unsigned	numOutputConnections(MStatus *ReturnStatus = NULL) const;
	unsigned	indexForOutputConnection(unsigned connectionIndex, 
										 MStatus *ReturnStatus = NULL) const;
	MObject		deformerSet(MStatus *ReturnStatus = NULL) const;
	float		envelope(MStatus *ReturnStatus = NULL) const;
	MStatus		setEnvelope(float envelope);

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnGeometryFilter */
