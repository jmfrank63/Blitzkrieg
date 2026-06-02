#ifndef LINUX
#pragma once
#endif
#ifndef _MFnComponent
#define _MFnComponent

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MString.h>



class MIntArray;



/**
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnComponent : public MFnBase 
{

	declareMFn( MFnComponent, MFnBase );

public:
	int        elementCount( MStatus* ReturnStatus = NULL ); 
	MFn::Type	type( MStatus * ReturnStatus = NULL );
	bool		isEmpty( MStatus * ReturnStatus = NULL ) const;
	bool		isEqual( MObject &, MStatus * ReturnStatus = NULL ) const;
	bool		isComplete( MStatus * ReturnStatus = NULL ) const;
	MStatus		setComplete( bool );
    
protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnComponent */
