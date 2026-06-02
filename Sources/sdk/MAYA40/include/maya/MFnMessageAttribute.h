#ifndef LINUX
#pragma once
#endif
#ifndef _MFnMessageAttribute
#define _MFnMessageAttribute

#if defined __cplusplus



#include <maya/MFnAttribute.h>




class MString;


/**
  Function set for message attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnMessageAttribute : public MFnAttribute 
{

	declareMFn(MFnMessageAttribute, MFnAttribute);

public:
	MObject    create( const MString& fullName,
					   const MString& briefName,
					   MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnMessageAttribute */



