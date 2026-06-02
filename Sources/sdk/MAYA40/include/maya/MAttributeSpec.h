#ifndef LINUX
#pragma once
#endif
#ifndef _MAttributeSpec
#define _MAttributeSpec

#if defined __cplusplus




#include <maya/MTypes.h>



class MAttributeIndex;
class MString;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MAttributeSpec {
public:
	MAttributeSpec();
	MAttributeSpec( const char * );
	MAttributeSpec( const MString & );
	MAttributeSpec( const MAttributeSpec & );
	~MAttributeSpec();

public:
	const MString 			name() const;
	int						dimensions() const;
	void					setName( const MString & );
	void					setDimensions( int );

public:
	MAttributeSpec &		operator =(const MAttributeSpec &);
	const MAttributeIndex	operator [](int) const;
	MAttributeIndex 		operator[](int);
	bool					operator==(const MAttributeSpec &) const;

private:
	MAttributeSpec( void* );


	void*	data;
	bool	own;
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAttributeSpec */
