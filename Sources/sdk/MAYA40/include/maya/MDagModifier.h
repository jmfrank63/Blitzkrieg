#ifndef LINUX
#pragma once
#endif
#ifndef _MDagModifier
#define _MDagModifier

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDGModifier.h>
#include <maya/MObject.h>





/**
  A class that is used to modify the DAG and also supports undo 
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDagModifier : public MDGModifier
{
public:
	MDagModifier();
	~MDagModifier();
    
    MObject     createNode(     const MTypeId &typeId, 
                                const MObject & parent =  MObject::kNullObj,
                                MStatus* ReturnStatus = NULL );
    MObject     createNode(     const MString &type,
                                const MObject & parent =  MObject::kNullObj,
                                MStatus* ReturnStatus = NULL );
    MStatus     reparentNode(   const MObject & node, 
                                const MObject & newParent = MObject::kNullObj );
 

protected:
	MDagModifier(void*);
	MDagModifier( const MDagModifier & other );
	MDagModifier&	operator =( const MDagModifier & rhs );

private:
	static const  char*	className();



};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDagModifier */
