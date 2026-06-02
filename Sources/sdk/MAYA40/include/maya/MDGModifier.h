#ifndef LINUX
#pragma once
#endif
#ifndef _MDGModifier
#define _MDGModifier

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MFnDependencyNode.h>



class MObject;
class MPlug;
class MTypeId;
class MString;



/**
  A class that is used to modify the dependency graph and also supports undo 
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDGModifier  
{
public:
	MDGModifier();
	~MDGModifier();    
    
    MObject     createNode( const MTypeId &typeId,
                            MStatus* ReturnStatus = NULL );
    MObject     createNode( const MString &type,
                            MStatus* ReturnStatus = NULL );
    MStatus     deleteNode( const MObject & node );
    
    MStatus     renameNode( const MObject & node, const MString &newName );

	MStatus		connect(	const MObject & sourceNode,
							const MObject & sourceAttr,
							const MObject & destNode,	
							const MObject & destAttr );
	MStatus		disconnect(	const MObject & sourceNode,
							const MObject & sourceAttr,
							const MObject & destNode,
							const MObject & destAttr );
	MStatus		connect(	const MPlug& source, const MPlug& dest );
	MStatus		disconnect(	const MPlug& source, const MPlug& dest );

    MStatus     addAttribute( const MObject& node, const MObject& attribute,
							  MFnDependencyNode::MAttrClass type 
									= MFnDependencyNode::kLocalDynamicAttr );
    MStatus     removeAttribute( const MObject& node, const MObject& attribute,
								 MFnDependencyNode::MAttrClass type 
									= MFnDependencyNode::kLocalDynamicAttr  );

	MStatus		commandToExecute( const MString& command );
	MStatus		doIt();
	MStatus		undoIt();


protected:
    MDGModifier( void * );
	MDGModifier( const MDGModifier & other );
	MDGModifier&	operator =( const MDGModifier & rhs );
	void*		  data;
	bool		  fOwn;

private:
	static const  char*	className();


};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDGModifier */
