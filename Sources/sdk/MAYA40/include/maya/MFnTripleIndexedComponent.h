#ifndef LINUX
#pragma once
#endif
#ifndef _MFnTripleIndexedComponent
#define _MFnTripleIndexedComponent

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

class OPENMAYA_EXPORT MFnTripleIndexedComponent : public MFnComponent 
{

	declareMFn( MFnTripleIndexedComponent, MFnComponent );

public:

	MObject		create( MFn::Type compType, MStatus * ReturnStatus = NULL );

    MStatus 	addElement( int sIndex, int tIndex, int uIndex );
    MStatus 	addElements( const MIntArray& sIndexArray, 
							 const MIntArray& tIndexArray,
							 const MIntArray& uIndexArray );

    MStatus		getElement( int index,
							int & sIndex, int & tIndex, int & uIndex ) const;
    MStatus 	getElements( MIntArray& sIndexArray, 
							 MIntArray& tIndexArray,
							 MIntArray& uIndexArray ) const;

	MStatus		setCompleteData( int maxS, int maxT, int maxU );
	MStatus		getCompleteData( int & maxS, int & maxT, int & maxU ) const;
    
protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnTripleIndexedComponent */
