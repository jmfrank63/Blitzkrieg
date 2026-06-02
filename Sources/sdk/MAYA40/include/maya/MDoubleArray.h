#ifndef LINUX
#pragma once
#endif
#ifndef _MDoubleArray
#define _MDoubleArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
  Implement an array of doubles data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDoubleArray  
{
public:
					MDoubleArray();
					MDoubleArray( const MDoubleArray& other );
					MDoubleArray( const double src[], unsigned count );
					MDoubleArray( const float src[], unsigned count );
					MDoubleArray( unsigned initialSize, 
								  double initialValue = 0 );
					~MDoubleArray();
 	double			operator[]( unsigned index ) const;
 	double &	 	operator[]( unsigned index );
 	MDoubleArray &  operator=( const MDoubleArray & other );
	MStatus			set( double element, unsigned index );
	MStatus			set( float element, unsigned index );
 	unsigned        length() const;
 	MStatus         remove( unsigned index );
 	MStatus         insert( double element, unsigned index );
 	MStatus         append( double element );
 	MStatus         copy( const MDoubleArray& source );
 	MStatus		 	clear();
	MStatus			get( double[] ) const;
	MStatus			get( float[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MDoubleArray &array);

protected:

private:
	MDoubleArray( void* );

 	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDoubleArray */
