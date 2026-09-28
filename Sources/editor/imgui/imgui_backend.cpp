#include "imgui_backend.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <SDL3/SDL.h>

#include <string.h> // memset

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

static void CopyWindowName( char *buffer, size_t capacity, const ImGuiWindow *window )
{
    size_t length = 0;
    if ( window != nullptr && window->Name != nullptr )
    {
        while ( length + 1 < capacity && window->Name[length] != '\0' )
        {
            buffer[length] = window->Name[length];
            length++;
        }
    }
    buffer[length] = '\0';
}

extern "C" void bk_imgui_backend_pointer_state( BkImguiPointerState *out )
{
    if ( out == nullptr )
        return;
    memset( out, 0, sizeof( *out ) );
    ImGuiContext *context = ImGui::GetCurrentContext();
    if ( context == nullptr )
        return;
    ImGuiContext &g = *context;
    const ImGuiIO &io = g.IO;
    out->mouse_x = io.MousePos.x;
    out->mouse_y = io.MousePos.y;
    out->display_w = io.DisplaySize.x;
    out->display_h = io.DisplaySize.y;
    out->want_capture_mouse = io.WantCaptureMouse;
    out->app_focus_lost = io.AppFocusLost;
    for ( int button = 0; button < 3; button++ )
    {
        out->mouse_down[button] = io.MouseDown[button];
        out->mouse_down_owned[button] = io.MouseDownOwned[button];
    }
    out->open_popups = g.OpenPopupStack.Size;
    out->want_capture_mouse_next_frame = g.WantCaptureMouseNextFrame;
    out->queued_events = g.InputEventsQueue.Size;
    for ( const ImGuiInputEvent &event : g.InputEventsQueue )
    {
        if ( event.Type == ImGuiInputEventType_MousePos )
        {
            out->queued_mouse_pos++;
            out->queued_mouse_pos_valid = true;
            out->queued_mouse_x = event.MousePos.PosX;
            out->queued_mouse_y = event.MousePos.PosY;
        }
        else if ( event.Type == ImGuiInputEventType_MouseButton )
            out->queued_mouse_button++;
        else if ( event.Type == ImGuiInputEventType_MouseWheel )
            out->queued_mouse_wheel++;
        else if ( event.Type == ImGuiInputEventType_Key )
            out->queued_key++;
        else if ( event.Type == ImGuiInputEventType_Focus )
            out->queued_focus++;
    }
    CopyWindowName( out->hovered_window, sizeof( out->hovered_window ), g.HoveredWindow );
    CopyWindowName( out->hovered_before_clear, sizeof( out->hovered_before_clear ), g.HoveredWindowBeforeClear );
    // Frontmost first, the order FindHoveredWindowEx walks them in.
    for ( int n = g.Windows.Size - 1; n >= 0; n-- )
    {
        const ImGuiWindow *window = g.Windows[n];
        if ( !window->WasActive || window->Hidden )
            continue;
        if ( window->OuterRectClipped.Contains( io.MousePos ) )
        {
            CopyWindowName( out->window_at_pointer, sizeof( out->window_at_pointer ), window );
            break;
        }
    }
}
