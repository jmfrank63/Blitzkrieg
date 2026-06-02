#ifndef __UI_MASK_H__
#define __UI_MASK_H__
#include "MaskSystem.h"
class CUIMask : public IUIMask
{
	OBJECT_COMPLETE_METHODS( CUIMask );
	SHARED_RESOURCE_METHODS( nRefData.a, "Mask" );
	CArray2D<BYTE> data;

	friend class CMaskObjectLoader;			//для загрузки маски непосредственно в объект
public:
	virtual void STDCALL SwapData( ISharedResource *pResource );
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
	virtual const CArray2D<BYTE>* STDCALL GetMask() { return &data; }
};
#endif		//__UI_MASK_H__
