#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDagNode
#define _MFnDagNode

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <stdlib.h>



class MMatrix;
class MDagPath;
class MDagPathArray;
class MBoundingBox;



/**

Provides methods for attaching Function Sets to, querying, and adding
children to DAG Nodes.  Particularly useful when used in conjunction
with the DAG Iterator class (MItDag).

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnDagNode : public MFnDependencyNode 
{

	declareMinimalMFn( MFnDagNode );

public:

	enum {
		kNextPos = 0xff
	};

	MFnDagNode();
	MFnDagNode( MObject & object, MStatus * ret = NULL );
	MFnDagNode( const MObject & object, MStatus * ret = NULL );
	MFnDagNode( const MDagPath & object, MStatus * ret = NULL );

    MObject         create( const MTypeId &typeId,
							MObject &parent = MObject::kNullObj,
							MStatus* ReturnStatus = NULL );
    MObject         create( const MTypeId &typeId,
							const MString &name,
							MObject &parent = MObject::kNullObj,
							MStatus* ReturnStatus = NULL );

    MObject         create( const MString &type,
							MObject &parent = MObject::kNullObj,
							MStatus* ReturnStatus = NULL );
    MObject         create( const MString &type,
							const MString &name,
							MObject &parent = MObject::kNullObj,
							MStatus* ReturnStatus = NULL );

	unsigned int	parentCount( MStatus * ReturnStatus = NULL ) const;
	MObject 		parent( unsigned int i,
							MStatus * ReturnStatus = NULL ) const;
	MStatus	        addChild( MObject & child, unsigned int index = kNextPos );
	MStatus			removeChild( MObject & child );
	MStatus			removeChildAt( unsigned int index );
	unsigned int    childCount(  MStatus * ReturnStatus = NULL ) const;
	MObject 	    child( unsigned int i,
						   MStatus * ReturnStatus = NULL ) const;
	MObject  		dagRoot( MStatus * ReturnStatus = NULL );
	bool			hasParent( const MObject & node,
							   MStatus * ReturnStatus = NULL ) const;
	bool			hasChild (const MObject& node,
							  MStatus * ReturnStatus = NULL ) const;
	bool			isChildOf (const MObject& node,
							   MStatus * ReturnStatus = NULL ) const;
	bool			isParentOf (const MObject& node,
								MStatus * ReturnStatus = NULL ) const;
	bool			inUnderWorld ( MStatus * ReturnStatus = NULL ) const;
	bool			isInstanced( bool indirect = true,
						           MStatus * ReturnStatus = NULL ) const;
	unsigned int	instanceCount( bool total,
						           MStatus * ReturnStatus = NULL ) const;
	MObject			duplicate( bool instance = false,
					           bool instanceLeaf = false,
					           MStatus * ReturnStatus = NULL ) const;
	MStatus		    getPath( MDagPath& path );
	MStatus		    getAllPaths( MDagPathArray& paths );
	MString         fullPathName(MStatus *ReturnStatus = NULL);
    MString         partialPathName(MStatus *ReturnStatus = NULL);
	MMatrix			transformationMatrix( MStatus * ReturnStatus = NULL ) const;

	bool            isIntermediateObject( MStatus * ReturnStatus = NULL ) const;
	MStatus         setIntermediateObject( bool isIntermediate );

	int				objectColor( MStatus * ReturnStatus = NULL ) const;
	MStatus			setObjectColor( int color );
	bool			usingObjectColor ( MStatus * ReturnStatus = NULL ) const;
	MStatus			setUseObjectColor( bool useObjectColor );

	MBoundingBox	boundingBox( MStatus * ReturnStatus = NULL ) const;

	MDagPath		dagPath( MStatus * ReturnStatus = NULL ) const;
	virtual MStatus setObject( const MDagPath & path );
 	virtual MStatus setObject( MObject & object );
 	virtual MStatus setObject( const MObject & object );

	MObject         model( MStatus * ReturnStatus = NULL ) const;

protected:
	void * f_path; 
	void * f_xform;
	void * f_data1; 
	void * f_data2; 
private:

};
#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#define declareDagMFn( MFnClass, MFnParentClass )			  	 	\
	declareMinimalMFn( MFnClass );								 	\
	public:	        											 	\
		MFnClass();											     	\
		MFnClass( MObject & object, MStatus * ret = NULL );	   	 	\
		MFnClass( const MObject & object, MStatus * ret = NULL );	\
		MFnClass( const MDagPath & object, MStatus * ret = NULL )

#endif /* __cplusplus */
#endif /* _MFnDagNode */
