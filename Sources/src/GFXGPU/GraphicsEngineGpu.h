#ifndef BLITZKRIEG_GRAPHICS_ENGINE_GPU_H
#define BLITZKRIEG_GRAPHICS_ENGINE_GPU_H

#include "..\\GFX\\GFX.H"
#include "gfxgpu_c.h"

#include <string>
#include <vector>

class GraphicsEngineGpu final : public IGFX
{
public:
    GraphicsEngineGpu();
    explicit GraphicsEngineGpu( const GfxGpuApi &api );
    ~GraphicsEngineGpu() = default;
    static IRefCount * STDCALL CreateNewClassInstanceInternal() { return new GraphicsEngineGpu(); }
    void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ += nRef; }
    void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ -= nRef; if ( ref_count_ == 0 ) delete this; }
    bool STDCALL IsValid() const override { return ref_count_ >= 0; }

    bool STDCALL Init( const char *pszAdapterName, GFXNativeWindow window ) override;
    bool STDCALL Done() override;
    void STDCALL Clear() override;
    bool STDCALL SetMode( int nSizeX, int nSizeY, int nBpp, int nStencilBPP, EGFXFullscreen eFullscreen, int nFreq = 0 ) override;
    EGFXVideoCard STDCALL GetVideoCard() override;
    void STDCALL MoveTo( int nX, int nY ) override;
    RECT STDCALL GetScreenRect() const override;
    int STDCALL GetScreenBPP() const override;
    const char * STDCALL GetAdapterName() const override;
    const SGFXDisplayMode * STDCALL GetDisplayModes() const override;
    void STDCALL PushViewport() override;
    bool STDCALL PopViewport() override;
    bool STDCALL ChangeViewport( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ ) override;
    bool STDCALL ChangeViewport( int nWidth, int nHeight ) override;
    bool STDCALL SetWorldTransforms( const int nStartIndex, const SHMatrix *pMatrices, const int nNumMatrices ) override;
    bool STDCALL SetViewTransform( const SHMatrix &matrix ) override;
    bool STDCALL SetProjectionTransform( const SHMatrix &matrix ) override;
    bool STDCALL SetTextureTransform( int nIndex, const SHMatrix &matrix ) override;
    bool STDCALL SetupDirectTransform() override;
    bool STDCALL RestoreTransform() override;
    const SHMatrix & STDCALL GetViewMatrix() const override;
    const SHMatrix & STDCALL GetBillboardMatrix() const override;
    const SHMatrix & STDCALL GetInverseViewMatrix() const override;
    const SHMatrix & STDCALL GetProjectionMatrix() const override;
    const SHMatrix & STDCALL GetViewportMatrix() const override;
    void STDCALL GetViewVolume( SPlane *pPlanes ) const override;
    void STDCALL GetViewVolumeCrosses( const CVec2 &vPoint, CVec3 *pvNear, CVec3 *pvFar ) override;
    void STDCALL SetLight( int nIndex, const SGFXLightDirectional &light ) override;
    void STDCALL SetLight( int nIndex, const SGFXLightPoint &light ) override;
    void STDCALL SetLight( int nIndex, const SGFXLightSpot &light ) override;
    void STDCALL EnableLight( int nIndex, bool bEnable ) override;
    void STDCALL SetMaterial( const SGFXMaterial &material ) override;
    bool STDCALL SetTexture( int nStage, IGFXBaseTexture *pTexture ) override;
    bool STDCALL SetWireframe( bool bWireframe ) override;
    bool STDCALL SetCullMode( EGFXCull cull ) override;
    bool STDCALL SetDepthBufferMode( EGFXDepthBuffer depth, EGFXCmpFunction cmp = GFXCMP_DEFAULT ) override;
    bool STDCALL EnableLighting( bool bLighting ) override;
    bool STDCALL EnableSpecular( bool bEnable ) override;
    bool STDCALL SetFont( IGFXFont *pFont ) override;
    bool STDCALL IsActive() override;
    bool STDCALL BeginScene() override;
    bool STDCALL EndScene() override;
    bool STDCALL IsSafeToPresent() const override;
    bool STDCALL Clear( int nNumRects, RECT *pRects, DWORD dwFlags, DWORD dwColor = 0, float fDepth = 1.0f, DWORD dwStencil = 0 ) override;
    bool STDCALL Flip() override;
    bool STDCALL SetRenderTarget( IGFXRTexture *pRT ) override;
    void STDCALL SetOptimizedBuffers( bool bEnable ) override;
    IGFXVertices * STDCALL CreateVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXVertices *pVertices = 0 ) override;
    IGFXIndices * STDCALL CreateIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXIndices *pIndices = 0 ) override;
    bool STDCALL BeginSolidVertexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic ) override;
    bool STDCALL EndSolidVertexBlock() override;
    bool STDCALL BeginSolidIndexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic ) override;
    bool STDCALL EndSolidIndexBlock() override;
    void * STDCALL GetTempVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type ) override;
    void * STDCALL GetTempIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type ) override;
    IGFXTexture * STDCALL CreateTexture( int nSizeX, int nSizeY, int nNumMipLevels, EGFXPixelFormat format, EGFXDynamic eDynamic, IGFXTexture *pTexture = 0 ) override;
    IGFXRTexture * STDCALL CreateRTexture( int nSizeX, int nSizeY ) override;
    bool STDCALL UpdateTexture( IGFXTexture *pSrc, IGFXTexture *pDst, bool bAsync = true ) override;
    bool STDCALL Draw( IGFXVertices *pVertices, IGFXIndices *pIndices ) override;
    bool STDCALL DrawTemp() override;
    bool STDCALL DrawMesh( IGFXMesh *pMesh, const SHMatrix *matrices, int nNumMatrices ) override;
    bool STDCALL DrawStringA( const char *pszString, int nX, int nY, DWORD dwColor = 0xFFFFFFFF ) override;
    bool STDCALL DrawString( const wchar_t *pszString, int nX, int nY, DWORD dwColor = 0xFFFFFFFF ) override;
    bool STDCALL DrawText( IGFXText *pText, const RECT &rect, int nY, DWORD dwFlags = FNT_FORMAT_LEFT ) override;
    bool STDCALL DrawRects( const SGFXRect2 *pRects, int nNumRects, bool bSolid = true ) override;
    bool STDCALL SetGammaRamp( const SGFXGammaRamp &ramp, bool bCalibrate ) override;
    bool STDCALL GetGammaRamp( const SGFXGammaRamp *pRamp ) override;
    void STDCALL SetGammaCorrectionValues( const float fBrightness, const float fContrast, const float fGamma ) override;
    void STDCALL GetGammaCorrectionValues( float *pfBrightness, float *pfContrast, float *pfGamma ) override;
    bool STDCALL TakeScreenShot( IImage *pImage ) override;
    int STDCALL GetNumPassedVertices() const override;
    int STDCALL GetNumPassedPrimitives() const override;
    bool STDCALL SetShadingEffect( int nEffect ) override;

    bool CreateTextureHandle( int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage, GfxGpuHandle *out_handle );
    bool UploadTexture( GfxGpuHandle handle, int mip, const void *data, size_t bytes, int row_pitch );
    bool DestroyTextureHandle( GfxGpuHandle handle );
    bool CreateRenderTargetHandle( int width, int height, EGFXPixelFormat format, GfxGpuHandle *out_handle );
    bool BindRenderTargetHandle( GfxGpuHandle handle );
    bool CreateBufferHandle( uint32_t elements, uint32_t format, uint32_t stride, EGFXDynamic usage, GfxGpuHandle *out_handle );
    bool UploadBuffer( GfxGpuHandle handle, const void *data, size_t bytes, uint32_t offset = 0 );
    bool DestroyBufferHandle( GfxGpuHandle handle );
    bool DrawBufferHandle( GfxGpuHandle handle, uint32_t primitives );
    bool DrawIndexedBufferHandle( GfxGpuHandle handle, uint32_t index_size, uint32_t count );

private:
    bool fail( const char *message );
    bool Check( GfxGpuResult result, const char *operation );
    bool SetState( uint32_t kind, uint32_t index, uint32_t value, const void *data, size_t data_size, const char *operation );

    GfxGpuApi api_{};
    GfxGpuRenderer *renderer_ = nullptr;
    struct SDL_Window *sdl_window_ = nullptr;
    bool video_subsystem_owned_ = false;
    bool api_valid_ = false;
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
    int ref_count_ = 0;
    std::string adapter_name_;
    std::string last_error_;
    SGFXDisplayMode display_mode_{};
    SHMatrix view_matrix_{};
    SHMatrix billboard_matrix_{};
    SHMatrix inverse_view_matrix_{};
    SHMatrix projection_matrix_{};
    SHMatrix viewport_matrix_{};
    std::vector<unsigned char> temporary_vertex_bytes_;
    std::vector<unsigned char> temporary_index_bytes_;
    int temporary_vertex_stride_ = 0;
    int temporary_vertex_count_ = 0;
    int temporary_index_stride_ = 0;
    int temporary_index_count_ = 0;
    EGFXPrimitiveType temporary_type_ = GFXPT_TRIANGLELIST;
    int passed_vertices_ = 0;
    int passed_primitives_ = 0;
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float gamma_ = 0.0f;
};

#endif
