#ifndef LINUX
#pragma once
#endif
#ifndef _MFloatPointArray
#define _MFloatPointArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>
#include <maya/MFloatPoint.h>






/**
  Implement an array of MFloatPoint data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFloatPointArray  
{
public:
	MFloatPointArray();
	MFloatPointArray( const MFloatPointArray& other );
	MFloatPointArray( const MFloatPoint points[], unsigned count );
	MFloatPointArray( const float points[][4], unsigned count );
	MFloatPointArray( const double points[][4], unsigned count );
	MFloatPointArray( unsigned initialSize, 
					  const MFloatPoint &initialValue 
					  = MFloatPoint::origin );
	~MFloatPointArray();

	const MFloatPoint&	operator[]( unsigned index ) const;
	MFloatPoint&	    operator[]( unsigned index );
 	MFloatPointArray &  operator=( const MFloatPointArray & other );
	MStatus			set( const MFloatPoint& element, unsigned index);
	MStatus 		set( unsigned index, float x, float y, float z=0, 
						 float w=1);
	MStatus			set( float element[4], unsigned index);
	MStatus			set( double element[4], unsigned index);
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MFloatPoint & element, unsigned index );
	MStatus			append( const MFloatPoint & element );
	MStatus 		append( float x, float y, float z=0, float w=1 );
 	MStatus         copy( const MFloatPointArray& source );
	MStatus			clear();
	MStatus			get( float [][4] ) const;
	MStatus			get( double [][4] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MFloatPointArray &array);

protected:

private:


	MFloatPointArray( void* );
	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFloatPointArray */
