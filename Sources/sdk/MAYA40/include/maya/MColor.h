#ifndef LINUX
#pragma once
#endif
#ifndef _MColor
#define _MColor

#if defined __cplusplus




#include <maya/MTypes.h>


class ostream;



/**
    This class is used to store values of color-related dependency graph node
    attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MColor  
{
public:
	enum MColorType {
		kRGB,
		kHSV,
		kCMY,
		kCMYK
	};

					MColor();
					MColor( const MColor&);
					MColor( float rr, float gg, float bb=0.0, float aa=1.0 );
					MColor( const float[3] );

					MColor( MColorType colorModel,
							unsigned char, unsigned char, unsigned char,
							unsigned char alpha=255U );
					MColor( MColorType colorModel,
							unsigned short, unsigned short, unsigned short,
							unsigned short alpha=65535U );
					MColor( MColorType colorModel, float, float, float,
							float alpha=1.0 );
					MColor( MColorType colorModel, double, double, double,
							double alpha=1.0 );

					~MColor();
 	MColor&		    operator= ( const MColor& src );
 	float&      	operator()( unsigned i );
 	float   		operator()( unsigned i ) const;
 	float&      	operator[]( unsigned i );
	float			operator[]( unsigned i )const;
 	MColor&   		operator/=( float scalar );
 	MColor     	    operator/( float scalar ) const;
 	MColor& 		operator*=( float scalar );
 	MColor   		operator*( float scalar ) const;
 	friend OPENMAYA_EXPORT MColor	operator*( float, const MColor&);
 	MColor   		operator+( const MColor& other) const;
	MColor&		    operator+=( const MColor& other );
 	MColor   		operator-() const;
 	MColor   		operator-( const MColor& other ) const;
 	MColor          operator*( const MColor& other ) const;
	MColor&         operator*=( const MColor& other );
 	bool          	operator!=( const MColor& other ) const;
 	bool           	operator==( const MColor& other ) const;
	bool			get( float[3] ) const;

	friend OPENMAYA_EXPORT ostream& operator<<(ostream& os, const MColor& c);

	float r;
	float g;
	float b;
	float a;


protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MColor */
