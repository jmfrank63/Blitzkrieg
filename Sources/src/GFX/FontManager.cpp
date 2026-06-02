#include "StdAfx.h"

#include "FontManager.h"

#include "Font.h"
void CFontManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount ) 
{ 
	if ( eMode == ISharedManager::CLEAR_ALL ) 
		share.Clear(); 
	else
		share.ClearUnreferencedResources();
}
int CFontManager::operator&( IStructureSaver &ss )
{
	share.Serialize( &ss );
	return 0;
}
