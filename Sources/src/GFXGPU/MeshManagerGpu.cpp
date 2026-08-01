#include "StdAfx.h"

#include "MeshManagerGpu.h"

bool STDCALL MeshManagerGpu::Init()
{
    return true;
}

void STDCALL MeshManagerGpu::Clear( const EClearMode mode, const int usage, const int amount )
{
    if ( mode == CLEAR_ALL ) share_.Clear();
    else if ( mode == CLEAL_UNREFERENCED ) share_.ClearUnreferencedResources();
    else if ( mode == CLEAR_LRU ) share_.ClearLRUResources( usage, amount );
}

IGFXMesh * STDCALL MeshManagerGpu::GetMesh( const char *name )
{
    return name ? share_.Get( name ) : nullptr;
}

int STDCALL MeshManagerGpu::operator&( IStructureSaver &saver )
{
    share_.Serialize( &saver );
    return 0;
}
