#include "StdAfx.h"
#include "GFX.H"
#include "../../Sources/src/GFXGPU/GraphicsEngineGpu.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace
{
    char trace[256]{};
    size_t trace_length = 0;
    int texture_creates = 0, texture_uploads = 0, texture_releases = 0, target_creates = 0, target_binds = 0, buffer_creates = 0, buffer_uploads = 0, buffer_releases = 0;
    void record( char value ) { trace[trace_length++] = value; }
    GfxGpuResult fakeCreate( const GfxGpuCreateInfo *, GfxGpuRenderer **out ) { record( 'C' ); *out = reinterpret_cast<GfxGpuRenderer *>( 1 ); return GFXGPU_OK; }
    void fakeDestroy( GfxGpuRenderer * ) { record( 'D' ); }
    GfxGpuResult fakeBegin( GfxGpuRenderer * ) { record( 'B' ); return GFXGPU_OK; }
    GfxGpuResult fakeEnd( GfxGpuRenderer * ) { record( 'E' ); return GFXGPU_OK; }
    GfxGpuResult fakePresent( GfxGpuRenderer * ) { record( 'P' ); return GFXGPU_OK; }
    void fakeCancel( GfxGpuRenderer * ) { record( 'X' ); }
    GfxGpuResult fakeClear( GfxGpuRenderer *, const GfxGpuClearInfo *info ) { if ( !info || info->color_rgba8 != 0x12345678u ) return GFXGPU_INVALID_ARGUMENT; record( 'L' ); return GFXGPU_OK; }
    GfxGpuResult fakeResize( GfxGpuRenderer *, uint32_t width, uint32_t height ) { if ( width != 800 || height != 600 ) return GFXGPU_INVALID_ARGUMENT; record( 'R' ); return GFXGPU_OK; }
    GfxGpuResult fakeViewport( GfxGpuRenderer *, const GfxGpuViewportInfo *info ) { if ( !info || info->width != 800.0f ) return GFXGPU_INVALID_ARGUMENT; record( 'V' ); return GFXGPU_OK; }
    GfxGpuResult fakeTransform( GfxGpuRenderer *, const GfxGpuMatrixInfo *, const GfxGpuMatrixInfo * ) { record( 'T' ); return GFXGPU_OK; }
    GfxGpuResult fakeState( GfxGpuRenderer *, const GfxGpuStateInfo *info ) { if ( !info || info->kind != GFXGPU_STATE_WIREFRAME || info->value != 1 ) return GFXGPU_INVALID_ARGUMENT; record( 'S' ); return GFXGPU_OK; }
    GfxGpuResult fakeSetTexture( GfxGpuRenderer *, GfxGpuHandle handle ) { return handle == 42 ? GFXGPU_OK : GFXGPU_INVALID_HANDLE; }
    GfxGpuResult fakeCreateTexture( GfxGpuRenderer *, const GfxGpuTextureCreateInfo *info, GfxGpuHandle *out ) { if ( !info || !out || info->width != 2 || info->height != 2 ) return GFXGPU_INVALID_ARGUMENT; ++texture_creates; *out = 42; return GFXGPU_OK; }
    GfxGpuResult fakeUploadTexture( GfxGpuRenderer *, GfxGpuHandle handle, const GfxGpuTextureUploadInfo *info ) { if ( handle != 42 || !info || info->byte_length != 16 || info->row_pitch != 8 ) return GFXGPU_INVALID_ARGUMENT; ++texture_uploads; return GFXGPU_OK; }
    GfxGpuResult fakeDestroyTexture( GfxGpuRenderer *, GfxGpuHandle handle ) { if ( handle != 42 ) return GFXGPU_INVALID_ARGUMENT; ++texture_releases; return GFXGPU_OK; }
    GfxGpuResult fakeCreateTarget( GfxGpuRenderer *, const GfxGpuRenderTargetCreateInfo *, GfxGpuHandle *out ) { ++target_creates; *out = 43; return GFXGPU_OK; }
    GfxGpuResult fakeBindTarget( GfxGpuRenderer *, GfxGpuHandle handle ) { if ( handle != 0 && handle != 43 ) return GFXGPU_INVALID_HANDLE; ++target_binds; return GFXGPU_OK; }
    GfxGpuResult fakeCreateBuffer( GfxGpuRenderer *, const GfxGpuBufferCreateInfo *, GfxGpuHandle *out ) { ++buffer_creates; *out = 44 + buffer_creates; return GFXGPU_OK; }
    GfxGpuResult fakeUploadBuffer( GfxGpuRenderer *, GfxGpuHandle, const GfxGpuBufferUploadInfo *info ) { if ( !info || info->byte_length == 0 ) return GFXGPU_INVALID_ARGUMENT; ++buffer_uploads; return GFXGPU_OK; }
    GfxGpuResult fakeDestroyBuffer( GfxGpuRenderer *, GfxGpuHandle ) { ++buffer_releases; return GFXGPU_OK; }
    GfxGpuResult fakeDraw( GfxGpuRenderer *, uint32_t, uint32_t count ) { return count ? GFXGPU_OK : GFXGPU_INVALID_ARGUMENT; }
    GfxGpuResult fakeDrawIndexed( GfxGpuRenderer *, GfxGpuHandle, uint32_t, uint32_t, uint32_t count, int32_t ) { return count ? GFXGPU_OK : GFXGPU_INVALID_ARGUMENT; }
}

static int RunRecordingTest()
{
    GfxGpuApi api{};
    api.abi_version = GFXGPU_ABI_VERSION;
    api.struct_size = sizeof( api );
    api.create = fakeCreate; api.destroy = fakeDestroy;
    api.begin_frame = fakeBegin; api.end_frame = fakeEnd; api.present = fakePresent;
    api.cancel_frame = fakeCancel; api.clear = fakeClear; api.resize = fakeResize;
    api.set_viewport = fakeViewport; api.set_transform = fakeTransform; api.set_state = fakeState;
    api.set_texture = fakeSetTexture;
    api.create_texture = fakeCreateTexture; api.upload_texture = fakeUploadTexture; api.destroy_texture = fakeDestroyTexture;
    api.create_render_target = fakeCreateTarget; api.bind_render_target = fakeBindTarget;
    api.create_buffer = fakeCreateBuffer; api.upload_buffer = fakeUploadBuffer; api.destroy_buffer = fakeDestroyBuffer;
    api.draw = fakeDraw; api.draw_indexed = fakeDrawIndexed;
    GraphicsEngineGpu adapter( api );
    if ( !adapter.Init( nullptr, GFXNativeWindow( nullptr ) ) ) return 10;
    if ( !adapter.SetMode( 800, 600, 32, 0, GFXFS_WINDOWED ) ) return 11;
    if ( !adapter.ChangeViewport( 800, 600 ) ) return 12;
    if ( !adapter.SetWireframe( true ) ) return 13;
    if ( !adapter.BeginScene() ) return 14;
    RECT rect{};
    if ( !adapter.Clear( 0, &rect, GFXCLEAR_TARGET, 0x12345678u ) ) return 15;
    SHMatrix matrix{};
    if ( !adapter.SetViewTransform( matrix ) ) return 16;
    if ( !adapter.EndScene() || !adapter.Flip() ) return 17;
    IGFXTexture *texture = adapter.CreateTexture( 2, 2, 1, GFXPF_ARGB8888, GFXD_STATIC );
    if ( !texture ) return 20;
    texture->AddRef();
    SSurfaceLockInfo lock{};
    if ( !texture->Lock( 0, &lock ) || lock.nPitch != 8 || !texture->Unlock( 0 ) ) return 21;
    if ( !adapter.SetTexture( 0, texture ) || !adapter.SetTexture( 1, texture ) ) return 22;
    texture->Release();
    IGFXRTexture *target = adapter.CreateRTexture( 2, 2 );
    if ( !target ) return 23;
    target->AddRef();
    if ( !adapter.SetRenderTarget( target ) || !adapter.SetRenderTarget( nullptr ) ) return 24;
    target->Release();
    if ( texture_creates != 1 || texture_uploads != 1 || texture_releases != 1 || target_creates != 1 || target_binds != 2 ) return 25;
    IGFXVertices *vertices = adapter.CreateVertices( 3, GFXFVF_XYZ, GFXPT_TRIANGLELIST, GFXD_STATIC );
    IGFXIndices *indices = adapter.CreateIndices( 3, GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_STATIC );
    if ( !vertices || !indices ) return 26;
    vertices->AddRef(); indices->AddRef();
    if ( !vertices->Lock() || !indices->Lock() ) return 27;
    vertices->Unlock(); indices->Unlock();
    if ( !adapter.Draw( vertices, indices ) ) return 28;
    vertices->Release(); indices->Release();
    if ( buffer_creates != 2 || buffer_uploads != 2 || buffer_releases != 2 ) return 29;
    if ( !adapter.GetTempVertices( 3, GFXFVF_XYZ, GFXPT_TRIANGLELIST ) || !adapter.DrawTemp() ) return 30;
    if ( std::strcmp( trace, "CRVSBLTEP" ) != 0 ) return 18;
    adapter.Done();
    if ( std::strcmp( trace, "CRVSBLTEPD" ) != 0 ) return 19;
    return 0;
}

int main( int argc, char **argv )
{
    const int recording = RunRecordingTest();
    if ( recording != 0 ) { std::fprintf( stderr, "recording test failed: %d\n", recording ); return recording; }
    const char *path = argc > 1 ? argv[1] : "zig-out/bin/GFXGPU.dll";
    HMODULE module = LoadLibraryA( path );
    if ( !module )
    {
        std::fprintf( stderr, "LoadLibrary failed for %s (%lu)\n", path, GetLastError() );
        return 1;
    }

    const auto getDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( GetProcAddress( module, "GetModuleDescriptor" ) );
    if ( !getDescriptor )
    {
        std::fprintf( stderr, "GetModuleDescriptor export is missing\n" );
        FreeLibrary( module );
        return 2;
    }

    const SModuleDescriptor *descriptor = getDescriptor();
    if ( !descriptor || descriptor->nType != GFX_GFX || !descriptor->pFactory )
    {
        std::fprintf( stderr, "invalid GFXGPU module descriptor\n" );
        FreeLibrary( module );
        return 3;
    }

    IRefCount *object = descriptor->pFactory->CreateObject( GFX_GFX );
    if ( !object || !object->IsValid() )
    {
        std::fprintf( stderr, "GFX_GFX factory object was not created\n" );
        FreeLibrary( module );
        return 4;
    }

    object->Release();
    FreeLibrary( module );
    std::puts( "GFXGPU factory export and GFX_GFX object verified" );
    return 0;
}
