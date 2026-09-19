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
/* Uploads the current draw data (igRender must have run) and draws it onto
   target with a LOAD pass, so what is already there stays. command_buffer:
   SDL_GPUCommandBuffer*; target: SDL_GPUTexture*. No pass may be open. */
void bk_imgui_backend_render(void *command_buffer, void *target);

#ifdef __cplusplus
}
#endif

#endif
