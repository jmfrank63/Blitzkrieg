#ifndef LINUX
#pragma once
#endif
#ifndef _MTime
#define _MTime

#if defined __cplusplus




#include <maya/MTypes.h>



class ostream;



/**
 Methods for setting and retreiving animation times in various unit systems.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MTime  
{

public:
	enum Unit {
		kInvalid,
		kHours,                             //      3600    sec 
		kMinutes,                           //      60      sec
		kSeconds,                           //      1       sec
		kMilliseconds,                      //      1/1000  sec
		kGames,                             //      1/15    sec
		kFilm,                              //      1/24    sec
		kPALFrame,                          //      1/25 sec (approx)
		kNTSCFrame,                         //      1/30 sec (approx)
		kShowScan,                          //      1/48    sec
		kPALField,                          //      1/50 sec (approx)
		kNTSCField,                         //      1/60 sec (approx)
		kUserDef,                           //      user defined
		kLast
	};
					MTime();
					MTime( const MTime & );
					MTime( double time_val, Unit = kFilm );
					~MTime();
 	Unit     		unit() const;
 	double   		value() const;
 	MStatus 	 	setUnit( Unit new_unit );
 	MStatus 		setValue( double new_value );
 	double    		as( Unit other_unit ) const;
	static Unit		uiUnit();
	static MStatus	setUIUnit( Unit new_unit);

	MTime&			operator =  ( const MTime& rhs );
	bool			operator == ( const MTime& rhs ) const;
	bool			operator != ( const MTime& rhs ) const;
	bool			operator <= ( const MTime& rhs ) const;
	bool			operator >= ( const MTime& rhs ) const;
	bool			operator <  ( const MTime& rhs ) const;
	bool			operator >  ( const MTime& rhs ) const;
	MTime			operator +  ( const MTime& rhs ) const;
	MTime&			operator += ( const MTime& rhs );
	MTime			operator +  ( double rhs ) const;
	MTime&			operator += ( double rhs );
	MTime&			operator ++ ();
	MTime&			operator ++ (int);
	MTime			operator -  ( const MTime& rhs ) const;
	MTime&			operator -= ( const MTime& rhs );
	MTime			operator -  ( double rhs ) const;
	MTime&			operator -= ( double rhs );
	MTime&			operator -- ();
	MTime&			operator -- (int);
	MTime			operator *  ( double rhs ) const;
	MTime&			operator *= ( double rhs );
	MTime			operator /  ( double rhs ) const;
	MTime&			operator /= ( double rhs );

	friend OPENMAYA_EXPORT ostream&	operator << ( ostream& os, const MTime& t );

protected:

private:


	friend class MTimeHelper;
	MTime			( int );
	static const	char* className();
 	int	val;
 	Unit	valUnit;
	void*   data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MTime */
