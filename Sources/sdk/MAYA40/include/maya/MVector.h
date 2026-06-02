#ifndef LINUX
#pragma once
#endif
#ifndef _MVector
#define _MVector

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MTransformationMatrix.h>



class MMatrix;
class MFloatVector;
class MPoint;
class MQuaternion;
class MEulerRotation;
class ostream;
#define MVector_kTol 1.0e-10



/**
  This class provides access to Maya's vector math library.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MVector  
{
public:
	
	enum Axis {
		kXaxis,
		kYaxis,
		kZaxis,
		kWaxis
	};

					MVector();
					MVector( const MVector&);
					MVector( const MFloatVector&);
					MVector( const MPoint&);
					MVector( double xx, double yy, double zz = 0.0);
					MVector( const double[3] );
					~MVector();
 	MVector&		operator= ( const MVector& src );
 	double&      	operator()( unsigned i );
 	double   		operator()( unsigned i ) const;
 	double&      	operator[]( unsigned i );
	double			operator[]( unsigned i )const;
 	MVector			operator^( const MVector& right) const;
 	double          operator*( const MVector& right ) const;
 	MVector&   		operator/=( double scalar );
 	MVector     	operator/( double scalar ) const;
 	MVector& 		operator*=( double scalar );
 	MVector   		operator*( double scalar ) const;
 	friend OPENMAYA_EXPORT MVector operator*( int, const MVector&);
 	friend OPENMAYA_EXPORT MVector operator*( short, const MVector&);
 	friend OPENMAYA_EXPORT MVector operator*( unsigned int, const MVector&);
 	friend OPENMAYA_EXPORT MVector operator*( unsigned short, const MVector&);
 	friend OPENMAYA_EXPORT MVector operator*( float, const MVector&);
 	friend OPENMAYA_EXPORT MVector operator*( double, const MVector&);
 	MVector   		operator+( const MVector& other) const;
	MVector&		operator+=( const MVector& other );
 	MVector   		operator-() const;
 	MVector   		operator-( const MVector& other ) const;
 	MVector  		operator*( const MMatrix&) const;
 	MVector&		operator*=( const MMatrix&);
 	friend OPENMAYA_EXPORT MVector operator*( const MMatrix&, const MVector&);
 	bool          	operator!=( const MVector& other ) const;
 	bool           	operator==( const MVector& other ) const;
	MVector			rotateBy( double x, double y, double z, double w) const;
	MVector			rotateBy( const double rotXYZ[3], 
							  MTransformationMatrix::RotationOrder order )
                              const;
	MVector			rotateBy( MVector::Axis axis, const double angle ) const;
	MVector			rotateBy( const MQuaternion & ) const;
	MVector			rotateBy( const MEulerRotation & ) const;
	MQuaternion		rotateTo( const MVector & ) const;
	MStatus			get( double[3] ) const;
 	double         	length() const;
 	MVector  		normal() const;
	MStatus			normalize();
 	double       	angle( const MVector& other ) const;
	bool			isEquivalent( const MVector& other,
						double tolerance = MVector_kTol ) const;
 	bool          	isParallel( const MVector& other,
						double tolerance = MVector_kTol ) const;
	MVector			transformAsNormal( const MMatrix & matrix ) const;


	friend OPENMAYA_EXPORT ostream&	operator<<(ostream& os, const MVector& v);

	static const MVector zero;
	static const MVector xAxis;
	static const MVector yAxis;
	static const MVector zAxis;
	static const MVector xNegAxis;
	static const MVector yNegAxis;
	static const MVector zNegAxis;
	double x;
	double y;
	double z;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MVector */
