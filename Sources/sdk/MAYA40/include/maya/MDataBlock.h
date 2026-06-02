#ifndef LINUX
#pragma once
#endif
#ifndef _MDataBlock
#define _MDataBlock
#if defined __cplusplus

#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>

class MObject;
class MPlug;
class MDGContext;


/**
  The storage for the data of all of a node's plugs and attributes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDataBlock  
{

public:

	virtual		     ~MDataBlock ();
	MDataHandle      inputValue ( const MPlug & plug, 
								        MStatus * ReturnStatus = NULL );
	MDataHandle      inputValue ( const MObject & attribute, 
								        MStatus * ReturnStatus = NULL ); 
	MDataHandle      outputValue ( const MPlug & plug, 
								        MStatus * ReturnStatus = NULL );
	MDataHandle      outputValue ( const MObject & attribute, 
								        MStatus * ReturnStatus = NULL );
	MArrayDataHandle inputArrayValue ( const MPlug & plug, 
								        MStatus * ReturnStatus = NULL );
	MArrayDataHandle inputArrayValue ( const MObject & attribute, 
								        MStatus * ReturnStatus = NULL );
	MArrayDataHandle outputArrayValue ( const MPlug & plug, 
										MStatus * ReturnStatus = NULL );
	MArrayDataHandle outputArrayValue ( const MObject & attribute, 
										MStatus * ReturnStatus = NULL );
	MStatus          setClean ( const MPlug & plug );
	MStatus          setClean ( const MObject & attribute );

	bool			isClean ( const MPlug & plug );
	bool			isClean ( const MObject & attribute, 
							  MStatus* ReturnStatus=NULL );

	MDGContext       context ( MStatus * ReturnStatus = NULL );
	MStatus          setContext ( const MDGContext & ctx );

protected:

private:
	const char*		 className() const;












	friend class MPxNode;
	MDataBlock( void * ptr ); 
	void * data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDataBlock */
