#ifndef LINUX
#pragma once
#endif
#ifndef _MPxNode
#define _MPxNode

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>

#include <maya/MTypeId.h>
#include <maya/MDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MPlug.h>
#include <maya/MString.h>
#include <maya/MDGContext.h>





/**
  Create user defined dependency nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxNode  
{
public:
	enum Type {
        kDependNode,
		kLocatorNode,
		kDeformerNode,
		kManipContainer,
		kSurfaceShape,
		kFieldNode,
		kEmitterNode,
		kSpringNode,
		kIkSolverNode,
		kHwShaderNode,
		kLast
    }; 

	MPxNode();
	virtual ~MPxNode();
	virtual void			postConstructor();
	virtual MStatus			compute( const MPlug& plug,
									 MDataBlock& dataBlock );

	virtual bool			getInternalValue( const MPlug&,
											  MDataHandle&);
    virtual bool			setInternalValue( const MPlug&,
											  const MDataHandle&);
	
	virtual MStatus			legalConnection( const MPlug& plug,
											 const MPlug& otherPlug,
											 bool asSrc,
											 bool& result ) const;
	virtual MStatus			legalDisconnection( const MPlug& plug,
											 const MPlug& otherPlug,
											 bool asSrc,
											 bool& result ) const;
	virtual MStatus			connectionMade( const MPlug& plug,
											 const MPlug& otherPlug,
											 bool asSrc );
	virtual MStatus			connectionBroken( const MPlug& plug,
											 const MPlug& otherPlug,
											 bool asSrc );
	virtual MStatus			shouldSave( const MPlug& plug, bool& result );

	MTypeId					typeId() const;
	MString					typeName() const;
	MString					name() const; 
	virtual Type            type() const;
	virtual bool			isAbstractClass  () const;
	MObject                 thisMObject() const;

	static MStatus          addAttribute( const MObject & attr );
	static MStatus		    inheritAttributesFrom(
											const MString & parentClassName );
	static MStatus          attributeAffects( const MObject & whenChanges,
											  const MObject & isAffected );

	MStatus					setExistWithoutInConnections( bool flag );
	bool					existWithoutInConnections(
								MStatus* ReturnStatus = NULL ) const;
	MStatus					setExistWithoutOutConnections( bool flag );
	bool					existWithoutOutConnections(
								MStatus* ReturnStatus = NULL ) const;


	static MObject          message;
	static MObject          isHistoricallyInteresting;
	static MObject          caching;
	static MObject          state;

protected:
	MDataBlock				forceCache( MDGContext& ctx=MDGContext::fsNormal );

	void					setMPSafe ( bool flag );

	void*					instance;
	  
private:
	static void				initialSetup();
	static const char*	    className();



	static void*            initClass;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxNode */
