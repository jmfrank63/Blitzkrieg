#ifndef LINUX
#pragma once
#endif
#ifndef _MPlugArray
#define _MPlugArray

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MPlug;



/**
  Implement an array of MPlugs data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPlugArray  
{

public: 
					MPlugArray();
					MPlugArray( const MPlugArray& other );
					MPlugArray( const MPlug src[], unsigned count );
					~MPlugArray();
	MPlug			operator[]( unsigned index ) const;
	MPlug			operator[]( unsigned index );
	MPlugArray&		operator=( const MPlugArray & other );
	MStatus			set( const MPlug& element, unsigned index );
	unsigned		length() const;
	MStatus			remove( unsigned index );
	MStatus			insert( const MPlug & element, unsigned index );
	MStatus			append( const MPlug & element );
	MStatus			clear();
	MStatus			get( MPlug dest[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;

protected:

private:
	MPlugArray ( void * );
	friend class MPlug;


	void * arr;
	bool   own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPlugArray */
