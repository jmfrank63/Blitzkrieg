#ifndef LINUX
#pragma once
#endif
#ifndef _MFnSingleIndexedComponent
#define _MFnSingleIndexedComponent

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MString.h>
#include <maya/MFnComponent.h>



class MIntArray;



/**
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnSingleIndexedComponent : public MFnComponent 
{

	declareMFn( MFnSingleIndexedComponent, MFnComponent );

public:

	MObject		create( MFn::Type compType, MStatus * ReturnStatus = NULL );

    MStatus 	addElement( int element );
    MStatus 	addElements( MIntArray& elements );

    int			element( int index, MStatus * ReturnStatus = NULL ) const;
    MStatus		getElements( MIntArray& elements ) const;

	MStatus		setCompleteData( int numElements );
	MStatus		getCompleteData( int & numElements ) const;

    
protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnSingleIndexedComponent */
