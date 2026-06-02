#ifndef LINUX
#pragma once
#endif
#ifndef _MString
#define _MString


#if defined __cplusplus




#include <maya/MTypes.h>



class MStringArray;
class ostream;



/**
 Methods for handling strings.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MString  
{


public:
				MString();
				MString( const char* charString );
                MString( const char* charString, int charLength );
				MString( const MString& other );
	        	~MString();
	MString&	operator += ( const MString& other );
	MString&	operator += ( const char * other );
	MString&	operator += ( double other );
	MString&	operator += ( int other );
	MString&	operator =  ( const MString& other );
	MString&	operator =  ( const char * other );
	MString&	operator =  ( double value );
	bool		operator == ( const MString& other ) const;
	bool		operator == ( const char * other ) const;
	bool		operator != ( const MString& other ) const;
	bool		operator != ( const char * other ) const;
	MString     operator + (const MString& other ) const;
	MString     operator + (const char * other ) const;
	MString     operator + ( double value ) const;
	MStatus		set( const char * charString );
    MStatus     set( const char * charString, int charLength );
	MStatus		set( double value );
	MStatus		set( double value, int precision );
	const char*	asChar() const;
	unsigned	length() const;
	void		clear();
	int			index(char) const;
	int			rindex(char) const;
	MStatus		split(char, MStringArray&) const;
	MString		substring(int start, int end) const;
	MString&	toUpperCase();
	MString&	toLowerCase();
	bool		isShort() const;
	short		asShort() const;
	bool		isInt() const;
	int			asInt() const;
	bool		isUnsigned() const;
	unsigned	asUnsigned() const;
	bool		isFloat() const;
	float		asFloat() const;
	bool		isDouble() const;
	double		asDouble() const;

	friend OPENMAYA_EXPORT ostream& operator<<(ostream&, const MString& );
	friend OPENMAYA_EXPORT MString operator+(const char *, const MString& );

protected:

private:
	void*	api_data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MString */
