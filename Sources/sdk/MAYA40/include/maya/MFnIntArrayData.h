#ifndef LINUX
#pragma once
#endif
#ifndef _MFnIntArrayData
#define _MFnIntArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MIntArray;



/**
  Create and manipulate MIntArray dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnIntArrayData : public MFnData 
{

	declareMFn(MFnIntArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	int             operator[]( unsigned index ) const;
	int&	        operator[]( unsigned index );
	MStatus			copyTo( MIntArray& ) const;
	MStatus			set( const MIntArray& newArray );
	MIntArray	    array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MIntArray& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnIntArrayData */
