#ifndef LINUX
#pragma once
#endif
#ifndef _MFnWeightGeometryFilter
#define _MFnWeightGeometryFilter

#if defined __cplusplus



#include <maya/MFnGeometryFilter.h>
#include <maya/MObject.h>

class MObject;
class MDagPath;
class MIntArray;
class MFloatArray;
class MObjectArray;
class MSelectionList;
class MString;



/**
MFnWeightGeometryFilter is the function set for editing the weights of
clusters, cluster flexors, and user-defined deformers derived from
MPxDeformerNode.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnWeightGeometryFilter : public MFnGeometryFilter
{
	declareMFn(MFnWeightGeometryFilter, MFnGeometryFilter);

public:
	MStatus		getWeights(unsigned index,
						   const MObject &components,
						   MFloatArray &weights) const;
	MStatus		getWeights(const MDagPath &path,
						   const MObject &components,
						   MFloatArray &weights) const;
	MStatus		setWeight(const MDagPath &path,
						  unsigned index,
						  const MObject &components,
						  float weight,
						  MFloatArray *oldValues = NULL);
	MStatus		setWeight(const MDagPath &path,
						  const MObject &components,
						  float weight,
						  MFloatArray *oldValues = NULL);
	MStatus		setWeight(const MDagPath &path,
						  unsigned index,
						  const MObject &components,
						  MFloatArray &values);
	MStatus		setWeight(const MDagPath &path,
						  const MObject &components,
						  MFloatArray &values);
	MStatus		getWeightPlugStrings(const MSelectionList &list,
									 MString &plugStrings) const;
	MStatus		getWeightPlugStrings(const MSelectionList &list,
									 MStringArray &plugStringArray) const;

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnWeightGeometryFilter */
