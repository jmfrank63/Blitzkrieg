#ifndef LINUX
#pragma once
#endif
#ifndef _MFileObject
#define _MFileObject

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MString;



/**
  Manipulate Unix filenames and search paths
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFileObject  
{

public:
				MFileObject();
				MFileObject( const MFileObject & other );
				virtual ~MFileObject();
	MFileObject& operator=( const MFileObject & other );
	MStatus   	setName( const MString & fileName );
	MStatus   	setRawPath( const MString & pathString );
	MStatus   	setFullName( const MString & fileName );
	MString  	name() const;
	MString  	rawPath() const;
	MString  	path() const;
	MString  	fullName() const;
	unsigned	pathCount();
	MString  	ithPath( unsigned index );
	MString  	ithFullName( unsigned index );
	bool        exists();
	bool        exists( unsigned index );
	bool        isSet() const;

protected:

private:
	void * data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFileObject */
