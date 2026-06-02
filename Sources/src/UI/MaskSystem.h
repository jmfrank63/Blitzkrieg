#ifndef __MASK_SYSTEM_H__
#define __MASK_SYSTEM_H__
enum
{
	MASK_BASE_VALUE			= 0x100c0000,
	MASK_MANAGER				= MASK_BASE_VALUE + 1,
	UI_MASK							= MASK_BASE_VALUE + 2,
};
interface IUIMask : public ISharedResource
{
	virtual const CArray2D<BYTE>* STDCALL GetMask() = 0;
};
interface IMaskManager : public ISharedManager
{
	enum { tidTypeID = MASK_MANAGER };
	virtual IUIMask* STDCALL GetMask( const char *pszKey ) = 0;
	virtual const char* STDCALL GetMaskName( IUIMask *pMask ) = 0;
};
IObjectLoader* STDCALL GetMaskLoader();
#endif		//__MASK_MANAGER_H__
