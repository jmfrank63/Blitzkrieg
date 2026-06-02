#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNurbsSurfaceData
#define _MFnNurbsSurfaceData

#if defined __cplusplus



#include <maya/MFnGeometryData.h>





/**
  Create and manipulate Nurbs Surface dependency node data
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnNurbsSurfaceData : public MFnGeometryData 
{

	declareMFn(MFnNurbsSurfaceData, MFnGeometryData);

public:
	MObject		create( MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNurbsSurfaceData */
