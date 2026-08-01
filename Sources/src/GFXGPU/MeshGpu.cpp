#include "StdAfx.h"

#include "MeshGpu.h"
#include "..\\GFX\\GFXHelper.h"

MeshGpu::MeshGpu() : owner_( nullptr ) {}
MeshGpu::MeshGpu( GraphicsEngineGpu *owner ) : owner_( owner ) {}

bool STDCALL MeshGpu::Load( bool )
{
    if ( !parts_.empty() ) return true;
    if ( !GetSingletonGlobal() || name_.empty() ) return false;
    return LoadAsset( name_.c_str() );
}

bool MeshGpu::LoadAsset( const char *stream_name )
{
    if ( !stream_name ) return false;
    CPtr<IDataStream> stream = GetSingleton<IDataStorage>()->OpenStream( stream_name, STREAM_ACCESS_READ );
    if ( !stream ) return false;
    std::vector<SMeshFormat> meshes;
    SAABBFormat bounds{};
    CPtr<IStructureSaver> structure = CreateStructureSaver( stream, IStructureSaver::READ );
    if ( !structure ) return false;
    CSaverAccessor saver = structure;
    saver.Add( 2, &meshes );
    saver.Add( 4, &bounds );
    return Build( meshes, bounds );
}

bool MeshGpu::Build( const std::vector<SMeshFormat> &meshes, const SAABBFormat &bounds )
{
    parts_.clear();
    aabb_.vCenter = bounds.vCenter;
    aabb_.vHalfSize = bounds.vHalfSize;
    sphere_.vCenter = bounds.vCenter;
    sphere_.fRadius = fabs( bounds.vHalfSize );
    if ( !owner_ ) owner_ = dynamic_cast<GraphicsEngineGpu *>( GetSingleton<IGFX>() );
    if ( !owner_ || meshes.empty() ) return false;
    for ( const SMeshFormat &mesh : meshes )
    {
        if ( mesh.components.empty() || mesh.indices.empty() ) { parts_.clear(); return false; }
        IGFXVertices *vertices = owner_->CreateVertices( static_cast<int>( mesh.components.size() ), SGFXVertex::format, GFXPT_TRIANGLELIST, GFXD_STATIC );
        IGFXIndices *indices = owner_->CreateIndices( static_cast<int>( mesh.indices.size() ), GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_STATIC );
        if ( !vertices || !indices ) { if ( vertices ) delete vertices; if ( indices ) delete indices; parts_.clear(); return false; }
        void *vertex_memory = vertices->Lock();
        void *index_memory = indices->Lock();
        if ( !vertex_memory || !index_memory ) { if ( vertex_memory ) vertices->Unlock(); if ( index_memory ) indices->Unlock(); delete vertices; delete indices; parts_.clear(); return false; }
        SGFXVertex *vertex_data = static_cast<SGFXVertex *>( vertex_memory );
        for ( size_t i = 0; i < mesh.components.size(); ++i )
        {
            const SMeshFormat::SVertexComponent &component = mesh.components[i];
            if ( component.geom < 0 || component.norm < 0 || component.tex < 0 || component.geom >= static_cast<int>( mesh.geoms.size() ) || component.norm >= static_cast<int>( mesh.norms.size() ) || component.tex >= static_cast<int>( mesh.texes.size() ) ) { vertices->Unlock(); indices->Unlock(); delete vertices; delete indices; parts_.clear(); return false; }
            vertex_data[i].Setup( mesh.geoms[component.geom], mesh.norms[component.norm], mesh.texes[component.tex] );
        }
        std::memcpy( index_memory, mesh.indices.data(), mesh.indices.size() * sizeof( WORD ) );
        vertices->Unlock(); indices->Unlock();
        Part part; part.vertices = vertices; part.indices = indices; part.matrix_index = static_cast<int>( parts_.size() ); part.priority = mesh.szName == "propeller" ? 1 : 0; parts_.push_back( part );
    }
    return !parts_.empty();
}

void STDCALL MeshGpu::SwapData( ISharedResource *other )
{
    MeshGpu *mesh = dynamic_cast<MeshGpu *>( other );
    if ( !mesh ) return;
    std::swap( parts_, mesh->parts_ ); std::swap( aabb_, mesh->aabb_ ); std::swap( sphere_, mesh->sphere_ );
}
