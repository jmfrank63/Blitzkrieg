#ifndef BK_EDITOR_IMGUI_BACKEND_H
#define BK_EDITOR_IMGUI_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dear ImGui's SDL3 platform backend and SDL GPU renderer backend behind a C
   API, for the Zig editors. SDL objects are passed as void * so this header
   stays free of SDL declarations. The ImGui context must exist (igCreateContext)
   before init. */

/* sdl_window: SDL_Window*; gpu_device: SDL_GPUDevice*; color_target_format:
   the SDL_GPUTextureFormat of the targets render() draws into. */
bool bk_imgui_backend_init(void *sdl_window, void *gpu_device, uint32_t color_target_format);
void bk_imgui_backend_shutdown(void);
/* sdl_event: const SDL_Event*. Returns true when ImGui used it. */
bool bk_imgui_backend_process_event(const void *sdl_event);
/* Both backends' NewFrame; call before igNewFrame. */
void bk_imgui_backend_new_frame(void);

/* Isolates synthetic input (--smoke, BK_EDITOR_AUTO) from the real cursor:
   enabled=false disables the SDL3 backend's own mouse capture (never calls
   SDL_CaptureMouse) and clears its auto-capture hint, so a synthetic press
   cannot claim OS capture over, or be overridden by, the real hardware
   pointer's drag. This does not reach the backend's separate "no window is
   hovered - fall back to SDL_GetGlobalMouseState" read inside its own
   per-frame update: that flag (ImGui_ImplSDL3_Data::MouseCanUseGlobalState)
   is set once at ImGui_ImplSDL3_InitForSDLGPU from a driver whitelist and
   has no public accessor in imgui_impl_sdl3.h - the vendored backend offers
   no switch for it (03-12-PLAN.md Task 2's own accepted fallback). Call once
   per host, before the first automated frame; enabled=true restores the
   backend's own default (Enabled) capture mode. */
void bk_imgui_backend_use_global_mouse(bool enabled);
/* Uploads the current draw data (igRender must have run) and draws it onto
   target with a LOAD pass, so what is already there stays. command_buffer:
   SDL_GPUCommandBuffer*; target: SDL_GPUTexture*. No pass may be open. */
void bk_imgui_backend_render(void *command_buffer, void *target);

/* What ImGui's own state says about the pointer, for a smoke's FAIL line: its
   position, what it hovers, its buttons, and the input events it has not yet
   trickled in. Read between frames, so it describes the last igNewFrame.
   Window names are cut to fit and always null-terminated, "" for none. */
typedef struct BkImguiPointerState
{
    float mouse_x, mouse_y;         /* io.MousePos; -FLT_MAX when ImGui has none */
    float display_w, display_h;     /* io.DisplaySize */
    bool want_capture_mouse;
    bool app_focus_lost;
    bool mouse_down[3];             /* left, right, middle */
    bool mouse_down_owned[3];
    int open_popups;
    int want_capture_mouse_next_frame; /* -1 when not overridden */
    int queued_events;              /* still waiting in ImGui's input queue */
    int queued_mouse_pos, queued_mouse_button, queued_mouse_wheel, queued_key, queued_focus;
    bool queued_mouse_pos_valid;    /* the last queued MousePos, where ImGui is headed */
    float queued_mouse_x, queued_mouse_y;
    char hovered_window[48];        /* g.HoveredWindow */
    char hovered_before_clear[48];  /* g.HoveredWindowBeforeClear: hovered, before modal/ownership clears it */
    char window_at_pointer[48];     /* the frontmost active window whose rect holds io.MousePos */
    char active_window[48];         /* g.ActiveIdWindow: the window of the widget being used, "" for none */
    unsigned int active_id;         /* g.ActiveId (0: no widget is active) */
    unsigned int hovered_id;        /* g.HoveredId */
    bool want_capture_keyboard;     /* io.WantCaptureKeyboard: keys then go to ImGui, not the view */
    bool want_text_input;           /* io.WantTextInput */
    char nav_window[48];            /* g.NavWindow: the window that has keyboard focus, "" for none */
} BkImguiPointerState;
void bk_imgui_backend_pointer_state(BkImguiPointerState *out);

/* View > Reset layout: every top-level window forgets its remembered position,
   size and collapse state (and the .ini settings do), so the app's own
   FirstUseEver placement applies again from the next frame. */
void bk_imgui_reset_window_layout(void);

#ifdef __cplusplus
}
#endif

#endif
