#ifndef LINUX
#pragma once
#endif
#ifndef _MDGContext
#define _MDGContext

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MObject;
class MTime;



/**
  Control the way in which dependency nodes are evaluated.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDGContext  
{
public:

	MDGContext( );

	MDGContext( const MTime & when );

	MDGContext( const MDGContext& in );

	~MDGContext();



	bool     	isNormal( MStatus * ReturnStatus = NULL ) const;

	MStatus 	getTime( MTime & );

	MDGContext&	operator =( const MDGContext& other );


	static		MDGContext	fsNormal;


protected:

private:
	const void * data;
	bool fOwn;
	friend class MPlug;
	friend class MDataBlock;


	const char* className() const;
	MDGContext( const void* );
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDGContext */
