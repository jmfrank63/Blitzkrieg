#ifndef LINUX
#pragma once
#endif
#ifndef _MFnGenericAttribute
#define _MFnGenericAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>
#include <maya/MFnData.h>
#include <maya/MFnNumericData.h>
class MTypeId;
class MString;





/**
  Function set for generic attributes of a dependency node
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnGenericAttribute : public MFnAttribute 
{

	declareMFn(MFnGenericAttribute, MFnAttribute);

public:
	MObject 	create( const MString& full,
						const MString& brief,
						MStatus* ReturnStatus = NULL );
	MStatus		addAccept( MFnData::Type newType );
	MStatus		addAccept( MFnNumericData::Type newType );
	MStatus		addAccept( const MTypeId& id );
	MStatus		removeAccept( MFnData::Type oldType );
	MStatus		removeAccept( MFnNumericData::Type oldType );
	MStatus		removeAccept( const MTypeId& id );

protected:
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnGenericAttribute */
