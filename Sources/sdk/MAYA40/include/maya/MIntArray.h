#ifndef LINUX
#pragma once
#endif
#ifndef _MIntArray
#define _MIntArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
  Implement an array of integers data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MIntArray  
{

public:
					MIntArray();
					MIntArray( const MIntArray& other );
					MIntArray( const int src[], unsigned count );
					MIntArray( unsigned initialSize, 
							   int initialValue = 0 );
					~MIntArray();
 	int				operator[]( unsigned index ) const;
 	int&	 		operator[]( unsigned index );
 	MIntArray &     operator=( const MIntArray & other );
	MStatus			set( int element, unsigned index );
 	unsigned        length() const;
 	MStatus			remove( unsigned index );
 	MStatus			insert( int element, unsigned index );
 	MStatus			append( int element );
 	MStatus         copy( const MIntArray& source );
 	MStatus		 	clear();
	MStatus			get( int[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MIntArray &array);

protected:

private:
	MIntArray( void* );

 	void* fArray;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MIntArray */
