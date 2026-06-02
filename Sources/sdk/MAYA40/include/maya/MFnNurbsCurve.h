#ifndef LINUX
#pragma once
#endif
#ifndef _MFnNurbsCurve
#define _MFnNurbsCurve

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MObject.h>



class MPointArray;
class MDoubleArray;
class MVector;
class MPoint;
class MDagPath;
class MPtrBase;

#define kMFnNurbsEpsilon  1.0e-3



/**
  Manipulate NURBS curve objects
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnNurbsCurve : public MFnDagNode 
{

	declareDagMFn(MFnNurbsCurve, MFnDagNode);

public: 
	enum Form {
		kInvalid=0,
		kOpen,
		kClosed, 
		kPeriodic,
		kLast
	};
	MObject  	create(	const MPointArray &controlVertices,
						const MDoubleArray &knotSequences,
						unsigned int degree,
						Form agForm,
						bool create2D,
						bool createRational,
						MObject & parentOrOwner = MObject::kNullObj,
						MStatus* ReturnStatus = NULL );
	MObject     copy (const MObject &source,
					  MObject &parentOrOwner = MObject::kNullObj,
					  MStatus* ReturnStatus = NULL);
	MObject  	cv( unsigned index, MStatus * ReturnStatus = NULL ) const;
	MStatus		getCV( unsigned index, MPoint &pt,
						MSpace::Space space = MSpace::kObject ) const;
	MStatus		setCV( unsigned index, const MPoint &pt,
						MSpace::Space space = MSpace::kObject );
	Form		form( MStatus * ReturnStatus = NULL ) const;
	int			degree( MStatus * ReturnStatus = NULL ) const;
	int			numCVs( MStatus * ReturnStatus = NULL ) const;
	int			numSpans( MStatus * ReturnStatus = NULL ) const;
	int			numKnots( MStatus * ReturnStatus = NULL ) const;
	MStatus		getKnotDomain( double &start, double &end ) const;
	MStatus		getKnots( MDoubleArray &array ) const;
    MStatus     setKnots( const MDoubleArray &array, unsigned startIndex,
                          unsigned endIndex );
	MStatus		setKnot( unsigned index, double param );
	MObject  	cvs( unsigned startIndex, unsigned endIndex,
						MStatus * ReturnStatus = NULL ) const;
	MStatus		getCVs( MPointArray &array,
						MSpace::Space space = MSpace::kObject ) const;
	MStatus		setCVs( const MPointArray &array,
						MSpace::Space space = MSpace::kObject );
	double		knot( unsigned index, MStatus * ReturnStatus = NULL ) const;
	MStatus		removeKnot( double atThisParam, bool removeAll = false );
	bool		isPointOnCurve( const MPoint &point,
								double tolerance = kMFnNurbsEpsilon,
								MSpace::Space space = MSpace::kObject,
								MStatus * ReturnStatus = NULL ) const;
	MStatus		getPointAtParam( double param, MPoint &point,
								MSpace::Space space = MSpace::kObject
								) const;
	MStatus		getParamAtPoint( const MPoint & atThisPoint, double &param,
								 MSpace::Space space = MSpace::kObject )
								 const;
	bool		isParamOnCurve( double param,
								MStatus * ReturnStatus = NULL ) const;
	MVector		normal( double param,
						MSpace::Space space = MSpace::kObject,
						MStatus * ReturnStatus = NULL ) const;
	MVector		tangent( double param,
						 MSpace::Space space = MSpace::kObject,
						 MStatus * ReturnStatus = NULL ) const;
	bool		isPlanar( MVector * planeNormal = NULL,
							MStatus * ReturnStatus = NULL ) const;
	MPoint		closestPoint( const MPoint &toThisPoint, 
								double * param = NULL,
								double tolerance = kMFnNurbsEpsilon,
								MSpace::Space space = MSpace::kObject,
								MStatus * ReturnStatus = NULL ) const;
	double		distanceToPoint( const MPoint &pt,
								MSpace::Space space = MSpace::kObject,
								MStatus * ReturnStatus = NULL ) const;
	double		area( double tolerance = kMFnNurbsEpsilon,
							MStatus * ReturnStatus = NULL ) const;
	double		length( double tolerance = kMFnNurbsEpsilon,
							MStatus * ReturnStatus = NULL ) const;
	double		findParamFromLength( double partLength,
							MStatus * ReturnStatus = NULL ) const;
	bool        hasHistoryOnCreate( MStatus * ReturnStatus = NULL );
	MStatus     updateCurve();
protected:  
	virtual bool objectChanged( MFn::Type, MStatus * );
private:
	inline void * updateGeomPtr() const;
	inline void * updateConstGeomPtr() const;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnNurbsCurve */
