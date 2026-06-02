#ifndef LINUX
#pragma once
#endif
#ifndef _MDagPathArray
#define _MDagPathArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MDagPath;



/**

Provides methods for manipulating arrays of DAG Paths.

Arrays of DAG Paths are useful for storing and manipluating multiple Paths
to a particular DAG Node.  The DAG Path method MDagPath::getAllPathsTo()
and DAG Node Function Set method MFnDagNode::getAllPaths() implicitly
return an array of DAG Paths.

These arrays may also be used to manage Paths for a number of different
Nodes.

DAG Path arrays are used in conjunction with DAG Paths (MDagPath) and
individual elements of the arrays can be parameters to some methods of the
DAG Node Function Set (MFnDagNode).

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDagPathArray  
{

public:
					MDagPathArray();
					~MDagPathArray();
	const MDagPath&	operator[]( unsigned index ) const;
	MDagPath &		operator[]( unsigned index );
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MDagPath & element, unsigned index );
	MStatus			append( const MDagPath & element );
	MStatus			clear();
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MDagPathArray &array);

protected:

private:
	void * arr;

	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDagPathArray */
