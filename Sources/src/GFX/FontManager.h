#ifndef __FONTMANAGER_H__
#define __FONTMANAGER_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../Misc/BasicShare.h"
#include "Font.h"
BASIC_SHARE_DECLARE( CFontShare, std::string, CFont, GFX_FONT, 104, "\\1.tfd" );
class CFontManager : public IFontManager
{
	OBJECT_COMPLETE_METHODS( CFontManager );
	DECLARE_SERIALIZE;
	CFontShare share;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) { share.SetSerialMode( eSerialMode ); }
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) { share.SetShareMode( eShareMode ); }
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init() { return share.Init(); }
	virtual IGFXFont* STDCALL GetFont( const char *pszName ) 
	{ 
		return share.Get( pszName ); 
	}
};
#endif // __FONTMANAGER_H__
