#ifndef LINUX
#pragma once
#endif
#ifndef _MFnStringArrayData
#define _MFnStringArrayData

#if defined __cplusplus



#include <maya/MFnData.h>



class MString;
class MStringArray;



/**
  Create and manipulate MStringArray dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnStringArrayData : public MFnData 
{

	declareMFn(MFnStringArrayData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	const MString&	operator[]( unsigned index ) const;
	MString&	    operator[]( unsigned index );
	MStatus			copyTo( MStringArray& ) const;
	MStatus			set( const MStringArray& newArray );
	MStringArray	array( MStatus*ReturnStatus=NULL );
	MObject			create( MStatus*ReturnStatus=NULL );
	MObject			create( const MStringArray& in, MStatus*ReturnStatus=NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnStringArrayData */
