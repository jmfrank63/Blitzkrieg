#ifndef LINUX
#pragma once
#endif
#ifndef _MDagPath
#define _MDagPath

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MStatus.h>
#include <maya/MObject.h>



class MMatrix;
class MDagPathArray;
class MString;



/**

Provides methods for obtaining one or all Paths to a specified DAG Node,
determining if a Path is valid and if two Paths are equivalent, obtaining the
length, transform, and inclusive and exclusive matrices of a Path, as well as
performing Path to Path assignment.

DAG Paths may be used as parameters to some methods in the DAG Node Function
Set (MFnDagNode).

It is often useful to combine DAG Paths into DAG Path arrays (MDagPathArray).

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDagPath  
{
 
public:
 	MDagPath();
 	MDagPath( const MDagPath& src );
 	virtual ~MDagPath();

	static MStatus	getAllPathsTo( const MObject & entity, 
								   MDagPathArray & pathArray );
	static MStatus	getAPathTo( const MObject & entity, 
								MDagPath & path );
	bool            hasFn( MFn::Type type,
						   MStatus * ReturnStatus = NULL ) const;
	MFn::Type       apiType(MStatus * ReturnStatus = NULL) const;

	bool        	isValid(MStatus * ReturnStatus = NULL) const;
	MObject      	node(MStatus * ReturnStatus = NULL) const;
	MObject   		transform( MStatus * ReturnStatus = NULL ) const;
	unsigned int    length(MStatus * ReturnStatus = NULL) const; 
    MStatus         extendToShape();
	MStatus         push( const MObject &child );
	MStatus         pop( unsigned num = 1 );
	unsigned        childCount(  MStatus * ReturnStatus = NULL ) const;
	MObject 	    child( unsigned i, MStatus * ReturnStatus = NULL ) const;
	MMatrix      	inclusiveMatrix(MStatus * ReturnStatus = NULL) const;
	MMatrix      	exclusiveMatrix(MStatus * ReturnStatus = NULL) const;
	MMatrix      	inclusiveMatrixInverse(MStatus * ReturnStatus = NULL)const;
	MMatrix      	exclusiveMatrixInverse(MStatus * ReturnStatus = NULL)const;
	MDagPath&		operator = ( const MDagPath& src);
	bool			operator == ( const MDagPath& src) const;
 	MStatus         set( const MDagPath& src);

	unsigned		pathCount(MStatus * ReturnStatus = NULL) const;
	MStatus			getPath( MDagPath & path, unsigned i = 0 ) const;
	MString         fullPathName(MStatus *ReturnStatus = NULL) const;
	MString         partialPathName(MStatus *ReturnStatus = NULL) const;

	bool            isInstanced( MStatus *ReturnStatus = NULL ) const;
	unsigned        instanceNumber( MStatus *ReturnStatus = NULL ) const;

	static MDagPath getAPathTo( const MObject & entity, 
								MStatus * ReturnStatus = NULL );

protected:

private:



	static const char* className();
 	void * data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDagPath */
