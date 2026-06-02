#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPartition
#define _MFnPartition

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MString.h>
#include <maya/MObject.h>


 
class MObjectArray;
class MSelectionList;
class MDagPath;




/**
    Function partition for partitions of objects
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnPartition : public MFnDependencyNode 
{

	declareMFn(MFnPartition, MFnDependencyNode);

public:
	
	enum Restriction { 
		kNone,
		kVerticesOnly,
		kEdgesOnly,
		kFacetsOnly,
		kEditPointsOnly,
		kRenderableOnly
	};

	MObject     create( bool isRenderPartition = false,
						MStatus * ReturnStatus = NULL );
	bool        isRenderPartition( MStatus * ReturnStatus = NULL ) const;
	MStatus     addMember( const MObject &set );
	MStatus     removeMember( const MObject &set );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPartition */
