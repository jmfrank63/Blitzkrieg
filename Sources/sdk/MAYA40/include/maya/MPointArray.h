#ifndef LINUX
#pragma once
#endif
#ifndef _MPointArray
#define _MPointArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>
#include <maya/MPoint.h>






/**
  Implement an array of MPoint data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPointArray  
{
public:
					MPointArray();
					MPointArray( const MPointArray& other );
					MPointArray( const MPoint points[], unsigned count );
					MPointArray( const double points[][4], unsigned count );
					MPointArray( const float points[][4], unsigned count );
					MPointArray( unsigned initialSize, 
								 const MPoint &initialValue 
								 = MPoint::origin );
					~MPointArray();
	const MPoint&	operator[]( unsigned index ) const;
	MPoint&	        operator[]( unsigned index );
 	MPointArray &   operator=( const MPointArray & other );
	MStatus			set( const MPoint& element, unsigned index);
	MStatus 		set( unsigned index, double x, double y, double z=0, 
						 double w=1);
	MStatus			set( double element[4], unsigned index);
	MStatus			set( float element[4], unsigned index);
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MPoint & element, unsigned index );
	MStatus			append( const MPoint & element );
	MStatus 		append( double x, double y, double z=0, double w=1 );
 	MStatus         copy( const MPointArray& source );
	MStatus			clear();
	MStatus			get( double [][4] ) const;
	MStatus			get( float [][4] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MPointArray &array);

protected:

private:


	MPointArray( void* );
	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPointArray */
