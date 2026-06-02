#ifndef LINUX
#pragma once
#endif
#ifndef _MItGeometry
#define _MItGeometry

#if defined __cplusplus



#include <maya/MObject.h>



class MPoint;
class MDataHandle;
class MDagPath;




/**
  Iterate over lattice points/CV's/vertices of a geometry shape such as a mesh,
  nurbs surface, nurbs curve or lattice.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItGeometry
{
public:
	MItGeometry( const MDagPath& dagPath,
				 MStatus * ReturnStatus = NULL);
    MItGeometry( const MDagPath& dagPath,
                 MObject & component,
                 MStatus * ReturnStatus = NULL );
    MItGeometry( MObject& dagObject,
                 MStatus * ReturnStatus = NULL );
    MItGeometry( MDataHandle& dataHandle,
                 unsigned int groupId,
                 bool readOnly = true,
                 MStatus * ReturnStatus = NULL );
    MItGeometry( MDataHandle& dataHandle,
                 bool readOnly = true,
                 MStatus * ReturnStatus = NULL );
    virtual ~MItGeometry();
    bool        isDone( MStatus * ReturnStatus = NULL ) const;
    MStatus     next();
    MPoint      position( MSpace::Space space = MSpace::kObject,
                          MStatus * ReturnStatus = NULL ) const;
    MStatus     setPosition( const MPoint& point,
                             MSpace::Space space = MSpace::kObject  );
    int	    index( MStatus * ReturnStatus = NULL ) const;
	MObject		component( MStatus * ReturnStatus = NULL ) const;
	int		count( MStatus * ReturnStatus = NULL ) const;
	MStatus		reset( );

protected:

private:
	void 		createIterator(MDataHandle& dataHandle,
							   bool useComponents,
							   void* comps = NULL,
							   unsigned int groupId = 0,
							   bool readOnly = true,
							   MStatus* status = NULL);
	
private:
	void *      f_it;
    void *      f_path;
    void *      f_matrix;
    void *      f_clist;
	bool        f_readOnly;
	bool        f_own;
    
    static const char* className();    

	MItGeometry( void * ptr );
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItGeometry */
