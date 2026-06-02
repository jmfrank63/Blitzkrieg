#ifndef LINUX
#pragma once
#endif
#ifndef _MFnComponentListData
#define _MFnComponentListData

#if defined __cplusplus



#include <maya/MFnData.h>





/**
  Create and manipulate Component Lists dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnComponentListData : public MFnData 
{

	declareMFn(MFnComponentListData, MFnData);

public:
	unsigned		length( MStatus* ReturnStatus = NULL ) const;
	bool			has( const MObject& obj,
						 MStatus* ReturnStatus = NULL ) const;
	MObject			operator[]( unsigned index) const;
	MStatus			add( MObject& );
	MStatus			remove( const MObject& );
	MStatus			remove( unsigned index );
	MStatus			clear();
	MObject			create( MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnComponentListData */
