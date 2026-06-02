#ifndef LINUX
#pragma once
#endif
#ifndef _MFnArrayAttrsData
#define _MFnArrayAttrsData

#if defined __cplusplus



#include <maya/MFnData.h>



class MVectorArray;
class MDoubleArray;
class MIntArray;
class MStringArray;



/**
  Function set for multiple arrays of attributes for dependency node data.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnArrayAttrsData : public MFnData 
{
	declareMFn( MFnArrayAttrsData, MFnData );

public:
	enum Type {
	kInvalid,
	kVectorArray,
	kDoubleArray,
	kIntArray,
	kStringArray,
	kLast
	};

	MStatus			clear();

	unsigned		count() const;

	MStringArray	list( MStatus *ReturnStatus = NULL) const;

	bool			checkArrayExist( const MString attrName,
									 MFnArrayAttrsData::Type &arrayType,
									 MStatus *ReturnStatus = NULL);

	MVectorArray	vectorArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MDoubleArray	doubleArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MIntArray		intArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MStringArray	stringArray( const MString attrName,
									MStatus *ReturnStatus = NULL );

	MObject			create( MStatus *ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnArrayAttrsData */
