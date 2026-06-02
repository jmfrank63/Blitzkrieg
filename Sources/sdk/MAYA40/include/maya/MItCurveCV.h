#ifndef LINUX
#pragma once
#endif
#ifndef _MItCurveCV
#define _MItCurveCV

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>



class MPointArray;
class MDoubleArray;
class MVector;
class MPoint;
class MDagPath;
class MPtrBase;



/**
  Iterate over CVs of a NURBS curve.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItCurveCV
{
public:
    MItCurveCV( MObject & curve, MStatus * ReturnStatus = NULL );
    MItCurveCV( const MDagPath & curve,
				MObject & component = MObject::kNullObj,
                MStatus * ReturnStatus = NULL );
    virtual ~MItCurveCV();
    bool        isDone( MStatus * ReturnStatus = NULL ) const;
    MStatus     next();
    MStatus     reset();
    MStatus     reset( MObject & curve );
    MStatus     reset( const MDagPath & curve,
						MObject & component = MObject::kNullObj );
    MPoint      position( MSpace::Space space = MSpace::kObject,
                          MStatus * ReturnStatus = NULL ) const;
    MStatus     setPosition( const MPoint & pt,
							 MSpace::Space space = MSpace::kObject );
    MStatus     translateBy( const MVector & vec,
							 MSpace::Space space = MSpace::kObject );
    int	    index( MStatus * ReturnStatus = NULL ) const;
	MObject		cv( MStatus * ReturnStatus = NULL ) const;

	bool        hasHistoryOnCreate( MStatus * ReturnStatus = NULL ) const;
    MStatus     updateCurve();

protected:

private:
    static const char* className();
	inline void * updateGeomPtr() const;
    MPtrBase * f_shape;
    MPtrBase * f_geom;
    void *     f_path;
	void *     f_it;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItCurveCV */
