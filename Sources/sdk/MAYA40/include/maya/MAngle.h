#ifndef LINUX
#pragma once
#endif
#ifndef _MAngle
#define _MAngle

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
 Methods for setting and retreiving angular data in various unit systems
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MAngle  
{

public:
	enum Unit
	{
		kInvalid,
		kRadians,
		kDegrees,
		kAngMinutes,
		kAngSeconds,
		kLast
	};
	MAngle();
	MAngle( const MAngle & src );
	MAngle( double value, Unit u = kRadians );
 	~MAngle();
 	MAngle &     	operator=( const MAngle & other );
 	Unit    		unit() const;
 	double  		value() const;
 	MStatus         setUnit( Unit newUnit );
 	MStatus         setValue( double newValue );
 	double         	as( Unit otherUnit, MStatus *ReturnStatus = NULL ) const;
 	double         	asRadians() const;
 	double         	asDegrees() const;
 	double         	asAngMinutes() const;
 	double         	asAngSeconds() const;
 	static Unit		uiUnit();
 	static MStatus	setUIUnit( Unit newUnit );
	static Unit		internalUnit();
	static MStatus	setInternalUnit( Unit internalUnit );
	static double	internalToUI( double internalValue );
	static double	uiToInternal( double uiValue );

protected:

 	double 	val;
	Unit	valUnit;

private:
	static const char* className();


};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAngle */
