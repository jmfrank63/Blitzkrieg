#ifndef LINUX
#pragma once
#endif
#ifndef _MFnWireDeformer
#define _MFnWireDeformer

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MPoint.h>

class MDagPath;
class MObjectArray;



/**
 Function set for wire deformer
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnWireDeformer : public MFnDependencyNode
{

	declareMFn(MFnWireDeformer, MFnDependencyNode );

public:

	MObject  create( MStatus * ReturnStatus = NULL );


	MStatus addGeometry( const MObject & object );
	MStatus removeGeometry( const MObject & object );
	MStatus getAffectedGeometry( MObjectArray & objects );

	unsigned numWires( MStatus * ReturnStatus = NULL ) const;
	MStatus  addWire( const MObject & object );
	MObject  wire( unsigned wireIndex, MStatus * ReturnStatus = NULL );
	float    wireDropOffDistance( unsigned wireIndex, 
								  MStatus * ReturnStatus = NULL ) const;
	MStatus  setWireDropOffDistance( unsigned wireIndex, float dropOff );
	float    wireScale( unsigned wireIndex, 
								  MStatus * ReturnStatus = NULL ) const;
	MStatus  setWireScale( unsigned wireIndex, float scale );
	MObject  holdingShape( unsigned wireIndex, 
						          MStatus * ReturnStatus = NULL ) const;
	MStatus  setHoldingShape( unsigned wireIndex, MObject holdingCurve ); 
	
	float    envelope( MStatus * ReturnStatus = NULL ) const;
	MStatus  setEnvelope( float envelope );
	float    rotation( MStatus * ReturnStatus = NULL ) const;
	MStatus  setRotation( float rotation );
	float    localIntensity( MStatus * ReturnStatus = NULL ) const;
	MStatus  setLocalIntensity( float localIntensity );
	float    crossingEffect( MStatus * ReturnStatus = NULL ) const;
	MStatus  setCrossingEffect( float crossingEffect );

	unsigned numDropoffLocators( unsigned wireIndex, 
								 MStatus * ReturnStatus = NULL ) const; 
	MStatus  setDropoffLocator( unsigned wireIndex, unsigned locatorIndex,
						         float param, float percentage );
	MStatus  getDropoffLocator( unsigned wireIndex, unsigned locatorIndex,
						         float &param, float &percentage );
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnWireDeformer */
