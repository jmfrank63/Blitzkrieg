#ifndef LINUX
#pragma once
#endif
#ifndef _MItDependencyNodes
#define _MItDependencyNodes

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>



class MDagPath;
class MDagPathArray;
class MString;



/**

Iterate over all nodes in the dependency graph.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItDependencyNodes  
{
public:
				MItDependencyNodes( MFn::Type filter = MFn::kInvalid,
									MStatus * ReturnStatus = NULL );
	virtual		~MItDependencyNodes();

	MStatus		reset( MFn::Type filter = MFn::kInvalid );
						 
	MObject		item( MStatus * ReturnStatus = NULL ) const;
	MStatus		next();
						
	bool		isDone( MStatus * ReturnStatus = NULL ) const; 


protected:

private:
	static const char* className();
	void*		f_data;
	MFn::Type	f_filter;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItDependencyNodes */
