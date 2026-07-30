#include "StdAfx.h"

#include "GraphicsEngineGpu.h"
#include "TextureGpu.h"
#include "GeometryBufferGpu.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace
{
    size_t CopySize( size_t left, size_t right ) { return left < right ? left : right; }
}

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

GraphicsEngineGpu::GraphicsEngineGpu( const GfxGpuApi &api ) : api_( api ), api_valid_( IsApiUsable( api_ ) )
{
    if ( !api_valid_ ) last_error_ = "GfxGpu API version or size mismatch";
}

bool GraphicsEngineGpu::fail( const char *message )
{
    last_error_ = message;
    return false;
}

bool GraphicsEngineGpu::Check( GfxGpuResult result, const char *operation )
{
    if ( result == GFXGPU_OK )
        return true;
    char diagnostic[256]{};
    uint32_t written = 0;
    if ( renderer_ && api_.get_last_error )
        api_.get_last_error( renderer_, diagnostic, sizeof( diagnostic ), &written );
    last_error_ = operation ? operation : "GfxGpu operation";
    last_error_ += ": ";
    last_error_ += written ? diagnostic : "GfxGpu operation failed";
    return false;
}

bool GraphicsEngineGpu::SetState( uint32_t kind, uint32_t index, uint32_t value, const void *data, size_t data_size, const char *operation )
{
    if ( !renderer_ || !api_.set_state ) return fail( "GfxGpu state API is unavailable" );
    GfxGpuStateInfo state{};
    state.struct_size = sizeof( state );
    state.kind = kind;
    state.index = index;
    state.value = value;
    if ( data && data_size )
        std::memcpy( state.values, data, CopySize( data_size, sizeof( state.values ) ) );
    return Check( api_.set_state( renderer_, &state ), operation );
}

bool GraphicsEngineGpu::CreateTextureHandle( int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage, GfxGpuHandle *out_handle )
{
    if ( !renderer_ || !out_handle || !api_.create_texture ) return fail( "create_texture is unavailable" );
    GfxGpuTextureCreateInfo info{ sizeof( info ), static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), static_cast<uint32_t>( mips ), static_cast<uint32_t>( format ), static_cast<uint32_t>( usage ) };
    return Check( api_.create_texture( renderer_, &info, out_handle ), "create_texture" );
}

bool GraphicsEngineGpu::UploadTexture( GfxGpuHandle handle, int mip, const void *data, size_t bytes, int row_pitch )
{
    if ( !renderer_ || !api_.upload_texture || bytes > 0xffffffffu ) return fail( "upload_texture is unavailable" );
    GfxGpuTextureUploadInfo info{ sizeof( info ), data, static_cast<uint32_t>( bytes ), static_cast<uint32_t>( row_pitch ), static_cast<uint32_t>( mip ) };
    return Check( api_.upload_texture( renderer_, handle, &info ), "upload_texture" );
}

bool GraphicsEngineGpu::DestroyTextureHandle( GfxGpuHandle handle )
{
    if ( !handle || !renderer_ || !api_.destroy_texture ) return false;
    return Check( api_.destroy_texture( renderer_, handle ), "destroy_texture" );
}

bool GraphicsEngineGpu::CreateRenderTargetHandle( int width, int height, EGFXPixelFormat format, GfxGpuHandle *out_handle )
{
    if ( !renderer_ || !out_handle || !api_.create_render_target ) return fail( "create_render_target is unavailable" );
    GfxGpuRenderTargetCreateInfo info{ sizeof( info ), static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), static_cast<uint32_t>( format ) };
    return Check( api_.create_render_target( renderer_, &info, out_handle ), "create_render_target" );
}

bool GraphicsEngineGpu::BindRenderTargetHandle( GfxGpuHandle handle )
{
    if ( !renderer_ || !api_.bind_render_target ) return fail( "bind_render_target is unavailable" );
    return Check( api_.bind_render_target( renderer_, handle ), "bind_render_target" );
}

bool GraphicsEngineGpu::CreateBufferHandle( uint32_t elements, uint32_t format, uint32_t stride, EGFXDynamic usage, GfxGpuHandle *out_handle )
{
    if ( !renderer_ || !out_handle || !api_.create_buffer ) return fail( "create_buffer is unavailable" );
    GfxGpuBufferCreateInfo info{ sizeof( info ), elements, format, stride, static_cast<uint32_t>( usage ) };
    return Check( api_.create_buffer( renderer_, &info, out_handle ), "create_buffer" );
}
bool GraphicsEngineGpu::UploadBuffer( GfxGpuHandle handle, const void *data, size_t bytes, uint32_t offset )
{
    if ( !renderer_ || !api_.upload_buffer || bytes > 0xffffffffu ) return fail( "upload_buffer is unavailable" );
    GfxGpuBufferUploadInfo info{ sizeof( info ), data, static_cast<uint32_t>( bytes ), offset };
    return Check( api_.upload_buffer( renderer_, handle, &info ), "upload_buffer" );
}
bool GraphicsEngineGpu::DestroyBufferHandle( GfxGpuHandle handle )
{
    if ( !renderer_ || !api_.destroy_buffer ) return false;
    return Check( api_.destroy_buffer( renderer_, handle ), "destroy_buffer" );
}
bool GraphicsEngineGpu::DrawBufferHandle( GfxGpuHandle handle, uint32_t primitives )
{
    return renderer_ && api_.draw && Check( api_.draw( renderer_, 0, primitives ), "draw_temporary" );
}
bool GraphicsEngineGpu::DrawIndexedBufferHandle( GfxGpuHandle handle, uint32_t index_size, uint32_t count )
{
    return renderer_ && api_.draw_indexed && Check( api_.draw_indexed( renderer_, handle, index_size, 0, count, 0 ), "draw_temporary_indexed" );
}

bool STDCALL GraphicsEngineGpu::Init( const char *pszAdapterName, GFXNativeWindow window )
{
    if ( !api_valid_ ) return false;
    if ( initialized_ ) return true;
    if ( window.value )
    {
        if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) )
        {
            last_error_ = SDL_GetError();
            return false;
        }
        video_subsystem_owned_ = true;
        SDL_PropertiesID properties = SDL_CreateProperties();
        if ( !properties ) { last_error_ = SDL_GetError(); return false; }
        SDL_SetPointerProperty( properties, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, window.value );
        SDL_SetNumberProperty( properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width_ > 0 ? width_ : GFX_DEFAULT_SCREEN_WIDTH );
        SDL_SetNumberProperty( properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height_ > 0 ? height_ : GFX_DEFAULT_SCREEN_HEIGHT );
        sdl_window_ = SDL_CreateWindowWithProperties( properties );
        SDL_DestroyProperties( properties );
        if ( !sdl_window_ ) { last_error_ = SDL_GetError(); return false; }
    }
    GfxGpuCreateInfo info{};
    info.struct_size = sizeof( info );
    info.sdl_window = sdl_window_;
    info.width = width_ > 0 ? static_cast<uint32_t>( width_ ) : GFX_DEFAULT_SCREEN_WIDTH;
    info.height = height_ > 0 ? static_cast<uint32_t>( height_ ) : GFX_DEFAULT_SCREEN_HEIGHT;
    info.preferred_driver_utf8 = pszAdapterName;
    if ( !Check( api_.create( &info, &renderer_ ), "create" ) ) return false;
    initialized_ = true;
    adapter_name_ = pszAdapterName ? pszAdapterName : "SDL GPU";
    return true;
}

bool STDCALL GraphicsEngineGpu::Done()
{
    if ( renderer_ ) api_.destroy( renderer_ );
    renderer_ = nullptr;
    if ( sdl_window_ ) SDL_DestroyWindow( sdl_window_ );
    sdl_window_ = nullptr;
    if ( video_subsystem_owned_ ) SDL_QuitSubSystem( SDL_INIT_VIDEO );
    video_subsystem_owned_ = false;
    initialized_ = false;
    return true;
}

void STDCALL GraphicsEngineGpu::Clear() { Done(); }

bool STDCALL GraphicsEngineGpu::SetMode( int nSizeX, int nSizeY, int nBpp, int, EGFXFullscreen, int )
{
    if ( nBpp != 0 && nBpp != 32 ) return fail( "SDL GPU adapter supports 32-bit color only" );
    width_ = nSizeX; height_ = nSizeY;
    display_mode_ = { nSizeX, nSizeY, 32 };
    return !renderer_ || Check( api_.resize( renderer_, static_cast<uint32_t>( nSizeX ), static_cast<uint32_t>( nSizeY ) ), "resize" );
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
    return Check( api_.set_viewport( renderer_, &viewport ), "set_viewport" );
}
bool STDCALL GraphicsEngineGpu::ChangeViewport( int nWidth, int nHeight ) { return ChangeViewport( 0, 0, nWidth, nHeight, 0.0f, 1.0f ); }
bool STDCALL GraphicsEngineGpu::SetWorldTransforms( const int, const SHMatrix *, const int ) { return true; }
bool STDCALL GraphicsEngineGpu::SetViewTransform( const SHMatrix &matrix )
{
    view_matrix_ = matrix;
    if ( !renderer_ ) return true;
    GfxGpuMatrixInfo world{ sizeof( world ) }, view_projection{ sizeof( view_projection ) };
    std::memcpy( world.values, &matrix, CopySize( sizeof( world.values ), sizeof( matrix ) ) );
    std::memcpy( view_projection.values, &projection_matrix_, CopySize( sizeof( view_projection.values ), sizeof( projection_matrix_ ) ) );
    return Check( api_.set_transform( renderer_, &world, &view_projection ), "set_view_transform" );
}
bool STDCALL GraphicsEngineGpu::SetProjectionTransform( const SHMatrix &matrix )
{
    projection_matrix_ = matrix;
    if ( !renderer_ ) return true;
    GfxGpuMatrixInfo world{ sizeof( world ) }, view_projection{ sizeof( view_projection ) };
    std::memcpy( world.values, &view_matrix_, CopySize( sizeof( world.values ), sizeof( view_matrix_ ) ) );
    std::memcpy( view_projection.values, &matrix, CopySize( sizeof( view_projection.values ), sizeof( matrix ) ) );
    return Check( api_.set_transform( renderer_, &world, &view_projection ), "set_projection_transform" );
}
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
void STDCALL GraphicsEngineGpu::SetLight( int index, const SGFXLightDirectional &light ) { SetState( GFXGPU_STATE_LIGHT, static_cast<uint32_t>( index ), 1, &light, sizeof( light ), "set_directional_light" ); }
void STDCALL GraphicsEngineGpu::SetLight( int index, const SGFXLightPoint &light ) { SetState( GFXGPU_STATE_LIGHT, static_cast<uint32_t>( index ), 2, &light, sizeof( light ), "set_point_light" ); }
void STDCALL GraphicsEngineGpu::SetLight( int index, const SGFXLightSpot &light ) { SetState( GFXGPU_STATE_LIGHT, static_cast<uint32_t>( index ), 3, &light, sizeof( light ), "set_spot_light" ); }
void STDCALL GraphicsEngineGpu::EnableLight( int index, bool enable ) { SetState( GFXGPU_STATE_LIGHT, static_cast<uint32_t>( index ), enable ? 1u : 0u, nullptr, 0, "enable_light" ); }
void STDCALL GraphicsEngineGpu::SetMaterial( const SGFXMaterial &material ) { SetState( GFXGPU_STATE_MATERIAL, 0, 0, &material, sizeof( material ), "set_material" ); }
bool STDCALL GraphicsEngineGpu::SetTexture( int stage, IGFXBaseTexture *texture )
{
    if ( stage < 0 || stage > 1 ) return fail( "only texture stages 0 and 1 are supported" );
    TextureGpu *gpu_texture = dynamic_cast<TextureGpu *>( texture );
    if ( !gpu_texture ) return fail( "texture does not belong to the SDL GPU adapter" );
    if ( !renderer_ || !api_.set_texture ) return fail( "set_texture is unavailable" );
    return Check( api_.set_texture( renderer_, gpu_texture->Handle() ), "set_texture" );
}
bool STDCALL GraphicsEngineGpu::SetWireframe( bool enable ) { return SetState( GFXGPU_STATE_WIREFRAME, 0, enable ? 1u : 0u, nullptr, 0, "set_wireframe" ); }
bool STDCALL GraphicsEngineGpu::SetCullMode( EGFXCull cull ) { return SetState( GFXGPU_STATE_CULL_MODE, 0, static_cast<uint32_t>( cull ), nullptr, 0, "set_cull_mode" ); }
bool STDCALL GraphicsEngineGpu::SetDepthBufferMode( EGFXDepthBuffer depth, EGFXCmpFunction cmp ) { return SetState( GFXGPU_STATE_DEPTH_MODE, static_cast<uint32_t>( cmp ), static_cast<uint32_t>( depth ), nullptr, 0, "set_depth_mode" ); }
bool STDCALL GraphicsEngineGpu::EnableLighting( bool enable ) { return SetState( GFXGPU_STATE_LIGHTING, 0, enable ? 1u : 0u, nullptr, 0, "set_lighting" ); }
bool STDCALL GraphicsEngineGpu::EnableSpecular( bool enable ) { return SetState( GFXGPU_STATE_SPECULAR, 0, enable ? 1u : 0u, nullptr, 0, "set_specular" ); }
bool STDCALL GraphicsEngineGpu::SetFont( IGFXFont * ) { return fail( "Font adapter is not implemented in P06-M01" ); }
bool STDCALL GraphicsEngineGpu::IsActive() { return initialized_; }
bool STDCALL GraphicsEngineGpu::BeginScene() { return renderer_ && Check( api_.begin_frame( renderer_ ), "begin_frame" ); }
bool STDCALL GraphicsEngineGpu::EndScene() { return renderer_ && Check( api_.end_frame( renderer_ ), "end_frame" ); }
bool STDCALL GraphicsEngineGpu::IsSafeToPresent() const { return initialized_; }

bool STDCALL GraphicsEngineGpu::Clear( int, RECT *, DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil )
{
    if ( !renderer_ ) return fail( "Clear requires Init" );
    GfxGpuClearInfo clear{ sizeof( clear ), static_cast<uint32_t>( dwFlags ), dwColor, fDepth, dwStencil };
    return Check( api_.clear( renderer_, &clear ), "clear" );
}
bool STDCALL GraphicsEngineGpu::Flip() { return renderer_ && Check( api_.present( renderer_ ), "present" ); }
bool STDCALL GraphicsEngineGpu::SetRenderTarget( IGFXRTexture *target )
{
    if ( !target ) return BindRenderTargetHandle( 0 );
    RenderTargetGpu *gpu_target = dynamic_cast<RenderTargetGpu *>( target );
    if ( !gpu_target ) return fail( "render target does not belong to the SDL GPU adapter" );
    return BindRenderTargetHandle( gpu_target->Handle() );
}
void STDCALL GraphicsEngineGpu::SetOptimizedBuffers( bool ) {}
IGFXVertices * STDCALL GraphicsEngineGpu::CreateVertices( int elements, DWORD format, EGFXPrimitiveType type, EGFXDynamic usage, IGFXVertices * )
{
    VerticesGpu *buffer = new VerticesGpu( this, elements, format, 32, usage, type );
    if ( !buffer->IsValid() ) { delete buffer; return nullptr; }
    return buffer;
}
IGFXIndices * STDCALL GraphicsEngineGpu::CreateIndices( int elements, DWORD format, EGFXPrimitiveType type, EGFXDynamic usage, IGFXIndices * )
{
    const int stride = format == GFXIF_INDEX32 ? 4 : 2;
    IndicesGpu *buffer = new IndicesGpu( this, elements, format, stride, usage, type );
    if ( !buffer->IsValid() ) { delete buffer; return nullptr; }
    return buffer;
}
bool STDCALL GraphicsEngineGpu::BeginSolidVertexBlock( int, DWORD, EGFXDynamic ) { return false; }
bool STDCALL GraphicsEngineGpu::EndSolidVertexBlock() { return false; }
bool STDCALL GraphicsEngineGpu::BeginSolidIndexBlock( int, DWORD, EGFXDynamic ) { return false; }
bool STDCALL GraphicsEngineGpu::EndSolidIndexBlock() { return false; }
void * STDCALL GraphicsEngineGpu::GetTempVertices( int elements, DWORD, EGFXPrimitiveType type )
{
    if ( !temporary_bytes_.empty() ) return nullptr;
    temporary_stride_ = 32; temporary_count_ = elements; temporary_type_ = type; temporary_indices_ = false;
    try { temporary_bytes_.assign( static_cast<size_t>( elements ) * temporary_stride_, 0 ); } catch ( ... ) { return nullptr; }
    return temporary_bytes_.data();
}
void * STDCALL GraphicsEngineGpu::GetTempIndices( int elements, DWORD format, EGFXPrimitiveType type )
{
    if ( !temporary_bytes_.empty() ) return nullptr;
    temporary_stride_ = format == GFXIF_INDEX32 ? 4 : 2; temporary_count_ = elements; temporary_type_ = type; temporary_indices_ = true;
    try { temporary_bytes_.assign( static_cast<size_t>( elements ) * temporary_stride_, 0 ); } catch ( ... ) { return nullptr; }
    return temporary_bytes_.data();
}
IGFXTexture * STDCALL GraphicsEngineGpu::CreateTexture( int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage, IGFXTexture * )
{
    TextureGpu *texture = new TextureGpu( this, width, height, mips, format, usage );
    if ( !texture->IsValid() ) { delete texture; return nullptr; }
    return texture;
}
IGFXRTexture * STDCALL GraphicsEngineGpu::CreateRTexture( int width, int height )
{
    RenderTargetGpu *target = new RenderTargetGpu( this, width, height, GFXPF_ARGB8888 );
    if ( !target->IsValid() ) { delete target; return nullptr; }
    return target;
}
bool STDCALL GraphicsEngineGpu::UpdateTexture( IGFXTexture *, IGFXTexture *, bool ) { return false; }
bool STDCALL GraphicsEngineGpu::Draw( IGFXVertices *vertices, IGFXIndices *indices )
{
    VerticesGpu *vb = dynamic_cast<VerticesGpu *>( vertices );
    if ( !vb || !renderer_ ) return fail( "vertex buffer does not belong to the SDL GPU adapter" );
    if ( indices )
    {
        IndicesGpu *ib = dynamic_cast<IndicesGpu *>( indices );
        if ( !ib || !api_.draw_indexed ) return fail( "index buffer does not belong to the SDL GPU adapter" );
        return Check( api_.draw_indexed( renderer_, ib->Handle(), ib->Stride(), 0, ib->Count(), 0 ), "draw_indexed" );
    }
    if ( !api_.draw ) return fail( "draw is unavailable" );
    uint32_t primitives = vb->Count();
    if ( vb->Type() == GFXPT_TRIANGLELIST ) primitives /= 3;
    else if ( vb->Type() == GFXPT_TRIANGLESTRIP ) primitives = primitives > 2 ? primitives - 2 : 0;
    else if ( vb->Type() == GFXPT_LINELIST ) primitives /= 2;
    else if ( vb->Type() == GFXPT_LINESTRIP ) primitives = primitives > 1 ? primitives - 1 : 0;
    if ( primitives == 0 ) return fail( "geometry has no drawable primitives" );
    return Check( api_.draw( renderer_, 0, primitives ), "draw" );
}
bool STDCALL GraphicsEngineGpu::DrawTemp()
{
    if ( temporary_bytes_.empty() ) return false;
    GfxGpuHandle handle = 0;
    const bool created = CreateBufferHandle( static_cast<uint32_t>( temporary_count_ ), 0, static_cast<uint32_t>( temporary_stride_ ), GFXD_DYNAMIC, &handle ) && UploadBuffer( handle, temporary_bytes_.data(), temporary_bytes_.size() );
    bool drawn = false;
    if ( created ) {
        if ( temporary_indices_ ) drawn = DrawIndexedBufferHandle( handle, static_cast<uint32_t>( temporary_stride_ ), static_cast<uint32_t>( temporary_count_ ) );
        else { uint32_t primitives = static_cast<uint32_t>( temporary_count_ ); if ( temporary_type_ == GFXPT_TRIANGLELIST ) primitives /= 3; drawn = DrawBufferHandle( handle, primitives ); }
        DestroyBufferHandle( handle );
    }
    temporary_bytes_.clear();
    return created && drawn;
}
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
bool STDCALL GraphicsEngineGpu::SetShadingEffect( int effect ) { return SetState( GFXGPU_STATE_SHADE_EFFECT, 0, static_cast<uint32_t>( effect ), nullptr, 0, "set_shade_effect" ); }
