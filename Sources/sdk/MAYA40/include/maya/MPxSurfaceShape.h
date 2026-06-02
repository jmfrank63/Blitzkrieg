#ifndef LINUX
#pragma once
#endif
#ifndef _MPxSurfaceShape
#define _MPxSurfaceShape

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MBoundingBox.h>


 
class MDagPath;
class MSelectArgs;
class MSelectionList;
class MPointArray;
class MObjectArray;
class MSelectionMask;
class MAttributeSpecArray;
class MVectorArray;
class MDoubleArray;
class MPxGeometryIterator;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxSurfaceShape : public MPxNode  
{
public:
	MPxSurfaceShape();
	virtual ~MPxSurfaceShape();
	virtual MPxNode::Type type() const;



	virtual bool		    isBounded() const;
	virtual MBoundingBox    boundingBox() const; 

	virtual void		    transformUsing( const MMatrix& mat,
							    			const MObjectArray& componentList );

	enum MVertexOffsetMode {
		kNormal,
		kUTangent,
		kVTangent,
		kUVNTriad
	};
	virtual bool			vertexOffsetDirection( MObject & component,
                                                   MVectorArray & direction,
                                                   MVertexOffsetMode mode,
												   bool normalize );
	virtual void			componentToPlugs( MObject& component,
											  MSelectionList& selectionList
											) const;
	virtual bool			match( const MSelectionMask & mask,
					    		   const MObjectArray& componentList ) const;

	enum MatchResult {
		kMatchOk,
		kMatchNone,
		kMatchTooMany,
		kMatchInvalidName,
		kMatchInvalidAttribute,
		kMatchInvalidAttributeIndex,
		kMatchInvalidAttributeRange,
		kMatchInvalidAttributeDim
	};
	virtual MatchResult		matchComponent( const MSelectionList& item,
											const MAttributeSpecArray& spec,
											MSelectionList& list );

	virtual MObject			createFullVertexGroup() const;

	virtual bool deleteComponents( const MObjectArray& componentList,
								   MDoubleArray& undoInfo );
	virtual bool undeleteComponents( const MObjectArray& componentList,
									 MDoubleArray& undoInfo ); 

	virtual MObject 		localShapeInAttr() const;
	virtual MObject 		localShapeOutAttr() const;
	virtual MObject 		worldShapeOutAttr() const;

	virtual MObject			geometryData() const;

	virtual void			closestPoint( const MPoint& toThisPoint,
										  MPoint& theClosestPoint,
										  double tolerance );
	virtual bool			pointAtParm( const MPoint& atThisParm,
										  MPoint& evaluatedPoint );

	virtual	MPxGeometryIterator *
							geometryIteratorSetup( MObjectArray&, MObject&,
												   bool forReadOnly = false );
	virtual bool			acceptsGeometryIterator( bool  writeable=true );
	virtual bool			acceptsGeometryIterator( MObject&, 
													 bool writeable=true,
													 bool forReadOnly = false);


	MObjectArray 			activeComponents() const;
	bool					hasActiveComponents() const;

	enum MChildChanged { 
		kObjectChanged, 
		kBoundingBoxChanged 
	};
	void                    childChanged( MChildChanged = kObjectChanged );

    bool                    isRenderable() const;
	void			        setRenderable( bool );


	
	static MObject mHasHistoryOnCreate;

	static MObject mControlPoints;
		static MObject mControlValueX;
		static MObject mControlValueY;
		static MObject mControlValueZ;

	static MObject nodeBoundingBox;
	    static MObject nodeBoundingBoxMin;
	        static MObject nodeBoundingBoxMinX;
	        static MObject nodeBoundingBoxMinY;
	        static MObject nodeBoundingBoxMinZ;
	    static MObject nodeBoundingBoxMax;
	        static MObject nodeBoundingBoxMaxX;
	        static MObject nodeBoundingBoxMaxY;
	        static MObject nodeBoundingBoxMaxZ;
	    static MObject nodeBoundingBoxSize;
	        static MObject nodeBoundingBoxSizeX;
	        static MObject nodeBoundingBoxSizeY;
	        static MObject nodeBoundingBoxSizeZ;
	static MObject center;
	    static MObject boundingBoxCenterX;
	    static MObject boundingBoxCenterY;
	    static MObject boundingBoxCenterZ;
	static MObject matrix;
	static MObject inverseMatrix;
	static MObject worldMatrix;
	static MObject worldInverseMatrix;
	static MObject parentMatrix;
	static MObject parentInverseMatrix;
	static MObject visibility;
	static MObject intermediateObject;
	static MObject isTemplated;
	static MObject instObjGroups;
	    static MObject objectGroups;
	        static MObject objectGrpCompList;
	        static MObject objectGroupId;
	        static MObject objectGroupColor;
	static MObject useObjectColor;
	static MObject objectColor;

protected:
	  
private:
	static void				initialSetup();
	static const char*	    className();



};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxSurfaceShape */
