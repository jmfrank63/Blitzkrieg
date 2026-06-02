#include "StdAfx.h"

#include "TextureManager.h"
#include "Texture.h"
void CTextureManager::SetQuality( const ETextureQuality eQuality )
{
	switch ( eQuality ) 
	{
		case TEXTURE_QUALITY_COMPRESSED:
			share.SetExt( "_c.dds" );
			break;
		case TEXTURE_QUALITY_LOW:
			share.SetExt( "_l.dds" );
			break;
		case TEXTURE_QUALITY_HIGH:
			share.SetExt( "_h.dds" );
			break;
		default:
			NI_ASSERT_T( false, NStr::Format("Unknown texture quality %d - can be only [1..3]", eQuality) );
	}
}
void CTextureManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount ) 
{ 
	switch ( eMode ) 
	{
		case ISharedManager::CLEAR_ALL:
			share.Clear(); 
			break;
		case ISharedManager::CLEAL_UNREFERENCED:
			share.ClearUnreferencedResources();
			break;
		case ISharedManager::CLEAR_LRU:
			share.ClearLRUResources( nUsage, nAmount );
			break;
	}
}
int CTextureManager::operator&( IStructureSaver &ss )
{
	share.Serialize( &ss );
	return 0;
}
