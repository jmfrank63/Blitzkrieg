#ifndef LINUX
#pragma once
#endif
#ifndef _MFnUInt64ArrayData
#define _MFnUInt64ArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MUint64Array;



/**
  Create and manipulate MUint64Array dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnUInt64ArrayData : public MFnData 
{

	declareMFn(MFnUInt64ArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	MUint64          operator[]( unsigned index ) const;
	MUint64&	        operator[]( unsigned index );
	MStatus			copyTo( MUint64Array& ) const;
	MStatus			set( const MUint64Array& newArray );
	MUint64Array	array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MUint64Array& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnUInt64ArrayData */
