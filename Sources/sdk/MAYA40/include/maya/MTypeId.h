#ifndef LINUX
#pragma once
#endif
#ifndef _MTypeId
#define _MTypeId

#if defined __cplusplus



#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
  Create, copy, and query Maya Object type identifiers.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MTypeId  
{

public:
				MTypeId();
				~MTypeId();

				MTypeId( unsigned id );
				MTypeId( unsigned prefix, unsigned id );

				MTypeId( const MTypeId& src );
	MTypeId&	operator=( const MTypeId& rhs );
	bool		operator==( const MTypeId& rhs ) const;
	bool		operator!=( const MTypeId& rhs ) const;
	unsigned	id( MStatus * ReturnStatus = NULL ) const;

protected:

private:
	union {
		unsigned fId;
		unsigned char fTag[4];
	};

	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MTypeId */
