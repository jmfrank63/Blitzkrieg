// bk-map obj2mod and validate-obj2mod: bzmconvertor's -obj2mod and
// -validateobj2mod (deleted by D-25; git show
// 5f6434d99:Sources/src/bzmconvertor/main.cpp), the leg of the Blender
// pipeline (tools/blender) that turns its OBJ and the skeleton and
// animation sidecars into a .mod. D-16 moves the code and the sidecar formats
// over unchanged; what differs is named where it is:
//   - every message starts "bk-map: " (D-04), the legacy text after it;
//   - a face naming a vertex the OBJ does not have says so (the legacy exe
//     exited 0xDEAD without a word);
//   - "Validation passed" is printed by validate-obj2mod only, so obj2mod
//     prints its one summary line;
//   - the .mod is created through a backslash path, StreamIO's form on every
//     host (NToolHost::EnginePath).
// The .mod is chunks 1..6 of the engine's structure saver, written with the
// engine's own SSkeletonFormat, SMeshFormat and SAnimationFormat savers.
#include "StdAfx.h"

#include <cfloat>
#include <fstream>
#include <map>
#include <sstream>

#include "bk_l10n.h"
#include "bk_map_obj2mod.h"
#include "tool_host.h"
#include "../../Sources/src/Formats/fmtMesh.h"

// A failure as "bk-map: <reason>": the prefix is English in every language, the reason comes from
// the string table with {0}.. filled from the arguments.
static std::string Str( const std::string &sz ) { return sz; }
static std::string Str( const char *psz ) { return psz; }
static std::string Str( int n ) { return std::to_string( n ); }
static std::string Str( unsigned n ) { return std::to_string( n ); }
static void Say( const char *pszKey, std::initializer_list<std::string> args )
{
	fprintf( stderr, "bk-map: %s\n", BkFormat( pszKey, args ).c_str() );
}

// The legacy command line's settings that the moved functions read.
namespace NParams
{
	static bool bValidateObjToMod = false;
	static std::string szSkeletonInput;
	static std::string szAnimationInput;
};

namespace
{
	struct SObjIndex
	{
		int v;
		int vt;
		int vn;
		bool operator<( const SObjIndex &rhs ) const
		{
			if ( v != rhs.v ) return v < rhs.v;
			if ( vt != rhs.vt ) return vt < rhs.vt;
			return vn < rhs.vn;
		}
	};

	static bool ParseObjFaceVertex( const std::string &token, SObjIndex *pOut )
	{
		if ( pOut == 0 )
			return false;

		pOut->v = -1;
		pOut->vt = -1;
		pOut->vn = -1;

		const size_t s0 = token.find('/');
		if ( s0 == std::string::npos )
		{
			if ( token.empty() )
				return false;
			pOut->v = NStr::ToInt( token.c_str() ) - 1;
			return pOut->v >= 0;
		}

		const size_t s1 = token.find('/', s0 + 1);
		const std::string vStr = token.substr( 0, s0 );
		if ( vStr.empty() )
			return false;
		pOut->v = NStr::ToInt( vStr.c_str() ) - 1;

		if ( s1 == std::string::npos )
		{
			const std::string vtStr = token.substr( s0 + 1 );
			if ( !vtStr.empty() )
				pOut->vt = NStr::ToInt( vtStr.c_str() ) - 1;
		}
		else
		{
			const std::string vtStr = token.substr( s0 + 1, s1 - s0 - 1 );
			const std::string vnStr = token.substr( s1 + 1 );
			if ( !vtStr.empty() )
				pOut->vt = NStr::ToInt( vtStr.c_str() ) - 1;
			if ( !vnStr.empty() )
				pOut->vn = NStr::ToInt( vnStr.c_str() ) - 1;
		}

		return pOut->v >= 0;
	}

	static bool LoadSkeletonFromTxt( const std::string &szSkeletonInput, SSkeletonFormat *pSkeleton )
	{
		if ( pSkeleton == 0 )
			return false;

		std::ifstream ifs( szSkeletonInput.c_str() );
		if ( !ifs.is_open() )
		{
			Say( "obj2mod.err.skeleton_open", { Str( szSkeletonInput.c_str() ) } );
			return false;
		}

		struct SNodeRaw
		{
			int index;
			int parent;
			std::string name;
			CVec3 bone;
			CVec4 quat;
			int locator;
		};

		std::vector<SNodeRaw> nodes;
		std::string line;
		while ( std::getline( ifs, line ) )
		{
			if ( line.empty() || line[0] == '#' )
				continue;
			std::istringstream iss( line );
			std::string tag;
			iss >> tag;
			if ( tag != "node" )
				continue;

			SNodeRaw raw;
			raw.index = -1;
			raw.parent = -1;
			raw.name = "Node";
			raw.bone = VNULL3;
			raw.quat.Set( 0.0f, 0.0f, 0.0f, 1.0f );
			raw.locator = 0;
			iss >> raw.index >> raw.parent >> raw.name
				>> raw.bone.x >> raw.bone.y >> raw.bone.z
				>> raw.quat.x >> raw.quat.y >> raw.quat.z >> raw.quat.w
				>> raw.locator;
			if ( raw.index >= 0 )
				nodes.push_back( raw );
		}

		if ( nodes.empty() )
			return false;

		int nMaxIndex = -1;
		for ( std::vector<SNodeRaw>::const_iterator it = nodes.begin(); it != nodes.end(); ++it )
			nMaxIndex = Max( nMaxIndex, it->index );

		pSkeleton->nodes.clear();
		pSkeleton->locators.clear();
		pSkeleton->nodes.resize( nMaxIndex + 1 );
		pSkeleton->nTopNode = 0;

		for ( std::vector<SNodeRaw>::const_iterator it = nodes.begin(); it != nodes.end(); ++it )
		{
			SSkeletonFormat::SNodeFormat &dst = pSkeleton->nodes[it->index];
			dst.nIndex = it->index;
			dst.szName = it->name;
			dst.bone = it->bone;
			dst.quat = it->quat;
			if ( it->locator != 0 )
				pSkeleton->locators.push_back( it->index );
			if ( it->parent < 0 )
				pSkeleton->nTopNode = it->index;
		}

		for ( std::vector<SNodeRaw>::const_iterator it = nodes.begin(); it != nodes.end(); ++it )
		{
			if ( it->parent >= 0 && it->parent < (int)pSkeleton->nodes.size() )
				pSkeleton->nodes[it->parent].children.push_back( it->index );
		}

		for ( SSkeletonFormat::CNodesList::iterator it = pSkeleton->nodes.begin(); it != pSkeleton->nodes.end(); ++it )
		{
			std::sort( it->children.begin(), it->children.end() );
			it->children.erase( std::unique(it->children.begin(), it->children.end()), it->children.end() );
		}
		std::sort( pSkeleton->locators.begin(), pSkeleton->locators.end() );
		pSkeleton->locators.erase( std::unique(pSkeleton->locators.begin(), pSkeleton->locators.end()), pSkeleton->locators.end() );

		return true;
	}

	static bool LoadAnimationsFromTxt( const std::string &szAnimationInput, const SSkeletonFormat &skeleton,
		std::vector<SAnimationFormat> *pAnimations )
	{
		if ( pAnimations == 0 )
			return false;

		std::ifstream ifs( szAnimationInput.c_str() );
		if ( !ifs.is_open() )
		{
			Say( "obj2mod.err.animation_open", { Str( szAnimationInput.c_str() ) } );
			return false;
		}

		SAnimationFormat *pCurrent = 0;
		std::string line;
		while ( std::getline( ifs, line ) )
		{
			if ( line.empty() || line[0] == '#' )
				continue;

			std::istringstream iss( line );
			std::string tag;
			iss >> tag;
			if ( tag == "anim" )
			{
				std::string name;
				int nKeys = 0;
				int nAction = 0;
				int nAABB_A = -1;
				int nAABB_D = -1;
				iss >> name >> nKeys >> nAction >> nAABB_A >> nAABB_D;
				if ( nKeys <= 0 )
					continue;

				pAnimations->push_back( SAnimationFormat() );
				pCurrent = &( pAnimations->back() );
				pCurrent->szName = name;
				pCurrent->nAction = nAction;
				pCurrent->nAABB_AIndex = nAABB_A;
				pCurrent->nAABB_DIndex = nAABB_D;
				pCurrent->nodes.SetSizes( nKeys, skeleton.GetNumNodes() );

				for ( int n = 0; n < skeleton.GetNumNodes(); ++n )
				{
					for ( int k = 0; k < nKeys; ++k )
					{
						pCurrent->nodes[n][k].vPos = VNULL3;
						pCurrent->nodes[n][k].vRot.Set( 0.0f, 0.0f, 0.0f, 1.0f );
					}
				}
			}
			else if ( tag == "key" && pCurrent != 0 )
			{
				int nNode = 0;
				int nKey = 0;
				SAnimNodeFormat node;
				node.vPos = VNULL3;
				node.vRot.Set( 0.0f, 0.0f, 0.0f, 1.0f );
				iss >> nNode >> nKey
					>> node.vPos.x >> node.vPos.y >> node.vPos.z
					>> node.vRot.x >> node.vRot.y >> node.vRot.z >> node.vRot.w;

				if ( nNode < 0 || nNode >= skeleton.GetNumNodes() )
					continue;
				if ( nKey < 0 || nKey >= pCurrent->nodes.GetSizeX() )
					continue;
				pCurrent->nodes[nNode][nKey] = node;
			}
		}

		return !pAnimations->empty();
	}

	static bool ValidateSkeleton( const SSkeletonFormat &skeleton )
	{
		if ( skeleton.nodes.empty() )
		{
			Say( "obj2mod.err.no_nodes", {} );
			return false;
		}
		if ( skeleton.nTopNode < 0 || skeleton.nTopNode >= (int)skeleton.nodes.size() )
		{
			Say( "obj2mod.err.top_range", { Str( skeleton.nTopNode ), Str( (int)skeleton.nodes.size() ) } );
			return false;
		}

		std::vector<int> nParents( skeleton.nodes.size(), -1 );
		for ( int i = 0; i < (int)skeleton.nodes.size(); ++i )
		{
			const SSkeletonFormat::SNodeFormat &node = skeleton.nodes[i];
			if ( node.nIndex != i )
			{
				Say( "obj2mod.err.node_slot", { Str( i ), Str( node.nIndex ) } );
				return false;
			}
			if ( node.szName.empty() )
			{
				Say( "obj2mod.err.node_name", { Str( i ) } );
				return false;
			}

			for ( SSkeletonFormat::SNodeFormat::CChildrenList::const_iterator child = node.children.begin();
				child != node.children.end(); ++child )
			{
				if ( *child < 0 || *child >= (int)skeleton.nodes.size() )
				{
					Say( "obj2mod.err.child_range", { Str( i ), Str( *child ) } );
					return false;
				}
				if ( nParents[*child] != -1 )
				{
					Say( "obj2mod.err.parents", { Str( *child ), Str( nParents[*child] ), Str( i ) } );
					return false;
				}
				nParents[*child] = i;
			}
		}

		int nRoots = 0;
		for ( int i = 0; i < (int)nParents.size(); ++i )
			nRoots += ( nParents[i] == -1 );
		if ( nRoots != 1 )
		{
			Say( "obj2mod.err.roots", { Str( nRoots ) } );
			return false;
		}
		if ( nParents[skeleton.nTopNode] != -1 )
		{
			Say( "obj2mod.err.top_parent", { Str( skeleton.nTopNode ), Str( nParents[skeleton.nTopNode] ) } );
			return false;
		}

		std::vector<int> nState( skeleton.nodes.size(), 0 ); // 0=unseen,1=visiting,2=done
		std::vector<int> stack;
		stack.push_back( skeleton.nTopNode );
		while ( !stack.empty() )
		{
			const int n = stack.back();
			if ( nState[n] == 0 )
			{
				nState[n] = 1;
				const SSkeletonFormat::SNodeFormat &node = skeleton.nodes[n];
				for ( SSkeletonFormat::SNodeFormat::CChildrenList::const_iterator child = node.children.begin();
					child != node.children.end(); ++child )
				{
					if ( nState[*child] == 1 )
					{
						Say( "obj2mod.err.cycle", { Str( n ), Str( *child ) } );
						return false;
					}
					if ( nState[*child] == 0 )
						stack.push_back( *child );
				}
			}
			else
			{
				nState[n] = 2;
				stack.pop_back();
			}
		}

		for ( int i = 0; i < (int)nState.size(); ++i )
		{
			if ( nState[i] == 0 )
			{
				Say( "obj2mod.err.unreachable", { Str( i ), Str( skeleton.nTopNode ) } );
				return false;
			}
		}

		for ( std::vector<int>::const_iterator it = skeleton.locators.begin(); it != skeleton.locators.end(); ++it )
		{
			if ( *it < 0 || *it >= (int)skeleton.nodes.size() )
			{
				Say( "obj2mod.err.locator_range", { Str( *it ), Str( (int)skeleton.nodes.size() ) } );
				return false;
			}
		}

		return true;
	}

	static bool ValidateAnimations( const std::vector<SAnimationFormat> &animations, const SSkeletonFormat &skeleton )
	{
		for ( int i = 0; i < (int)animations.size(); ++i )
		{
			const SAnimationFormat &anim = animations[i];
			if ( anim.szName.empty() )
			{
				Say( "obj2mod.err.anim_name", { Str( i ) } );
				return false;
			}
			if ( anim.nodes.GetSizeY() != skeleton.GetNumNodes() )
			{
				Say( "obj2mod.err.anim_rows", { Str( anim.szName.c_str() ), Str( anim.nodes.GetSizeY() ), Str( skeleton.GetNumNodes() ) } );
				return false;
			}
			if ( anim.nodes.GetSizeX() <= 0 )
			{
				Say( "obj2mod.err.anim_keys", { Str( anim.szName.c_str() ) } );
				return false;
			}
			if ( anim.nAction < 0 || anim.nAction >= anim.nodes.GetSizeX() )
			{
				Say( "obj2mod.err.anim_action", { Str( anim.szName.c_str() ), Str( anim.nAction ), Str( anim.nodes.GetSizeX() ) } );
				return false;
			}
		}
		return true;
	}

	static bool ValidateObjMesh( const SMeshFormat &mesh )
	{
		if ( mesh.components.empty() )
		{
			Say( "obj2mod.err.no_components", {} );
			return false;
		}
		if ( mesh.indices.empty() || ( mesh.indices.size() % 3 ) != 0 )
		{
			Say( "obj2mod.err.index_buffer", { Str( (int)mesh.indices.size() ) } );
			return false;
		}
		for ( int i = 0; i < (int)mesh.indices.size(); ++i )
		{
			if ( mesh.indices[i] >= mesh.components.size() )
			{
				Say( "obj2mod.err.index_component", { Str( i ), Str( (unsigned)mesh.indices[i] ), Str( (int)mesh.components.size() ) } );
				return false;
			}
		}
		return true;
	}

	static bool ValidateObjToModData( const SMeshFormat &mesh, const SSkeletonFormat &skeleton, const std::vector<SAnimationFormat> &animations )
	{
		if ( !ValidateObjMesh(mesh) )
			return false;
		if ( !ValidateSkeleton(skeleton) )
			return false;
		if ( !ValidateAnimations(animations, skeleton) )
			return false;

		if ( NParams::bValidateObjToMod )
			printf( "Validation passed: mesh=%d verts, skeleton=%d nodes, animations=%d\n",
			(int)mesh.geoms.size(), skeleton.GetNumNodes(), (int)animations.size() );
		return true;
	}

	static bool ConvertObjToMod( const std::string &szObjInput, const std::string &szModOutput )
	{
		std::ifstream ifs( szObjInput.c_str() );
		if ( !ifs.is_open() )
		{
			Say( "obj2mod.err.obj_open", { Str( szObjInput.c_str() ) } );
			return false;
		}

		std::vector<CVec3> srcVerts;
		std::vector<CVec3> srcNorms;
		std::vector<CVec2> srcUVs;

		SMeshFormat mesh;
		mesh.nIndex = 0;
		mesh.szName = "BlenderMesh";

		std::map<SObjIndex, int> componentMap;

		std::string line;
		while ( std::getline( ifs, line ) )
		{
			if ( line.empty() || line[0] == '#' )
				continue;

			std::istringstream iss( line );
			std::string tag;
			iss >> tag;
			if ( tag == "v" )
			{
				float x = 0.0f, y = 0.0f, z = 0.0f;
				iss >> x >> y >> z;
				srcVerts.push_back( CVec3(x, y, z) );
			}
			else if ( tag == "vn" )
			{
				float x = 0.0f, y = 0.0f, z = 1.0f;
				iss >> x >> y >> z;
				srcNorms.push_back( CVec3(x, y, z) );
			}
			else if ( tag == "vt" )
			{
				float u = 0.0f, v = 0.0f;
				iss >> u >> v;
				srcUVs.push_back( CVec2(u, 1.0f - v) );
			}
			else if ( tag == "f" )
			{
				std::vector<SObjIndex> face;
				std::string token;
				while ( iss >> token )
				{
					SObjIndex idx;
					if ( ParseObjFaceVertex(token, &idx) )
						face.push_back( idx );
				}

				if ( face.size() < 3 )
					continue;

				for ( size_t tri = 1; tri + 1 < face.size(); ++tri )
				{
					const SObjIndex triIdx[3] = { face[0], face[tri], face[tri + 1] };
					for ( int k = 0; k < 3; ++k )
					{
						const SObjIndex &idx = triIdx[k];
						if ( idx.v < 0 || idx.v >= (int)srcVerts.size() )
						{
							Say( "obj2mod.err.face_vertex", { Str( idx.v + 1 ), Str( (int)srcVerts.size() ), Str( szObjInput.c_str() ) } );
							return false;
						}

						std::map<SObjIndex, int>::const_iterator found = componentMap.find( idx );
						if ( found != componentMap.end() )
						{
							mesh.indices.push_back( (WORD)found->second );
							continue;
						}

						SMeshFormat::SVertexComponent c;
						c.geom = (int)mesh.geoms.size();
						c.norm = (int)mesh.norms.size();
						c.tex = (int)mesh.texes.size();

						mesh.geoms.push_back( srcVerts[idx.v] );

						if ( idx.vn >= 0 && idx.vn < (int)srcNorms.size() )
							mesh.norms.push_back( srcNorms[idx.vn] );
						else
							mesh.norms.push_back( V3_AXIS_Z );

						if ( idx.vt >= 0 && idx.vt < (int)srcUVs.size() )
							mesh.texes.push_back( srcUVs[idx.vt] );
						else
							mesh.texes.push_back( CVec2(0.0f, 0.0f) );

						const int componentIndex = (int)mesh.components.size();
						mesh.components.push_back( c );
						componentMap[idx] = componentIndex;
						mesh.indices.push_back( (WORD)componentIndex );
					}
				}
			}
		}

		if ( mesh.components.empty() )
		{
			Say( "obj2mod.err.no_faces", { Str( szObjInput.c_str() ) } );
			return false;
		}

		CVec3 vMin( FLT_MAX, FLT_MAX, FLT_MAX );
		CVec3 vMax( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		for ( std::vector<CVec3>::const_iterator it = mesh.geoms.begin(); it != mesh.geoms.end(); ++it )
		{
			vMin.Minimize( *it );
			vMax.Maximize( *it );
		}
		mesh.aabb.vCenter = (vMin + vMax) / 2.0f;
		mesh.aabb.vHalfSize = (vMax - vMin) / 2.0f;
		mesh.bsphere.vCenter = mesh.aabb.vCenter;
		mesh.bsphere.fRadius = 0.0f;
		for ( std::vector<CVec3>::const_iterator it = mesh.geoms.begin(); it != mesh.geoms.end(); ++it )
		{
			const CVec3 vDist = (*it - mesh.bsphere.vCenter);
			mesh.bsphere.fRadius = Max( mesh.bsphere.fRadius, fabs(vDist) );
		}

		SSkeletonFormat skeleton;
		if ( NParams::szSkeletonInput.empty() || !LoadSkeletonFromTxt(NParams::szSkeletonInput, &skeleton) )
		{
			skeleton.nTopNode = 0;
			skeleton.nodes.resize( 1 );
			skeleton.nodes[0].szName = "Root";
			skeleton.nodes[0].nIndex = 0;
			skeleton.nodes[0].bone = VNULL3;
			skeleton.nodes[0].quat.Set( 0.0f, 0.0f, 0.0f, 1.0f );
		}

		std::vector<SMeshFormat> meshes;
		meshes.push_back( mesh );
		std::vector<SAnimationFormat> animations;
		if ( !NParams::szAnimationInput.empty() )
			LoadAnimationsFromTxt( NParams::szAnimationInput, skeleton, &animations );

		if ( !ValidateObjToModData(mesh, skeleton, animations) )
			return false;

		if ( NParams::bValidateObjToMod )
			return true;

		SAABBFormat modelAabb = mesh.aabb;
		std::vector<SAABBFormat> aabbAs;
		std::vector<SAABBFormat> aabbDs;

		std::string outPath = szModOutput;
		const size_t extPos = outPath.rfind( '.' );
		std::string szExt = ( extPos == std::string::npos ) ? std::string() : outPath.substr( extPos );
		NStr::ToLower( szExt );
		if ( szExt != ".mod" )
			outPath += ".mod";

		CPtr<IDataStream> pStream = CreateFileStream( NToolHost::EnginePath( outPath ).c_str(), STREAM_ACCESS_WRITE );
		if ( pStream == 0 )
		{
			Say( "obj2mod.err.mod_create", { Str( outPath.c_str() ) } );
			return false;
		}

		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &skeleton );
		saver.Add( 2, &meshes );
		saver.Add( 3, &animations );
		saver.Add( 4, &modelAabb );
		saver.Add( 5, &aabbAs );
		saver.Add( 6, &aabbDs );

		printf( "Converted OBJ to MOD: %s -> %s\n", szObjInput.c_str(), outPath.c_str() );
		return true;
	}
}

namespace NObjToMod
{
bool Run( const std::string &szObjInput, const std::string &szModOutput, const std::string &szSkeletonInput,
	const std::string &szAnimationInput, bool bValidateOnly )
{
	NParams::bValidateObjToMod = bValidateOnly;
	NParams::szSkeletonInput = szSkeletonInput;
	NParams::szAnimationInput = szAnimationInput;
	return ConvertObjToMod( szObjInput, szModOutput );
}
}
