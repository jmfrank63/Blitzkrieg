#ifndef LINUX
#pragma once
#endif
#ifndef _MItDag
#define _MItDag

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>



class MDagPath;
class MDagPathArray;
class MString;



/**

Class MItDag provides the capability to traverse the DAG and to retrieve
specific nodes for subsequent querying and editing using compatible
Function Sets.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItDag  
{
public:
	enum TraversalType {
		kInvalidType = 0,
		kDepthFirst,
		kBreadthFirst
	};
	MItDag( TraversalType = kDepthFirst,
			MFn::Type = MFn::kInvalid,
			MStatus * ReturnStatus = NULL );
	MStatus       reset();
	MStatus       reset( const MObject & object,
						 TraversalType = kDepthFirst,
						 MFn::Type = MFn::kInvalid );
	MStatus       reset( const MDagPath & path,
						 TraversalType = kDepthFirst,
						 MFn::Type = MFn::kInvalid );
						 
	MObject       item( MStatus * ReturnStatus = NULL );
	MStatus       next();
						
	MStatus       prune();
	bool          isDone( MStatus * ReturnStatus = NULL ) const; 
	MObject       root( MStatus * ReturnStatus = NULL ); 
	unsigned      depth( MStatus * ReturnStatus = NULL ) const; 
	TraversalType getType( MStatus * ReturnStatus = NULL ) const; 

	MStatus       getPath( MDagPath& path ) const;
	MStatus       getAllPaths( MDagPathArray& paths ) const;
	MString       fullPathName(MStatus *ReturnStatus = NULL) const;
    MString       partialPathName(MStatus *ReturnStatus = NULL) const;
	bool          isInstanced( bool indirect = true,
                               MStatus * ReturnStatus = NULL ) const;
	unsigned int  instanceCount( bool total,
				                 MStatus * ReturnStatus = NULL ) const;
	MStatus	      traverseUnderWorld( bool flag );
	bool          willTraverseUnderWorld( MStatus * ReturnStatus = NULL ) const;

	virtual ~MItDag();

protected:

private:
	static const char* className();
	void*		iterator_data;
	MFn::Type	type_filter;
    bool        df;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItDag */
