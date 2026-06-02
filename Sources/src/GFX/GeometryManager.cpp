#include "StdAfx.h"

#include "GeometryManager.h"

#include "GeometryMesh.h"
#include "GFXHelper.h"
void CMeshManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount ) 
{ 
	if ( eMode == ISharedManager::CLEAR_ALL ) 
		share.Clear(); 
	else
		share.ClearUnreferencedResources();
}
int CMeshManager::operator&( IStructureSaver &ss )
{
	share.Serialize( &ss );
	return 0;
}
