#ifndef BLITZKRIEG_MESH_GPU_H
#define BLITZKRIEG_MESH_GPU_H

#include "GraphicsEngineGpu.h"
#include "..//Formats//fmtMesh.h"

class MeshGpu final : public IGFXMesh
{
public:
    struct Part { CPtr<IGFXVertices> vertices; CPtr<IGFXIndices> indices; int matrix_index = 0; int priority = 0; };
    static IRefCount * STDCALL CreateNewClassInstanceInternal() { return new MeshGpu(); }
    MeshGpu();
    explicit MeshGpu( GraphicsEngineGpu *owner );
    ~MeshGpu() = default;
    bool Build( const std::vector<SMeshFormat> &meshes, const SAABBFormat &bounds );
    bool LoadAsset( const char *stream_name );
    const std::vector<Part> &Parts() const { return parts_; }
    void STDCALL AddRef( int n = 1, int m = 0x7fffffff ) override { (void)m; refs_ += n; }
    void STDCALL Release( int n = 1, int m = 0x7fffffff ) override { (void)m; refs_ -= n; if ( refs_ == 0 ) delete this; }
    bool STDCALL IsValid() const override { return refs_ >= 0 && !parts_.empty(); }
    void STDCALL SwapData( ISharedResource *other ) override;
    int STDCALL GetRefCounter() const override { return refs_; }
    const char * STDCALL GetSharedResourceName() const override { return name_.c_str(); }
    void STDCALL SetSharedResourceName( const std::string &name ) override { name_ = name; }
    static const char *GetSharedResourceExtVarName() { return "SharedResource.Mesh.Ext"; }
    void SetSharedResourceLastUsage( int ) {}
    int GetSharedResourceLastUsage() const { return 0; }
    int GetResourceConsumption() const override { return static_cast<int>( parts_.size() ); }
    bool STDCALL Load( bool = false ) override;
    void STDCALL ClearInternalContainer() override { parts_.clear(); }
    const SGFXBoundSphere & STDCALL GetBS() override { return sphere_; }
    const SGFXAABB & STDCALL GetAABB() override { return aabb_; }
    bool STDCALL IsHit( const CVec2 &, const SHMatrix * ) override { return false; }

private:
    GraphicsEngineGpu *owner_;
    std::vector<Part> parts_;
    SGFXBoundSphere sphere_{};
    SGFXAABB aabb_{};
    int refs_ = 0;
    std::string name_;
};

#endif
