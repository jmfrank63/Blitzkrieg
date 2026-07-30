#include "StdAfx.h"

#include "GraphicsEngineGpu.h"

#include "..\\GFX\\GFXObjectFactory.h"

class CGfxGpuObjectFactory : public CBasicObjectFactory
{
public:
    CGfxGpuObjectFactory() { REGISTER_CLASS( this, GFX_GFX, GraphicsEngineGpu ); }
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
