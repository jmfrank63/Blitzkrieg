#ifndef LINUX
#pragma once
#endif
#ifndef _MStringArray
#define _MStringArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MString;



/**
  Implement an array of MStrings data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MStringArray  
{

public:
				MStringArray();
				MStringArray( const MStringArray& other );
				MStringArray( const MString strings[], unsigned count );
				MStringArray( const char* strings[], unsigned count );
				MStringArray( unsigned initialSize, 
							  const MString &initialValue );
				~MStringArray();
	MString		operator[]( unsigned index ) const;
	MString&	operator[]( unsigned index );
 	MStringArray & operator=( const MStringArray & other );
	MStatus		set( const MString& element, unsigned index );
	MStatus		set( char* element, unsigned index );
	unsigned	length() const;
	MStatus		remove( unsigned index );
	MStatus		insert( const MString & element, unsigned index );
	MStatus		append( const MString & element );
	MStatus		clear();
	MStatus		get( MString array[] ) const;
	MStatus		get( char* array[] ) const;
	void		setSizeIncrement ( unsigned newIncrement );
	unsigned	sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MStringArray &array);

protected:

private:
	MStringArray( void* );


	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MStringArray */
