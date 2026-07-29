#ifndef __GFXVK_C_H__
#define __GFXVK_C_H__

// Plain C ABI between the C++ bridge and the Zig Vulkan core.
// All functions use __stdcall calling convention (Win64 ignores
// the attribute but Zig must mark them .callconv(.stdcall)).
//
// The bridge loads GFXVK.zig at DLL init and calls these
// functions on the Vulkan device context held opaque in Zig.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Context life-cycle ──────────────────────────────────────── */

typedef struct VkContext VkContext;

VkContext * STDCALL gfxvk_create_context(void *hwnd);
void        STDCALL gfxvk_destroy_context(VkContext *ctx);

/* ── Frame life-cycle ────────────────────────────────────────── */

bool STDCALL gfxvk_begin_scene(VkContext *ctx);
void STDCALL gfxvk_end_scene(VkContext *ctx);
void STDCALL gfxvk_clear(
    VkContext *ctx,
    int        nNumRects,
    void      *pRects,
    uint32_t   dwFlags,
    uint32_t   dwColor,
    float      fDepth,
    uint32_t   dwStencil
);
bool STDCALL gfxvk_flip(VkContext *ctx);

/* ── Phase 3 — 2D rendering ─────────────────────────────────── */

void STDCALL gfxvk_set_effect(VkContext *ctx, uint32_t effect_id);
void STDCALL gfxvk_set_texture(VkContext *ctx, void *tex_ptr);
bool STDCALL gfxvk_create_texture(
    VkContext *ctx,
    void      *tex_ptr,
    uint32_t   width,
    uint32_t   height,
    uint32_t   format,
    uint32_t   mips,
    const uint8_t *data,
    size_t     data_size
);
uint8_t * STDCALL gfxvk_lock_vb(VkContext *ctx, size_t num_vertices);
void      STDCALL gfxvk_unlock_vb(VkContext *ctx);
uint8_t * STDCALL gfxvk_lock_ib(VkContext *ctx, size_t num_indices);
void      STDCALL gfxvk_unlock_ib(VkContext *ctx);
void STDCALL gfxvk_draw_indexed(VkContext *ctx, size_t index_count, size_t vertex_base, size_t index_base);
void STDCALL gfxvk_set_viewport(VkContext *ctx, int x, int y, int width, int height);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __GFXVK_C_H__
