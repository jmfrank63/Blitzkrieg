#include "../../Sources/src/GFXGPU/gfxgpu_c.h"

#include <cstdio>

int main()
{
    GfxGpuApi api{};
    api.struct_size = static_cast<uint32_t>( sizeof( api ) );
    if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &api ) != GFXGPU_OK ) return 1;
    if ( api.abi_version != GFXGPU_ABI_VERSION || api.struct_size != sizeof( api ) ) return 2;
    if ( gfxgpu_get_api( GFXGPU_ABI_VERSION + 1u, &api ) != GFXGPU_UNSUPPORTED ) return 3;

    GfxGpuCreateInfo info{};
    info.struct_size = static_cast<uint32_t>( sizeof( info ) );
    info.flags = GFXGPU_CREATE_DEBUG;
    info.width = 320;
    info.height = 200;
    GfxGpuRenderer *renderer = nullptr;
    if ( api.create( &info, &renderer ) != GFXGPU_OK || renderer == nullptr ) return 4;

    char diagnostic[4]{};
    uint32_t diagnostic_size = 0;
    if ( api.get_last_error( renderer, diagnostic, sizeof( diagnostic ), &diagnostic_size ) != GFXGPU_OK ) return 5;
    if ( diagnostic_size > sizeof( diagnostic ) ) return 6;

    GfxGpuLiveCounts counts{};
    counts.struct_size = static_cast<uint32_t>( sizeof( counts ) );
    if ( api.get_live_counts( renderer, &counts ) != GFXGPU_OK ) return 7;
    if ( counts.textures != 0 || counts.buffers != 0 || counts.samplers != 0 || counts.render_targets != 0 ) return 8;

    api.destroy( renderer );
    std::printf( "GfxGpu ABI v%u, table %u bytes, live resources 0\n", api.abi_version, api.struct_size );
    return 0;
}
