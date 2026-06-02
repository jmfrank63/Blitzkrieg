#ifndef LINUX
#pragma once
#endif
#ifndef _MFnAttribute
#define _MFnAttribute

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnData.h>



class MTypeId;
class MString;



/**
 Function set for attributes of dependency nodes.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnAttribute : public MFnBase 
{

	declareMFn(MFnAttribute, MFnBase);

public:

	enum DisconnectBehavior {
		kDelete,
		kReset,
		kNothing
	};

	bool		isReadable( MStatus* ReturnStatus=NULL ) const;
	bool		isWritable( MStatus* ReturnStatus=NULL ) const;
	bool		isConnectable( MStatus* ReturnStatus=NULL ) const;
	bool		isStorable( MStatus* ReturnStatus=NULL ) const;
	bool		isCached( MStatus* ReturnStatus=NULL ) const;
	bool		isArray( MStatus* ReturnStatus=NULL ) const;
	bool		indexMatters( MStatus* ReturnStatus=NULL ) const;
	bool		isKeyable( MStatus* ReturnStatus=NULL ) const;
	bool		isHidden( MStatus* ReturnStatus=NULL ) const; 
	bool		isUsedAsColor( MStatus* ReturnStatus=NULL ) const;
	bool		isIndeterminant( MStatus* ReturnStatus=NULL ) const;

	bool		isRenderSource( MStatus* ReturnStatus=NULL ) const;

	DisconnectBehavior disconnectBehavior( MStatus* ReturnStatus=NULL ) const;
	bool        usesArrayDataBuilder( MStatus* ReturnStatus=NULL ) const;
	bool     	internal( MStatus* ReturnStatus=NULL ) const;

	MStatus		setReadable( bool state );
	MStatus		setWritable( bool state ); 
	MStatus		setConnectable( bool state );
	MStatus	 	setStorable( bool state );
	MStatus		setCached( bool state );
	MStatus	 	setArray( bool state );
	MStatus	 	setIndexMatters( bool state );
	MStatus	 	setKeyable( bool state );
	MStatus	 	setHidden( bool state );
	MStatus     setUsedAsColor( bool state );
	MStatus     setIndeterminant( bool state );

	MStatus		setRenderSource( bool state );

	MStatus     setDisconnectBehavior( DisconnectBehavior behavior );
	MStatus     setUsesArrayDataBuilder( bool state );
	MStatus     setInternal( bool state );
	bool		accepts( MFnData::Type type, MStatus* ReturnStatus=NULL ) const;
	bool		accepts( const MTypeId& id, MStatus* ReturnStatus=NULL ) const;
	MStatus		setParent( const MObject & parent );
    MString     name( MStatus* ReturnStatus=NULL ) const; 

protected:
	void * ca[3];
	void setPtr( MPtrBase* );

private:
	virtual bool objectChanged( MFn::Type, MStatus * );
};
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnAttribute */
