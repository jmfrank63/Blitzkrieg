#ifndef __GFX_H__
#define __GFX_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "GFXTypes.h"
enum
{
	GFX_BASE_VALUE			= 0x10030000,
	GFX_GFX             = GFX_BASE_VALUE + 1,
	GFX_TEXTURE_MANAGER = GFX_BASE_VALUE + 2,
	GFX_MESH_MANAGER    = GFX_BASE_VALUE + 3,
	GFX_FONT_MANAGER    = GFX_BASE_VALUE + 4,
	GFX_TEXTURE         = GFX_BASE_VALUE + 5,
	GFX_CUBE_TEXTURE    = GFX_BASE_VALUE + 6,
	GFX_VOLUME_TEXTURE  = GFX_BASE_VALUE + 7,
	GFX_SURFACE         = GFX_BASE_VALUE + 8,
	GFX_VERTICES        = GFX_BASE_VALUE + 9,
	GFX_INDICES         = GFX_BASE_VALUE + 10,
	GFX_MESH            = GFX_BASE_VALUE + 11,
	GFX_FONT            = GFX_BASE_VALUE + 12,
	GFX_TEXT            = GFX_BASE_VALUE + 13,
	GFX_RT_TEXTURE			= GFX_BASE_VALUE + 14
};
interface IGFXVertices : public ISharedResource
{
	virtual void* STDCALL Lock() = 0;
	virtual void STDCALL Unlock() = 0;
};
interface IGFXIndices : public ISharedResource
{
	virtual void* STDCALL Lock() = 0;
	virtual void STDCALL Unlock() = 0;
	virtual void STDCALL SetNumUsedVertices( int nNumUsedVertices ) = 0;
};
interface IGFXMesh : public ISharedResource
{
	virtual const SGFXBoundSphere& STDCALL GetBS() = 0;
	virtual const SGFXAABB& STDCALL GetAABB() = 0;
	virtual bool STDCALL IsHit( const CVec2 &vPos, const SHMatrix *matrices ) = 0;
};
interface IMeshManager : public ISharedManager
{
	enum { tidTypeID = GFX_MESH_MANAGER };
	virtual IGFXMesh* STDCALL GetMesh( const char *pszName ) = 0;
};
interface IGFXBaseTexture : public ISharedResource
{
};
interface IGFXTexture : public IGFXBaseTexture
{
  virtual bool STDCALL Lock( int nLevel, SSurfaceLockInfo *pLockInfo ) = 0;
  virtual bool STDCALL Unlock( int nLevel ) = 0;
  virtual bool STDCALL AddDirtyRect( const RECT *pRect ) = 0;
	virtual int STDCALL GetSizeX( int nLevel ) const = 0;
	virtual int STDCALL GetSizeY( int nLevel ) const = 0;
	virtual EGFXPixelFormat STDCALL GetFormat() const = 0;
};
interface IGFXRTexture : public IGFXBaseTexture
{
	virtual int STDCALL GetSizeX( int nLevel ) const = 0;
	virtual int STDCALL GetSizeY( int nLevel ) const = 0;
	virtual EGFXPixelFormat STDCALL GetFormat() const = 0;
};
interface IGFXSurface : public IRefCount
{
  virtual bool STDCALL Lock( SSurfaceLockInfo *pLockInfo ) = 0;
  virtual bool STDCALL Unlock() = 0;
	virtual int STDCALL GetSizeX() const = 0;
	virtual int STDCALL GetSizeY() const = 0;
	virtual EGFXPixelFormat STDCALL GetFormat() const = 0;
};
interface ITextureManager : public ISharedManager
{
	enum { tidTypeID = GFX_TEXTURE_MANAGER };
	enum ETextureQuality
	{
		TEXTURE_QUALITY_COMPRESSED	= 1,
		TEXTURE_QUALITY_LOW					= 2,
		TEXTURE_QUALITY_HIGH				= 3,
	};
	virtual IGFXTexture* STDCALL GetTexture( const char *pszName ) = 0;
	virtual const char* STDCALL GetTextureName( IGFXTexture *pTexture ) = 0;
	virtual void STDCALL SetQuality( const ETextureQuality eQuality ) = 0;
};
interface IGFXFont : public ISharedResource
{
  virtual int STDCALL GetHeight() const = 0;
  virtual int STDCALL GetLineSpace() const = 0;
  virtual int STDCALL GetAscent() const = 0;
  virtual int STDCALL GetDescent() const = 0;
	virtual const struct SFontFormat& STDCALL GetFormat() const = 0;
  virtual int STDCALL GetTextWidth( const char *pszString, int nCounter = 2000000000 ) const = 0;
  virtual int STDCALL GetTextWidth( const WORD *pszString, int nCounter = 2000000000 ) const = 0;
};
interface IGFXText : public IRefCount
{
	virtual void STDCALL SetFont( IGFXFont *pFont ) = 0;
	virtual void STDCALL SetText( interface IText *pText ) = 0;
	virtual interface IText* STDCALL GetText() = 0;
	virtual void STDCALL SetWidth( int nWidth ) = 0;
	virtual void STDCALL SetColor( DWORD color ) = 0;
	virtual void STDCALL EnableRedLine( bool bEnable ) = 0;
	virtual void STDCALL SetRedLine( const int nRedLineIndention ) = 0;
	virtual void STDCALL SetChanged() = 0;
	virtual void STDCALL SetScale( float fScale ) = 0;
	virtual float STDCALL GetScale() const = 0;
	virtual int STDCALL GetNumLines() const = 0;
	virtual int STDCALL GetLineSpace() const = 0;
	virtual int STDCALL GetWidth( int nNumCharacters = -1 ) const = 0;
};
interface IFontManager : public ISharedManager
{
	enum { tidTypeID = GFX_FONT_MANAGER };
	virtual IGFXFont* STDCALL GetFont( const char *pszName ) = 0;
};
interface IGFX : public IRefCount
{
	enum { tidTypeID = GFX_GFX };
	virtual bool STDCALL Init( const char *pszAdapterName, HWND hWnd ) = 0;
	virtual bool STDCALL Done() = 0;
	virtual void Clear() = 0;
	virtual bool STDCALL SetMode( int nSizeX, int nSizeY, int nBpp, int nStencilBPP, EGFXFullscreen eFullscreen, int nFreq = 0 ) = 0;
	virtual EGFXVideoCard STDCALL GetVideoCard() = 0;

	virtual void STDCALL MoveTo( int nX, int nY ) = 0;
	
	virtual RECT STDCALL GetScreenRect() const = 0;
	virtual int STDCALL GetScreenBPP() const = 0;
	virtual const char* STDCALL GetAdapterName() const = 0;
	virtual const struct SGFXDisplayMode* STDCALL GetDisplayModes() const = 0;
	
	virtual void STDCALL PushViewport() = 0;
	virtual bool STDCALL PopViewport() = 0;
	virtual bool STDCALL ChangeViewport( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ ) = 0;
	virtual bool STDCALL ChangeViewport( int nWidth, int nHeight ) = 0;
	virtual bool STDCALL SetWorldTransforms( const int nStartIndex, const SHMatrix *pMatrices, const int nNumMatrices ) = 0;
	virtual bool STDCALL SetViewTransform( const SHMatrix &matrix ) = 0;
	virtual bool STDCALL SetProjectionTransform( const SHMatrix &matrix ) = 0;
	virtual bool STDCALL SetTextureTransform( int nIndex, const SHMatrix &matrix ) = 0;
	virtual bool STDCALL SetupDirectTransform() = 0;
	virtual bool STDCALL RestoreTransform() = 0;
	virtual const SHMatrix& STDCALL GetViewMatrix() const = 0;
	virtual const SHMatrix& STDCALL GetBillboardMatrix() const = 0;
	virtual const SHMatrix& STDCALL GetInverseViewMatrix() const = 0;
	virtual const SHMatrix& STDCALL GetProjectionMatrix() const = 0;
	virtual const SHMatrix& STDCALL GetViewportMatrix() const = 0;
	virtual void STDCALL GetViewVolume( SPlane *pPlanes ) const = 0;
	virtual void STDCALL GetViewVolumeCrosses( const CVec2 &vPoint, CVec3 *pvNear, CVec3 *pvFar ) = 0;
	virtual void STDCALL SetLight( int nIndex, const SGFXLightDirectional &light ) = 0;
	virtual void STDCALL SetLight( int nIndex, const SGFXLightPoint &light ) = 0;
	virtual void STDCALL SetLight( int nIndex, const SGFXLightSpot &light ) = 0;
	virtual void STDCALL EnableLight( int nIndex, bool bEnable ) = 0;
	virtual void STDCALL SetMaterial( const SGFXMaterial &material ) = 0;
	
	virtual bool STDCALL SetTexture( int nStage, IGFXBaseTexture *pTexture ) = 0;

	virtual bool STDCALL SetWireframe( bool bWireframe ) = 0;
	virtual bool STDCALL SetCullMode( EGFXCull cull ) = 0;
	virtual bool STDCALL SetDepthBufferMode( EGFXDepthBuffer depth, EGFXCmpFunction cmp = GFXCMP_DEFAULT ) = 0;
	virtual bool STDCALL EnableLighting( bool bLighting ) = 0;
	virtual bool STDCALL EnableSpecular( bool bEnable ) = 0;

	virtual bool STDCALL SetFont( IGFXFont *pFont ) = 0;

	virtual bool STDCALL IsActive() = 0;
	virtual bool STDCALL BeginScene() = 0;
	virtual bool STDCALL EndScene() = 0;
	virtual bool STDCALL Clear( int nNumRects, RECT *pRects, DWORD dwFlags, DWORD dwColor = 0, float fDepth = 1.0f, DWORD dwStencil = 0 ) = 0;
	virtual bool STDCALL Flip() = 0;
	virtual bool STDCALL SetRenderTarget( IGFXRTexture *pRT ) = 0;
	
	virtual void STDCALL SetOptimizedBuffers( bool bEnable ) = 0;
	virtual IGFXVertices* STDCALL CreateVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXVertices *pVertices = 0 ) = 0;
	virtual IGFXIndices* STDCALL CreateIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXIndices *pIndices = 0 ) = 0;
	virtual bool STDCALL BeginSolidVertexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic ) = 0;
	virtual bool STDCALL EndSolidVertexBlock() = 0;
	virtual bool STDCALL BeginSolidIndexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic ) = 0;
	virtual bool STDCALL EndSolidIndexBlock() = 0;
	virtual void* STDCALL GetTempVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type ) = 0;
	virtual void* STDCALL GetTempIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type ) = 0;

	virtual IGFXTexture* STDCALL CreateTexture( int nSizeX, int nSizeY, int nNumMipLevels, EGFXPixelFormat format, EGFXDynamic eDynamic, IGFXTexture *pTexture = 0 ) = 0;
	virtual IGFXRTexture* STDCALL CreateRTexture( int nSizeX, int nSizeY ) = 0;
	virtual bool STDCALL UpdateTexture( IGFXTexture *pSrc, IGFXTexture *pDst, bool bAsync = true ) = 0;
	
	virtual bool STDCALL Draw( IGFXVertices *pVertices, IGFXIndices *pIndices ) = 0;
	virtual bool STDCALL DrawTemp() = 0;
	virtual bool STDCALL DrawMesh( IGFXMesh *pMesh, const SHMatrix *matrices, int nNumMatrices ) = 0;
	virtual bool STDCALL DrawStringA( const char *pszString, int nX, int nY, DWORD dwColor = 0xFFFFFFFF ) = 0;
	virtual bool STDCALL DrawString( const wchar_t *pszString, int nX, int nY, DWORD dwColor = 0xFFFFFFFF ) = 0;
	virtual bool STDCALL DrawText( IGFXText *pText, const RECT &rect, int nY, DWORD dwFlags = FNT_FORMAT_LEFT ) = 0;
	virtual bool STDCALL DrawRects( const SGFXRect2 *pRects, int nNumRects, bool bSolid = true ) = 0;
	
	virtual bool STDCALL SetGammaRamp( const SGFXGammaRamp &ramp, bool bCalibrate ) = 0;
	virtual bool STDCALL GetGammaRamp( const SGFXGammaRamp *pRamp ) = 0;
	virtual void STDCALL SetGammaCorrectionValues( const float fBrightness, const float fContrast, const float fGamma ) = 0;
	virtual void STDCALL GetGammaCorrectionValues( float *pfBrightness, float *pfContrast, float *pfGamma ) = 0;

	virtual bool STDCALL TakeScreenShot( interface IImage *pImage ) = 0;
	
	virtual int STDCALL GetNumPassedVertices() const = 0;
	virtual int STDCALL GetNumPassedPrimitives() const = 0;
	
	virtual bool STDCALL SetShadingEffect( int nEffect ) = 0;
};
#endif // __GFX_H__
