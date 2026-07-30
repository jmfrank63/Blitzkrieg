#include "StdAfx.h"

#include "GraphicsEngineGpu.h"

#include <cstring>

namespace
{
    bool IsApiUsable( const GfxGpuApi &api )
    {
        return api.abi_version == GFXGPU_ABI_VERSION &&
            api.struct_size >= sizeof( GfxGpuApi ) &&
            api.create != nullptr && api.destroy != nullptr &&
            api.begin_frame != nullptr && api.end_frame != nullptr &&
            api.present != nullptr;
    }
}

GraphicsEngineGpu::GraphicsEngineGpu()
{
    api_valid_ = gfxgpu_get_api( GFXGPU_ABI_VERSION, &api_ ) == GFXGPU_OK && IsApiUsable( api_ );
    if ( !api_valid_ )
        last_error_ = "GfxGpu API version or size mismatch";
}

bool GraphicsEngineGpu::fail( const char *message )
{
    last_error_ = message;
    return false;
}

bool GraphicsEngineGpu::call( GfxGpuResult result )
{
    if ( result == GFXGPU_OK )
        return true;
    last_error_ = "GfxGpu operation failed";
    return false;
}

bool STDCALL GraphicsEngineGpu::Init( const char *pszAdapterName, GFXNativeWindow window )
{
    if ( !api_valid_ ) return false;
    if ( initialized_ ) return true;
    GfxGpuCreateInfo info{};
    info.struct_size = sizeof( info );
    info.sdl_window = window.value;
    info.width = width_ > 0 ? static_cast<uint32_t>( width_ ) : GFX_DEFAULT_SCREEN_WIDTH;
    info.height = height_ > 0 ? static_cast<uint32_t>( height_ ) : GFX_DEFAULT_SCREEN_HEIGHT;
    info.preferred_driver_utf8 = pszAdapterName;
    if ( !call( api_.create( &info, &renderer_ ) ) ) return false;
    initialized_ = true;
    adapter_name_ = pszAdapterName ? pszAdapterName : "SDL GPU";
    return true;
}

bool STDCALL GraphicsEngineGpu::Done()
{
    if ( renderer_ ) api_.destroy( renderer_ );
    renderer_ = nullptr;
    initialized_ = false;
    return true;
}

void STDCALL GraphicsEngineGpu::Clear() { Done(); }

bool STDCALL GraphicsEngineGpu::SetMode( int nSizeX, int nSizeY, int nBpp, int, EGFXFullscreen, int )
{
    if ( nBpp != 0 && nBpp != 32 ) return fail( "SDL GPU adapter supports 32-bit color only" );
    width_ = nSizeX; height_ = nSizeY;
    display_mode_ = { nSizeX, nSizeY, 32 };
    return !renderer_ || call( api_.resize( renderer_, static_cast<uint32_t>( nSizeX ), static_cast<uint32_t>( nSizeY ) ) );
}

EGFXVideoCard STDCALL GraphicsEngineGpu::GetVideoCard() { return GFXVC_DEFAULT; }
void STDCALL GraphicsEngineGpu::MoveTo( int, int ) {}
RECT STDCALL GraphicsEngineGpu::GetScreenRect() const { RECT r = { 0, 0, width_, height_ }; return r; }
int STDCALL GraphicsEngineGpu::GetScreenBPP() const { return 32; }
const char * STDCALL GraphicsEngineGpu::GetAdapterName() const { return adapter_name_.c_str(); }
const SGFXDisplayMode * STDCALL GraphicsEngineGpu::GetDisplayModes() const { return &display_mode_; }
void STDCALL GraphicsEngineGpu::PushViewport() {}
bool STDCALL GraphicsEngineGpu::PopViewport() { return true; }

bool STDCALL GraphicsEngineGpu::ChangeViewport( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ )
{
    if ( !renderer_ ) return fail( "ChangeViewport requires Init" );
    GfxGpuViewportInfo viewport{ sizeof( viewport ), static_cast<float>( nX ), static_cast<float>( nY ), static_cast<float>( nWidth ), static_cast<float>( nHeight ), fMinZ, fMaxZ };
    return call( api_.set_viewport( renderer_, &viewport ) );
}
bool STDCALL GraphicsEngineGpu::ChangeViewport( int nWidth, int nHeight ) { return ChangeViewport( 0, 0, nWidth, nHeight, 0.0f, 1.0f ); }
bool STDCALL GraphicsEngineGpu::SetWorldTransforms( const int, const SHMatrix *, const int ) { return true; }
bool STDCALL GraphicsEngineGpu::SetViewTransform( const SHMatrix &matrix ) { view_matrix_ = matrix; return true; }
bool STDCALL GraphicsEngineGpu::SetProjectionTransform( const SHMatrix &matrix ) { projection_matrix_ = matrix; return true; }
bool STDCALL GraphicsEngineGpu::SetTextureTransform( int, const SHMatrix & ) { return true; }
bool STDCALL GraphicsEngineGpu::SetupDirectTransform() { return true; }
bool STDCALL GraphicsEngineGpu::RestoreTransform() { return true; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetViewMatrix() const { return view_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetBillboardMatrix() const { return billboard_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetInverseViewMatrix() const { return inverse_view_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetProjectionMatrix() const { return projection_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetViewportMatrix() const { return viewport_matrix_; }
void STDCALL GraphicsEngineGpu::GetViewVolume( SPlane * ) const {}
void STDCALL GraphicsEngineGpu::GetViewVolumeCrosses( const CVec2 &, CVec3 *, CVec3 * ) {}
void STDCALL GraphicsEngineGpu::SetLight( int, const SGFXLightDirectional & ) {}
void STDCALL GraphicsEngineGpu::SetLight( int, const SGFXLightPoint & ) {}
void STDCALL GraphicsEngineGpu::SetLight( int, const SGFXLightSpot & ) {}
void STDCALL GraphicsEngineGpu::EnableLight( int, bool ) {}
void STDCALL GraphicsEngineGpu::SetMaterial( const SGFXMaterial & ) {}
bool STDCALL GraphicsEngineGpu::SetTexture( int, IGFXBaseTexture * ) { return fail( "Texture adapter is not implemented in P06-M01" ); }
bool STDCALL GraphicsEngineGpu::SetWireframe( bool ) { return true; }
bool STDCALL GraphicsEngineGpu::SetCullMode( EGFXCull ) { return true; }
bool STDCALL GraphicsEngineGpu::SetDepthBufferMode( EGFXDepthBuffer, EGFXCmpFunction ) { return true; }
bool STDCALL GraphicsEngineGpu::EnableLighting( bool ) { return true; }
bool STDCALL GraphicsEngineGpu::EnableSpecular( bool ) { return true; }
bool STDCALL GraphicsEngineGpu::SetFont( IGFXFont * ) { return fail( "Font adapter is not implemented in P06-M01" ); }
bool STDCALL GraphicsEngineGpu::IsActive() { return initialized_; }
bool STDCALL GraphicsEngineGpu::BeginScene() { return renderer_ && call( api_.begin_frame( renderer_ ) ); }
bool STDCALL GraphicsEngineGpu::EndScene() { return renderer_ && call( api_.end_frame( renderer_ ) ); }
bool STDCALL GraphicsEngineGpu::IsSafeToPresent() const { return initialized_; }

bool STDCALL GraphicsEngineGpu::Clear( int, RECT *, DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil )
{
    if ( !renderer_ ) return fail( "Clear requires Init" );
    GfxGpuClearInfo clear{ sizeof( clear ), static_cast<uint32_t>( dwFlags ), dwColor, fDepth, dwStencil };
    return call( api_.clear( renderer_, &clear ) );
}
bool STDCALL GraphicsEngineGpu::Flip() { return renderer_ && call( api_.present( renderer_ ) ); }
bool STDCALL GraphicsEngineGpu::SetRenderTarget( IGFXRTexture * ) { return fail( "Render target adapter is not implemented in P06-M01" ); }
void STDCALL GraphicsEngineGpu::SetOptimizedBuffers( bool ) {}
IGFXVertices * STDCALL GraphicsEngineGpu::CreateVertices( int, DWORD, EGFXPrimitiveType, EGFXDynamic, IGFXVertices * ) { return nullptr; }
IGFXIndices * STDCALL GraphicsEngineGpu::CreateIndices( int, DWORD, EGFXPrimitiveType, EGFXDynamic, IGFXIndices * ) { return nullptr; }
bool STDCALL GraphicsEngineGpu::BeginSolidVertexBlock( int, DWORD, EGFXDynamic ) { return false; }
bool STDCALL GraphicsEngineGpu::EndSolidVertexBlock() { return false; }
bool STDCALL GraphicsEngineGpu::BeginSolidIndexBlock( int, DWORD, EGFXDynamic ) { return false; }
bool STDCALL GraphicsEngineGpu::EndSolidIndexBlock() { return false; }
void * STDCALL GraphicsEngineGpu::GetTempVertices( int, DWORD, EGFXPrimitiveType ) { return nullptr; }
void * STDCALL GraphicsEngineGpu::GetTempIndices( int, DWORD, EGFXPrimitiveType ) { return nullptr; }
IGFXTexture * STDCALL GraphicsEngineGpu::CreateTexture( int, int, int, EGFXPixelFormat, EGFXDynamic, IGFXTexture * ) { return nullptr; }
IGFXRTexture * STDCALL GraphicsEngineGpu::CreateRTexture( int, int ) { return nullptr; }
bool STDCALL GraphicsEngineGpu::UpdateTexture( IGFXTexture *, IGFXTexture *, bool ) { return false; }
bool STDCALL GraphicsEngineGpu::Draw( IGFXVertices *, IGFXIndices * ) { return fail( "Geometry adapter is not implemented in P06-M01" ); }
bool STDCALL GraphicsEngineGpu::DrawTemp() { return false; }
bool STDCALL GraphicsEngineGpu::DrawMesh( IGFXMesh *, const SHMatrix *, int ) { return false; }
bool STDCALL GraphicsEngineGpu::DrawStringA( const char *, int, int, DWORD ) { return false; }
bool STDCALL GraphicsEngineGpu::DrawString( const wchar_t *, int, int, DWORD ) { return false; }
bool STDCALL GraphicsEngineGpu::DrawText( IGFXText *, const RECT &, int, DWORD ) { return false; }
bool STDCALL GraphicsEngineGpu::DrawRects( const SGFXRect2 *, int, bool ) { return false; }
bool STDCALL GraphicsEngineGpu::SetGammaRamp( const SGFXGammaRamp &, bool ) { return false; }
bool STDCALL GraphicsEngineGpu::GetGammaRamp( const SGFXGammaRamp * ) { return false; }
void STDCALL GraphicsEngineGpu::SetGammaCorrectionValues( const float, const float, const float ) {}
void STDCALL GraphicsEngineGpu::GetGammaCorrectionValues( float *b, float *c, float *g ) { if ( b ) *b = 1.0f; if ( c ) *c = 1.0f; if ( g ) *g = 1.0f; }
bool STDCALL GraphicsEngineGpu::TakeScreenShot( IImage * ) { return false; }
int STDCALL GraphicsEngineGpu::GetNumPassedVertices() const { return 0; }
int STDCALL GraphicsEngineGpu::GetNumPassedPrimitives() const { return 0; }
bool STDCALL GraphicsEngineGpu::SetShadingEffect( int ) { return true; }
