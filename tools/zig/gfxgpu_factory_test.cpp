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
