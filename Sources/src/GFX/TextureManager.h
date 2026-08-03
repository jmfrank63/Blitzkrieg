#ifndef __TEXTUREMANAGER_H__
#define __TEXTUREMANAGER_H__
#pragma ONCE
#include "..\Misc\BasicShare.h"
#include "Texture.h"
#include "..\Image\Image.h"
BASIC_SHARE_DECLARE( CTextureShare, std::string, CTexture, GFX_TEXTURE, 106, ".tga" );
class CTextureManager : public ITextureManager
{
	OBJECT_COMPLETE_METHODS( CTextureManager );
	DECLARE_SERIALIZE;
	CTextureShare share;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) { share.SetSerialMode( eSerialMode ); }
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) { share.SetShareMode( eShareMode ); }
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init() { return share.Init(); }
	virtual IGFXTexture* STDCALL GetTexture( const char *pszName ) 
	{ 
		return share.Get( pszName ); 
	}
	virtual const char* STDCALL GetTextureName( IGFXTexture *pTexture )
	{
		const std::string *pName = share.GetKey( checked_cast<CTexture*>(pTexture) );
		return pName != 0 ? pName->c_str() : "default";
	}
	virtual void STDCALL SetQuality( const ETextureQuality eQuality );
	void ClearContainers() { share.ClearContainers(); }
	void ReloadAllData() { share.ReloadAllData(); }
};
#endif // __TEXTUREMANAGER_H__
