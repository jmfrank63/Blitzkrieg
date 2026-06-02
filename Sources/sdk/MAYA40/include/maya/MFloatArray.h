#ifndef LINUX
#pragma once
#endif
#ifndef _MFloatArray
#define _MFloatArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
  Implement an array of floats data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFloatArray  
{
public:
					MFloatArray();
					MFloatArray( const MFloatArray& other );
					MFloatArray( const float src[], unsigned count );
					MFloatArray( const double src[], unsigned count );
					MFloatArray( unsigned initialSize, 
								 float initialValue = 0 );
					~MFloatArray();
 	float			operator[]( unsigned index ) const;
 	float &	 	    operator[]( unsigned index );
 	MFloatArray &   operator=( const MFloatArray & other );
	MStatus			set( float element, unsigned index );
	MStatus			set( double element, unsigned index );
 	unsigned        length() const;
 	MStatus         remove( unsigned index );
 	MStatus         insert( float element, unsigned index );
 	MStatus         append( float element );
 	MStatus         copy( const MFloatArray& source );
 	MStatus		 	clear();
	MStatus			get( float[] ) const;
	MStatus			get( double[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MFloatArray &array);

protected:

private:
	MFloatArray( void* );

 	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFloatArray */
