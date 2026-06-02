#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPointArrayData
#define _MFnPointArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MPoint;
class MPointArray;



/**
  Create and manipulate MPointArray dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnPointArrayData : public MFnData 
{

	declareMFn(MFnPointArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	const MPoint&	operator[]( unsigned index ) const;
	MPoint&			operator[]( unsigned index );
	MStatus			copyTo( MPointArray& ) const;
	MStatus			set( const MPointArray& newArray );
	MPointArray		array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MPointArray& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPointArrayData */
