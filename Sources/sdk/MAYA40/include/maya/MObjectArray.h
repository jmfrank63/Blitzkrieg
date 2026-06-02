#ifndef LINUX
#pragma once
#endif
#ifndef _MObjectArray
#define _MObjectArray

#if defined __cplusplus



#include <maya/MObject.h>
#include <maya/MStatus.h>





/**
  Implement an array of MObjects data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MObjectArray  
{

public:
					MObjectArray();
					MObjectArray( const MObjectArray& other );
					MObjectArray( const MObject src[], unsigned count );
					MObjectArray( unsigned initialSize, 
								  const MObject &initialValue 
								  = MObject::kNullObj );
					~MObjectArray();
 	const MObject&	operator[]( unsigned index ) const;
 	MObject&		operator[]( unsigned index ); 
 	MStatus			set( const MObject& element, unsigned index ); 
 	unsigned		length() const;
 	MStatus			remove( unsigned index );
 	MStatus			insert( const MObject & element, unsigned index );
 	MStatus			append( const MObject & element );
 	MStatus			clear();
	MStatus			get( MObject array[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;

protected:

private:

 	MObjectArray& operator = (const MObjectArray&) const;
 	MObjectArray& operator = (const MObjectArray&);
 	void* fArray;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MObjectArray */
