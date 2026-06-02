#ifndef LINUX
#pragma once
#endif
#ifndef _MFnBlendShapeDeformer
#define _MFnBlendShapeDeformer

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MPoint.h>

class MDagPath;
class MObjectArray;
class MIntArray;



/**
 Function set for blend shape deformer
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnBlendShapeDeformer : public MFnDependencyNode
{

	declareMFn(MFnBlendShapeDeformer, MFnDependencyNode );

public:

	enum Origin {
		kLocalOrigin,
		kWorldOrigin
	};

	enum HistoryLocation {
		kFrontOfChain,
		kNormal
	};
	MObject  create(  MObject baseObject,
					  Origin originSpace = kLocalOrigin, 
					  MStatus * ReturnStatus = NULL );

	MObject  create(  MObjectArray baseObjects,
					  Origin originSpace = kLocalOrigin,
					  HistoryLocation = kNormal,
					  MStatus * ReturnStatus = NULL );

	MStatus  addBaseObject( MObject & object );
	MStatus  getBaseObjects( MObjectArray & objects ) const;
 
	MStatus  addTarget( const MObject & baseObject, int weightIndex,
						const MObject & newTarget, double fullWeight );

    MStatus  removeTarget( const MObject & baseObject,
						   int weightIndex,
						   const MObject & target, 
						   double fullWeight );
	
	MStatus  getTargets( MObject baseObject, int weightIndex,
						 MObjectArray & targetObjects ) const;

	unsigned numWeights( MStatus * ReturnStatus = NULL ) const;

	MStatus  weightIndexList( MIntArray& indexList ) const;

    MStatus  targetItemIndexList( unsigned targetIndex,
								  MObject baseObject,
								  MIntArray& inbetweens ) const;
	
	float    weight( unsigned index, MStatus * ReturnStatus = NULL ) const;
	MStatus  setWeight( unsigned index, float weight );

	float    envelope( MStatus * ReturnStatus = NULL ) const;
	MStatus  setEnvelope( float envelope );

	Origin   origin( MStatus * ReturnStatus = NULL ) const;
	MStatus  setOrigin( Origin space );
	
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnBlendShapeDeformer */
