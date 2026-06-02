#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDoubleIndexedComponent
#define _MFnDoubleIndexedComponent

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

class OPENMAYA_EXPORT MFnDoubleIndexedComponent : public MFnComponent 
{

	declareMFn( MFnDoubleIndexedComponent, MFnComponent );

public:

	MObject		create( MFn::Type compType, MStatus * ReturnStatus = NULL );

    MStatus 	addElement( int uIndex, int vIndex );
    MStatus 	addElements( const MIntArray& uIndexArray, 
							 const MIntArray& vIndexArray );

    MStatus		getElement( int index, int & uIndex, int & vIndex ) const;
    MStatus 	getElements( MIntArray& uIndexArray, 
							 MIntArray& vIndexArray ) const;

	MStatus		setCompleteData( int maxU, int maxV );
	MStatus		getCompleteData( int & maxU, int & maxV ) const;
    
protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDoubleIndexedComponent */
