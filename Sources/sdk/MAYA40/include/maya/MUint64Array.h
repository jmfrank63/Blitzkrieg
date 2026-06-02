#ifndef LINUX
#pragma once
#endif
#ifndef _MUint64Array
#define _MUint64Array

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
  Implement an array of MUint64 data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MUint64Array  
{
public:
					MUint64Array();
					MUint64Array( const MUint64Array& other );
					MUint64Array( const MUint64 src[], unsigned count );
					MUint64Array( unsigned initialSize, 
								  MUint64 initialValue = 0 );
					~MUint64Array();
 	MUint64			operator[]( unsigned index ) const;
 	MUint64 &	 	operator[]( unsigned index );
 	MUint64Array &  operator=( const MUint64Array & other );
	MStatus			set( MUint64 element, unsigned index );
 	unsigned        length() const;
 	MStatus         remove( unsigned index );
 	MStatus         insert( MUint64 element, unsigned index );
 	MStatus         append( MUint64 element );
 	MStatus         copy( const MUint64Array& source );
 	MStatus		 	clear();
	MStatus			get( MUint64[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MUint64Array &array);

protected:

private:
	MUint64Array( void* );

 	void * fArray;
	bool   fOwn;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MUint64Array */
