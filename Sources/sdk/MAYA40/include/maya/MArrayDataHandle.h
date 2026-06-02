#ifndef LINUX
#pragma once
#endif
#ifndef _MArrayDataHandle
#define _MArrayDataHandle

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <string.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>

class MArrayDataBuilder;


/**
  An MArrayDataHandle is a smart pointer into an MDataBlock that
  handles array data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MArrayDataHandle {

public:
	MArrayDataHandle( const MDataHandle & in, MStatus * ReturnStatus = NULL );

	MDataHandle       inputValue(MStatus *ReturnStatus = NULL);
	MDataHandle       outputValue(MStatus *ReturnStatus = NULL);
	MArrayDataHandle  inputArrayValue( MStatus * ReturnStatus = NULL );
	MArrayDataHandle  outputArrayValue( MStatus * ReturnStatus = NULL );
    MStatus           next();
	unsigned          elementCount(MStatus *ReturnStatus = NULL);
	unsigned          elementIndex(MStatus *ReturnStatus = NULL);
    MStatus           jumpToElement( unsigned index ); 
    MStatus           setClean();
    MStatus           setAllClean();
	MArrayDataBuilder builder(MStatus *ReturnStatus = NULL);
	MStatus           set( MArrayDataBuilder & );

    MArrayDataHandle( const MArrayDataHandle &other );
	MArrayDataHandle& operator=( const MArrayDataHandle& other );

protected:

private:
	const char*		className() const;

	friend class MDataBlock;
	friend class MArrayDataBuilder;
	MArrayDataHandle( void* );
	MArrayDataHandle();
	char data[20];
	bool fIsNull;
};

inline MArrayDataHandle::MArrayDataHandle( const MArrayDataHandle &other )
{
	memcpy( this, &other, sizeof(MArrayDataHandle) ); 
}

inline MArrayDataHandle& MArrayDataHandle::operator=(
											const MArrayDataHandle &other )
{
	memcpy( this, &other, sizeof(MArrayDataHandle) ); 
	return *this;
}

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MArrayDataHandle */
