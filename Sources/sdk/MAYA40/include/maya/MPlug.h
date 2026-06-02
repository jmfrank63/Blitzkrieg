#ifndef LINUX
#pragma once
#endif
#ifndef _MPlug
#define _MPlug
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MDGContext.h>
#include <maya/MObject.h>
#include <maya/MIntArray.h>



class MString;
class MPlugArray;
class MTime;
class MPxData;
class MAngle;
class MDistance;



/**
  Methods for creating and accessing plugs and attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPlug  
{
public:
	MPlug();
	MPlug( const MPlug& in );
	MPlug( const MObject & node, const MObject & attribute );
	~MPlug();

	MStatus     setAttribute (MObject &attribute);
	MObject		attribute( MStatus* ReturnStatus = NULL ) const;
	MObject 	node( MStatus* ReturnStatus = NULL ) const;
	MString		name( MStatus* ReturnStatus = NULL ) const;
	bool		isNetworked( MStatus* ReturnStatus = NULL ) const;
	bool        isArray( MStatus* ReturnStatus = NULL ) const;
	bool        isElement( MStatus* ReturnStatus = NULL) const;
	bool        isCompound( MStatus* ReturnStatus = NULL ) const;
	bool        isChild( MStatus* ReturnStatus = NULL ) const;
	unsigned	logicalIndex( MStatus* ReturnStatus = NULL ) const;
	MStatus	    selectAncestorLogicalIndex( unsigned index,
											const MObject &attribute =
											MObject::kNullObj);

	unsigned    getExistingArrayAttributeIndices( MIntArray& indices, 
												  MStatus* ReturnStatus = NULL);

	unsigned    numElements( MStatus* ReturnStatus = NULL ) const;
	unsigned    evaluateNumElements( MStatus* ReturnStatus = NULL );
	unsigned    numChildren( MStatus* ReturnStatus = NULL ) const;
	unsigned    numConnectedElements( MStatus* ReturnStatus = NULL ) const;
	unsigned    numConnectedChildren( MStatus* ReturnStatus = NULL ) const;
	MPlug		child(	MObject& attr, MStatus* ReturnStatus = NULL ) const;
	MPlug		child(	unsigned index, MStatus* ReturnStatus = NULL ) const;
	MPlug		parent( MStatus* ReturnStatus = NULL ) const;
	MPlug       array( MStatus* ReturnStatus = NULL ) const;
	MPlug       elementByLogicalIndex( unsigned logicalIndex,
									   MStatus* ReturnStatus = NULL)
                const;
	MPlug       elementByPhysicalIndex( unsigned physicalIndex,
										MStatus* ReturnStatus = NULL)
                const;
	MPlug       connectionByPhysicalIndex( unsigned physicalIndex,
										MStatus* ReturnStatus = NULL)
                const;
	bool		connectedTo( MPlugArray & array, bool asDst, bool asSrc,
							 MStatus* ReturnStatus = NULL ) const;
	bool		isConnected( MStatus* ReturnStatus = NULL ) const;
	bool        isKeyable( MStatus* ReturnStatus = NULL ) const;
	MStatus     setKeyable( bool keyable );
	bool        isLocked( MStatus* ReturnStatus = NULL ) const;
	MStatus     setLocked( bool locked );
	bool		isNull( MStatus* ReturnStatus = NULL ) const;
	MString 	info( MStatus* ReturnStatus = NULL ) const;

	
	MStatus		getValue( MObject &val, MDGContext& ctx=MDGContext::fsNormal )
				const; 
	MStatus		getValue( double&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( float&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( int&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( short&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( bool&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( MDistance&,
						  MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( MAngle&,
						  MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( MTime&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		getValue( char&, MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus     getValue( MString&,
						  MDGContext& ctx=MDGContext::fsNormal ) const;
	MStatus		setValue( MObject & val );
	MStatus		setValue( MPxData * data );
	MStatus		setValue( double );
	MStatus		setValue( float );
	MStatus		setValue( int );
	MStatus		setValue( short );
	MStatus		setValue( bool );
	MStatus		setValue( MDistance& ); 
	MStatus		setValue( MAngle& );
	MStatus		setValue( MTime& );
	MStatus		setValue( char );
	MStatus     setValue( MString& );
	MStatus     setValue( const char* );


	MPlug&		operator =( const MPlug& other );
	MPlug		operator[] ( MObject& attr ) const; // child(attr)
	MPlug		operator[] ( unsigned physicalIndex ) const;	// index(index)
	bool		operator!() const;						// false if valid
	bool		operator ==( const MPlug &other ) const;
	bool        operator ==( const MObject &other ) const;
	bool        operator !=( const MPlug &other ) const;
	bool        operator !=( const MObject &other ) const;
	operator	MObject() const;					// attribute()

protected:

private:
	const char*	 className() const;












	MPlug( const void*, bool );
	const void*	 fPlug;
	bool         ownPlug;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPlug */
