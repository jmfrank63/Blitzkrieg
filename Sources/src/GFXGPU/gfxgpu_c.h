#ifndef BLITZKRIEG_GFXGPU_C_H
#define BLITZKRIEG_GFXGPU_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFXGPU_ABI_VERSION UINT32_C(1)

typedef uint64_t GfxGpuHandle;
typedef struct GfxGpuRenderer GfxGpuRenderer;
typedef uint32_t GfxGpuResult;

#define GFXGPU_OK               UINT32_C(0)
#define GFXGPU_INVALID_ARGUMENT UINT32_C(1)
#define GFXGPU_INVALID_STATE    UINT32_C(2)
#define GFXGPU_INVALID_HANDLE   UINT32_C(3)
#define GFXGPU_UNSUPPORTED      UINT32_C(4)
#define GFXGPU_OUT_OF_MEMORY    UINT32_C(5)
#define GFXGPU_SDL_ERROR        UINT32_C(6)
#define GFXGPU_IO_ERROR         UINT32_C(7)
#define GFXGPU_SHADER_ERROR     UINT32_C(8)
#define GFXGPU_INTERNAL_ERROR   UINT32_C(9)

#define GFXGPU_CREATE_NONE      UINT32_C(0)
#define GFXGPU_CREATE_DEBUG     UINT32_C(1)
#define GFXGPU_CREATE_NO_DEVICE UINT32_C(2)

typedef struct GfxGpuExtent {
    uint32_t width;
    uint32_t height;
} GfxGpuExtent;

typedef struct GfxGpuViewport {
    float x;
    float y;
    float width;
    float height;
    float min_depth;
    float max_depth;
} GfxGpuViewport;

typedef struct GfxGpuCreateInfo {
    uint32_t struct_size;
    uint32_t flags;
    void *sdl_window;
    uint32_t width;
    uint32_t height;
    const char *shader_directory_utf8;
    const char *preferred_driver_utf8;
} GfxGpuCreateInfo;

typedef struct GfxGpuLiveCounts {
    uint32_t struct_size;
    uint32_t textures;
    uint32_t buffers;
    uint32_t samplers;
    uint32_t render_targets;
} GfxGpuLiveCounts;

typedef struct GfxGpuClearInfo {
    uint32_t struct_size;
    uint32_t mask;
    uint32_t color_rgba8;
    float depth;
    uint32_t stencil;
} GfxGpuClearInfo;
typedef struct GfxGpuViewportInfo { uint32_t struct_size; float x, y, width, height, min_depth, max_depth; } GfxGpuViewportInfo;
typedef struct GfxGpuMatrixInfo { uint32_t struct_size; float values[16]; } GfxGpuMatrixInfo;

typedef struct GfxGpuApi {
    uint32_t abi_version;
    uint32_t struct_size;
    GfxGpuResult (*create)(const GfxGpuCreateInfo *, GfxGpuRenderer **);
    void (*destroy)(GfxGpuRenderer *);
    GfxGpuResult (*get_last_error)(GfxGpuRenderer *, char *, uint32_t, uint32_t *);
    GfxGpuResult (*get_live_counts)(GfxGpuRenderer *, GfxGpuLiveCounts *);
    GfxGpuResult (*begin_frame)(GfxGpuRenderer *);
    GfxGpuResult (*end_frame)(GfxGpuRenderer *);
    GfxGpuResult (*present)(GfxGpuRenderer *);
    void (*cancel_frame)(GfxGpuRenderer *);
    GfxGpuResult (*clear)(GfxGpuRenderer *, const GfxGpuClearInfo *);
    GfxGpuResult (*resize)(GfxGpuRenderer *, uint32_t, uint32_t);
    GfxGpuResult (*set_viewport)(GfxGpuRenderer *, const GfxGpuViewportInfo *);
    GfxGpuResult (*set_transform)(GfxGpuRenderer *, const GfxGpuMatrixInfo *, const GfxGpuMatrixInfo *);
    GfxGpuResult (*set_color)(GfxGpuRenderer *, uint32_t);
    GfxGpuResult (*set_fog)(GfxGpuRenderer *, int);
    GfxGpuResult (*set_texture)(GfxGpuRenderer *, uint64_t);
    GfxGpuResult (*set_sampler)(GfxGpuRenderer *, uint64_t);
    GfxGpuResult (*draw)(GfxGpuRenderer *, uint32_t, uint32_t);
} GfxGpuApi;

GfxGpuResult gfxgpu_get_api(uint32_t requested_version, GfxGpuApi *out_api);

#ifdef __cplusplus
}
#endif

#endif
