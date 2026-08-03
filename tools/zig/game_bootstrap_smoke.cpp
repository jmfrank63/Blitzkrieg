#include "../../Sources/src/Platform/SDLApplication.h"
#include "../../Sources/src/GFXGPU/gfxgpu_c.h"

#include <cstdio>

int main()
{
    NPlatform::SDLApplication application;
    if ( !application.Initialize( "Blitzkrieg bootstrap", 320, 200 ) ) return 1;
    application.Show();

    GfxGpuApi api{};
    api.struct_size = sizeof( api );
    if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &api ) != GFXGPU_OK ) return 2;
    GfxGpuCreateInfo info{};
    info.struct_size = sizeof( info );
    info.flags = GFXGPU_CREATE_DEBUG;
    info.sdl_window = application.BorrowWindow().value;
    info.width = 320;
    info.height = 200;
    GfxGpuRenderer *renderer = nullptr;
    if ( api.create( &info, &renderer ) != GFXGPU_OK || !renderer ) return 3;

    GfxGpuClearInfo clear{};
    clear.struct_size = sizeof( clear );
    clear.color_rgba8 = 0xff101820u;
    for ( int frame = 0; frame != 3; ++frame )
    {
        if ( api.begin_frame( renderer ) != GFXGPU_OK || api.clear( renderer, &clear ) != GFXGPU_OK || api.end_frame( renderer ) != GFXGPU_OK || api.present( renderer ) != GFXGPU_OK ) return 4;
    }
    api.destroy( renderer );
    application.Shutdown();
    std::puts( "game bootstrap: SDL window, GfxGpu, 3 clear/present frames, clean shutdown" );
    return 0;
}
