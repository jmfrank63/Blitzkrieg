#ifndef LINUX
#pragma once
#endif
#ifndef _MFloatPoint
#define _MFloatPoint

#if defined __cplusplus




#include <maya/MTypes.h>



class MFloatMatrix;
class MFloatVector;
class ostream;

#define MFloatPoint_kTol	1.0e-10



/**
  This class implements the Maya representation of a point in floats
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFloatPoint  
{
public:
						MFloatPoint();	// defaults to 0,0,0,1
						MFloatPoint( const MFloatPoint & srcpt );  
						MFloatPoint( float xx, float yy,
									 float zz = 0.0, float ww = 1.0 );
						MFloatPoint( const float[4] );
						~MFloatPoint();
	bool				get( float[4] ) const;
	MFloatPoint &		operator=( const MFloatPoint & src );
	float &         	operator()(unsigned i);
	float				operator()(unsigned i) const;
	float &         	operator[](unsigned i);
	float				operator[](unsigned i) const;
	MFloatVector		operator-( const MFloatPoint & other ) const;
	MFloatPoint			operator+( const MFloatVector & other ) const;
	MFloatPoint			operator-( const MFloatVector & other ) const;
	MFloatPoint &		operator+=( const MFloatVector & vector );
	MFloatPoint &		operator-=( const MFloatVector & vector );
	MFloatPoint			operator*(const float scale) const;
	MFloatPoint			operator/(const float scale) const;
	MFloatPoint			operator*(const MFloatMatrix &) const;
	MFloatPoint &		operator*=(const MFloatMatrix &);
	friend OPENMAYA_EXPORT MFloatPoint operator*( const MFloatMatrix &,
												  const MFloatPoint & );
	bool				operator==( const MFloatPoint & other ) const;
	bool				operator!=( const MFloatPoint & other ) const;
	MFloatPoint &		cartesianize();
	MFloatPoint &		rationalize();
	MFloatPoint &		homogenize();
	float				distanceTo( const MFloatPoint & other ) const;
	bool				isEquivalent( const MFloatPoint & other,
									  float tolerance = MFloatPoint_kTol)
									const;

	friend OPENMAYA_EXPORT ostream& operator<< ( ostream& os,
												 const MFloatPoint& p );

	static const MFloatPoint origin;
	float				x;
	float				y;
	float				z;
	float				w;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFloatPoint */
