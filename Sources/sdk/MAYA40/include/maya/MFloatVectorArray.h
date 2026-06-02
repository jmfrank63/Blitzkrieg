#ifndef LINUX
#pragma once
#endif
#ifndef _MFloatVectorArray
#define _MFloatVectorArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



#include <maya/MFloatVector.h>



/**
  Implement an array of MFloatVectors data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFloatVectorArray  
{

public:
	MFloatVectorArray();
	MFloatVectorArray( const MFloatVectorArray& other );
	MFloatVectorArray( const MFloatVector vectors[], unsigned count );
	MFloatVectorArray( const double vectors[][3], unsigned count );
	MFloatVectorArray( const float vectors[][3], unsigned count );
	MFloatVectorArray( unsigned initialSize, 
					   const MFloatVector &initialValue 
					   = MFloatVector::zero );
	~MFloatVectorArray();

 	const MFloatVector&	operator[]( unsigned index ) const;
 	MFloatVector &		operator[]( unsigned index );
 	MFloatVectorArray &  operator=( const MFloatVectorArray & other );
	bool			set( const MFloatVector& element, unsigned index );
	bool			set( double element[3], unsigned index );
	bool			set( float element[3], unsigned index );
	unsigned		length() const;
	bool			remove( unsigned index );
	bool			insert( const MFloatVector & element, unsigned index );
	bool			append( const MFloatVector & element );
 	MStatus         copy( const MFloatVectorArray& source );
	bool			clear();
	bool			get( double [][3] ) const;
	bool			get( float [][3] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MFloatVectorArray &array);

protected:

private:
	MFloatVectorArray( void* );

	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFloatVectorArray */
