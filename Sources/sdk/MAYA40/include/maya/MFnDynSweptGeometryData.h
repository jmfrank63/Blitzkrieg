#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDynSweptGeometryData
#define _MFnDynSweptGeometryData

#if defined __cplusplus



#include <maya/MFnData.h>



class MDynSweptLine;
class MDynSweptTriangle;



/**
	Access MDynSweptLine and MDynSweptTriangle dependency node data
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MFnDynSweptGeometryData : public MFnData 
{

	declareMFn(MFnDynSweptGeometryData, MFnData);

public:
	int					lineCount( MStatus* ReturnStatus = NULL ) const;
	int					triangleCount( MStatus* ReturnStatus = NULL ) const;
	MDynSweptLine		sweptLine( int index,
							MStatus* ReturnStatus = NULL ) const;
	MDynSweptTriangle	sweptTriangle( int index,
							MStatus* ReturnStatus = NULL ) const;

	MObject				create( MStatus* ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDynSweptGeometryData */
