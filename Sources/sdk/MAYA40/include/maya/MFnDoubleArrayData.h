#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDoubleArrayData
#define _MFnDoubleArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MDoubleArray;



/**
  Create and manipulate MDoubleArray dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnDoubleArrayData : public MFnData 
{

	declareMFn(MFnDoubleArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	double          operator[]( unsigned index ) const;
	double&	        operator[]( unsigned index );
	MStatus			copyTo( MDoubleArray& ) const;
	MStatus			set( const MDoubleArray& newArray );
	MDoubleArray	array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MDoubleArray& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDoubleArrayData */
