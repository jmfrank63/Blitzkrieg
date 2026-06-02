#ifndef LINUX
#pragma once
#endif
#ifndef _MTimeArray
#define _MTimeArray

#if defined __cplusplus



#include <maya/MTime.h>
#include <maya/MStatus.h>





/**
  Implement an array of MTime data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MTimeArray  
{

public:
					MTimeArray();
					MTimeArray( const MTimeArray& other );
					MTimeArray( const MTime src[], unsigned count );
					MTimeArray( unsigned initialSize, 
								const MTime &initialValue );
					~MTimeArray();
 	const MTime&	operator[]( unsigned index ) const;
 	MTime&			operator[]( unsigned index ); 
 	MStatus			set( const MTime& element, unsigned index ); 
 	unsigned		length() const;
 	MStatus			remove( unsigned index );
 	MStatus			insert( const MTime & element, unsigned index );
 	MStatus			append( const MTime & element );
 	MStatus			clear();
	MStatus			get( MTime array[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MTimeArray &array);

protected:

private:

 	MTimeArray& operator = (const MTimeArray&) const;
 	MTimeArray& operator = (const MTimeArray&);
 	void* fArray;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MTimeArray */
