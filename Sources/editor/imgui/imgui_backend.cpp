#include "imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <SDL3/SDL.h>

extern "C" bool bk_imgui_backend_init( void *sdl_window, void *gpu_device, uint32_t color_target_format )
{
    SDL_Window *window = static_cast<SDL_Window *>( sdl_window );
    if ( !ImGui_ImplSDL3_InitForSDLGPU( window ) )
        return false;
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = static_cast<SDL_GPUDevice *>( gpu_device );
    info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>( color_target_format );
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if ( !ImGui_ImplSDLGPU3_Init( &info ) )
    {
        ImGui_ImplSDL3_Shutdown();
        return false;
    }
    return true;
}

extern "C" void bk_imgui_backend_shutdown( void )
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

extern "C" bool bk_imgui_backend_process_event( const void *sdl_event )
{
    return ImGui_ImplSDL3_ProcessEvent( static_cast<const SDL_Event *>( sdl_event ) );
}

extern "C" void bk_imgui_backend_new_frame( void )
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

extern "C" void bk_imgui_backend_render( void *command_buffer, void *target )
{
    ImDrawData *draw_data = ImGui::GetDrawData();
    if ( draw_data == nullptr || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f )
        return;
    SDL_GPUCommandBuffer *command = static_cast<SDL_GPUCommandBuffer *>( command_buffer );
    // Uploads happen in a copy pass, which must not overlap a render pass.
    ImGui_ImplSDLGPU3_PrepareDrawData( draw_data, command );
    SDL_GPUColorTargetInfo color = {};
    color.texture = static_cast<SDL_GPUTexture *>( target );
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( command, &color, 1, nullptr );
    if ( pass == nullptr )
        return;
    ImGui_ImplSDLGPU3_RenderDrawData( draw_data, command, pass );
    SDL_EndGPURenderPass( pass );
}
