// GFXVK bridge — implements the IGFX interface by delegating to
// the Zig Vulkan core via the gfxvk_c.h ABI.
//
// This module reuses the backend-agnostic manager layer
// (TextureManager, FontManager, GeometryManager, etc.) and only
// replaces the D3D9-touching GraphicsEngine.

#include "StdAfx.h"
#include "gfxvk_c.h"

// Static context pointer for VkTexture::Unlock
static VkContext *s_gfxvk_ctx = nullptr;

// ── VkTexture — minimal IGFXTexture for the Vulkan backend ─────
class VkTexture : public IGFXTexture
{
    OBJECT_NORMAL_METHODS(VkTexture);
    int nSizeX = 0, nSizeY = 0;
    EGFXPixelFormat format = GFXPF_UNKNOWN;
    std::vector<BYTE> staging;
    int nPitch = 0;
public:
    VkTexture() = default;
    void Init(int w, int h, EGFXPixelFormat fmt)
    {
        nSizeX = w; nSizeY = h; format = fmt;
        nPitch = w * (GetBPP(fmt) / 8);
        staging.resize((size_t)nPitch * h);
    }
    bool STDCALL Lock(int nLevel, SSurfaceLockInfo *pLockInfo) override
    {
        pLockInfo->nPitch = nPitch;
        pLockInfo->pData = staging.data();
        return true;
    }
    bool STDCALL Unlock(int nLevel) override
    {
        if (s_gfxvk_ctx)
            gfxvk_create_texture(s_gfxvk_ctx, (void*)this, nSizeX, nSizeY, (uint32_t)format, 1, staging.data(), staging.size());
        return true;
    }
    bool STDCALL AddDirtyRect(const RECT *pRect) override { return true; }
    int STDCALL GetSizeX(int nLevel) const override { return nSizeX >> nLevel; }
    int STDCALL GetSizeY(int nLevel) const override { return nSizeY >> nLevel; }
    EGFXPixelFormat STDCALL GetFormat() const override { return format; }
};

// ── GfxVkEngine — IGFX implementation ─────────────────────────

class GfxVkEngine : public IGFX
{
    OBJECT_NORMAL_METHODS(GfxVkEngine);
    DECLARE_SERIALIZE;

    VkContext *ctx_ = nullptr;
    HWND hwnd_ = nullptr;

    // Phase 3 — temp buffer tracking for 2D rendering
    size_t temp_vb_base_ = 0;   // vertex base offset (in vertices)
    size_t temp_vb_count_ = 0;  // vertices locked since last DrawTemp
    size_t temp_ib_base_ = 0;   // index base offset (in indices)
    size_t temp_ib_count_ = 0;  // indices locked since last DrawTemp
    DWORD temp_format_ = 0;     // FVF format of current temp vertices
    EGFXPrimitiveType temp_type_ = GFXPT_TRIANGLELIST;
    size_t temp_vb_total_ = 0;   // cumulative vertices locked this frame
    size_t temp_ib_total_ = 0;   // cumulative indices locked this frame

public:
    GfxVkEngine() = default;
    ~GfxVkEngine() override { Done(); }

    bool STDCALL Init(const char *pszAdapterName, HWND hWnd) override
    {
        hwnd_ = hWnd;
        ctx_ = gfxvk_create_context((void*)hWnd);
        s_gfxvk_ctx = ctx_;
        return ctx_ != nullptr;
    }

    bool STDCALL Done() override
    {
        if (ctx_)
        {
            s_gfxvk_ctx = nullptr;
            gfxvk_destroy_context(ctx_);
            ctx_ = nullptr;
        }
        return true;
    }

    // ── Frame ──────────────────────────────────────────────
    bool STDCALL IsActive() override { return ctx_ != nullptr; }
    bool STDCALL BeginScene() override
    {
        temp_vb_total_ = 0;
        temp_ib_total_ = 0;
        temp_vb_count_ = 0;
        temp_ib_count_ = 0;
        return gfxvk_begin_scene(ctx_);
    }
    bool STDCALL EndScene() override { gfxvk_end_scene(ctx_); return true; }
    bool STDCALL IsSafeToPresent() const override { return ctx_ != nullptr; }
    bool STDCALL Clear(int nNumRects, RECT *pRects, DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil) override
    {
        gfxvk_clear(ctx_, nNumRects, pRects, dwFlags, dwColor, fDepth, dwStencil);
        return true;
    }
    bool STDCALL Flip() override { return gfxvk_flip(ctx_); }
    bool STDCALL SetRenderTarget(IGFXRTexture *pRT) override { return false; }

    // ── Stubs for later phases ────────────────────────────
    void STDCALL SetViewMatrix(const SHMatrix &mat) override {}
    void STDCALL SetWorldMatrix(const SHMatrix &mat) override {}
    void STDCALL SetProjectionMatrix(const SHMatrix &mat) override {}
    void STDCALL SetViewport(int nX, int nY, int nWidth, int nHeight) override
    {
        gfxvk_set_viewport(ctx_, nX, nY, nWidth, nHeight);
    }
    void STDCALL GetViewport(int *pnX, int *pnY, int *pnWidth, int *pnHeight) override {}
    const SHMatrix& STDCALL GetViewMatrix() const override { static SHMatrix m; return m; }
    const SHMatrix& STDCALL GetInverseViewMatrix() const override { static SHMatrix m; return m; }
    const SHMatrix& STDCALL GetProjectionMatrix() const override { static SHMatrix m; return m; }
    const SHMatrix& STDCALL GetViewportMatrix() const override { static SHMatrix m; return m; }
    void STDCALL GetViewVolume(SPlane *pPlanes) const override {}
    void STDCALL GetViewVolumeCrosses(const CVec2 &vPoint, CVec3 *pvNear, CVec3 *pvFar) override {}
    void STDCALL SetLight(int nIndex, const SGFXLightDirectional &light) override {}
    void STDCALL SetLight(int nIndex, const SGFXLightPoint &light) override {}
    void STDCALL SetLight(int nIndex, const SGFXLightSpot &light) override {}
    void STDCALL EnableLight(int nIndex, bool bEnable) override {}
    void STDCALL SetMaterial(const SGFXMaterial &material) override {}
    bool STDCALL SetTexture(int nStage, IGFXBaseTexture *pTexture) override
    {
        if (nStage != 0) return false;
        gfxvk_set_texture(ctx_, (void*)pTexture);
        return true;
    }
    bool STDCALL SetWireframe(bool bWireframe) override { return false; }
    bool STDCALL SetCullMode(EGFXCull cull) override { return false; }
    bool STDCALL SetDepthBufferMode(EGFXDepthBuffer depth, EGFXCmpFunction cmp) override { return false; }
    bool STDCALL EnableLighting(bool bLighting) override {}
    bool STDCALL EnableSpecular(bool bEnable) override {}
    bool STDCALL SetFont(IGFXFont *pFont) override { return false; }

    void STDCALL SetOptimizedBuffers(bool bEnable) override {}
    IGFXVertices* STDCALL CreateVertices(int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXVertices *pVertices) override { return nullptr; }
    IGFXIndices* STDCALL CreateIndices(int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXIndices *pIndices) override { return nullptr; }
    bool STDCALL BeginSolidVertexBlock(int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic) override { return false; }
    bool STDCALL EndSolidVertexBlock() override { return false; }
    bool STDCALL BeginSolidIndexBlock(int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic) override { return false; }
    bool STDCALL EndSolidIndexBlock() override { return false; }
    void* STDCALL GetTempVertices(int nNumElements, DWORD dwFormat, EGFXPrimitiveType type) override
    {
        if (temp_vb_count_ == 0)
            temp_vb_base_ = temp_vb_total_;
        temp_vb_count_ += nNumElements;
        temp_vb_total_ += nNumElements;
        temp_format_ = dwFormat;
        temp_type_ = type;
        return gfxvk_lock_vb(ctx_, nNumElements);
    }
    void* STDCALL GetTempIndices(int nNumElements, DWORD dwFormat, EGFXPrimitiveType type) override
    {
        if (temp_ib_count_ == 0)
            temp_ib_base_ = temp_ib_total_;
        temp_ib_count_ += nNumElements;
        temp_ib_total_ += nNumElements;
        return gfxvk_lock_ib(ctx_, nNumElements);
    }
    IGFXTexture* STDCALL CreateTexture(int nSizeX, int nSizeY, int nNumMipLevels, EGFXPixelFormat format, EGFXDynamic eDynamic, IGFXTexture *pTexture) override
    {
        VkTexture *pTx = pTexture ? static_cast<VkTexture*>(pTexture) : new VkTexture();
        pTx->Init(nSizeX, nSizeY, format);
        return pTx;
    }
    IGFXRTexture* STDCALL CreateRTexture(int nSizeX, int nSizeY) override { return nullptr; }
    bool STDCALL UpdateTexture(IGFXTexture *pSrc, IGFXTexture *pDst, bool bAsync) override { return false; }
    bool STDCALL DrawTemp() override
    {
        if (temp_ib_count_ == 0) return true;
        gfxvk_unlock_vb(ctx_);
        gfxvk_unlock_ib(ctx_);
        gfxvk_draw_indexed(ctx_, temp_ib_count_, temp_vb_base_, temp_ib_base_);
        temp_vb_count_ = 0;
        temp_ib_count_ = 0;
        return true;
    }
    bool STDCALL Draw(IGFXVertices *pVertices, IGFXIndices *pIndices) override { return false; }
    bool STDCALL DrawMesh(IGFXMesh *pMesh, const SHMatrix *matrices, int nNumMatrices) override { return false; }
    bool STDCALL DrawStringA(const char *pszString, int nX, int nY, DWORD dwColor) override { return false; }
    bool STDCALL DrawString(const wchar_t *pszString, int nX, int nY, DWORD dwColor) override { return false; }
    bool STDCALL DrawText(IGFXText *pText, const RECT &rect, int nY, DWORD dwFlags) override { return false; }
    bool STDCALL DrawRects(const SGFXRect2 *pRects, int nNumRects, bool bSolid) override
    {
        if (nNumRects <= 0) return true;
        EGFXPrimitiveType type = bSolid ? GFXPT_TRIANGLELIST : GFXPT_LINELIST;
        SGFXLVertex *verts = (SGFXLVertex*)GetTempVertices(nNumRects * 4, SGFXLVertex::format, type);
        if (!verts) return false;
        for (int i = 0; i < nNumRects; ++i)
        {
            const SGFXRect2 &r = pRects[i];
            verts[i*4+0].Setup(r.rect.minx, r.rect.maxy, r.fZ, 1, r.color, r.specular, r.maps.minx, r.maps.maxy);
            verts[i*4+1].Setup(r.rect.minx, r.rect.miny, r.fZ, 1, r.color, r.specular, r.maps.minx, r.maps.miny);
            verts[i*4+2].Setup(r.rect.maxx, r.rect.maxy, r.fZ, 1, r.color, r.specular, r.maps.maxx, r.maps.maxy);
            verts[i*4+3].Setup(r.rect.maxx, r.rect.miny, r.fZ, 1, r.color, r.specular, r.maps.maxx, r.maps.miny);
        }
        if (bSolid)
        {
            WORD *idx = (WORD*)GetTempIndices(nNumRects * 6, GFXIF_INDEX16, type);
            if (!idx) return false;
            WORD base = 0;
            for (int i = 0; i < nNumRects; ++i, base += 4)
            {
                idx[i*6+0] = base + 2;
                idx[i*6+1] = base + 1;
                idx[i*6+2] = base + 0;
                idx[i*6+3] = base + 1;
                idx[i*6+4] = base + 2;
                idx[i*6+5] = base + 3;
            }
        }
        else
        {
            WORD *idx = (WORD*)GetTempIndices(nNumRects * 8, GFXIF_INDEX16, type);
            if (!idx) return false;
            WORD base = 0;
            for (int i = 0; i < nNumRects; ++i, base += 4)
            {
                idx[i*8+0] = base + 0; idx[i*8+1] = base + 1;
                idx[i*8+2] = base + 1; idx[i*8+3] = base + 3;
                idx[i*8+4] = base + 3; idx[i*8+5] = base + 2;
                idx[i*8+6] = base + 2; idx[i*8+7] = base + 0;
            }
        }
        return DrawTemp();
    }
    bool STDCALL SetGammaRamp(const SGFXGammaRamp &ramp, bool bCalibrate) override { return false; }
    bool STDCALL GetGammaRamp(const SGFXGammaRamp *pRamp) override { return false; }
    void STDCALL SetGammaCorrectionValues(float fBrightness, float fContrast, float fGamma) override {}
    void STDCALL GetGammaCorrectionValues(float *pfBrightness, float *pfContrast, float *pfGamma) override {}
    bool STDCALL TakeScreenShot(interface IImage *pImage) override { return false; }
    int STDCALL GetNumPassedVertices() const override { return 0; }
    int STDCALL GetNumPassedPrimitives() const override { return 0; }
    bool STDCALL SetShadingEffect(int nEffect) override
    {
        gfxvk_set_effect(ctx_, (uint32_t)nEffect);
        return true;
    }
};
