#ifndef LINUX
#pragma once
#endif
#ifndef _MArgList
#define _MArgList

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MString.h>



class MVector;
class MPoint;
class MString;
class MVectorArray;
class MPointArray;
class MStringArray;
class MMatrix;
class MDoubleArray;
class MIntArray;
class MDistance;
class MAngle;
class MTime;



/**
  Manipulate argument lists.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MArgList  
{
public:
	static const unsigned kInvalidArgIndex;
					MArgList();
					MArgList( const MArgList& in );
					~MArgList();
	unsigned		length( MStatus* ReturnStatus= NULL ) const;
	bool			asBool(		unsigned index, MStatus* ret=NULL ) const;
	int				asInt(		unsigned index, MStatus* ret=NULL ) const;
	double			asDouble(	unsigned index, MStatus* ret=NULL ) const;
	MString			asString(	unsigned index, MStatus* ret=NULL ) const;
	MDistance		asDistance(	unsigned index, MStatus* ret=NULL ) const;
	MAngle			asAngle(	unsigned index, MStatus* ret=NULL ) const;
	MTime			asTime(		unsigned index, MStatus* ret=NULL ) const;
	MVector			asVector(	unsigned& indexReadAndUpdate,
								unsigned numElements=3,
								MStatus* ret=NULL ) const;
	MPoint			asPoint(	unsigned& indexReadAndUpdate,
							 unsigned numElements=3, MStatus* ret=NULL ) const;
	MStatus			get( unsigned index, bool&	) const;
	MStatus			get( unsigned index, int&		) const;
	MStatus			get( unsigned index, double&	) const;
	MStatus			get( unsigned index, MString&	) const;
	MStatus			get( unsigned index, MDistance& ) const;
	MStatus			get( unsigned index, MAngle& ) const;
	MStatus			get( unsigned index, MTime& ) const;
	MStatus			get( unsigned& indexReadAndUpdate, MVector&,
						 unsigned numElements=3 ) const;
	MStatus			get( unsigned& indexReadAndUpdate, MPoint&,
						 unsigned numElements=3 ) const;
	MStatus			get( unsigned& indexReadAndUpdate, MMatrix&	) const;
	MStatus			get( unsigned& indexReadAndUpdate, MIntArray& ) const;
	MStatus			get( unsigned& indexReadAndUpdate, MDoubleArray& ) const;
	MStatus         get( unsigned& indexReadAndUpdate, MStringArray& ) const;
	MArgList&		operator =( const MArgList& other );

	unsigned		flagIndex (	const char *shortFlag,
								const char *longFlag = NULL ) const;


	MStatus			addArg( bool arg );
	MStatus			addArg( int arg );
	MStatus			addArg( double arg );
	MStatus 		addArg( const MString & arg );
	MStatus 		addArg( const MDistance & arg );
	MStatus 		addArg( const MAngle & arg );
	MStatus 		addArg( const MTime & arg );
	MStatus			addArg( const MVector & arg );
	MStatus			addArg( const MPoint & arg );


protected:

private:
	const char*		className() const;



	MArgList ( const void*, unsigned offset );
	const void*		fArgList;
	unsigned		fOffset;
	bool			fOwn;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MArgList */
