#ifndef LINUX
#pragma once
#endif
#ifndef _MStatus
#define _MStatus


#if defined __cplusplus



#include <maya/MTypes.h>



class ostream;
class MString;



/**
  Methods for passing status codes between user code and Maya
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MStatus
{

public:

	enum MStatusCode
	{
		kSuccess = 0,
		kFailure,
		kInsufficientMemory,
		kInvalidParameter,
		kLicenseFailure,
		kUnknownParameter,
		kNotImplemented,
		kNotFound,
		kEndOfFile
	};

						MStatus();
						MStatus( MStatusCode );
						MStatus( const MStatus& );

	MStatus&			operator=( const MStatus& rhs);
	bool				operator==( const MStatus& rhs ) const;
	bool				operator==( const MStatusCode rhs ) const;
	bool				operator!=( const MStatus& rhs ) const;
	bool				operator!=( const MStatusCode rhs ) const;
	operator			bool() const;
	bool				error() const;
	void				clear();
	MStatusCode	        statusCode() const;
	MString				errorString() const;
	void				perror( const char * ) const;
	void				perror( const MString& ) const;
	void				set( bool status,
							 unsigned char statusCode,
							 unsigned char internalStatusCode);

	friend OPENMAYA_EXPORT ostream& operator<<( ostream&, MStatus&);
	friend OPENMAYA_EXPORT bool operator==( const MStatus::MStatusCode,
									const MStatus& );
	friend OPENMAYA_EXPORT bool operator!=( const MStatus::MStatusCode,
									const MStatus& );


protected:

private:

	unsigned char		fStatusCode; 
	unsigned char		fInternalStatusCode; 
	bool				fStatus;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

typedef MStatus MS;

inline bool MStatus::operator==( const MStatus& rhs ) const
{
	return ( fStatusCode == rhs.fStatusCode );
}

inline bool MStatus::operator==( const MStatus::MStatusCode rhs ) const
{
	return ( fStatusCode == rhs );
}

inline bool MStatus::operator!=( const MStatus& rhs ) const
{
	return ( fStatusCode != rhs.fStatusCode );
}

inline bool MStatus::operator!=( const MStatus::MStatusCode rhs ) const
{
	return ( fStatusCode != rhs );
}

inline MStatus::operator bool() const
{ 
	return fStatus;
} 

inline bool MStatus::error() const
{
	return !fStatus;
}

inline MStatus::MStatusCode MStatus::statusCode() const
{
	return ( MStatus::MStatusCode ) fStatusCode;
}

inline void MStatus::set( bool status,
				 unsigned char statusCode,
				 unsigned char internalStatusCode)
{
	fStatus = status;
	fStatusCode = statusCode; 
	fInternalStatusCode = internalStatusCode; 
}

inline bool operator==( const MStatus::MStatusCode code, 
						const MStatus& status ) 
{
	return ( status.fStatusCode == code );
}

inline bool operator!=( const MStatus::MStatusCode code, 
						const MStatus& status )
{
	return ( status.fStatusCode != code );
}

#endif /* __cplusplus */
#endif /* _MStatus */
