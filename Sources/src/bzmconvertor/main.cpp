#include "StdAfx.h"

#include "../RandomMapGen/MapInfo_Types.h"
#include "../Misc/FileUtils.h"
#include "../Formats/fmtMesh.h"
#include <fstream>
#include <sstream>
namespace NParams
{
	static std::string szMapName;
	static std::string szStartDir;
	static std::string szDestDir;
	static bool bConvertFences = false;
	static bool bConvertBZM = true;
	static bool bConvertObjToMod = false;
	static bool bValidateObjToMod = false;
	static std::string szObjInput;
	static std::string szModOutput;
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
			fprintf( stderr, "Can't open skeleton file: %s\n", szSkeletonInput.c_str() );
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
			fprintf( stderr, "Can't open animation file: %s\n", szAnimationInput.c_str() );
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
			fprintf( stderr, "Validation error: skeleton has no nodes\n" );
			return false;
		}
		if ( skeleton.nTopNode < 0 || skeleton.nTopNode >= (int)skeleton.nodes.size() )
		{
			fprintf( stderr, "Validation error: top node index %d is out of range [0, %d)\n",
				skeleton.nTopNode, (int)skeleton.nodes.size() );
			return false;
		}

		std::vector<int> nParents( skeleton.nodes.size(), -1 );
		for ( int i = 0; i < (int)skeleton.nodes.size(); ++i )
		{
			const SSkeletonFormat::SNodeFormat &node = skeleton.nodes[i];
			if ( node.nIndex != i )
			{
				fprintf( stderr, "Validation error: node slot %d has nIndex=%d\n", i, node.nIndex );
				return false;
			}
			if ( node.szName.empty() )
			{
				fprintf( stderr, "Validation error: node %d has empty name\n", i );
				return false;
			}

			for ( SSkeletonFormat::SNodeFormat::CChildrenList::const_iterator child = node.children.begin();
				child != node.children.end(); ++child )
			{
				if ( *child < 0 || *child >= (int)skeleton.nodes.size() )
				{
					fprintf( stderr, "Validation error: node %d has out-of-range child %d\n", i, *child );
					return false;
				}
				if ( nParents[*child] != -1 )
				{
					fprintf( stderr, "Validation error: node %d has multiple parents (%d, %d)\n",
						*child, nParents[*child], i );
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
			fprintf( stderr, "Validation error: skeleton graph must have exactly 1 root, found %d\n", nRoots );
			return false;
		}
		if ( nParents[skeleton.nTopNode] != -1 )
		{
			fprintf( stderr, "Validation error: declared top node %d has parent %d\n", skeleton.nTopNode, nParents[skeleton.nTopNode] );
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
						fprintf( stderr, "Validation error: cycle detected at node %d -> %d\n", n, *child );
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
				fprintf( stderr, "Validation error: node %d is not reachable from top node %d\n", i, skeleton.nTopNode );
				return false;
			}
		}

		for ( std::vector<int>::const_iterator it = skeleton.locators.begin(); it != skeleton.locators.end(); ++it )
		{
			if ( *it < 0 || *it >= (int)skeleton.nodes.size() )
			{
				fprintf( stderr, "Validation error: locator index %d is out of range [0, %d)\n",
					*it, (int)skeleton.nodes.size() );
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
				fprintf( stderr, "Validation error: animation #%d has empty name\n", i );
				return false;
			}
			if ( anim.nodes.GetSizeY() != skeleton.GetNumNodes() )
			{
				fprintf( stderr, "Validation error: animation '%s' has %d node rows but skeleton has %d nodes\n",
					anim.szName.c_str(), anim.nodes.GetSizeY(), skeleton.GetNumNodes() );
				return false;
			}
			if ( anim.nodes.GetSizeX() <= 0 )
			{
				fprintf( stderr, "Validation error: animation '%s' has no keys\n", anim.szName.c_str() );
				return false;
			}
			if ( anim.nAction < 0 || anim.nAction >= anim.nodes.GetSizeX() )
			{
				fprintf( stderr, "Validation error: animation '%s' action key %d is out of range [0, %d)\n",
					anim.szName.c_str(), anim.nAction, anim.nodes.GetSizeX() );
				return false;
			}
		}
		return true;
	}

	static bool ValidateObjMesh( const SMeshFormat &mesh )
	{
		if ( mesh.components.empty() )
		{
			fprintf( stderr, "Validation error: OBJ generated no mesh components\n" );
			return false;
		}
		if ( mesh.indices.empty() || ( mesh.indices.size() % 3 ) != 0 )
		{
			fprintf( stderr, "Validation error: mesh index buffer must be non-empty triangles (size=%d)\n", (int)mesh.indices.size() );
			return false;
		}
		for ( int i = 0; i < (int)mesh.indices.size(); ++i )
		{
			if ( mesh.indices[i] >= mesh.components.size() )
			{
				fprintf( stderr, "Validation error: mesh index %d references component %u out of range [0, %d)\n",
					i, (unsigned)mesh.indices[i], (int)mesh.components.size() );
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

		printf( "Validation passed: mesh=%d verts, skeleton=%d nodes, animations=%d\n",
			(int)mesh.geoms.size(), skeleton.GetNumNodes(), (int)animations.size() );
		return true;
	}

	static bool ConvertObjToMod( const std::string &szObjInput, const std::string &szModOutput )
	{
		std::ifstream ifs( szObjInput.c_str() );
		if ( !ifs.is_open() )
		{
			fprintf( stderr, "Can't open OBJ file: %s\n", szObjInput.c_str() );
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
							return false;

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
			fprintf( stderr, "OBJ has no mesh faces: %s\n", szObjInput.c_str() );
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

		CPtr<IDataStream> pStream = CreateFileStream( outPath.c_str(), STREAM_ACCESS_WRITE );
		if ( pStream == 0 )
		{
			fprintf( stderr, "Can't create output MOD: %s\n", outPath.c_str() );
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

void ProcessCommandLine( int argc, char *argv[] )
{
	std::vector<std::string> strings;
	strings.reserve( argc - 1 );
	for ( int i = 1; i < argc; ++i )
		strings.push_back( argv[i] );

	if ( strings.size() >= 3 && strings[0] == "-obj2mod" )
	{
		NParams::bConvertObjToMod = true;
		NParams::szObjInput = strings[1];
		NParams::szModOutput = strings[2];
		if ( strings.size() >= 4 )
			NParams::szSkeletonInput = strings[3];
		if ( strings.size() >= 5 )
			NParams::szAnimationInput = strings[4];
		return;
	}

	if ( strings.size() >= 2 && strings[0] == "-validateobj2mod" )
	{
		NParams::bConvertObjToMod = true;
		NParams::bValidateObjToMod = true;
		NParams::szObjInput = strings[1];
		NParams::szModOutput = ".\\_validation_only.mod";
		if ( strings.size() >= 3 )
			NParams::szSkeletonInput = strings[2];
		if ( strings.size() >= 4 )
			NParams::szAnimationInput = strings[3];
		return;
	}

	for ( std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); ++it )
	{
		if ( (it->size() > 4) && (it->find(".xml") == it->size() - 4) )
		{
			NParams::szMapName = *it;
			NParams::szMapName = NParams::szMapName.substr( 0, NParams::szMapName.rfind('.') );
		}
		else if ( *it == "-fences" )
			NParams::bConvertFences = true;
		else if ( *it == "-nobzm" ) 
			NParams::bConvertBZM = false;
		else
		{
			if ( NParams::szStartDir.empty() ) 
			{
				NParams::szStartDir = *it;
				if ( NParams::szStartDir.empty() ) 
					NParams::szStartDir = ".\\";
				else if ( NParams::szStartDir[NParams::szStartDir.size() - 1] != '\\' ) 
					NParams::szStartDir += "\\";
			}
			else
			{
				NParams::szDestDir = *it;
				if ( NParams::szDestDir.empty() ) 
					NParams::szDestDir = ".\\";
				else if ( NParams::szDestDir[NParams::szDestDir.size() - 1] != '\\' ) 
					NParams::szDestDir += "\\";
			}
		}
	}
}
template <class TRPGStats>
void ConvertFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo, const TRPGStats *pStats, const char *pszType )
{
	if ( pStats == 0 ) 
		return;
	const int nType = pStats->GetTypeFromIndex( pInfo->nFrameIndex );
	if ( nType == -1 ) 
	{
		NI_ASSERT_T( nType != -1, NStr::Format("Unknown type for %s \"%s\". Was frame index %d", pszType, pInfo->szName.c_str(), pInfo->nFrameIndex) );
	}
	else
		pInfo->nFrameIndex = nType;
}
void ConvertFences( CMapInfo *pMapInfo )
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	for ( std::vector<SMapObjectInfo>::iterator it = pMapInfo->objects.begin(); it != pMapInfo->objects.end(); ++it )
	{
		const SGDBObjectDesc *pDesc = pGDB->GetDesc( it->szName.c_str() );
		NI_ASSERT_TF( pDesc != 0, NStr::Format("Can't find descriptor for \"%s\"", it->szName.c_str()), continue );
		switch ( pDesc->eGameType ) 
		{
			case SGVOGT_FENCE:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SFenceRPGStats>(pGDB, pDesc), "fence" );
				break;
			case SGVOGT_ENTRENCHMENT:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SEntrenchmentRPGStats>(pGDB, pDesc), "entrenchment" );
				break;
			case SGVOGT_BRIDGE:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SBridgeRPGStats>(pGDB, pDesc), "bridge" );
				break;
		}
	}
}
class CProcessMap
{
	bool bConvertFences;
	bool bSaveAsBZM;
public:
	CProcessMap( bool _bConvertFences, bool _bSaveAsBZM )
		: bConvertFences( _bConvertFences ), bSaveAsBZM( _bSaveAsBZM ) {  }
	int Convert( const std::string &szMapName, const std::string &szResultName ) const
	{
		printf( "Processing map \"%s\"...\n", szMapName.c_str() );
		CMapInfo mapinfo;
		if ( CPtr<IDataStream> pStream = OpenFileStream(szMapName.c_str(), STREAM_ACCESS_READ) )
		{
			CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
			saver.AddTypedSuper( &mapinfo );
		}
		else
			return 0xDEAD;
		if ( NParams::bConvertFences ) 
			ConvertFences( &mapinfo );
		if ( NParams::bConvertBZM ) 
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream(szResultName.c_str(), STREAM_ACCESS_WRITE) )
			{
				CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
				CSaverAccessor saver = pSS;
				saver.Add( 1, &mapinfo );
			}
			else
				return 0xDEAD;
		}
		else if ( NParams::bConvertFences ) 
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream(szResultName.c_str(), STREAM_ACCESS_WRITE) )
			{
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
				saver.AddTypedSuper( &mapinfo );
			}
			else
				return 0xDEAD;
		}
		printf( "Done processing map \"%s\"...\n", szMapName.c_str() );
		return 0;
	}
	int operator()( NFile::CFileIterator &file ) const
	{
		if ( file.IsDirectory() ) 
			return 0;

		std::string szMapName = file.GetFilePath();
		szMapName = szMapName.substr( 0, szMapName.rfind('.') );
		std::string szDestMapName = szMapName;
		const char *pszResultExt = bSaveAsBZM ? ".bzm" : ".xml";
		if ( !NParams::szStartDir.empty() && !NParams::szDestDir.empty() ) 
		{
			szDestMapName = szDestMapName.substr( NParams::szStartDir.size(), std::string::npos );
			szDestMapName = NParams::szDestDir + szDestMapName;
		}
		return Convert( szMapName + ".xml", szDestMapName + pszResultExt );
	}
};
int main( int argc, char *argv[] )
{
	ProcessCommandLine( argc, argv );
	if ( NParams::bConvertObjToMod )
		return ConvertObjToMod( NParams::szObjInput, NParams::szModOutput ) ? 0 : 0xDEAD;

	if ( NParams::szMapName.empty() && NParams::szStartDir.empty() ) 
		return 0xDEAD;
	{
		CPtr<IDataStorage> pStorage = OpenStorage( ".\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
		RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	}
	{
		CPtr<IObjectsDB> pODB = CreateObjectsDB();
		pODB->LoadDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pODB );
		GetSLS()->SetGDB( pODB );
	}
	CProcessMap processor( NParams::bConvertFences, NParams::bConvertBZM );
	if ( NParams::szMapName.empty() ) 
		NFile::EnumerateFiles( NParams::szStartDir.c_str(), "*.xml", processor, true );
	else
	{
		std::string szMapName = NParams::szMapName.substr( 0, NParams::szMapName.rfind('.') );
		std::string szDestMapName = szMapName;
		const char *pszResultExt = NParams::bConvertBZM ? ".bzm" : ".xml";
		if ( !NParams::szStartDir.empty() && !NParams::szDestDir.empty() ) 
		{
			szDestMapName = szDestMapName.substr( NParams::szStartDir.size() + 1, std::string::npos );
			szDestMapName = NParams::szDestDir + szDestMapName;
		}
		processor.Convert( szMapName + ".xml", szDestMapName + pszResultExt );
	}
	return 0;
}
