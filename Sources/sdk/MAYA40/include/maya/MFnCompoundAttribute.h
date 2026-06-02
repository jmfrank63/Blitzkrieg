#ifndef LINUX
#pragma once
#endif
#ifndef _MFnCompoundAttribute
#define _MFnCompoundAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>



class MString;



/**
   Function set for compound attributes of dependency nodes
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnCompoundAttribute : public MFnAttribute 
{

	declareMFn(MFnCompoundAttribute, MFnAttribute);

public:
	MObject 	create( const MString& full,
						const MString& brief,
						MStatus* ReturnStatus = NULL );
	MStatus		addChild( const MObject & child );
	MStatus		removeChild( const MObject & child );
	unsigned	numChildren( MStatus* ReturnStatus = NULL ) const;
	MObject 	child( unsigned index, MStatus* ReturnStatus = NULL ) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnCompoundAttribute */
