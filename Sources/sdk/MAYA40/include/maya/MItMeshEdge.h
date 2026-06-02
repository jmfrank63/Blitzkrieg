#ifndef LINUX
#pragma once
#endif
#ifndef _MItMeshEdge
#define _MItMeshEdge

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MPoint.h>



class MPointArray;
class MDoubleArray;
class MIntArray;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItMeshEdge
{
public:
    MItMeshEdge( MObject & polyObject, MStatus * ReturnStatus = NULL );
    MItMeshEdge( const MDagPath &polyObject,
					MObject & component = MObject::kNullObj,
					MStatus * ReturnStatus = NULL );
	virtual ~MItMeshEdge();
    bool        isDone( MStatus * ReturnStatus = NULL );
    MStatus     next();
    MStatus     reset();
    MStatus     reset( MObject & polyObject );
    MStatus     reset( const MDagPath &polyObject,
						MObject & component = MObject::kNullObj );
    int        count( MStatus * ReturnStatus = NULL );
    MPoint      center( MSpace::Space space = MSpace::kObject,
						MStatus * ReturnStatus = NULL );
    MPoint      point( int index, MSpace::Space space = MSpace::kObject,
						MStatus * ReturnStatus = NULL );
    MStatus     setPoint( const MPoint & point, unsigned int index,
						MSpace::Space space = MSpace::kObject );
	bool		isSmooth( MStatus * ReturnStatus = NULL ) const;
	MStatus		setSmoothing( bool smooth = true );
	MStatus		cleanupSmoothing();
    int         index( int index, MStatus * ReturnStatus = NULL );
    int         index( MStatus * ReturnStatus = NULL ) const;
	MObject		edge( MStatus * ReturnStatus = NULL );
    MStatus     updateSurface();
    MStatus     geomChanged();
	MStatus		setIndex(int index, int &prevIndex );
	int			getConnectedFaces(MIntArray & faceList,
								  MStatus * ReturnStatus = NULL ) const;
	int			getConnectedEdges(MIntArray & edgeList,
								  MStatus * ReturnStatus = NULL ) const;
	MStatus		numConnectedFaces(int &faceCount ) const;
	MStatus		numConnectedEdges(int &edgeCount ) const;
	bool		connectedToFace( int index, MStatus * ReturnStatus = NULL);
	bool		connectedToEdge( int index, MStatus * ReturnStatus = NULL);
	bool		onBoundary(MStatus * ReturnStatus = NULL );
	MStatus		getLength(double &length,
						MSpace::Space space = MSpace::kObject );

protected:

private:
    static const char*  className();
    void     *       f_it;
    MPtrBase *       f_shape;
    void	 *       f_path;
    void	 *       f_geom;
	void     *       fElements;
	int             fCurrentElement;
	int             fMaxElements;
	int			 fCurrentIndex;
	void	*		 f_edge;
	void	*		 f_ref;
	bool			 fDirectIndex;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItMeshEdge */
