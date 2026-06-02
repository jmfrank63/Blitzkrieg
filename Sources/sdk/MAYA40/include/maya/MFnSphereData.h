#ifndef LINUX
#pragma once
#endif
#ifndef _MFnSphereData
#define _MFnSphereData

#if defined __cplusplus



#include <maya/MFnData.h>





/**
  Create and manipulate Sphere dependency node data
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnSphereData : public MFnData 
{

	declareMFn(MFnSphereData, MFnData);

public:
	MObject			create( double rad=1, MStatus* ReturnStatus = NULL );
	double			radius( MStatus* ReturnStatus = NULL ) const;
	MStatus			setRadius( double rad );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnSphereData */
