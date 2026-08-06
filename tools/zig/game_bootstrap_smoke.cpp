#include "../../Sources/src/Platform/SDLApplication.h"
#include "../../Sources/src/GFXGPU/gfxgpu_c.h"

#include <cstdio>
#include <string>

int main()
{
    std::string trace;
    for ( int cycle = 0; cycle != 3; ++cycle )
    {
        NPlatform::SDLApplication application;
        if ( !application.Initialize( "Blitzkrieg bootstrap", 320, 200 ) ) return 1;
        trace += 'A';
        GfxGpuApi api{};
        api.struct_size = sizeof( api );
        if ( gfxgpu_get_api( GFXGPU_ABI_VERSION, &api ) != GFXGPU_OK ) return 2;
        trace += 'M';
        application.Show();
        if ( !application.SetFullscreen( true ) || !application.SetFullscreen( false ) ) return 10;
        application.SetClipboardText( "platform-bootstrap" );
        if ( application.GetClipboardText() != "platform-bootstrap" ) return 3;
        if ( !application.AddVirtualControllerForTests( 77, "bootstrap-controller" ) ) return 4;
        char controller_name[64] = {};
        if ( !application.GetControllerName( 77, controller_name, sizeof( controller_name ) ) ) return 5;
        if ( std::string( controller_name ) != "bootstrap-controller" ) return 6;
        application.RemoveVirtualControllerForTests( 77 );
        NPlatform::PlatformEvent event;
        while ( application.PollEvent( event ) )
            if ( event.type == NPlatform::EventType::quit ) return 11;
        trace += 'E';

        GfxGpuCreateInfo info{};
        info.struct_size = sizeof( info );
        info.flags = GFXGPU_CREATE_DEBUG;
        info.sdl_window = application.BorrowWindow().value;
        info.width = 320;
        info.height = 200;
        GfxGpuRenderer *renderer = nullptr;
        if ( api.create( &info, &renderer ) != GFXGPU_OK || !renderer ) return 7;
        trace += 'R';
        GfxGpuClearInfo clear{};
        clear.struct_size = sizeof( clear );
        clear.color_rgba8 = 0xff101820u;
        for ( int frame = 0; frame != 3; ++frame )
        {
            if ( api.begin_frame( renderer ) != GFXGPU_OK || api.clear( renderer, &clear ) != GFXGPU_OK || api.end_frame( renderer ) != GFXGPU_OK || api.present( renderer ) != GFXGPU_OK ) return 8;
        }
        api.destroy( renderer );
        trace += 'r';
        application.Shutdown();
        trace += 'a';
    }
    if ( trace != "AMERraAMERraAMERra" ) return 9;
    std::puts( "game bootstrap: SDL window, event/controller services, 3 GfxGpu restart cycles, clean shutdown" );
    return 0;
}
