#ifndef LINUX
#pragma once
#endif
#ifndef _MFnTypedAttribute
#define _MFnTypedAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>
#include <maya/MFnData.h>
#include <maya/MObject.h>



class MTypeId; 
class MString;



/**
 Function set for type attributes of dependency nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnTypedAttribute : public MFnAttribute 
{

	declareMFn(MFnTypedAttribute, MFnAttribute);

public:
	MObject 	create( const MString& fullName,
						const MString& briefName,
						const MTypeId& id,
						MObject defaultData = MObject::kNullObj,
						MStatus* ReturnStatus = NULL );
	MObject 	create( const MString& fullName,
						const MString& briefName,
						MFnData::Type type,
						MObject defaultData = MObject::kNullObj,
						MStatus* ReturnStatus = NULL );
	MFnData::Type	attrType ( MStatus* ReturnStatus = NULL ) const;
	MStatus		setDefault( const MObject & defaultCustomData );
	MObject 	create( const MString& fullName,
						const MString& briefName,
						MFnData::Type type,
						MStatus* ReturnStatus );

protected:

private:

};
#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnTypedAttribute */
