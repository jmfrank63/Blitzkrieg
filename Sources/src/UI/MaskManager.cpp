#include "StdAfx.h"

#include "MaskManager.h"
#include "UIMask.h"
bool CMaskManager::Init()
{
	maskShare.Init();
	return true;
}
void CMaskManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount ) 
{ 
	if ( eMode == ISharedManager::CLEAR_ALL ) 
		maskShare.Clear(); 
	else
		maskShare.ClearUnreferencedResources();
}
int CMaskManager::operator&( IStructureSaver &ss )
{
	maskShare.Serialize( &ss );
	return 0;
}
