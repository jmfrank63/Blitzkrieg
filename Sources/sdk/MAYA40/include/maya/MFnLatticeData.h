#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLatticeData
#define _MFnLatticeData

#if defined __cplusplus



#include <maya/MFnGeometryData.h>





/**
  Manage lattice data that is passed between dependency graph nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnLatticeData : public MFnGeometryData 
{

	declareMFn(MFnLatticeData, MFnGeometryData);

public:
	MObject create( MStatus* ReturnStatus = NULL );

	MObject lattice( MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLatticeData */
