#ifndef BLITZKRIEG_MESH_MANAGER_GPU_H
#define BLITZKRIEG_MESH_MANAGER_GPU_H

#include "MeshGpu.h"
#include "..\\Misc\\BasicShare.h"

BASIC_SHARE_DECLARE( MeshGpuShare, std::string, MeshGpu, GFX_MESH, 105, "" );

class MeshManagerGpu final : public IMeshManager
{
public:
    OBJECT_COMPLETE_METHODS( MeshManagerGpu );
    DECLARE_SERIALIZE;

    bool STDCALL Init() override;
    void STDCALL SetSerialMode( ESharedDataSerialMode mode ) override { share_.SetSerialMode( mode ); }
    void STDCALL SetShareMode( ESharedDataSharingMode mode ) override { share_.SetShareMode( mode ); }
    void STDCALL Clear( EClearMode mode = CLEAR_ALL, int usage = 0, int amount = 0 ) override;
    IGFXMesh * STDCALL GetMesh( const char *name ) override;

private:
    MeshGpuShare share_;
};

#endif
