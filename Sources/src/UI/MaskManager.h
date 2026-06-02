#ifndef __MASK_MANAGER_H__
#define __MASK_MANAGER_H__
#include "..\Misc\BasicShare.h"
#include "MaskSystem.h"
#include "UIMask.h"
BASIC_SHARE_DECLARE( CUIMaskShare, std::string, CUIMask, UI_MASK, 113, ".tga" );
class CMaskManager : public IMaskManager
{
	OBJECT_COMPLETE_METHODS( CMaskManager );
	DECLARE_SERIALIZE;
	CUIMaskShare maskShare;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) { maskShare.SetSerialMode( eSerialMode ); }
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) { maskShare.SetShareMode( eShareMode ); }
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init();
	virtual IUIMask* STDCALL GetMask( const char *pszKey ) { return maskShare.Get( pszKey ); }
	virtual const char* STDCALL GetMaskName( IUIMask *pMask )
	{
		const std::string *pName = maskShare.GetKey( checked_cast<CUIMask*>(pMask) );
		return pName != 0 ? pName->c_str() : "";
	}
};

#endif		//__MASK_MANAGER_H__
