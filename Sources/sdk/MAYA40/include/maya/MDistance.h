#ifndef LINUX
#pragma once
#endif
#ifndef _MDistance
#define _MDistance

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>





/**
  Methods for setting and retreiving linear data in various unit systems 
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDistance  
{
public:
	enum Unit
	{
		kInvalid,
		kInches,
		kFeet,
		kYards,
		kMiles,
		kMillimeters,
		kCentimeters,
		kKilometers,
		kMeters,
		kLast
	};
					MDistance();
					MDistance( double value, Unit unitSystem = kCentimeters );
					MDistance( const MDistance& src );
					~MDistance();  
	MDistance&		operator=( const MDistance& src ); 
	Unit			unit() const;
	double			value() const;
	MStatus			setUnit( Unit newUnit );
	MStatus			setValue( double newValue );
	double			as( Unit newUnit, MStatus *ReturnStatus = NULL ) const;
	double			asInches() const;
	double			asFeet() const;  
	double			asYards() const;  
	double			asMiles() const;  
	double			asMillimeters() const;
	double			asCentimeters() const;  
	double			asKilometers() const;  
	double			asMeters()  const;  
	static Unit		uiUnit();
	static MStatus	setUIUnit( Unit newUnit );
	static Unit		internalUnit();
	static MStatus	setInternalUnit( Unit internalUnit );
	static double	internalToUI( double internalValue );
	static double	uiToInternal( double uiValue );

protected:

	double			val;
	Unit			valUnit;

private:

	static const char* className();

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDistance */
