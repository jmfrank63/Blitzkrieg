#ifndef LINUX
#pragma once
#endif
#ifndef _MDataHandle
#define _MDataHandle

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <string.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnData.h>
#include <maya/MTypeId.h>
#include <maya/MObject.h>

class MTime;
class MVector;
class MFloatVector;
class MMatrix;
class MFloatMatrix;
class MPlug;
class MPxData;
class MString;


/**
 An MDataHandle is a smart pointer into an MDataBlock.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDataHandle {

public:  
	MDataHandle();
	bool                 isNumeric() const;
    MFnNumericData::Type numericType() const;
    MFnData::Type        type() const;
    MTypeId              typeId() const;

	MObject              data();
	MStatus				 copy( const MDataHandle& src );

	void                 setClean();

	bool&			     asBool()	const;
	char&			     asChar()	const;
	unsigned char&       asUChar()	const;
	short&			     asShort()	const;
	int&			     asLong()	const;
	int&			     asInt()	const;
	float&			     asFloat()	const;
	double&		         asDouble()	const; 
    MTime                asTime()	const;
     
	short2&              asShort2()	const;
	long2&               asLong2()	const;
	int2&				 asInt2()	const;
	float2&              asFloat2()	const;
	double2&             asDouble2()const;
	short3&              asShort3()	const;
	long3&               asLong3()	const;
	int3&                asInt3()	const;
	float3&              asFloat3()  const;
	double3&             asDouble3() const;
	MVector&             asVector()  const;
	MFloatVector&        asFloatVector() const;
    MMatrix&             asMatrix()  const;
    MFloatMatrix&        asFloatMatrix()  const;
	MString&             asString() const;

    MObject              asNurbsCurve() const; 
    MObject              asNurbsSurface() const;
    MObject              asMesh() const;
    MObject              asSubdSurface() const; 
    MObject              asNurbsCurveTransformed() const;
    MObject              asNurbsSurfaceTransformed() const;
    MObject              asMeshTransformed() const;
    MObject              asSubdSurfaceTransformed() const;
    const MMatrix &      geometryTransformMatrix() const; 
	MPxData *            asPluginData() const;

    void                 set( bool );
    void                 set( char );
    void                 set( short );
    void                 set( int );
    void                 set( float );
    void                 set( double );
    void                 set( const MMatrix& );
    void                 set( const MFloatMatrix& );
    void                 set( const MVector& );
	void                 set( const MFloatVector& );
    void                 set( const MTime& );
	void                 set( short, short );
	void                 set( int, int );
	void                 set( float, float );
	void                 set( double, double );
	void                 set( short, short, short );
	void                 set( int, int, int );
	void                 set( float, float, float );
	void                 set( double, double, double );
	void                 set( const MString  &);

	MStatus              set( const MObject &data );
	MStatus              set( MPxData * data );

    MDataHandle          child( const MPlug & plug );
    MDataHandle          child( const MObject & attribute );

	MDataHandle& operator=( const MDataHandle& other );
    MDataHandle( const MDataHandle & );

protected:

private:

	friend class MDataBlock;
	friend class MArrayDataBuilder;
	friend class MArrayDataHandle;
	friend class MItGeometry;
	const char*	className() const;
	MDataHandle( void* );
	char f_data[16];
};

inline MDataHandle::MDataHandle( const MDataHandle &other )
{
	memcpy( this, &other, sizeof(MDataHandle) ); 
}

inline MDataHandle& MDataHandle::operator=( const MDataHandle& other )
{
	memcpy( this, &other, sizeof(MDataHandle) );
	return *this;
}

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDataHandle */
