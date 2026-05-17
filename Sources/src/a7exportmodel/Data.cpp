#include "StdAfx.h"

#include "Data.h"

#include <float.h>
#include <fstream>

#include ".\MiniBall\BoundingSphere.h"
#include ".\MiniBall\Point.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NConverter
{
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	std::vector<SJoint> joints;
	std::vector<SMeshFormat> meshes;
	SSkeletonFormat skeleton;
	std::vector<SAnimationFormat> animations;
	SAABBFormat aabb;
	std::vector<SAABBFormat> aabb_as;
	std::vector<SAABBFormat> aabb_ds;
	typedef std::unordered_map<int, int> CIndexMap;
	CIndexMap mapAABB_AIndices;
	CIndexMap mapAABB_DIndices;

	SMeshFormat *pMesh;															// current mesh
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool AddAABB_A( const SAABBFormat &aabb, int nIndex )
{
	aabb_as.push_back( aabb );
	if ( mapAABB_AIndices.find(nIndex) == mapAABB_AIndices.end() ) 
	{
		mapAABB_AIndices[nIndex] = aabb_as.size() - 1;
		return true;
	}
	else
	{
		fprintf( stderr, "AABB_A%.2d exist more then one!\n", nIndex );
		return false;
	}
}
bool AddAABB_D( const SAABBFormat &aabb, int nIndex )
{
	aabb_ds.push_back( aabb );
	if ( mapAABB_DIndices.find(nIndex) == mapAABB_DIndices.end() ) 
	{
		mapAABB_DIndices[nIndex] = aabb_ds.size() - 1;
		return true;
	}
	else
	{
		fprintf( stderr, "AABB_A%.2d exist more then one!\n", nIndex );
		return false;
	}
}
inline int GetAABBIndex( CIndexMap &aabbs, int nAABB )
{
	CIndexMap::const_iterator pos = aabbs.find( nAABB );
	return pos != aabbs.end() ? pos->second : -1;
}
int GetAABB_AIndex( int nAABB ) { return GetAABBIndex( mapAABB_AIndices, nAABB ); }
int GetAABB_DIndex( int nAABB ) { return GetAABBIndex( mapAABB_DIndices, nAABB ); }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveMesh( const char *pszName )
{
	// ���������, � ��� �� � ��� ��� ������ ����
	for ( int i=0; i != meshes.size(); ++i )
	{
		if ( meshes[i].szName == pszName )
		{
			pMesh = &( meshes[i] );
			return;
		}
	}
	//
	meshes.push_back( SMeshFormat() );
	pMesh = &( meshes.back() );
	pMesh->szName = pszName;
}
void SetMeshIndex( int nIndex ) { pMesh->nIndex = nIndex; }
void AddPoint( const CVec3 &point ) { pMesh->geoms.push_back( point * fGeomScaleCoeff ); }
void AddNormale( const CVec3 &point ) { pMesh->norms.push_back( point ); }
void AddUV( const CVec2 &point ) { pMesh->texes.push_back( point ); }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CComponentEquatFunctional
{
	const SMeshFormat::SVertexComponent &c0;
public:
	explicit CComponentEquatFunctional( const SMeshFormat::SVertexComponent &_c0 ) : c0( _c0 ) {  }
	bool operator()( const SMeshFormat::SVertexComponent &c1 ) const
	{
		return ( c0.geom == c1.geom ) && ( c0.norm == c1.norm ) && ( c0.tex == c1.tex );
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddFace( int *pIndices )
{
	for ( int i=0; i != 9; i += 3 )
	{
		SMeshFormat::SVertexComponent component;
		component.geom = pIndices[i + 0];
		component.norm = pIndices[i + 1];
		component.tex = pIndices[i + 2];
		// add index and, if need, new component
		std::vector<SMeshFormat::SVertexComponent>::iterator pos = std::find_if( pMesh->components.begin(), pMesh->components.end(), 
			                                                                       CComponentEquatFunctional(component) );
		if ( pos != pMesh->components.end() )
		{
			const int nIndex = std::distance( pMesh->components.begin(), pos );
			pMesh->indices.push_back( nIndex );
		}
		else
		{
			const int nIndex = pMesh->components.size();
			pMesh->components.push_back( component );
			pMesh->indices.push_back( nIndex );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMeshesLessFunctional
{
	bool operator()( const SMeshFormat &m1, const SMeshFormat &m2 ) const { return m1.nIndex < m2.nIndex; }
};
bool SaveModel( std::string &szFileName )
{
	std::replace_if( szFileName.begin(), szFileName.end(), [](char c){ return c == '/'; }, '\\' );
	// check for extension alerady exist and add it
	int nPos = szFileName.rfind( '.' );
	if ( nPos != std::string::npos )
	{
		if ( szFileName.substr( nPos ) != ".mod" )
			szFileName += ".mod";
	}
	else
		szFileName += ".mod";
	//
	CPtr<IDataStream> pStream = CreateFileStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
	if ( pStream == 0 )
	{
		fprintf( stderr, "can't create file \"%s\" to write model\n", szFileName.c_str() );
		return false;
	}
	//
	// sort meshes by index
	std::sort( meshes.begin(), meshes.end(), SMeshesLessFunctional() );
	// ��������� ��� ������� ���� miniball � AABB
	using namespace MiniBall;
	for ( std::vector<SMeshFormat>::iterator it = meshes.begin(); it != meshes.end(); ++it )
	{
		// miniball
		{
			std::vector<Point> points( it->geoms.size() );
			for ( int i=0; i<it->geoms.size(); ++i )
			{
				points[i].x = it->geoms[i].x;
				points[i].y = it->geoms[i].y;
				points[i].z = it->geoms[i].z;
			}
			BoundingSphere bs;
			bs = Sphere::miniBall( &(points[0]), points.size() );
			if ( bs.radius == -1 )
				bs = Sphere::smallBall( &(points[0]), points.size() );
			it->bsphere.vCenter.Set( bs.center.x, bs.center.y, bs.center.z );
			it->bsphere.fRadius = bs.radius;
		}
		// AABB
		CVec3 vMin( FLT_MAX, FLT_MAX, FLT_MAX ), vMax( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		for ( std::vector<CVec3>::const_iterator it2 = it->geoms.begin(); it2 != it->geoms.end(); ++it2 )
		{
			vMin.Minimize( *it2 );
			vMax.Maximize( *it2 );
		}
		it->aabb.vCenter		= ( vMax + vMin ) / 2.0f;
		it->aabb.vHalfSize	= ( vMax - vMin ) / 2.0f;
		//
		/*
		fprintf( stderr, "mesh %s have BS {{%g, %g, %g}, %g} and AABB {{%g, %g, %g}, {%g, %g, %g}}\n",
			it->szName.c_str(), it->bsphere.vCenter.x, it->bsphere.vCenter.y, it->bsphere.vCenter.z, it->bsphere.fRadius,
			it->aabb.vCenter.x, it->aabb.vCenter.y, it->aabb.vCenter.z, 
			it->aabb.vHalfSize.x, it->aabb.vHalfSize.y, it->aabb.vHalfSize.z );
		*/
	}
	//
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSaver;
	saver.Add( 1, &skeleton );
	saver.Add( 2, &meshes );
	saver.Add( 3, &animations );
	saver.Add( 4, &aabb );
	saver.Add( 5, &aabb_as );
	saver.Add( 6, &aabb_ds );
	//
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ClearAll()
{
	joints.clear();
	meshes.clear();
	pMesh = 0;
	skeleton.nodes.clear();
	skeleton.locators.clear();
	skeleton.nTopNode = -1;
	animations.clear();
	memset( &aabb, 0, sizeof(aabb) );
	aabb_as.clear();
	aabb_ds.clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





