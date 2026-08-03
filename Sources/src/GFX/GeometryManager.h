#ifndef __GEOMETRYMANAGER_H__
#define __GEOMETRYMANAGER_H__
#pragma ONCE
#include "../Misc/BasicShare.h"
#include "GeometryMesh.h"
BASIC_SHARE_DECLARE( CMeshShare, std::string, CGeometryMesh, GFX_MESH, 105, "" );
class CMeshManager : public IMeshManager
{
	OBJECT_COMPLETE_METHODS( CMeshManager );
	DECLARE_SERIALIZE;
	CMeshShare share;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) { share.SetSerialMode( eSerialMode ); }
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) { share.SetShareMode( eShareMode ); }
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init() { return share.Init(); }
	virtual IGFXMesh* STDCALL GetMesh( const char *pszName ) { return share.Get( pszName ); }
	void ClearContainers() { share.ClearContainers(); }
	void ReloadAllData() { share.ReloadAllData(); }
};
#endif // __GEOMETRYMANAGER_H__
