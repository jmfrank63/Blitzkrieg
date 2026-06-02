#ifndef LINUX
#pragma once
#endif
#ifndef _MFnVectorArrayData
#define _MFnVectorArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MVectorArray;
class MVector;



/**
  Create and manipulate MVectorArray dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnVectorArrayData : public MFnData 
{

	declareMFn(MFnVectorArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	const MVector&  operator[]( unsigned index ) const;
	MVector&	    operator[]( unsigned index );
	MStatus			copyTo( MVectorArray& ) const;
	MStatus			set( const MVectorArray& newArray );
	MVectorArray	array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MVectorArray& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnVectorArrayData */
