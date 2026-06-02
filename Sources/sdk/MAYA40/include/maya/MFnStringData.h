#ifndef LINUX
#pragma once
#endif
#ifndef _MFnStringData
#define _MFnStringData

#if defined __cplusplus



#include <maya/MFnData.h>



class MString;
class MStringArray;



/**
  Create and manipulate MString dependency node data
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnStringData : public MFnData 
{

	declareMFn(MFnStringData, MFnData);

public:
	MString			string( MStatus* ReturnStatus = NULL ) const;
	MStatus			set( const MString& newString );
	MObject			create( const MString& str, MStatus* ReturnStatus = NULL );
	MObject			create( MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnStringData */
