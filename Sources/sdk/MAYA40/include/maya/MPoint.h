#ifndef LINUX
#pragma once
#endif
#ifndef _MPoint
#define _MPoint

#if defined __cplusplus




#include <maya/MTypes.h>



class MMatrix;
class MVector;
class MFloatVector;
class ostream;

#define MPoint_kTol	1.0e-10



/**
  This class implements the Maya representation of a point
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPoint  
{
public:
					MPoint();	// defaults to 0,0,0,1
					MPoint( const MPoint & srcpt );  
					MPoint( const MVector & src );  
					MPoint( const MFloatVector & src );  
					MPoint( double xx, double yy,
							double zz = 0.0, double ww = 1.0 );
					MPoint( const double[4] );
					MPoint( const float[4] );
					~MPoint();
	MStatus			get( double[4] ) const;
	MStatus			get( float[4] ) const;
	MPoint & 		operator=( const MPoint & src );
	double &        operator()(unsigned i);
	double  		operator()(unsigned i) const;
	double &        operator[](unsigned i);
	double  		operator[](unsigned i) const;
	MVector   		operator-( const MPoint & other ) const;
	MPoint   		operator+( const MVector & other ) const;
	MPoint   		operator-( const MVector & other ) const;
	MPoint & 		operator+=( const MVector & vector );
	MPoint & 		operator-=( const MVector & vector );
	MPoint			operator*(const double scale) const;
	MPoint			operator/(const double scale) const;
	MPoint   		operator*(const MMatrix &) const;
	MPoint & 		operator*=(const MMatrix &);
	friend OPENMAYA_EXPORT MPoint operator*(const MMatrix &, const MPoint &);
	bool           	operator==( const MPoint & other ) const;
	bool           	operator!=( const MPoint & other ) const;
	MPoint & 		cartesianize();
	MPoint & 		rationalize();
	MPoint & 		homogenize();
	double     		distanceTo( const MPoint & other ) const;
	bool           	isEquivalent( const MPoint & other,
								  double tolerance = MPoint_kTol) const;

	friend OPENMAYA_EXPORT ostream& operator<<(ostream& os, const MPoint& p);

	static const MPoint origin;
public:
	double			x;
	double			y;
	double			z;
	double			w;

protected:

private:

	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPoint */
