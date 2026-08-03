#include "StdAfx.h"

#include "GeometryMesh.h"
#include "../Formats/fmtMesh.h"
SSingleMesh::SSingleMesh( IGFXVertices *_pVertices, IGFXIndices *_pIndices, const SGFXAABB &_aabb, const SGFXBoundSphere &_sphere )
{
	pVertices = _pVertices;
	pIndices = _pIndices;
	aabb = _aabb;
	sphere = _sphere;
	nMatrixIndex = -1;
	nPriority = 0;
}
int CGeometryMesh::operator&( IStructureSaver &ss )
{
	return 0;
}
void CGeometryMesh::SetAABB( const CVec3 &vCenter, const CVec3 &vHalfSize ) 
{ 
	aabb.vCenter = vCenter;
	aabb.vHalfSize = vHalfSize;
	sphere.vCenter = vCenter;
	sphere.fRadius = fabs( vHalfSize );
}
bool CGeometryMesh::IsHit( const CVec2 &vPos, const SHMatrix *matrices )
{
	CVec3 vRes;
	int i = 0;
	for ( CMeshesList::const_iterator it = figures.begin(); it != figures.end(); ++it, ++i )
	{
		matrices[i].RotateHVector( &vRes, it->sphere.vCenter );
		if ( fabs2(vRes.x - vPos.x, vRes.y - vPos.y) <= fabs2(it->sphere.fRadius) )
			return true;
	}

	return false;
}
struct SSingleMeshLessFunctional 
{
	bool operator()( const SSingleMesh &m1, const SSingleMesh &m2 ) const { return m1.nPriority < m2.nPriority; }
};
void CGeometryMesh::SortMeshes() 
{ 
	std::sort( figures.begin(), figures.end(), SSingleMeshLessFunctional() ); 
}
bool CGeometryMesh::Load( const bool bPreLoad )
{
	Clear();
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	std::vector<SMeshFormat> meshes;
	SAABBFormat aabb;
	{
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSS;
		saver.Add( 2, &meshes );
		saver.Add( 4, &aabb );
	}
	int nTotalNumVertices = 0;
	int nTotalNumIndices = 0;
	for ( std::vector<SMeshFormat>::const_iterator it = meshes.begin(); it != meshes.end(); ++it )
	{
		nTotalNumVertices += it->components.size();
		nTotalNumIndices += it->indices.size();
	}
	IGFX *pGFX = GetSingleton<IGFX>();
	pGFX->BeginSolidVertexBlock( nTotalNumVertices, SGFXVertex::format, GFXD_STATIC );
	pGFX->BeginSolidIndexBlock( nTotalNumIndices, GFXIF_INDEX16, GFXD_STATIC );
	for ( std::vector<SMeshFormat>::iterator it = meshes.begin(); it != meshes.end(); ++it )
	{
		const SMeshFormat &mesh = *it;
		const int nNumVertices = mesh.components.size();
		CPtr<IGFXVertices> pVertices = pGFX->CreateVertices( nNumVertices, SGFXVertex::format, GFXPT_TRIANGLELIST, GFXD_STATIC );
		{
			CVerticesLock<SGFXVertex> verts( pVertices );
			for ( int i=0; i<nNumVertices; ++i )
			{
				const SMeshFormat::SVertexComponent &component = mesh.components[i];
				verts[i].pos = mesh.geoms[component.geom];
				verts[i].norm = mesh.norms[component.norm];
				verts[i].tex = mesh.texes[component.tex];
			}
		}
		const int nNumIndices = mesh.indices.size();
		CPtr<IGFXIndices> pIndices = pGFX->CreateIndices( nNumIndices, GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_STATIC );
		{
			CIndicesLock<WORD> inds( pIndices );
			memcpy( inds.GetBuffer(), &( mesh.indices[0] ), mesh.indices.size()*sizeof(WORD) );
		}
		{
			SGFXAABB _aabb;
			_aabb.vCenter = aabb.vCenter;
			_aabb.vHalfSize = aabb.vHalfSize;
			SGFXBoundSphere _sphere;
			_sphere.vCenter = mesh.bsphere.vCenter;
			_sphere.fRadius = mesh.bsphere.fRadius;
			NStr::ToLower( it->szName );
			AddSingleMesh( SSingleMesh(pVertices, pIndices, _aabb, _sphere), it->szName == "propeller" ? 1 : 0 );
		}
		SetAABB( aabb.vCenter, aabb.vHalfSize );
	}
	SortMeshes();
	pGFX->EndSolidVertexBlock();
	pGFX->EndSolidIndexBlock();
	return true;
}
