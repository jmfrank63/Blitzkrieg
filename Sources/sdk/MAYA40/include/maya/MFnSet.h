#ifndef LINUX
#pragma once
#endif
#ifndef _MFnSet
#define _MFnSet

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MString.h>
#include <maya/MObject.h>


 
class MObjectArray;
class MSelectionList;
class MDagPath;
class TsetCmd;
class Tstring;



/**
    Function set for sets of objects
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnSet : public MFnDependencyNode 
{

	declareMFn(MFnSet, MFnDependencyNode);

public:
	
	enum Restriction {
		kNone,
		kVerticesOnly,
		kEdgesOnly,
		kFacetsOnly,
		kEditPointsOnly,
		kRenderableOnly
	};

	MObject     create( const MSelectionList & members,
						         Restriction restriction = kNone, 
						         MStatus * ReturnStatus = NULL );

	MStatus     getUnion( const MObject & withSet, MSelectionList & result );
	MStatus     getUnion( const MObjectArray & setList,
						  MSelectionList & result );
	MStatus     getIntersection( const MObject & withSet, 
								 MSelectionList & result );
	MStatus     getIntersection( const MObjectArray & setList, 
								 MSelectionList & result );

	MStatus     clear();
	MStatus     getMembers( MSelectionList &members, bool flatten ) const;
	MStatus     addMember( const MObject &obj );
	MStatus     addMember( const MDagPath &obj, 
						   const MObject &component = MObject::kNullObj );
	MStatus     addMember( const MPlug &plug );
	MStatus     addMembers( const MSelectionList &list );
	MStatus     removeMember( const MObject &obj );
	MStatus     removeMember( const MDagPath &obj, const MObject &component );
	MStatus     removeMember( const MPlug &plug );
	MStatus     removeMembers( const MSelectionList &list );

	bool        isMember( const MObject &object,
						  MStatus * ReturnStatus = NULL ) const;
	bool        isMember( const MDagPath &object, 
						  const MObject &component = MObject::kNullObj,
						  MStatus * ReturnStatus = NULL ) const;
	bool        isMember( const MPlug &plug,
						  MStatus * ReturnStatus = NULL ) const;

	bool        intersectsWith( const MObject & otherSet, 
								 MStatus * ReturnStatus = NULL ) const;
	bool        hasRestrictions( MStatus * ReturnStatus = NULL ) const;
	Restriction restriction( MStatus * ReturnStatus = NULL ) const;
	MString     annotation( MStatus * ReturnStatus = NULL ) const;
	MStatus     setAnnotation( const MString &annotation );

	MObject     create( const MSelectionList & members,
						         Restriction restriction = kNone, 
						         bool isLayer = false,
						         MStatus * ReturnStatus = NULL );
protected:
	virtual		Tstring setCommandString();
	virtual		TsetCmd* setCommand();
private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnSet */
