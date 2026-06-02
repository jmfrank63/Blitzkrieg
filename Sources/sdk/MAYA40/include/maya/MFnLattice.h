#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLattice
#define _MFnLattice

#if defined __cplusplus



#include <maya/MFnDagNode.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MObject.h>
#include <maya/MTransformationMatrix.h>



class MPointArray;
class MDoubleArray; 
class MPoint;
class MVector;
class MDagPath;
class MPtrBase;



/**
  Function set for lattice shapes and lattice geometry
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnLattice : public MFnDagNode 
{
	declareDagMFn(MFnLattice, MFnDagNode);

public:
  
	MObject 	create( unsigned xDiv, unsigned yDiv, unsigned zDiv,
						MObject parentOrOwner = MObject::kNullObj,
						MStatus* ReturnStatus = NULL );
	MStatus     reset( double sSize = 1.0 , double tSize = 1.0,
						double uSize = 1.0 );
	MStatus     getDivisions( unsigned & s, unsigned & t, unsigned & u );
	MStatus     setDivisions( unsigned   s, unsigned   t, unsigned   u );
  
	MPoint &    point( unsigned s, unsigned t, unsigned u, 
					   MStatus* ReturnStatus = NULL );

protected: 
	virtual bool objectChanged( MFn::Type, MStatus * );
private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLattice */
