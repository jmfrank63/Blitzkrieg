#ifndef __TEXTURE_H__
#define __TEXTURE_H__
#pragma ONCE
template <class TBase>
class CBaseTexture : public TBase
{
protected:
  NWin32Helper::com_ptr<IDirect3DTexture8> pTexture; // internal D3D data container
	EGFXPixelFormat format;								// format
	int nSizeX, nSizeY;										// top level dimension
	int nMemUsage;												// memory usage (approx, in bytes)
public:
	CBaseTexture( EGFXPixelFormat _format, int _nSizeX, int _nSizeY, int _nMemUsage )
		: format( _format ), nSizeX( _nSizeX ), nSizeY( _nSizeY ), nMemUsage( _nMemUsage ) {  }
  D3DRESOURCETYPE GetType() const { return D3DRTYPE_TEXTURE; }
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		CBaseTexture<TBase> *pRes = dynamic_cast<CBaseTexture<TBase>*>( pResource );
		NI_ASSERT_TF( pRes != 0, NStr::Format("shared resource is not a %s", typeid(*this).name()), return );
		std::swap( pTexture, pRes->pTexture );
		std::swap( format, pRes->format );
		std::swap( nSizeX, pRes->nSizeX );
		std::swap( nSizeY, pRes->nSizeY );
		std::swap( nMemUsage, pRes->nMemUsage );
	}
	virtual void STDCALL ClearInternalContainer() { pTexture = 0; nMemUsage = 0; }
	virtual IDirect3DTexture8* GetInternalContainer() = 0;
	int STDCALL GetSizeX( int nLevel ) const { return nSizeX >> nLevel; }
	int STDCALL GetSizeY( int nLevel ) const { return nSizeY >> nLevel; }
	EGFXPixelFormat STDCALL GetFormat() const { return format; }
};
class CTexture : public CBaseTexture<IGFXTexture>
{
	OBJECT_COMPLETE_METHODS( CTexture );
	DECLARE_SERIALIZE;
	SHARED_RESOURCE_METHODS( nRefData.a, "Texture" );
public:
	CTexture() 
		: CBaseTexture<IGFXTexture>( GFXPF_UNKNOWN, -1, -1, 0 ) {  }
	void Init( IDirect3DTexture8 *_pTexture, const int _nMemUsage );
  D3DRESOURCETYPE GetType() const { return D3DRTYPE_TEXTURE; }
	IDirect3DTexture8* GetInternalContainer();
	bool STDCALL Load( const bool bPreLoad = false );
  bool STDCALL Lock( int nLevel, SSurfaceLockInfo *pLockInfo );
  bool STDCALL Unlock( int nLevel );
  bool STDCALL AddDirtyRect( const RECT *pRect );
	int STDCALL GetSizeX( int nLevel ) const { return nSizeX >> nLevel; }
	int STDCALL GetSizeY( int nLevel ) const { return nSizeY >> nLevel; }
	EGFXPixelFormat STDCALL GetFormat() const { return format; }
};
class CRenderTargetTexture : public CBaseTexture<IGFXRTexture>
{
	OBJECT_COMPLETE_METHODS( CRenderTargetTexture );
	SHARED_RESOURCE_METHODS( nRefData.a, "RTexture" );
	NWin32Helper::com_ptr<IDirect3DSurface8> pColor;	// color buffer
	NWin32Helper::com_ptr<IDirect3DSurface8> pDepth;	// depth buffer
public:
	CRenderTargetTexture()
		: CBaseTexture<IGFXRTexture>( GFXPF_ARGB8888, -1, -1, 0 ) {  }
	void Init( IDirect3DTexture8 *_pTexture, IDirect3DSurface8 *_pDepth, int _nMemUsage );
	IDirect3DSurface8* GetColorSurface() const { return pColor; }
	IDirect3DSurface8* GetDepthSurface() const { return pDepth; }
	IDirect3DTexture8* GetInternalContainer() { return pTexture; }
	void STDCALL SwapData( ISharedResource *pResource ) {  }
	void STDCALL ClearInternalContainer() { pColor = 0; pDepth = 0; pTexture = 0; }
	bool STDCALL Load( const bool bPreLoad = false ) { NI_ASSERT_T( false, "This method can't be implemented for RT" ); return false; }
};
class CSurface : public IGFXSurface
{
	OBJECT_COMPLETE_METHODS( CSurface );
	NWin32Helper::com_ptr<IDirect3DSurface8> pSurface;
	EGFXPixelFormat format;
	int nSizeX, nSizeY;
public:
	CSurface() : format( GFXPF_UNKNOWN ), nSizeX( -1 ), nSizeY( -1 ) {  }
	CSurface( IDirect3DSurface8 *_pSurface ) { Init( _pSurface ); }
	void Init( IDirect3DSurface8 *_pSurface );
  virtual bool STDCALL Lock( SSurfaceLockInfo *pLockInfo );
  virtual bool STDCALL Unlock();
	virtual int STDCALL GetSizeX() const { return nSizeX; }
	virtual int STDCALL GetSizeY() const { return nSizeY; }
	virtual EGFXPixelFormat STDCALL GetFormat() const { return format; }
	virtual void STDCALL ClearInternalContainer() { pSurface = 0; }
};
#endif // __TEXTURE_H__
