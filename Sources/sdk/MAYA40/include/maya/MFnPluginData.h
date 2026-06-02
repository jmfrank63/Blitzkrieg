#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPluginData
#define _MFnPluginData

#if defined __cplusplus



#include <maya/MFnData.h>



class MTypeId;
class MPxData;



/**
  Create and manipulate user defined dependency node data.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnPluginData : public MFnData 
{

	declareMFn(MFnPluginData, MFnData);

public:
	MTypeId			typeId( MStatus* ReturnStatus = NULL ) const;
	MPxData*		data( MStatus* ReturnStatus = NULL );
	const MPxData*	constData( MStatus* ReturnStatus = NULL ) const;
	MObject			create( const MTypeId& id, MStatus* ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPluginData */
