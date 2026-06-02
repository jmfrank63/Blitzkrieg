#ifndef LINUX
#pragma once
#endif
#ifndef _MFloatVector
#define _MFloatVector

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MFloatMatrix;
class MVector;
class MPoint;
class ostream;
#define MFloatVector_kTol 1.0e-5F



/**
  This class provides access to Maya's vector math library.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFloatVector  
{
public:
						MFloatVector();
						MFloatVector( const MFloatVector&);
						MFloatVector( const MVector&);
						MFloatVector( const MPoint&);
						MFloatVector( float xx, float yy, float zz = 0.0);
						MFloatVector( const float[3] );
						~MFloatVector();
 	MFloatVector&		operator= ( const MFloatVector& src );
 	float&     		 	operator()( unsigned i );
 	float   			operator()( unsigned i ) const;
 	float&     		 	operator[]( unsigned i );
	float				operator[]( unsigned i )const;
 	MFloatVector		operator^( const MFloatVector& right) const;
 	MFloatVector&   	operator/=( float scalar );
 	MFloatVector 	    operator/( float scalar ) const;
 	MFloatVector& 		operator*=( float scalar );
 	MFloatVector   		operator*( float scalar ) const;
 	friend OPENMAYA_EXPORT MFloatVector	operator*( int,
												   const MFloatVector& );
 	friend OPENMAYA_EXPORT MFloatVector	operator*( short,
												   const MFloatVector& );
 	friend OPENMAYA_EXPORT MFloatVector	operator*( unsigned int,
												   const MFloatVector& );
 	friend OPENMAYA_EXPORT MFloatVector	operator*( unsigned short,
												   const MFloatVector& );
 	friend OPENMAYA_EXPORT MFloatVector	operator*( float,
												   const MFloatVector& );
 	friend OPENMAYA_EXPORT MFloatVector	operator*( double,
												   const MFloatVector& );
 	MFloatVector   		operator+( const MFloatVector& other) const;
	MFloatVector&		operator+=( const MFloatVector& other );
 	MFloatVector   		operator-() const;
 	MFloatVector   		operator-( const MFloatVector& other ) const;
 	MFloatVector  		operator*( const MFloatMatrix&) const;
 	MFloatVector&		operator*=( const MFloatMatrix&);
 	friend OPENMAYA_EXPORT MFloatVector	operator*( const MFloatMatrix&,
												   const MFloatVector& );
 	float      		    operator*( const MFloatVector& other ) const;
 	bool       		   	operator!=( const MFloatVector& other ) const;
 	bool       	    	operator==( const MFloatVector& other ) const;
	MStatus				get( float[3] ) const;
 	float      		   	length() const;
 	MFloatVector  		normal() const;
	MStatus				normalize();
 	float      		 	angle( const MFloatVector& other ) const;
	bool				isEquivalent( const MFloatVector& other,
									  float tolerance = MFloatVector_kTol )
									  const;
 	bool       		   	isParallel( const MFloatVector& other,
									float tolerance = MFloatVector_kTol )
									const;
	MFloatVector		transformAsNormal( const MFloatMatrix & matrix ) const;

	friend OPENMAYA_EXPORT ostream& operator<<( ostream& os,
												const MFloatVector& v );

	static const MFloatVector zero;
	static const MFloatVector xAxis;
	static const MFloatVector yAxis;
	static const MFloatVector zAxis;
	static const MFloatVector xNegAxis;
	static const MFloatVector yNegAxis;
	static const MFloatVector zNegAxis;
	float x;
	float y;
	float z;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFloatVector */
