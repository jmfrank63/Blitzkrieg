// GFXVK object factory — standalone factory for the Vulkan renderer DLL.
//
// Phase 0 scaffolding: registers the engine and stub manager classes.
// The managers will be replaced with proper implementations in later
// phases as the Vulkan backend becomes functional.
//
// The factory does NOT include any GFX module headers that reference
// D3D9 types — it uses only the pure-interface header (GFX.h).

#include "StdAfx.h"

// ── GfxVkEngine — IGFX stub engine ────────────────────────────
// (defined in bridge.cpp, included via bridge.h)
class GfxVkEngine; // forward decl, registered below
class VkTexture;   // defined in bridge.cpp

// ── Manager stubs (Phase 0 — render nothing) ──────────────────
// These will be replaced with the real GFX manager classes in Phase 1,
// at which point GFXVK will compile the GFX manager sources directly.

class CTextureManagerStub : public ITextureManager
{
    OBJECT_NORMAL_METHODS(CTextureManagerStub);
public:
    void STDCALL SetSerialMode(ESharedDataSerialMode) override {}
    void STDCALL SetShareMode(ESharedDataSharingMode) override {}
    void STDCALL Clear(EClearMode, int, int) override {}
    bool STDCALL Init() override { return true; }
    IGFXTexture* STDCALL GetTexture(const char*) override { return nullptr; }
    const char* STDCALL GetTextureName(IGFXTexture*) override { return "default"; }
    void STDCALL SetQuality(ETextureQuality) override {}
};

class CFontManagerStub : public IFontManager
{
    OBJECT_NORMAL_METHODS(CFontManagerStub);
public:
    void STDCALL SetSerialMode(ESharedDataSerialMode) override {}
    void STDCALL SetShareMode(ESharedDataSharingMode) override {}
    void STDCALL Clear(EClearMode, int, int) override {}
    bool STDCALL Init() override { return true; }
    IGFXFont* STDCALL GetFont(const char*) override { return nullptr; }
};

class CMeshManagerStub : public IMeshManager
{
    OBJECT_NORMAL_METHODS(CMeshManagerStub);
public:
    void STDCALL SetSerialMode(ESharedDataSerialMode) override {}
    void STDCALL SetShareMode(ESharedDataSharingMode) override {}
    void STDCALL Clear(EClearMode, int, int) override {}
    bool STDCALL Init() override { return true; }
    IGFXMesh* STDCALL GetMesh(const char*) override { return nullptr; }
};

// ── Object factory — registers all classes ────────────────────

class CGfxVkObjectFactory : public CBasicObjectFactory
{
public:
    CGfxVkObjectFactory()
    {
        // Engine — defined in bridge.cpp
        REGISTER_CLASS(this, GFX_GFX,            GfxVkEngine);
        // Manager stubs — Phase 0 placeholders
        REGISTER_CLASS(this, GFX_TEXTURE_MANAGER, CTextureManagerStub);
        REGISTER_CLASS(this, GFX_MESH_MANAGER,    CMeshManagerStub);
        REGISTER_CLASS(this, GFX_FONT_MANAGER,    CFontManagerStub);
    }
};

static CGfxVkObjectFactory theGfxVkObjectFactory;

// ── Module checker ────────────────────────────────────────────

class CGfxVkModuleChecker : public IModuleChecker
{
public:
    int STDCALL CheckFunctionality() const override
    {
        // Phase 0 stub — always pass.
        return 0;
    }

    void STDCALL SetModuleFunctionalityLimits() const override
    {
        // Phase 0 stub — no limits.
        if (GetSingleton<IGlobalVars>() == nullptr)
            return;
    }
};

static CGfxVkModuleChecker theGfxVkModuleChecker;

// ── Module descriptor export ──────────────────────────────────

static SModuleDescriptor theModuleDescriptor(
    L"Graphics (Vulkan)",
    GFX_GFX,         // Same type ID as GFX.dll so engine finds it
    0x0100,
    &theGfxVkObjectFactory,
    &theGfxVkModuleChecker
);

extern "C" __declspec(dllexport) const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
    return &theModuleDescriptor;
}
