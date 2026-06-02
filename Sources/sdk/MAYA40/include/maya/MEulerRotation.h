#ifndef LINUX
#pragma once
#endif
#ifndef _MEulerRotation
#define _MEulerRotation

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MTransformationMatrix.h>



class MMatrix;
class MQuaternion;
class ostream;
#define kEulerRotationEpsilon 1.0e-10



/**
This class provides methods for working with euler rotations.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MEulerRotation  
{
public:

	enum RotationOrder {
		kXYZ, 
		kYZX, 
		kZXY, 
		kXZY, 
		kYXZ, 
		kZYX
	};

						MEulerRotation();
						MEulerRotation(const MEulerRotation &src);
						MEulerRotation(const MVector &v, 
									   RotationOrder ord = kXYZ);
						MEulerRotation(double xx, double yy, double zz, 
									   RotationOrder ord = kXYZ);
						~MEulerRotation();
	MEulerRotation &	operator=(const MEulerRotation &src);
 	MEulerRotation &	operator=(const MQuaternion &q);
 	MEulerRotation &	operator=(const MMatrix &m);
 	MEulerRotation &	operator=(const MVector &v);
	MEulerRotation &	setValue(const MVector &v, 
								 RotationOrder ord = kXYZ);
	MEulerRotation &	setValue(double xx, double yy, double zz,
								 RotationOrder ord = kXYZ);
	MQuaternion			asQuaternion() const;
	MMatrix				asMatrix() const;
 	MVector				asVector() const;
	double &			operator[](unsigned i);
	double				operator[](unsigned i) const;
 	MEulerRotation   	operator+(const MEulerRotation &other) const;
	MEulerRotation &	operator+=(const MEulerRotation &other);
 	MEulerRotation   	operator-(const MEulerRotation &other) const;
	MEulerRotation &	operator-=(const MEulerRotation &other);
 	MEulerRotation   	operator-() const;
	MEulerRotation		operator*(const MEulerRotation &other) const;
	MEulerRotation		operator*(const MQuaternion &other) const;
	MEulerRotation		operator*(double other) const;
 	friend OPENMAYA_EXPORT MEulerRotation operator*(double scale, 
													const MEulerRotation 
													    &other);
	MEulerRotation &	operator*=(const MEulerRotation &other);
	MEulerRotation &	operator*=(const MQuaternion &other);
	MEulerRotation &	operator*=(double other);
 	bool           		operator==(const MEulerRotation &other) const;
 	bool          		operator!=(const MEulerRotation &other) const;
 	bool          		isEquivalent(const MEulerRotation &other, 
									 double tolerance = kEulerRotationEpsilon) 
		                             const;
 	bool          		isZero(double tolerance = kEulerRotationEpsilon) const;
	MEulerRotation &	incrementalRotateBy(const MVector &axis,
											double angle);
	MEulerRotation		inverse() const;
	MEulerRotation &	invertIt();
	MEulerRotation		reorder(RotationOrder ord) const;
	MEulerRotation &	reorderIt(RotationOrder ord);
	static MEulerRotation bound(const MEulerRotation &src);
	MEulerRotation		bound() const;
	MEulerRotation &	boundIt(const MEulerRotation &src);
	MEulerRotation &	boundIt();
	static MEulerRotation alternateSolution(const MEulerRotation &src);
	MEulerRotation		alternateSolution() const;
	MEulerRotation &	setToAlternateSolution(const MEulerRotation &src);
	MEulerRotation &	setToAlternateSolution();
	static MEulerRotation closestSolution(const MEulerRotation &src,
										  const MEulerRotation &dst);
	MEulerRotation		closestSolution(const MEulerRotation &dst) const;
	MEulerRotation &	setToClosestSolution(const MEulerRotation &src,
											 const MEulerRotation &dst);
	MEulerRotation &	setToClosestSolution(const MEulerRotation &dst);
	static MEulerRotation closestCut(const MEulerRotation &src,
									 const MEulerRotation &dst);
	MEulerRotation		closestCut(const MEulerRotation &dst) const;
	MEulerRotation &	setToClosestCut(const MEulerRotation &src,
										const MEulerRotation &dst);
	MEulerRotation &	setToClosestCut(const MEulerRotation &dst);
	static MEulerRotation decompose(const MMatrix &matrix, RotationOrder ord);
	friend OPENMAYA_EXPORT ostream &operator<<(ostream &os, 
											   const MEulerRotation &rotation);
	static const MEulerRotation identity;

	double x;
	double y;
	double z;
	RotationOrder order;
#ifdef	LINUX
	char			_padding[4];		// Pad out sizeof class
#endif

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MEulerRotation */
