#ifndef LINUX
#pragma once
#endif
#ifndef _MAttributeSpecArray
#define _MAttributeSpecArray

#if defined __cplusplus



#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MAttributeSpec;



/**
  Implement an array of MAttributeSpecs data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MAttributeSpecArray  
{

public:
	MAttributeSpecArray();
	MAttributeSpecArray( const MAttributeSpecArray& other );
	~MAttributeSpecArray();

	MAttributeSpec	operator[]( unsigned index ) const;
 	MAttributeSpecArray & operator=( const MAttributeSpecArray & other );

	MStatus			set( const MAttributeSpec& element, unsigned index );
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MAttributeSpec & element, unsigned index );
	MStatus			append( const MAttributeSpec & element );
	MStatus			clear();
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;

protected:

private:
	MAttributeSpecArray( void* );



	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAttributeSpecArray */
