#ifndef LINUX
#pragma once
#endif
#ifndef _MFnData
#define _MFnData

#if defined __cplusplus



#include <maya/MFnBase.h>





/**
  Common methods for manipulating dependency graph data.
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnData : public MFnBase 
{

	declareMFn( MFnData, MFnBase );

public:
	enum Type {
		kInvalid,
		kNumeric,
		kPlugin,
		kPluginGeometry,
		kString,
		kMatrix,
		kStringArray,
		kDoubleArray,
		kIntArray,
		kPointArray,
		kVectorArray,
		kComponentList,
		kMesh,
		kLattice,
		kNurbsCurve,
		kNurbsSurface,
		kSphere,
		kDynArrayAttrs,
        kDynSweptGeometry,
		kSubdSurface,
		kLast
	};

protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnData */
