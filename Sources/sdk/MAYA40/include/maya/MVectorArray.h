#ifndef LINUX
#pragma once
#endif
#ifndef _MVectorArray
#define _MVectorArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



#include <maya/MVector.h>
#include <maya/MStatus.h>



/**
  Implement an array of MVectors data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MVectorArray  
{

public:
					MVectorArray();
					MVectorArray( const MVectorArray& other );
					MVectorArray( const MVector vectors[], unsigned count );
					MVectorArray( const double vectors[][3], unsigned count );
					MVectorArray( const float vectors[][3], unsigned count );
					MVectorArray( unsigned initialSize, 
								  const MVector &initialValue 
								  = MVector::zero );
					~MVectorArray();
 	const MVector&	operator[]( unsigned index ) const;
 	MVector &		operator[]( unsigned index );
 	MVectorArray &  operator=( const MVectorArray & other );
	MStatus			set( const MVector& element, unsigned index );
	MStatus			set( double element[3], unsigned index );
	MStatus			set( float element[3], unsigned index );
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MVector & element, unsigned index );
	MStatus			append( const MVector & element );
 	MStatus         copy( const MVectorArray& source );
	MStatus			clear();
	MStatus			get( double [][3] ) const;
	MStatus			get( float [][3] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MVectorArray &array);

protected:

private:
	MVectorArray( void* );

	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MVectorArray */
