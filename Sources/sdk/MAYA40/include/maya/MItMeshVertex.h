#ifndef LINUX
#pragma once
#endif
#ifndef _MItMeshVertex
#define _MItMeshVertex

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MPoint.h>
#include <maya/MColor.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MVectorArray.h>
#include <maya/MColorArray.h>
#include <maya/MString.h>



class MPointArray;
class MDoubleArray;
class MIntArray;



/**
  Iterate of polygonal vertices
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItMeshVertex
{
public:   
	MItMeshVertex( MObject & polyObject, MStatus * ReturnStatus = NULL );
	MItMeshVertex( const MDagPath &polyObject,
						MObject & component = MObject::kNullObj,
						MStatus * ReturnStatus = NULL );
	virtual ~MItMeshVertex();
	bool		isDone( MStatus * ReturnStatus = NULL );
	MStatus		next(); 
	MStatus		reset();
	MStatus		reset( MObject & polyObject );
	MStatus		reset( const MDagPath &polyObject,
					MObject & component = MObject::kNullObj );

	int		count( MStatus * ReturnStatus = NULL );

	int		index( MStatus * ReturnStatus = NULL );
	MObject		vertex( MStatus * ReturnStatus = NULL );

	MPoint		position( MSpace::Space space = MSpace::kObject, 
					      MStatus * ReturnStatus = NULL );
	MStatus		setPosition( const MPoint & point, 
						  MSpace::Space space = MSpace::kObject );
	MStatus		translateBy( const MVector & vector, 
						  MSpace::Space space = MSpace::kObject );

	MStatus		getNormal( MVector & vector,
						  MSpace::Space space = MSpace::kObject );
	MStatus		getNormal( MVector & vector, int faceIndex,
						  MSpace::Space space = MSpace::kObject );
	MStatus		getNormals( MVectorArray & vectorArray,
						  MSpace::Space space = MSpace::kObject );
	MStatus		numUVs( int &count, const MString * uvSet = NULL);
	MStatus		setUV( float2 & uvPoint, const MString * uvSet = NULL);
	MStatus		getUV( float2 & uvPoint, const MString * uvSet = NULL);
	MStatus		setUV( int faceId, float2 & uvPoint, const MString * uvSet = NULL);
	MStatus		getUV( int faceId, float2 & uvPoint, const MString * uvSet = NULL) const;
	MStatus 	setUVs( MFloatArray& uArray, MFloatArray& vArray, MIntArray& faceIds,
						const MString * uvSet = NULL );
	MStatus 	getUVs( MFloatArray& uArray, MFloatArray& vArray, MIntArray& faceIds,
						const MString * uvSet = NULL) const;
	MStatus		updateSurface();
    MStatus     geomChanged();
	MStatus		setIndex(int index, int &prevIndex);
	MStatus		getConnectedFaces( MIntArray & faceList);
	MStatus		getConnectedEdges( MIntArray & edgeList );
	MStatus		getConnectedVertices( MIntArray & vertexList );
	MStatus		numConnectedFaces(int &faceCount ) const;
	MStatus		numConnectedEdges(int &edgeCount ) const;
	bool		connectedToFace( int faceIndex, MStatus * ReturnStatus = NULL);
	bool		connectedToEdge( int edgeIndex, MStatus * ReturnStatus = NULL);
	MStatus		getOppositeVertex( int &vertexId, int edgeId);
	bool        onBoundary(MStatus * ReturnStatus = NULL );

	bool		hasColor(MStatus * ReturnStatus = NULL ) const;
	bool		hasColor(int faceIndex, MStatus * ReturnStatus = NULL ) const;
	MStatus		getColor(MColor &color, int index) const;
	MStatus		getColor(MColor &color) const;
	MStatus		getColors(MColorArray &colors) const;

protected:
    bool		getUVSetIndex( const MString * uvSetName,
								   int & uvSet) const;

private:
	static const char* 	className();
	void     *       f_it;
	MPtrBase *       f_shape;
	void     *       f_path; 
	void     *       f_geom;
	void     *       fElements;
	int             fCurrentElement;
	int             fMaxElements;
	int			 fCurrentIndex;
	void	*		 f_vertex;
	void	*		 f_ref;
	bool			 fDirectIndex;
}; 

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItMeshVertex */



