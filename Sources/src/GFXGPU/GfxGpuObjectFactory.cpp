#include "StdAfx.h"

#include "GraphicsEngineGpu.h"
#include "MeshManagerGpu.h"

#include "..\\GFX\\GFXObjectFactory.h"

class TextureManagerGpu final : public ITextureManager
{
public:
    OBJECT_COMPLETE_METHODS( TextureManagerGpu );
    bool STDCALL Init() override { return true; }
    void STDCALL SetSerialMode( ESharedDataSerialMode ) override {}
    void STDCALL SetShareMode( ESharedDataSharingMode ) override {}
    void STDCALL Clear( EClearMode, int, int ) override {}
    IGFXTexture * STDCALL GetTexture( const char * ) override { return nullptr; }
    const char * STDCALL GetTextureName( IGFXTexture * ) override { return "default"; }
    void STDCALL SetQuality( ETextureQuality ) override {}
};

class FontManagerGpu final : public IFontManager
{
public:
    OBJECT_COMPLETE_METHODS( FontManagerGpu );
    bool STDCALL Init() override { return true; }
    void STDCALL SetSerialMode( ESharedDataSerialMode ) override {}
    void STDCALL SetShareMode( ESharedDataSharingMode ) override {}
    void STDCALL Clear( EClearMode, int, int ) override {}
    IGFXFont * STDCALL GetFont( const char * ) override { return nullptr; }
};

class CGfxGpuObjectFactory : public CBasicObjectFactory
{
public:
    CGfxGpuObjectFactory()
    {
        REGISTER_CLASS( this, GFX_GFX, GraphicsEngineGpu );
        REGISTER_CLASS( this, GFX_TEXTURE_MANAGER, TextureManagerGpu );
        REGISTER_CLASS( this, GFX_MESH_MANAGER, MeshManagerGpu );
        REGISTER_CLASS( this, GFX_FONT_MANAGER, FontManagerGpu );
        REGISTER_CLASS( this, GFX_MESH, MeshGpu );
    }
};

class CGfxGpuModuleChecker : public IModuleChecker
{
public:
    int STDCALL CheckFunctionality() const override { return 0; }
    void STDCALL SetModuleFunctionalityLimits() const override {}
};

static CGfxGpuObjectFactory theGfxGpuObjectFactory;
static CGfxGpuModuleChecker theGfxGpuModuleChecker;
static SModuleDescriptor theModuleDescriptor( "Graphics (SDL GPU)", GFX_GFX, 0x0100, &theGfxGpuObjectFactory, &theGfxGpuModuleChecker );

extern "C" __declspec( dllexport ) const SModuleDescriptor * STDCALL GetModuleDescriptor()
{
    return &theModuleDescriptor;
}
