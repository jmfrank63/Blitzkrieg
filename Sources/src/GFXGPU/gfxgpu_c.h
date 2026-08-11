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
typedef struct GfxGpuTemporaryGeometryInfo { uint32_t struct_size; const void *data; uint32_t byte_length; uint32_t stride; } GfxGpuTemporaryGeometryInfo;
typedef struct GfxGpuStateInfo { uint32_t struct_size; uint32_t kind; uint32_t index; uint32_t value; float values[16]; } GfxGpuStateInfo;
typedef struct GfxGpuTextureCreateInfo { uint32_t struct_size; uint32_t width; uint32_t height; uint32_t mip_count; uint32_t format; uint32_t usage; } GfxGpuTextureCreateInfo;
typedef struct GfxGpuTextureUploadInfo { uint32_t struct_size; const void *data; uint32_t byte_length; uint32_t row_pitch; uint32_t mip_level; } GfxGpuTextureUploadInfo;
typedef struct GfxGpuRenderTargetCreateInfo { uint32_t struct_size; uint32_t width; uint32_t height; uint32_t format; } GfxGpuRenderTargetCreateInfo;
typedef struct GfxGpuBufferCreateInfo { uint32_t struct_size; uint32_t element_count; uint32_t format; uint32_t stride; uint32_t usage; } GfxGpuBufferCreateInfo;
typedef struct GfxGpuBufferUploadInfo { uint32_t struct_size; const void *data; uint32_t byte_length; uint32_t byte_offset; } GfxGpuBufferUploadInfo;
typedef struct GfxGpuReadbackInfo { uint32_t struct_size; uint32_t width; uint32_t height; uint32_t byte_length; uint32_t row_pitch; void *data; } GfxGpuReadbackInfo;

enum {
    GFXGPU_STATE_WIREFRAME = 1,
    GFXGPU_STATE_CULL_MODE = 2,
    GFXGPU_STATE_DEPTH_MODE = 3,
    GFXGPU_STATE_LIGHTING = 4,
    GFXGPU_STATE_SPECULAR = 5,
    GFXGPU_STATE_LIGHT = 6,
    GFXGPU_STATE_MATERIAL = 7,
    GFXGPU_STATE_SHADE_EFFECT = 8,
    // EGFXPrimitiveType of the geometry the next draw submits.
    GFXGPU_STATE_TOPOLOGY = 9,
    // Stage-0 texture matrix; index selects the stage, values carry the matrix.
    GFXGPU_STATE_TEXTURE_MATRIX = 10,
};

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
    GfxGpuResult (*set_state)(GfxGpuRenderer *, const GfxGpuStateInfo *);
    GfxGpuResult (*create_texture)(GfxGpuRenderer *, const GfxGpuTextureCreateInfo *, GfxGpuHandle *);
    GfxGpuResult (*upload_texture)(GfxGpuRenderer *, GfxGpuHandle, const GfxGpuTextureUploadInfo *);
    GfxGpuResult (*destroy_texture)(GfxGpuRenderer *, GfxGpuHandle);
    GfxGpuResult (*create_render_target)(GfxGpuRenderer *, const GfxGpuRenderTargetCreateInfo *, GfxGpuHandle *);
    GfxGpuResult (*bind_render_target)(GfxGpuRenderer *, GfxGpuHandle);
    GfxGpuResult (*create_buffer)(GfxGpuRenderer *, const GfxGpuBufferCreateInfo *, GfxGpuHandle *);
    GfxGpuResult (*upload_buffer)(GfxGpuRenderer *, GfxGpuHandle, const GfxGpuBufferUploadInfo *);
    GfxGpuResult (*destroy_buffer)(GfxGpuRenderer *, GfxGpuHandle);
    GfxGpuResult (*set_texture)(GfxGpuRenderer *, uint64_t);
    /* Binds a texture to an explicit stage. Stage 1 is the terrain's noise or
       crosset; stage 0 is equivalent to set_texture. Appended to the struct, so
       callers that predate it keep working via the struct_size check. */
    GfxGpuResult (*set_texture_stage)(GfxGpuRenderer *, uint32_t, uint64_t);
    GfxGpuResult (*set_sampler)(GfxGpuRenderer *, uint64_t);
    GfxGpuResult (*draw)(GfxGpuRenderer *, uint32_t, uint32_t);
    GfxGpuResult (*draw_indexed)(GfxGpuRenderer *, uint64_t, uint32_t, uint32_t, uint32_t, int32_t);
    GfxGpuResult (*draw_temporary)(GfxGpuRenderer *, const GfxGpuTemporaryGeometryInfo *, uint32_t);
    GfxGpuResult (*bind_vertex_buffer)(GfxGpuRenderer *, GfxGpuHandle);
    /* Appended: presentation of a scene that differs from the drawable.
       0 = centered 1:1 (borders/crop, gameplay), nonzero = aspect-fit scale
       (menus and videos, whose controls must never be clipped away). */
    GfxGpuResult (*set_present_fit)(GfxGpuRenderer *, int);
} GfxGpuApi;

GfxGpuResult gfxgpu_get_api(uint32_t requested_version, GfxGpuApi *out_api);
GfxGpuResult gfxgpu_readback(GfxGpuRenderer *, GfxGpuReadbackInfo *);

#ifdef __cplusplus
}
#endif

#endif
