#ifndef LINUX
#pragma once
#endif
#ifndef _MFnSubdData
#define _MFnSubdData

#if defined __cplusplus



#include <maya/MFnGeometryData.h>





/**
  Create and manipulate Nurbs Surface dependency node data
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnSubdData : public MFnGeometryData 
{

	declareMFn(MFnSubdData, MFnGeometryData);

public:
	MObject		create( MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnSubdData */
