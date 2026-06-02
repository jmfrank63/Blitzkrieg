#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDependencyNode
#define _MFnDependencyNode

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>



class MPlugArray;
class MPlug;
class MTypeId;
class MPxNode;
class MObjectArray;



/**
 MFnNumericData allows the manipulation of nodes in the dependency graph
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnDependencyNode : public MFnBase 
{

	declareMFn(MFnDependencyNode, MFnBase);

public:
	enum MAttrClass {
		kGlobalDynamicAttr = 1,
		kLocalDynamicAttr
	};
		
    MObject         create( const MTypeId &typeId,
								MStatus* ReturnStatus = NULL );
    MObject         create( const MTypeId &typeId,
								const MString& name,
								MStatus* ReturnStatus = NULL );

    MObject         create( const MString &type,
								MStatus* ReturnStatus = NULL );
    MObject         create( const MString &type,
								const MString& name,
								MStatus* ReturnStatus = NULL );
	
	MTypeId         typeId( MStatus* ReturnStatus = NULL ) const;
	MString         typeName( MStatus* ReturnStatus = NULL ) const;
	MString			name( MStatus * ReturnStatus = NULL ) const;
	MString			setName( const MString &name,
							 MStatus * ReturnStatus = NULL );
	MStatus			getConnections( MPlugArray& array ) const;
	unsigned		attributeCount( MStatus* ReturnStatus=NULL) const;
	MObject	        attribute(	unsigned index,
								MStatus* ReturnStatus=NULL) const;
	MObject	        attribute(	const MString& attrName,
								MStatus* ReturnStatus=NULL) const;
	MStatus			getAffectedAttributes ( const MObject& attr,
									MObjectArray& affectedAttributes ) const;
	MStatus			getAffectedByAttributes ( const MObject& attr,
									MObjectArray& affectedByAttributes ) const;
	MPlug			findPlug(	const MObject & attr,
								MStatus* ReturnStatus=NULL) const;
	MPlug			findPlug(	const MString & attrName,
								MStatus* ReturnStatus=NULL) const;
	MStatus			addAttribute( const MObject & attr,
								MAttrClass type = kLocalDynamicAttr );
	MStatus			removeAttribute( const MObject & attr,
								MAttrClass type = kLocalDynamicAttr );
	MPxNode *  		userNode( MStatus* ReturnStatus=NULL ) const;
	bool			isFromReferencedFile(MStatus* ReturnStatus=NULL) const;
	MString			parentNamespace(MStatus* ReturnStatus=NULL) const;
	static MString	classification( const MString & nodeTypeName );

protected:

private:

};
#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDependencyNode */
