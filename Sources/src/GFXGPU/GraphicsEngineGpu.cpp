#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "GraphicsEngineGpu.h"
#include "TextureGpu.h"
#include "FontGpu.h"
#include "GeometryBufferGpu.h"
#include "MeshGpu.h"
#include "..//GFX//GFXHelper.h"
#include "..//Main//TextSystem.h"
#include "..//Image//Image.h"

#include <SDL3/SDL.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
bool DrawFallbackGlyphs( GraphicsEngineGpu *graphics, const wchar_t *text, int x, int y, DWORD color, int max_width = 0 )
{
    if ( !text || !*text ) return true;
    std::vector<SGFXRect2> glyphs;
    int cursor = x;
    for ( const wchar_t *it = text; *it; ++it )
    {
        if ( max_width > 0 && cursor + 8 > max_width ) break;
        if ( *it != L' ' && *it != L'\t' )
        {
            SGFXRect2 glyph;
            glyph.rect.minx = static_cast<float>( cursor ); glyph.rect.miny = static_cast<float>( y );
            glyph.rect.maxx = static_cast<float>( cursor + 7 ); glyph.rect.maxy = static_cast<float>( y + 12 );
            glyph.color = color;   // the vertex shader swizzles D3DCOLOR now
            glyphs.push_back( glyph );
        }
        cursor += *it == L'\t' ? 32 : 8;
    }
    if ( glyphs.empty() ) return true;
    graphics->SetTexture( 0, nullptr );
    return graphics->DrawRects( glyphs.data(), static_cast<int>( glyphs.size() ), true );
}
}

#include <cstring>

namespace
{
    // Renderer failures were recorded in last_error_ and never surfaced, so a
    // draw that could not build its pipeline looked like geometry that simply
    // did not appear. BK_GFX_TRACE=1 reports them.
    bool GfxTraceEnabled()
    {
        static const bool enabled = getenv( "BK_GFX_TRACE" ) != 0;
        return enabled;
    }
}


namespace
{
    size_t CopySize( size_t left, size_t right ) { return left < right ? left : right; }

    // Byte size of one vertex in the given FVF. This mirrors decodeFvf in
    // vertex_layout.zig, which derives the pipeline's vertex pitch and attribute
    // offsets from the same bits, so the two must agree: the CPU writes at this
    // stride and the GPU reads at the one the Zig side computes.
    int FvfStride( DWORD format )
    {
        int stride = 0;
        switch ( format & 0x0f )
        {
            // GFXFVF_XYZRHW is a pre-transformed position of four floats.
            case GFXFVF_XYZ:    stride = 12; break;
            case GFXFVF_XYZRHW: stride = 16; break;
            case GFXFVF_XYZB1:  stride = 12 + 4; break;
            case GFXFVF_XYZB2:  stride = 12 + 8; break;
            case GFXFVF_XYZB3:  stride = 12 + 12; break;
            case GFXFVF_XYZB4:  stride = 12 + 16; break;
            default: return 0;
        }
        if ( format & GFXFVF_NORMAL ) stride += 12;
        if ( format & GFXFVF_PSIZE ) stride += 4;
        if ( format & GFXFVF_DIFFUSE ) stride += 4;
        if ( format & GFXFVF_SPECULAR ) stride += 4;
        const int textures = static_cast<int>( ( format >> 8 ) & 0x0f );
        if ( textures > 8 ) return 0;
        for ( int index = 0; index < textures; ++index )
        {
            switch ( ( format >> ( 16 + index * 2 ) ) & 3 )
            {
                case 0: stride += 8; break;      // two floats, the common case
                case 1: stride += 12; break;
                case 2: stride += 16; break;
                default: stride += 4; break;
            }
        }
        return stride;
    }

    // How many primitives a run of vertices makes. Only the triangle-list case
    // was ever applied, so a line list asked the renderer to draw one primitive
    // per vertex and it then read three vertices for each of them.
    uint32_t PrimitiveCount( EGFXPrimitiveType type, uint32_t vertices )
    {
        switch ( type )
        {
            case GFXPT_POINTLIST:     return vertices;
            case GFXPT_LINELIST:      return vertices / 2;
            case GFXPT_LINESTRIP:     return vertices > 1 ? vertices - 1 : 0;
            case GFXPT_TRIANGLESTRIP: return vertices > 2 ? vertices - 2 : 0;
            default:                  return vertices / 3;
        }
    }
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
    view_matrix_ = MONE;
    world_matrix_ = MONE;
    projection_matrix_ = MONE;
    viewport_matrix_ = MONE;
    api_.struct_size = sizeof( api_ );
    api_valid_ = gfxgpu_get_api( GFXGPU_ABI_VERSION, &api_ ) == GFXGPU_OK && IsApiUsable( api_ );
    if ( !api_valid_ )
        last_error_ = "GfxGpu API version or size mismatch";
}

GraphicsEngineGpu::GraphicsEngineGpu( const GfxGpuApi &api ) : api_( api ), api_valid_( IsApiUsable( api_ ) )
{
    view_matrix_ = MONE;
    world_matrix_ = MONE;
    projection_matrix_ = MONE;
    viewport_matrix_ = MONE;
    if ( !api_valid_ ) last_error_ = "GfxGpu API version or size mismatch";
}

bool GraphicsEngineGpu::fail( const char *message )
{
    last_error_ = message;
    if ( GfxTraceEnabled() ) fprintf( stderr, "BK_GFX_TRACE: %s\n", last_error_.c_str() );
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
    if ( GfxTraceEnabled() ) fprintf( stderr, "BK_GFX_TRACE: %s\n", last_error_.c_str() );
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

// Every pipeline was built as a triangle list, so the selection rectangle, UI
// borders, gun traces and minimap markers -- all line lists -- were rasterised
// as triangles over three times the vertices they own. Tell the renderer what
// the geometry is before each draw.
bool GraphicsEngineGpu::SetTopology( EGFXPrimitiveType type )
{
    return SetState( GFXGPU_STATE_TOPOLOGY, 0, static_cast<uint32_t>( type ), nullptr, 0, "set_topology" );
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
    return renderer_ && api_.draw && Check( api_.draw( renderer_, handle, primitives ), "draw" );
}
bool GraphicsEngineGpu::DrawIndexedBufferHandle( GfxGpuHandle handle, uint32_t index_size, uint32_t count )
{
    return renderer_ && api_.draw_indexed && Check( api_.draw_indexed( renderer_, handle, index_size, 0, count, 0 ), "draw_temporary_indexed" );
}

bool STDCALL GraphicsEngineGpu::Init( const char *pszAdapterName, GFXNativeWindow window )
{
    if ( !api_valid_ ) return false;
    if ( initialized_ ) return true;
    GfxGpuCreateInfo info{};
    info.struct_size = sizeof( info );
    info.sdl_window = window.value;
    sdl_window_ = window.value;
    info.width = width_ > 0 ? static_cast<uint32_t>( width_ ) : GFX_DEFAULT_SCREEN_WIDTH;
    info.height = height_ > 0 ? static_cast<uint32_t>( height_ ) : GFX_DEFAULT_SCREEN_HEIGHT;
    info.shader_directory_utf8 = "Shaders/GfxGpu";
    info.preferred_driver_utf8 = pszAdapterName;
    // The device was always created without the graphics debug layer, so a
    // driver that refuses a pipeline reported E_INVALIDARG and named nothing.
    // BK_GFX_DEBUG=1 turns the layer on, which makes the runtime say which part
    // of the description it rejected.
    if ( getenv( "BK_GFX_DEBUG" ) ) info.flags |= 1;
    if ( !Check( api_.create( &info, &renderer_ ), "create" ) ) return false;
    initialized_ = true;
    adapter_name_ = pszAdapterName ? pszAdapterName : "SDL GPU";
    return true;
}

bool STDCALL GraphicsEngineGpu::Done()
{
    if ( current_font_ ) current_font_->Release();
    current_font_ = nullptr;
    if ( renderer_ ) api_.destroy( renderer_ );
    renderer_ = nullptr;
    sdl_window_ = nullptr;
    initialized_ = false;
    return true;
}

// IGFX::Clear() releases cached scratch data between missions; it is not a
// shutdown. CGraphicsEngine::Clear drops the temporary vertex and index buffers
// and asks the texture and mesh managers to let go of anything unreferenced,
// then restarts the frame counter -- the device keeps running. Calling Done()
// here destroyed the renderer instead, so CMainLoop::ClearResources turned
// leaving a mission into a teardown: every later create_texture failed with
// "create_texture is unavailable", CTextureManagerGpu::GetTexture handed back a
// null texture, and the next CTerrain::LoadLocal dereferenced it.
void STDCALL GraphicsEngineGpu::Clear()
{
    temporary_vertex_bytes_.clear();
    temporary_index_bytes_.clear();
    temporary_vertex_stride_ = 0;
    temporary_vertex_count_ = 0;
    temporary_index_stride_ = 0;
    temporary_index_count_ = 0;
    temporary_vertex_format_ = 0;
    temporary_type_ = GFXPT_TRIANGLELIST;
    if ( ISingleton *globals = GetSingletonGlobal() )
    {
        if ( ITextureManager *textures = GetSingleton<ITextureManager>( globals ) )
            textures->Clear( ISharedManager::CLEAL_UNREFERENCED, 0, 0 );
        if ( IMeshManager *meshes = GetSingleton<IMeshManager>( globals ) )
            meshes->Clear( ISharedManager::CLEAL_UNREFERENCED, 0, 0 );
    }
}

static std::string LowerAscii( const char *pszText )
{
    std::string result = pszText ? pszText : "";
    for ( int i = 0; i < result.size(); ++i )
        result[i] = static_cast<char>( std::tolower( static_cast<unsigned char>( result[i] ) ) );
    return result;
}

// Which display the mode should land on. GFX.Monitor.Name is matched first
// because display indices shuffle as monitors are plugged in and unplugged;
// GFX.Monitor.Index keeps the meaning it already has for the legacy D3D9 path,
// where 0 is the primary display. Pure selection - the move itself happens in
// SetMode, after the window has been sized to fit the target.
static SDL_DisplayID SelectedDisplay()
{
    int nCount = 0;
    SDL_DisplayID *pDisplays = SDL_GetDisplays( &nCount );
    if ( pDisplays == 0 ) return 0;
    int nSelected = -1;
    const std::string szWanted = LowerAscii( GetGlobalVar( "GFX.Monitor.Name", "" ) );
    if ( !szWanted.empty() )
    {
        for ( int i = 0; i < nCount && nSelected < 0; ++i )
            if ( LowerAscii( SDL_GetDisplayName( pDisplays[i] ) ).find( szWanted ) != std::string::npos )
                nSelected = i;
    }
    if ( nSelected < 0 )
    {
        const int nIndex = Max( 0, GetGlobalVar( "GFX.Monitor.Index", 0 ) );
        if ( nIndex < nCount ) nSelected = nIndex;
    }
    const SDL_DisplayID chosen = nSelected >= 0 ? pDisplays[nSelected] : 0;
    if ( chosen != 0 && GfxTraceEnabled() )
        fprintf( stderr, "BK_GFX_TRACE: selected display %d \"%s\"\n", nSelected, SDL_GetDisplayName( chosen ) );
    SDL_free( pDisplays );
    return chosen;
}

bool STDCALL GraphicsEngineGpu::SetMode( int nSizeX, int nSizeY, int nBpp, int, EGFXFullscreen fullscreen, int )
{
    if ( nBpp != 0 && nBpp != 16 && nBpp != 32 ) return fail( "SDL GPU adapter supports 16/32-bit requests on an RGBA8 surface" );
    if ( !sdl_window_ && ( nSizeX <= 0 || nSizeY <= 0 ) ) return fail( "SDL GPU adapter rejects zero-sized display modes" );
    const int nRequestedX = nSizeX, nRequestedY = nSizeY;
    if ( sdl_window_ )
    {
        SDL_Window *window = static_cast<SDL_Window *>( sdl_window_ );
        // The display choice applies to windowed modes too, so picking another
        // monitor in the options moves the game there without a mode change.
        SDL_DisplayID target = SelectedDisplay();
        if ( nSizeX <= 0 || nSizeY <= 0 )
        {
            // An automatic mode adopts the desktop resolution of the display
            // the window is (about to be) on.
            const SDL_DisplayMode *pDesktop = SDL_GetDesktopDisplayMode( target != 0 ? target : SDL_GetDisplayForWindow( window ) );
            if ( pDesktop == 0 ) return fail( SDL_GetError() );
            nSizeX = pDesktop->w;
            nSizeY = pDesktop->h;
        }
        // A window that ends up filling its display's usable area is flagged
        // maximized, and SDL defers every size and position request on a
        // maximized window until it is restored. An automatic mode does
        // exactly that (it asks for the desktop size), so without the restore
        // the next mode change - switching back to a smaller display, or
        // picking a smaller resolution - was deferred forever and silently
        // did nothing.
        if ( SDL_GetWindowFlags( window ) & SDL_WINDOW_MAXIMIZED )
        {
            SDL_RestoreWindow( window );
            SDL_SyncWindow( window );
        }
        bool bDeferFullscreen = false;
        if ( target != 0 && SDL_GetDisplayForWindow( window ) != target )
        {
            if ( fullscreen == GFXFS_FULLSCREEN && ( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN ) )
            {
                // A macOS fullscreen space cannot be carried to another
                // display synchronously - the space toggle only knows the
                // screen the window is on. Start leaving the space now;
                // UpdatePendingFullscreen() moves the window and re-enters
                // fullscreen on the target over the next frames.
                SDL_SetWindowFullscreen( window, false );
                pending_fullscreen_display_ = static_cast<unsigned int>( target );
                pending_fullscreen_frames_ = 0;
                bDeferFullscreen = true;
                if ( GfxTraceEnabled() )
                    fprintf( stderr, "BK_GFX_TRACE: deferring fullscreen move to display %u\n", unsigned( target ) );
            }
            else
            {
                // A centered-display position records the target as the
                // window's pending display, and either the reposition itself
                // (windowed) or the SDL_SetWindowFullscreen below carries the
                // window there - the asynchronous window manager is handled
                // inside SDL (SDL_video.c, pending_displayID). The size is
                // set first so the centering math uses the size the window is
                // about to have, not the one it is leaving.
                const int nCentered = SDL_WINDOWPOS_CENTERED_DISPLAY( target );
                if ( fullscreen != GFXFS_FULLSCREEN )
                    SDL_SetWindowSize( window, nSizeX, nSizeY );
                SDL_SetWindowPosition( window, nCentered, nCentered );
                SDL_SyncWindow( window );
                if ( GfxTraceEnabled() )
                    fprintf( stderr, "BK_GFX_TRACE: moved %dx%d window toward display %u, now on %u (flags=0x%llx)\n",
                        nSizeX, nSizeY, unsigned( target ), unsigned( SDL_GetDisplayForWindow( window ) ),
                        (unsigned long long)SDL_GetWindowFlags( window ) );
            }
        }
        if ( !bDeferFullscreen && !SDL_SetWindowFullscreen( window, fullscreen == GFXFS_FULLSCREEN ) ) return fail( SDL_GetError() );
        // Never size a fullscreen window: its surface is the display, and on
        // macOS the request actually shrinks the Space's backing store, which
        // stretches the picture instead of letterboxing it. The requested
        // resolution lives in the scene texture; the present blit centers it.
        if ( fullscreen != GFXFS_FULLSCREEN && !SDL_SetWindowSize( window, nSizeX, nSizeY ) ) return fail( SDL_GetError() );
        // The app window is deliberately created hidden and stays hidden until a
        // GFX device exists, so SetMode owns making it visible. This is the
        // SWP_SHOWWINDOW that the legacy DirectX ResizeDeviceWindow performed;
        // without it the game runs correctly but renders to a window the
        // compositor never shows.
        SDL_ShowWindow( window );
        // The window manager is free to refuse the requested size: a fullscreen
        // window keeps the display size, and a windowed one is clamped to what
        // fits on screen. Report what we actually got, because the caller reads
        // GetScreenRect() straight back and adopts it as the current mode - if
        // this kept the requested size the engine would render at one resolution
        // into a drawable of another and the picture would not fit the window.
        SDL_SyncWindow( window );
        int pixel_width = 0, pixel_height = 0;
        const bool bReadBack = SDL_GetWindowSizeInPixels( window, &pixel_width, &pixel_height );
        // An explicit resolution is honored as the render size: the scene
        // renders at the requested size and the present blit centers it on
        // whatever surface actually exists - black borders when the surface
        // is larger, cropped when it is smaller, never scaled. In fullscreen
        // the surface is the display; in windowed mode it is the freely
        // resizable window, which behaves like a little monitor of its own.
        // Only automatic modes adopt what the window manager granted.
        const bool bKeepRequested = nRequestedX > 0 && nRequestedY > 0;
        if ( bReadBack && pixel_width > 0 && pixel_height > 0 && !bKeepRequested )
        {
            nSizeX = pixel_width;
            nSizeY = pixel_height;
        }
        // The interface letterboxes itself against whatever GetScreenRect
        // reports, so a mode that keeps the requested size while the window
        // covers a larger display renders the whole picture into a drawable of
        // another shape and the display stretches it back out. Report both
        // sizes: which of them the mode adopted says whether the stretch comes
        // from the layout or from the mode.
        if ( GfxTraceEnabled() )
        {
            int window_width = 0, window_height = 0;
            SDL_GetWindowSize( window, &window_width, &window_height );
            const SDL_DisplayMode *pDesktop = SDL_GetDesktopDisplayMode( SDL_GetDisplayForWindow( window ) );
            fprintf( stderr, "BK_GFX_TRACE: SetMode requested %dx%d fullscreen=%d -> window %dx%d pixels %dx%d (readback=%d) adopted %dx%d desktop %dx%d\n",
                nRequestedX, nRequestedY, fullscreen == GFXFS_FULLSCREEN, window_width, window_height,
                pixel_width, pixel_height, bReadBack ? 1 : 0, nSizeX, nSizeY,
                pDesktop ? pDesktop->w : -1, pDesktop ? pDesktop->h : -1 );
        }
    }
    width_ = nSizeX; height_ = nSizeY;
    UpdateViewportMatrix( 0, 0, nSizeX, nSizeY, 0.0f, 1.0f );
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

void GraphicsEngineGpu::UpdateViewportMatrix( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ )
{
    // NDC -> screen pixels, matching CGraphicsEngine::SetupViewport. The scene
    // multiplies this into its pick transform, so leaving it identity made
    // CScene::Pick test screen-space cursor positions against clip-space
    // coordinates and never hit anything.
    Zero( viewport_matrix_ );
    viewport_matrix_._11 = nWidth / 2.0f;
    viewport_matrix_._14 = nX + nWidth / 2.0f;
    viewport_matrix_._22 = -( nHeight / 2.0f );
    viewport_matrix_._24 = nY + nHeight / 2.0f;
    viewport_matrix_._33 = fMaxZ - fMinZ;
    viewport_matrix_._34 = fMinZ;
    viewport_matrix_._44 = 1.0f;
}
bool STDCALL GraphicsEngineGpu::ChangeViewport( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ )
{
    if ( !renderer_ ) return fail( "ChangeViewport requires Init" );
    UpdateViewportMatrix( nX, nY, nWidth, nHeight, fMinZ, fMaxZ );
    GfxGpuViewportInfo viewport{ sizeof( viewport ), static_cast<float>( nX ), static_cast<float>( nY ), static_cast<float>( nWidth ), static_cast<float>( nHeight ), fMinZ, fMaxZ };
    return Check( api_.set_viewport( renderer_, &viewport ), "set_viewport" );
}
bool STDCALL GraphicsEngineGpu::ChangeViewport( int nWidth, int nHeight ) { return ChangeViewport( 0, 0, nWidth, nHeight, 0.0f, 1.0f ); }
bool STDCALL GraphicsEngineGpu::SetWorldTransforms( const int nStartIndex, const SHMatrix *pMatrices, const int nNumMatrices )
{
    if ( nNumMatrices < 1 || pMatrices == nullptr ) return true;
    if ( nStartIndex != 0 ) return true;
    world_matrix_ = pMatrices[0];
    return !renderer_ || !frame_pending_ || ApplyTransforms();
}
bool GraphicsEngineGpu::ApplyTransforms()
{
    if ( !renderer_ || !api_.set_transform ) return fail( "set_transform is unavailable" );
    GfxGpuMatrixInfo world{ sizeof( world ) }, view_projection{ sizeof( view_projection ) };
    SHMatrix view_projection_matrix;
    Multiply( &view_projection_matrix, projection_matrix_, view_matrix_ );
    std::memcpy( world.values, &world_matrix_, CopySize( sizeof( world.values ), sizeof( world_matrix_ ) ) );
    std::memcpy( view_projection.values, &view_projection_matrix, CopySize( sizeof( view_projection.values ), sizeof( view_projection_matrix ) ) );
    return Check( api_.set_transform( renderer_, &world, &view_projection ), "set_transform" );
}
bool STDCALL GraphicsEngineGpu::SetViewTransform( const SHMatrix &matrix )
{
    if ( direct_transform_ ) return true;
    view_matrix_ = matrix;
    return !renderer_ || !frame_pending_ || ApplyTransforms();
}
bool STDCALL GraphicsEngineGpu::SetProjectionTransform( const SHMatrix &matrix )
{
    projection_matrix_ = matrix;
    return !renderer_ || !frame_pending_ || ApplyTransforms();
}
// CTerrainWater::DrawWater scrolls each river layer by translating u through the
// stage-0 texture matrix once per frame. Dropping it on the floor is what made
// the water stand still.
bool STDCALL GraphicsEngineGpu::SetTextureTransform( int index, const SHMatrix &matrix )
{
    if ( index != 0 ) return true;
    return SetState( GFXGPU_STATE_TEXTURE_MATRIX, 0, 0, &matrix, sizeof( matrix ), "set_texture_matrix" );
}
bool STDCALL GraphicsEngineGpu::SetupDirectTransform()
{
    if ( direct_transform_ ) return true;
    direct_view_stored_ = view_matrix_;
    // SHMatrix is a POD union with no zeroing constructor and only seven of
    // its sixteen elements are assigned below, so the shear and translation
    // terms were stack garbage. Every screen-space draw (war fog, markers,
    // the selection frame) went through this matrix and came out skewed.
    SHMatrix direct;
    Zero( direct );
    direct._11 = 1.0f;
    direct._14 = -static_cast<float>( width_ > 0 ? width_ : 1 ) * 0.5f;
    direct._22 = -1.0f;
    direct._24 = static_cast<float>( height_ > 0 ? height_ : 1 ) * 0.5f;
    direct._33 = projection_matrix_._33 != 0.0f ? 1.0f / projection_matrix_._33 : 1.0f;
    direct._34 = projection_matrix_._33 != 0.0f ? -projection_matrix_._34 / projection_matrix_._33 : 0.0f;
    direct._44 = 1.0f;
    view_matrix_ = direct;
    world_matrix_ = MONE;
    direct_transform_ = true;
    return !renderer_ || !frame_pending_ || ApplyTransforms();
}
bool STDCALL GraphicsEngineGpu::RestoreTransform()
{
    if ( !direct_transform_ ) return true;
    view_matrix_ = direct_view_stored_;
    world_matrix_ = MONE;
    direct_transform_ = false;
    return !renderer_ || !frame_pending_ || ApplyTransforms();
}
// While a direct transform is active the pipeline draws in screen space, but
// callers asking for the view matrix still want the camera's, exactly as
// CGraphicsEngine::GetViewMatrix does. Returning the direct matrix instead made
// viewport * projection * view collapse to the identity, which is correct for
// screen-space vertices and useless to CTerrain::MovePatches: it transforms the
// terrain origin through that product to find where the map starts on screen, so
// it got the world origin back, placed its patches thousands of pixels below the
// window, and AddVertices then clipped away every tile. That is the black ground.
const SHMatrix & STDCALL GraphicsEngineGpu::GetViewMatrix() const { return direct_transform_ ? direct_view_stored_ : view_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetBillboardMatrix() const { return billboard_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetInverseViewMatrix() const { return inverse_view_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetProjectionMatrix() const { return projection_matrix_; }
const SHMatrix & STDCALL GraphicsEngineGpu::GetViewportMatrix() const { return viewport_matrix_; }
// Still unimplemented, and deliberately left that way for now. SPlane's default
// constructor zeroes to VNULL4, so callers that declare SPlane planes[6] as a
// local -- CTerrain::ExtractVisiblePatches does -- get all-zero planes, every
// GetDistanceToPoint returns 0, no clip bit is ever set and nothing is culled.
// Leaving it empty is therefore "draw everything", which is wrong but safe. A
// port of CGraphicsEngine::GetViewVolume culled the entire scene, so its plane
// orientation does not carry over unexamined; it needs the clip convention
// checked against this renderer's view_proj before it can be turned on.
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
    if ( !renderer_ ) return fail( "set_texture is unavailable" );
    // Each stage goes to its own slot. Both used to be funnelled into a single
    // binding, so the terrain's noise or crosset on stage 1 overwrote the tileset
    // on stage 0, and the SetTexture( 1, 0 ) that disables noise cleared the
    // tileset outright.
    TextureGpu *gpu_texture = 0;
    if ( texture )
    {
        gpu_texture = dynamic_cast<TextureGpu *>( texture );
        if ( !gpu_texture ) return fail( "texture does not belong to the SDL GPU adapter" );
    }
    const GfxGpuHandle handle = gpu_texture ? gpu_texture->Handle() : 0;
    if ( api_.set_texture_stage )
    {
        const bool result = Check( api_.set_texture_stage( renderer_, static_cast<uint32_t>( stage ), handle ), "set_texture_stage" );
        if ( result && stage == 0 && api_.set_sampler ) (void)api_.set_sampler( renderer_, 1 );
        return result;
    }
    // Renderers predating the stage-aware entry point only have stage 0.
    if ( stage != 0 ) return true;
    if ( !api_.set_texture ) return fail( "set_texture is unavailable" );
    const bool result = Check( api_.set_texture( renderer_, handle ), handle ? "set_texture" : "clear_texture" );
    if ( result && api_.set_sampler ) (void)api_.set_sampler( renderer_, 1 );
    return result;
}
bool STDCALL GraphicsEngineGpu::SetWireframe( bool enable ) { return SetState( GFXGPU_STATE_WIREFRAME, 0, enable ? 1u : 0u, nullptr, 0, "set_wireframe" ); }
bool STDCALL GraphicsEngineGpu::SetCullMode( EGFXCull cull ) { return SetState( GFXGPU_STATE_CULL_MODE, 0, static_cast<uint32_t>( cull ), nullptr, 0, "set_cull_mode" ); }
bool STDCALL GraphicsEngineGpu::SetDepthBufferMode( EGFXDepthBuffer depth, EGFXCmpFunction cmp ) { return SetState( GFXGPU_STATE_DEPTH_MODE, static_cast<uint32_t>( cmp ), static_cast<uint32_t>( depth ), nullptr, 0, "set_depth_mode" ); }
bool STDCALL GraphicsEngineGpu::EnableLighting( bool enable ) { return SetState( GFXGPU_STATE_LIGHTING, 0, enable ? 1u : 0u, nullptr, 0, "set_lighting" ); }
bool STDCALL GraphicsEngineGpu::EnableSpecular( bool enable ) { return SetState( GFXGPU_STATE_SPECULAR, 0, enable ? 1u : 0u, nullptr, 0, "set_specular" ); }
bool STDCALL GraphicsEngineGpu::SetFont( IGFXFont *font )
{
    if ( font ) font->AddRef();
    if ( current_font_ ) current_font_->Release();
    current_font_ = font;
    return font != nullptr;
}
bool STDCALL GraphicsEngineGpu::IsActive() { return initialized_; }
bool STDCALL GraphicsEngineGpu::BeginScene()
{
    // CGraphicsEngine::BeginScene clears these every frame and callers read them
    // as per-frame totals. Letting them run for the whole session made
    // CInterfaceScreenBase::AddStatistics add a session total into an int once a
    // frame, which overflowed after a few minutes of play and trapped.
    passed_vertices_ = 0;
    passed_primitives_ = 0;
    const bool result = renderer_ && Check( api_.begin_frame( renderer_ ), "begin_frame" );
    frame_pending_ = result;
    if ( result ) (void)ApplyTransforms();
    return result;
}
bool STDCALL GraphicsEngineGpu::EndScene()
{
    const bool result = renderer_ && Check( api_.end_frame( renderer_ ), "end_frame" );
    if ( result ) frame_pending_ = false;
    return result;
}
bool STDCALL GraphicsEngineGpu::IsSafeToPresent() const { return initialized_; }

bool STDCALL GraphicsEngineGpu::Clear( int, RECT *, DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil )
{
    if ( !renderer_ ) return fail( "Clear requires Init" );
    GfxGpuClearInfo clear{ sizeof( clear ), static_cast<uint32_t>( dwFlags ), dwColor, fDepth, dwStencil };
    return Check( api_.clear( renderer_, &clear ), "clear" );
}
// A fullscreen space cannot be carried to another display in one synchronous
// call: leaving the old space, moving the windowed frame and entering the new
// space are three asynchronous window-manager transitions. SetMode starts the
// exit and stashes the target; this ticks the remaining phases once per frame.
void GraphicsEngineGpu::UpdatePendingFullscreen()
{
    if ( pending_fullscreen_display_ == 0 || sdl_window_ == nullptr ) return;
    SDL_Window *window = static_cast<SDL_Window *>( sdl_window_ );
    const SDL_DisplayID target = static_cast<SDL_DisplayID>( pending_fullscreen_display_ );
    ++pending_fullscreen_frames_;
    const bool bFullscreenNow = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN ) != 0;
    if ( bFullscreenNow )
    {
        // Either still leaving the old space, or something else (the
        // display-changed reaction) already re-entered on the target.
        if ( SDL_GetDisplayForWindow( window ) == target || pending_fullscreen_frames_ > 600 )
            pending_fullscreen_display_ = 0;
        return;
    }
    if ( SDL_GetDisplayForWindow( window ) != target && pending_fullscreen_frames_ <= 600 )
    {
        // Re-issue the move occasionally; the window manager drops requests
        // that arrive while its animations are still running.
        if ( ( pending_fullscreen_frames_ % 15 ) == 1 )
        {
            const int nCentered = SDL_WINDOWPOS_CENTERED_DISPLAY( target );
            SDL_SetWindowPosition( window, nCentered, nCentered );
        }
        return;
    }
    SDL_SetWindowFullscreen( window, true );
    pending_fullscreen_display_ = 0;
    if ( GfxTraceEnabled() )
        fprintf( stderr, "BK_GFX_TRACE: pending fullscreen re-entered on display %u after %d frames\n",
            unsigned( SDL_GetDisplayForWindow( window ) ), pending_fullscreen_frames_ );
}

// The scene is presented on the window either centered 1:1 (gameplay:
// borders/crop) or aspect-fit scaled (menus and videos, GFX.Present.Fit -
// their controls must never be clipped away). Mouse events arrive in window
// coordinates; these globals carry the transform the input pump applies to
// land in game coordinates: game = (window - Offset) * Scale. Re-derived
// every frame because the window size changes asynchronously (deferred
// fullscreen moves, OS drags, live window resizing).
void GraphicsEngineGpu::UpdatePresentOffsets()
{
    const bool bFit = GetGlobalVar( "GFX.Present.Fit", 1 ) != 0;
    if ( bFit != present_fit_ )
    {
        present_fit_ = bFit;
        if ( renderer_ && api_.set_present_fit )
            api_.set_present_fit( renderer_, bFit ? 1 : 0 );
    }
    float fOffsetX = 0.0f, fOffsetY = 0.0f, fScaleX = 1.0f, fScaleY = 1.0f;
    int pixel_width = 0, pixel_height = 0;
    if ( sdl_window_ != nullptr && width_ > 0 && height_ > 0 &&
         SDL_GetWindowSizeInPixels( static_cast<SDL_Window *>( sdl_window_ ), &pixel_width, &pixel_height ) &&
         pixel_width > 0 && pixel_height > 0 )
    {
        if ( bFit )
        {
            const double fScale = Min( double( pixel_width ) / width_, double( pixel_height ) / height_ );
            const double fFitW = width_ * fScale, fFitH = height_ * fScale;
            fOffsetX = float( ( pixel_width - fFitW ) / 2 );
            fOffsetY = float( ( pixel_height - fFitH ) / 2 );
            fScaleX = float( width_ / fFitW );
            fScaleY = float( height_ / fFitH );
        }
        else
        {
            // 1:1 - the offset is signed: positive margins when the scene is
            // smaller (borders), negative when it is larger (center crop).
            fOffsetX = float( ( pixel_width - width_ ) / 2 );
            fOffsetY = float( ( pixel_height - height_ ) / 2 );
        }
    }
    if ( fOffsetX != present_offset_x_ || fOffsetY != present_offset_y_ ||
         fScaleX != present_scale_x_ || fScaleY != present_scale_y_ )
    {
        present_offset_x_ = fOffsetX;
        present_offset_y_ = fOffsetY;
        present_scale_x_ = fScaleX;
        present_scale_y_ = fScaleY;
        SetGlobalVar( "GFX.Present.OffsetX", fOffsetX );
        SetGlobalVar( "GFX.Present.OffsetY", fOffsetY );
        SetGlobalVar( "GFX.Present.ScaleX", fScaleX );
        SetGlobalVar( "GFX.Present.ScaleY", fScaleY );
        if ( GfxTraceEnabled() )
            fprintf( stderr, "BK_GFX_TRACE: present fit=%d scene %dx%d window %dx%d offset %.1f,%.1f scale %.4f,%.4f\n",
                bFit ? 1 : 0, width_, height_, pixel_width, pixel_height, fOffsetX, fOffsetY, fScaleX, fScaleY );
    }
}

bool STDCALL GraphicsEngineGpu::Flip()
{
    UpdatePendingFullscreen();
    UpdatePresentOffsets();
    const bool result = renderer_ && Check( api_.present( renderer_ ), "present" );
    if ( result ) frame_pending_ = false;
    return result;
}
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
    const int stride = FvfStride( format );
    if ( stride <= 0 ) { fail( "unsupported vertex format" ); return nullptr; }
    VerticesGpu *buffer = new VerticesGpu( this, elements, format, stride, usage, type );
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
void * STDCALL GraphicsEngineGpu::GetTempVertices( int elements, DWORD format, EGFXPrimitiveType type )
{
    if ( !temporary_vertex_bytes_.empty() ) { fail( "GetTempVertices: previous temporary buffer not drawn" ); return nullptr; }
    // The caller memcpy's sizeof(TVertex) bytes per element into this buffer, so
    // the stride has to be the vertex's true size. Forcing 32 both overflowed
    // the allocation for larger formats and made the GPU stride past the wrong
    // number of bytes: STerrainTLVertex is 36, which is why terrain went black.
    const int stride = FvfStride( format );
    if ( stride <= 0 || elements <= 0 )
    {
        // Returning null here makes the caller memcpy into a null pointer, so
        // report the format rather than letting it crash silently.
        if ( GfxTraceEnabled() ) fprintf( stderr, "BK_GFX_TRACE: GetTempVertices unsupported fvf=0x%lx elements=%d stride=%d\n", static_cast<unsigned long>( format ), elements, stride );
        return nullptr;
    }
    temporary_vertex_stride_ = stride;
    temporary_vertex_format_ = format;
    temporary_vertex_count_ = elements; temporary_type_ = type;
    try { temporary_vertex_bytes_.assign( static_cast<size_t>( elements ) * temporary_vertex_stride_, 0 ); } catch ( ... ) { return nullptr; }
    return temporary_vertex_bytes_.data();
}
void * STDCALL GraphicsEngineGpu::GetTempIndices( int elements, DWORD format, EGFXPrimitiveType type )
{
    if ( !temporary_index_bytes_.empty() ) return nullptr;
    temporary_index_stride_ = format == GFXIF_INDEX32 ? 4 : 2; temporary_index_count_ = elements; temporary_type_ = type;
    try { temporary_index_bytes_.assign( static_cast<size_t>( elements ) * temporary_index_stride_, 0 ); } catch ( ... ) { return nullptr; }
    return temporary_index_bytes_.data();
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
    if ( !SetTopology( vb->Type() ) ) return false;
    if ( indices )
    {
        IndicesGpu *ib = dynamic_cast<IndicesGpu *>( indices );
        if ( !ib || !api_.draw_indexed || !api_.bind_vertex_buffer ) return fail( "index buffer does not belong to the SDL GPU adapter" );
        if ( !Check( api_.bind_vertex_buffer( renderer_, vb->Handle() ), "bind_vertex_buffer" ) ) return false;
        const bool result = Check( api_.draw_indexed( renderer_, ib->Handle(), ib->Stride(), 0, ib->Count(), 0 ), "draw_indexed" );
        if ( result ) { passed_vertices_ += static_cast<int>( ib->Count() ); passed_primitives_ += static_cast<int>( ib->Count() / 3 ); }
        return result;
    }
    if ( !api_.draw ) return fail( "draw is unavailable" );
    const uint32_t primitives = PrimitiveCount( vb->Type(), vb->Count() );
    if ( primitives == 0 ) return fail( "geometry has no drawable primitives" );
    const bool result = Check( api_.draw( renderer_, 0, primitives ), "draw" );
    if ( result ) { passed_vertices_ += static_cast<int>( vb->Count() ); passed_primitives_ += static_cast<int>( primitives ); }
    return result;
}
bool STDCALL GraphicsEngineGpu::DrawTemp()
{
    if ( temporary_vertex_bytes_.empty() ) return false;
    if ( !SetTopology( temporary_type_ ) ) return false;
    GfxGpuHandle vertex_handle = 0;
    GfxGpuHandle index_handle = 0;
    // No repacking: the buffer was allocated at the format's own stride, so the
    // bytes the caller wrote are already the layout the pipeline expects.
    const unsigned char *vertex_data = temporary_vertex_bytes_.data();
    const size_t vertex_bytes = temporary_vertex_bytes_.size();
    const bool vertex_created = CreateBufferHandle( static_cast<uint32_t>( temporary_vertex_count_ ), static_cast<uint32_t>( temporary_vertex_format_ ), static_cast<uint32_t>( temporary_vertex_stride_ ), GFXD_DYNAMIC, &vertex_handle ) &&
        UploadBuffer( vertex_handle, vertex_data, vertex_bytes );
    const bool indexed = !temporary_index_bytes_.empty();
    const bool index_created = !indexed || ( CreateBufferHandle( static_cast<uint32_t>( temporary_index_count_ ), 0, static_cast<uint32_t>( temporary_index_stride_ ), GFXD_DYNAMIC, &index_handle ) &&
        UploadBuffer( index_handle, temporary_index_bytes_.data(), temporary_index_bytes_.size() ) );
    bool drawn = false;
    const bool vertex_bound = !indexed || ( vertex_created && api_.bind_vertex_buffer && Check( api_.bind_vertex_buffer( renderer_, vertex_handle ), "bind_temporary_vertex_buffer" ) );
    if ( vertex_created && index_created && vertex_bound ) {
        if ( indexed ) drawn = DrawIndexedBufferHandle( index_handle, static_cast<uint32_t>( temporary_index_stride_ ), static_cast<uint32_t>( temporary_index_count_ ) );
        else drawn = DrawBufferHandle( vertex_handle, PrimitiveCount( temporary_type_, static_cast<uint32_t>( temporary_vertex_count_ ) ) );
        if ( drawn ) { passed_vertices_ += temporary_vertex_count_; passed_primitives_ += static_cast<int>( PrimitiveCount( temporary_type_, static_cast<uint32_t>( indexed ? temporary_index_count_ : temporary_vertex_count_ ) ) ); }
    }
    if ( index_handle ) DestroyBufferHandle( index_handle );
    if ( vertex_handle ) DestroyBufferHandle( vertex_handle );
    temporary_vertex_bytes_.clear();
    temporary_index_bytes_.clear();
    return vertex_created && index_created && drawn;
}
bool STDCALL GraphicsEngineGpu::DrawMesh( IGFXMesh *mesh, const SHMatrix *matrices, int matrix_count )
{
    if ( !mesh || !matrices || matrix_count <= 0 ) return fail( "DrawMesh requires a mesh and at least one matrix" );
    MeshGpu *gpu_mesh = dynamic_cast<MeshGpu *>( mesh );
    if ( !gpu_mesh ) return fail( "mesh does not belong to the SDL GPU adapter" );
    for ( const MeshGpu::Part &part : gpu_mesh->Parts() )
    {
        if ( part.matrix_index < 0 || part.matrix_index >= matrix_count || !part.vertices || !part.indices ) return fail( "mesh part is invalid" );
        if ( !SetWorldTransforms( 0, matrices + part.matrix_index, 1 ) ) return false;
        if ( !Draw( part.vertices, part.indices ) ) return false;
    }
    return true;
}
// SetFont used to fail outright, so DrawString and DrawStringA had no font to
// draw with and fell back to one filled rectangle per character. Everything that
// goes through them rendered as coloured blocks: the statistics overlay in the
// top left, the console, and the chat lines.
bool GraphicsEngineGpu::DrawFontLine( const wchar_t *text, int x, int y, DWORD color )
{
    FontGpu *font = dynamic_cast<FontGpu *>( current_font_ );
    if ( getenv( "BK_TEXT_TRACE" ) && text )
    {
        // Reports both halves of the question at once: how long the string
        // actually is, and whether the font knows each character. A glyph the
        // font is missing comes back zero-width, so the pen does not advance
        // and the survivors bunch up at the left edge -- which looks the same
        // as a truncated string until you can see the widths.
        std::size_t length = 0;
        while ( text[length] ) ++length;
        std::fprintf( stderr, "BK_TEXT_TRACE: font=%s len=%zu chars=",
            font ? font->GetSharedResourceName() : "<none>", length );
        for ( const wchar_t *it = text; *it && it - text < 32; ++it )
        {
            const unsigned code = static_cast<unsigned>( *it );
            if ( font )
            {
                const SFontFormat &format = font->GetFormat();
                const bool known = format.chars.find( static_cast<WORD>( code ) ) != format.chars.end();
                const SFontFormat::SCharDesc &desc = format.GetChar( static_cast<WORD>( code ) );
                std::fprintf( stderr, "%u%s(w=%.1f) ", code, known ? "" : "!MISSING", desc.fA + desc.fB + desc.fC );
            }
            else
                std::fprintf( stderr, "%u ", code );
        }
        if ( font ) std::fprintf( stderr, "| font has %d chars", static_cast<int>( font->GetFormat().chars.size() ) );
        std::fprintf( stderr, "\n" );
    }
    if ( !font || !font->Texture() || !text || !*text ) return false;
    std::vector<SGFXLVertex> vertices;
    std::vector<WORD> indices;
    font->AppendGeometry( text, static_cast<float>( x ), static_cast<float>( y ), 1.0f, color, vertices, indices );
    if ( vertices.empty() || indices.empty() ) return true;
    if ( !SetTexture( 0, font->Texture() ) ) return false;
    if ( api_.set_sampler ) (void)api_.set_sampler( renderer_, 2 );
    SGFXLVertex *destination = static_cast<SGFXLVertex *>( GetTempVertices( static_cast<int>( vertices.size() ), SGFXLVertex::format, GFXPT_TRIANGLELIST ) );
    if ( !destination ) return false;
    std::memcpy( destination, vertices.data(), vertices.size() * sizeof( SGFXLVertex ) );
    WORD *index_destination = static_cast<WORD *>( GetTempIndices( static_cast<int>( indices.size() ), GFXIF_INDEX16, GFXPT_TRIANGLELIST ) );
    if ( !index_destination ) return false;
    std::memcpy( index_destination, indices.data(), indices.size() * sizeof( WORD ) );
    return DrawTemp();
}
bool STDCALL GraphicsEngineGpu::DrawStringA( const char *text, int x, int y, DWORD color )
{
    if ( !text || !*text ) return true;
    std::wstring wide;
    while ( *text ) wide.push_back( static_cast<unsigned char>( *text++ ) );
    if ( DrawFontLine( wide.c_str(), x, y, color ) ) return true;
    return DrawFallbackGlyphs( this, wide.c_str(), x, y, color );
}
bool STDCALL GraphicsEngineGpu::DrawString( const wchar_t *text, int x, int y, DWORD color )
{
    if ( DrawFontLine( text, x, y, color ) ) return true;
    return DrawFallbackGlyphs( this, text, x, y, color );
}
bool STDCALL GraphicsEngineGpu::DrawText( IGFXText *text, const RECT &rect, int y, DWORD flags )
{
    if ( !text ) return false;
    IText *source = text->GetText();
    if ( !source ) return true;
    // GetString() is UTF-16. Casting it to wchar_t made AppendGeometry walk
    // two UTF-16 units per character on a 32-bit wchar_t, drawing every second
    // character. Keep the raw pointer for measuring and widen a copy to draw.
    const WORD *raw_value = source->GetString();
    const std::wstring wide_value = NPlatform::WideFromWordString( raw_value );
    const wchar_t *value = wide_value.c_str();
    IGFXTextGpuFontProvider *font_provider = dynamic_cast<IGFXTextGpuFontProvider *>( text );
    FontGpu *font = font_provider ? dynamic_cast<FontGpu *>( font_provider->Font() ) : nullptr;
    const int width = text->GetWidth();
    const float scale = font_provider ? font_provider->Scale() : 1.0f;
    const float wrap_width = static_cast<float>( rect.right - rect.left );
    float x = static_cast<float>( rect.left );
    if ( !font )
    {
        if ( (flags & FNT_FORMAT_CENTER) != 0 ) x = static_cast<float>( rect.left + (rect.right - rect.left - width) / 2 );
        else if ( (flags & FNT_FORMAT_RIGHT) != 0 ) x = static_cast<float>( rect.right - width );
    }
    if ( font )
    {
        // Shared with TextGpu::GetNumLines so the row height the list reserves
        // always matches the lines actually drawn here.
        std::vector<std::pair<size_t, size_t> > lines;
        WrapTextLines( font, raw_value, scale, wrap_width, lines );

        std::vector<SGFXLVertex> vertices;
        std::vector<WORD> indices;
        const float line_step = static_cast<float>( font->GetLineSpace() ) * scale;
        float pen_y = static_cast<float>( rect.top + y );
        // The D3D path clips every glyph to rect. Without this a scrolled or
        // oversized text box drew all of its lines and spilled over whatever
        // was below it.
        const float clip_top = static_cast<float>( rect.top );
        const float clip_bottom = static_cast<float>( rect.bottom );
        for ( size_t line_index = 0; line_index != lines.size(); ++line_index )
        {
            const size_t begin = lines[line_index].first;
            const size_t end = lines[line_index].second;
            if ( pen_y >= clip_bottom ) break;
            if ( end > begin && pen_y + line_step > clip_top )
            {
                const std::wstring line_text = wide_value.substr( begin, end - begin );
                const float line_width = font->TextWidthFloat( raw_value + begin, static_cast<int>( end - begin ) ) * scale;
                float line_x = static_cast<float>( rect.left );
                if ( (flags & FNT_FORMAT_CENTER) != 0 ) line_x += std::floor( ( wrap_width - line_width ) * 0.5f );
                else if ( (flags & FNT_FORMAT_RIGHT) != 0 ) line_x = static_cast<float>( rect.right ) - line_width;
                font->AppendGeometry( line_text.c_str(), line_x, pen_y, scale, font_provider->Color(), vertices, indices, clip_top, clip_bottom );
                if ( line_index == 0 ) x = line_x;
            }
            pen_y += line_step;
        }
        // An empty list means every line fell outside the clip rect - a
        // successful draw of nothing. Falling through to the block-glyph
        // fallback painted the clipped-away rows of a scrolled options list as
        // solid bars across whatever sat below the list (the OK/Cancel
        // buttons). The fallback is only for text that has no font at all.
        if ( vertices.empty() ) return true;
        if ( SetTexture( 0, font->Texture() ) )
        {
            if ( api_.set_sampler ) (void)api_.set_sampler( renderer_, 2 );
            SGFXLVertex *destination = static_cast<SGFXLVertex *>( GetTempVertices( static_cast<int>( vertices.size() ), SGFXLVertex::format, GFXPT_TRIANGLELIST ) );
            if ( destination )
            {
                std::memcpy( destination, vertices.data(), vertices.size() * sizeof( SGFXLVertex ) );
                WORD *index_destination = static_cast<WORD *>( GetTempIndices( static_cast<int>( indices.size() ), GFXIF_INDEX16, GFXPT_TRIANGLELIST ) );
                if ( index_destination )
                {
                    std::memcpy( index_destination, indices.data(), indices.size() * sizeof( WORD ) );
                    return DrawTemp();
                }
            }
        }
        return true;
    }
    return DrawFallbackGlyphs( this, value, static_cast<int>( x ), rect.top + y, font_provider ? font_provider->Color() : 0xffffffff, rect.right );
}
bool STDCALL GraphicsEngineGpu::DrawRects( const SGFXRect2 *rects, int count, bool solid )
{
    if ( !rects || count <= 0 ) return false;
    const int vertices_per_rect = solid ? 6 : 8;
    SGFXLVertex *vertices = static_cast<SGFXLVertex *>( GetTempVertices( count * vertices_per_rect, SGFXLVertex::format, solid ? GFXPT_TRIANGLELIST : GFXPT_LINELIST ) );
    if ( !vertices ) return false;
    DWORD specular_bits = 0;
    for ( int i = 0; i < count; ++i )
    {
        const SGFXRect2 &rect = rects[i];
        SGFXLVertex corners[4];
        const float minx = rect.rect.minx + 0.5f;
        const float miny = rect.rect.miny + 0.5f;
        const float maxx = rect.rect.maxx + 0.5f;
        const float maxy = rect.rect.maxy + 0.5f;
        corners[0].Setup( minx, maxy, rect.fZ, rect.color, rect.specular, rect.maps.minx, rect.maps.maxy );
        corners[1].Setup( minx, miny, rect.fZ, rect.color, rect.specular, rect.maps.minx, rect.maps.miny );
        corners[2].Setup( maxx, maxy, rect.fZ, rect.color, rect.specular, rect.maps.maxx, rect.maps.maxy );
        corners[3].Setup( maxx, miny, rect.fZ, rect.color, rect.specular, rect.maps.maxx, rect.maps.miny );
        if ( solid ) { *vertices++ = corners[2]; *vertices++ = corners[1]; *vertices++ = corners[0]; *vertices++ = corners[1]; *vertices++ = corners[2]; *vertices++ = corners[3]; }
        else { *vertices++ = corners[0]; *vertices++ = corners[1]; *vertices++ = corners[1]; *vertices++ = corners[3]; *vertices++ = corners[3]; *vertices++ = corners[2]; *vertices++ = corners[2]; *vertices++ = corners[0]; }
        specular_bits |= rect.specular;
    }
    // CGraphicsEngine::DrawRects turns D3DRS_SPECULARENABLE on for a batch whose
    // rects carry a non-black specular and off again afterwards. That is how a
    // blinking UI element flashes: CSimpleWindow::Draw puts its blink colour in
    // the specular, leaving 0xff000000 -- black, and so a no-op -- when it is not
    // blinking.
    const bool has_specular = ( specular_bits & 0x00ffffff ) != 0;
    if ( has_specular ) EnableSpecular( true );
    const bool result = DrawTemp();
    if ( has_specular ) EnableSpecular( false );
    return result;
}
bool STDCALL GraphicsEngineGpu::SetGammaRamp( const SGFXGammaRamp &, bool ) { return false; }
bool STDCALL GraphicsEngineGpu::GetGammaRamp( const SGFXGammaRamp * ) { return false; }
void STDCALL GraphicsEngineGpu::SetGammaCorrectionValues( const float b, const float c, const float g ) { brightness_ = b; contrast_ = c; gamma_ = g; }
void STDCALL GraphicsEngineGpu::GetGammaCorrectionValues( float *b, float *c, float *g ) { if ( b ) *b = brightness_; if ( c ) *c = contrast_; if ( g ) *g = gamma_; }
bool STDCALL GraphicsEngineGpu::TakeScreenShot( IImage *image )
{
    if ( !image || !renderer_ ) return fail( "SDL GPU screenshot readback is unavailable" );
    // Readback requires the queued frame to be submitted. The reference path
    // captures after EndScene and before the normal Flip, so present only when
    // this adaptor still owns a pending frame.
    if ( frame_pending_ ) {
        if ( !Check( api_.present( renderer_ ), "screenshot present" ) ) return false;
        frame_pending_ = false;
    }
    const uint32_t width = static_cast<uint32_t>( image->GetSizeX() );
    const uint32_t height = static_cast<uint32_t>( image->GetSizeY() );
    if ( width == 0 || height == 0 ) return fail( "screenshot image has invalid dimensions" );
    std::vector<unsigned char> pixels;
    try { pixels.resize( static_cast<size_t>( width ) * height * sizeof( SColor ) ); } catch ( ... ) { return fail( "screenshot allocation failed" ); }
    GfxGpuReadbackInfo info{ sizeof( info ), width, height, static_cast<uint32_t>( pixels.size() ), static_cast<uint32_t>( width * sizeof( SColor ) ), pixels.data() };
    if ( !Check( gfxgpu_readback( renderer_, &info ), "readback" ) ) return false;
    if ( info.row_pitch < width * sizeof( SColor ) || info.byte_length < info.row_pitch * height ) return fail( "screenshot readback returned an invalid layout" );
    SColor *destination = image->GetLFB();
    for ( uint32_t row = 0; row < height; ++row ) std::memcpy( destination + static_cast<size_t>( row ) * width, pixels.data() + static_cast<size_t>( row ) * info.row_pitch, width * sizeof( SColor ) );
    return true;
}
int STDCALL GraphicsEngineGpu::GetNumPassedVertices() const { return passed_vertices_; }
int STDCALL GraphicsEngineGpu::GetNumPassedPrimitives() const { return passed_primitives_; }
bool STDCALL GraphicsEngineGpu::SetShadingEffect( int effect ) { return SetState( GFXGPU_STATE_SHADE_EFFECT, 0, static_cast<uint32_t>( effect ), nullptr, 0, "set_shade_effect" ); }
