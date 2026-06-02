#ifndef LINUX
#pragma once
#endif
#ifndef _MFnEnumAttribute
#define _MFnEnumAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>




class MString;


/**
  Function set for enumerated attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnEnumAttribute : public MFnAttribute
{

	declareMFn(MFnEnumAttribute, MFnAttribute);

public:

	MObject     create( const MString& fullName,
					    const MString& briefName, 
						short defaultValue = 0,
					    MStatus* ReturnStatus = NULL ); 
	MStatus		addField( const MString & fieldString, short index);
	MString     fieldName( short index, MStatus *ReturnStatus = NULL ) const;
	short       fieldIndex( const MString & fieldString,
							MStatus *ReturnStatus = NULL ) const;
	MStatus		getMin ( short& minValue ) const;
	MStatus		getMax ( short& maxValue ) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnEnumAttribute */
