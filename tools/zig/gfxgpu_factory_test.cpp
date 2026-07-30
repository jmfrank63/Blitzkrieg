#include "StdAfx.h"
#include "GFX.H"

#include <windows.h>

#include <cstdio>

int main( int argc, char **argv )
{
    const char *path = argc > 1 ? argv[1] : "zig-out/bin/GFXGPU.dll";
    HMODULE module = LoadLibraryA( path );
    if ( !module )
    {
        std::fprintf( stderr, "LoadLibrary failed for %s (%lu)\n", path, GetLastError() );
        return 1;
    }

    const auto getDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( GetProcAddress( module, "GetModuleDescriptor" ) );
    if ( !getDescriptor )
    {
        std::fprintf( stderr, "GetModuleDescriptor export is missing\n" );
        FreeLibrary( module );
        return 2;
    }

    const SModuleDescriptor *descriptor = getDescriptor();
    if ( !descriptor || descriptor->nType != GFX_GFX || !descriptor->pFactory )
    {
        std::fprintf( stderr, "invalid GFXGPU module descriptor\n" );
        FreeLibrary( module );
        return 3;
    }

    IRefCount *object = descriptor->pFactory->CreateObject( GFX_GFX );
    if ( !object || !object->IsValid() )
    {
        std::fprintf( stderr, "GFX_GFX factory object was not created\n" );
        FreeLibrary( module );
        return 4;
    }

    object->Release();
    FreeLibrary( module );
    std::puts( "GFXGPU factory export and GFX_GFX object verified" );
    return 0;
}
