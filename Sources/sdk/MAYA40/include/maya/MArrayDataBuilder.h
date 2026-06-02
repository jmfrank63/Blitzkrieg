#ifndef LINUX
#pragma once
#endif
#ifndef _MArrayDataBuilder
#define _MArrayDataBuilder

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>





/**
 An MArrayDataBuilder is used to build array data for attributes/plugs
 that support arrays.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MArrayDataBuilder  
{

public:
	MArrayDataBuilder ( const MObject & attribute, unsigned numElements,
						MStatus * ReturnStatus = NULL );
	MDataHandle      addLast(MStatus * ReturnStatus = NULL);
	MDataHandle      addElement( unsigned index,
								 MStatus * ReturnStatus = NULL );
	MArrayDataHandle addLastArray(MStatus * ReturnStatus = NULL );
	MArrayDataHandle addElementArray( unsigned index,
									  MStatus * ReturnStatus = NULL  );
	MStatus          removeElement( unsigned index );
	unsigned         elementCount( MStatus * ReturnStatus = NULL ) const;
	MStatus          growArray( unsigned amount );
	MStatus          setGrowSize( unsigned size );

    MArrayDataBuilder( const MArrayDataBuilder &other );
	MArrayDataBuilder& operator=( const MArrayDataBuilder& other );
    ~MArrayDataBuilder();

protected:

private: 
	const char*		className() const;

	friend class MArrayDataHandle;
	MArrayDataBuilder();
	MArrayDataBuilder( void* );
	char data[24];
	bool fIsNull;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MArrayDataBuilder */
