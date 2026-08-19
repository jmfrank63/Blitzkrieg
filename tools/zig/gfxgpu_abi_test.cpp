#include "../../Sources/src/GFXGPU/gfxgpu_c.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

int main()
{
    GfxGpuApi api{};
    api.struct_size = static_cast<uint32_t>( sizeof( api ) );
    if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &api ) != GFXGPU_OK ) return 1;
    if ( api.abi_version != GFXGPU_ABI_VERSION || api.struct_size != sizeof( api ) ) return 2;
    if ( gfxgpu_get_api( GFXGPU_ABI_VERSION + 1u, &api ) != GFXGPU_UNSUPPORTED ) return 3;

    // A caller compiled before set_present_fit and set_present_mode were
    // appended asks for the shorter table. It has to be served - that is what
    // struct_size is for - and served without a single byte landing past the
    // end of its object. Modelled with a full-size table whose tail is
    // poisoned: an unbounded fill would overwrite the poison.
    {
        const size_t nShortSize = offsetof( GfxGpuApi, set_present_fit );
        GfxGpuApi legacy{};
        std::memset( &legacy, 0xAA, sizeof( legacy ) );
        legacy.struct_size = static_cast<uint32_t>( nShortSize );
        if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &legacy ) != GFXGPU_OK ) return 9;
        if ( legacy.struct_size != nShortSize ) return 10;
        if ( legacy.abi_version != GFXGPU_ABI_VERSION || legacy.create == nullptr || legacy.present == nullptr ) return 11;
        const unsigned char *pTail = reinterpret_cast<const unsigned char *>( &legacy ) + nShortSize;
        for ( size_t i = 0; i < sizeof( legacy ) - nShortSize; ++i )
            if ( pTail[i] != 0xAA ) return 12;
        // Anything shorter than the mandatory part of the table is still a
        // rejection rather than a partial fill.
        GfxGpuApi tiny{};
        tiny.struct_size = 1;
        if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &tiny ) != GFXGPU_INVALID_ARGUMENT ) return 13;
    }

    GfxGpuCreateInfo info{};
    info.struct_size = static_cast<uint32_t>( sizeof( info ) );
    info.flags = GFXGPU_CREATE_DEBUG | GFXGPU_CREATE_NO_DEVICE;
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
