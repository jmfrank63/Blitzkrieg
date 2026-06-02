#include "StdAfx.h"

#include <stdio.h>
#include <math.h>
#include <maya\MDagPathArray.h>
#include <maya\MSelectionList.h>
#include <maya\MGlobal.h>
#include <maya\MFnMesh.h>
#include <maya\MFloatVectorArray.h>
#include <maya\MPointArray.h>
#include <maya\MItMeshPolygon.h>
#include <maya\MItSelectionList.h>
#include <maya\MItDependencyGraph.h>
#include <maya\MItDependencyNodes.h>
#include <maya\MItDag.h>
#include <maya\MItGeometry.h>
#include <maya\MFnSkinCluster.h>
#include <maya\MFnTransform.h>
#include <maya\MQuaternion.h>
#include <maya\MFnIkJoint.h>
#include <maya\MMatrix.h>
#include <maya\MFnNumericAttribute.h>
#include <maya\MFnNumericData.h>
#include <maya\MFnTypedAttribute.h>
#include <maya\MFnMatrixData.h>
#include <maya\MFnMeshData.h>
#include <maya\MTime.h>


#include "Data.h"
#include "A7ExportModel.h"
#include "..\Misc\StrProc.h"
void* CA7ExportModel::creator()
{
	return new CA7ExportModel();
}

MStatus CA7ExportModel::reader( const MFileObject& file, const MString& optionsString, FileAccessMode mode )
{
	return MS::kFailure;
}

MPxFileTranslator::MFileKind CA7ExportModel::identifyFile( const MFileObject& fileName, const char* buffer, short size ) const
{
	return kNotMyFileType;
}
MStatus CA7ExportModel::writer( const MFileObject& file, const MString& options, FileAccessMode mode )
{
	fprintf( stderr, "file = \"%s\"\n", file.fullName().asChar() );
	MStatus status = MS::kFailure;
	try
	{
		NConverter::ClearAll();
		this->ClearAll();
		status = CollectHierarchy();
		if ( status == MS::kFailure ) throw 1;
		status = CollectMeshes();
		if ( status == MS::kFailure ) throw 1;
		status = ProcessHierarchy();
		if ( status == MS::kFailure ) throw 1;
		status = ProcessMeshes();
		if ( status == MS::kFailure ) throw 1;
		status = ProcessAABB();
		if ( status == MS::kFailure ) throw 1;
		status = ProcessAnimations();
		if ( status == MS::kFailure ) throw 1;

		std::string szFileName = file.fullName().asChar();
		if ( NConverter::SaveModel( szFileName ) == false )
			throw 1;
	}
	catch ( ... )
	{
		if ( status == MS::kFailure )
			fprintf( stderr, "Error: exporting file \"%s\"\n", file.fullName().asChar() );
		else
			fprintf( stderr, "Error: writing file \"%s\"\n", file.fullName().asChar() );
		
		return MS::kFailure;
	}
	NConverter::ClearAll();
	this->ClearAll();


	return MS::kSuccess;
}
void CA7ExportModel::ClearAll()
{
	NConverter::ClearAll();
	oHierarchy.clear();
	oMeshes.clear();
	oLocators.clear();
	oLocatorsHierarchy.clear();
	oAnimations.clear();
	oAABB = MObject();
	oAABB_As.clear();
	oAABB_Ds.clear();
}
int CA7ExportModel::FindIndexInHierarchy( MString &szName )
{
	for ( int i = 0; i != oHierarchy.length(); ++i )
	{
		MFnDagNode node( oHierarchy[i] );
		if ( node.name() == szName )
			return i;
	}
	return -1;
}

MObject CA7ExportModel::FindObjectInHierarchy( MString &szName )
{
	for ( int i = 0; i != oHierarchy.length(); ++i )
	{
		MFnDagNode node( oHierarchy[i] );
		if ( node.name() == szName )
			return oHierarchy[i];
	}
	return MObject();
}

int CA7ExportModel::FindIndexInLocators( MString &szName )
{
	for ( int i = 0; i != oLocatorsHierarchy.length(); ++i )
	{
		MFnDagNode node( oLocatorsHierarchy[i] );
		if ( node.name() == szName )
			return oHierarchy.length() + i;	// add number of normal meshes - locators must be last in the array
	}
	return -1;
}

MObject CA7ExportModel::FindObjectInLocators( MString &szName )
{
	for ( int i = 0; i != oLocatorsHierarchy.length(); ++i )
	{
		MFnDagNode node( oLocatorsHierarchy[i] );
		if ( node.name() == szName )
			return oLocatorsHierarchy[i];
	}
	return MObject();
}
MStatus CA7ExportModel::CollectHierarchy()
{
	MStatus status;
	MItDag itDAG( MItDag::kBreadthFirst, MFn::kTransform, &status );	
  if ( status != MS::kSuccess ) 
	{
    fprintf( stderr, "Failure in DAG iterator setup.\n" );
    return MS::kFailure;
  }
	for ( ; !itDAG.isDone(); itDAG.next() )
	{
		MDagPath path;
		itDAG.getPath( path );
		MFnDagNode node( path );
		if ( node.isIntermediateObject() )
			continue;
		if ( !( path.hasFn( MFn::kTransform ) && path.hasFn( MFn::kMesh ) ) &&
				 !( path.hasFn( MFn::kTransform ) && path.hasFn( MFn::kLocator ) ) )
			continue;
		for ( int i=0; i<path.childCount(); ++i )
		{
			if ( path.child(i).apiType() == MFn::kMesh )
			{
				MFnDagNode node( itDAG.item() );
				const std::string szNodeName = node.name().asChar();
				if ( szNodeName.compare(0, 4, "AABB") == 0 )
				{
					if ( szNodeName.compare(0, 6, "AABB_A") == 0 )			// AABB for animation
						oAABB_As.append( path.child(i) );
					else if ( szNodeName.compare(0, 6, "AABB_D") == 0 )	// AABB for death state
						oAABB_Ds.append( path.child(i) );
					else if ( szNodeName == "AABB" )										// exact main AABB match
						oAABB = path.child(i);
					else
						fprintf( stderr, "Unknown AABB found \"%s\"\n", szNodeName.c_str() );
				}
				else
					oHierarchy.append( itDAG.item() );
				break;
			}
			else if ( path.child(i).apiType() == MFn::kLocator )
			{
				MFnDagNode node( itDAG.item() );
				if ( node.name() == MString("Animations") )	// skip top-level animations 'organizer'
					break;
				else
				{
					MFnDagNode parent( node.parent(0) );
					if ( parent.name() == MString("Animations") )
					{
						oAnimations.append( itDAG.item() );
						break;
					}
				}
				oLocatorsHierarchy.append( itDAG.item() );
				break;
			}
		}
	}

	return MS::kSuccess;
}
MStatus CA7ExportModel::CollectMeshes()
{
	for ( int i = 0; i != oHierarchy.length(); ++i )
	{
		MFnTransform trans( oHierarchy[i] );
		MDagPath path;
		trans.getPath( path );
		for ( int j=0; j != path.childCount(); ++j )
		{
			MFnDagNode node( path.child(j) );
			if ( node.isIntermediateObject() )
			{
				fprintf( stderr, "skipping intermediate object \"%s\"\n", node.name().asChar() );
				continue;
			}
			if ( path.child(j).apiType() == MFn::kMesh )
				oMeshes.append( path.child(j) );
			else if ( path.child(j).apiType() == MFn::kLocator )
				oLocators.append( path.child(j) );
		}
	}
	return MS::kSuccess;
}
template <class TYPE>
TYPE GetValue( const char *pszName, MFnDagNode &node, TYPE defval )
{
	MStatus status;
	TYPE value = defval;
	MPlug plug = node.findPlug( MString(pszName), &status );
	if ( status == MStatus::kSuccess )
		plug.getValue( value );
	return value;
}

double GetLimit( const char *pszNameEnable, const char *pszNameLimit, MFnDagNode &node, double fDefault )
{
	bool bEnable = GetValue( pszNameEnable, node, false );
	if ( bEnable )
		return GetValue( pszNameLimit, node, fDefault );
	return fDefault;
}
void CA7ExportModel::ProcessJoint( MObjectArray &hierarchy, std::list<SJoint> &joints, std::list<std::string> &szTopNodes, bool bLocators )
{
	for ( int i = 0; i != hierarchy.length(); ++i )
	{
		SJoint joint;
		MFnTransform trans( hierarchy[i] );
		MFnDagNode parent( trans.parent(0) );
		MString szParentName = parent.name();
		joint.szParentName = szParentName.asChar();
		joint.nParent = FindIndexInHierarchy( szParentName );
		if ( joint.nParent == -1 )
			joint.nParent = FindIndexInLocators( szParentName );
		joint.szName = trans.name().asChar();
		joint.nIndex = FindIndexInHierarchy( trans.name() );
		if ( joint.nIndex == -1 )
			joint.nIndex = FindIndexInLocators( trans.name() );
		if ( joint.nParent == -1 )
			szTopNodes.push_back( joint.szName );
		{
			double quat[4];
			trans.getRotationQuaternion( quat[0], quat[1], quat[2], quat[3], MSpace::kObject );
			joint.rot.x = quat[0];
			joint.rot.y = quat[1];
			joint.rot.z = quat[2];
			joint.rot.w = quat[3];
		}
		{
			MVector bone = trans.translation( MSpace::kTransform );
			joint.bone.x = bone.x;
			joint.bone.y = bone.y;
			joint.bone.z = bone.z;
			joint.bone *= fGeomScaleCoeff;
		}
		{
			double scale[3];
			trans.getScale( scale );
			joint.scale.x = scale[0];
			joint.scale.y = scale[1];
			joint.scale.z = scale[2];
		}
		joint.bLocator = bLocators;
		{
			joint.fMinAngleX = GetLimit( "minRotXLimitEnable", "minRotXLimit", trans, 1e38 );
			joint.fMaxAngleX = GetLimit( "maxRotXLimitEnable", "maxRotXLimit", trans, 1e38 );
			joint.fMinAngleY = GetLimit( "minRotYLimitEnable", "minRotYLimit", trans, 1e38 );
			joint.fMaxAngleY = GetLimit( "maxRotYLimitEnable", "maxRotYLimit", trans, 1e38 );
			joint.fMinAngleZ = GetLimit( "minRotZLimitEnable", "minRotZLimit", trans, 1e38 );
			joint.fMaxAngleZ = GetLimit( "maxRotZLimitEnable", "maxRotZLimit", trans, 1e38 );
			joint.fMinTransX = GetLimit( "minTransXLimitEnable", "minTransXLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
			joint.fMaxTransX = GetLimit( "maxTransXLimitEnable", "maxTransXLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
			joint.fMinTransY = GetLimit( "minTransYLimitEnable", "minTransYLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
			joint.fMaxTransY = GetLimit( "maxTransYLimitEnable", "maxTransYLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
			joint.fMinTransZ = GetLimit( "minTransZLimitEnable", "minTransZLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
			joint.fMaxTransZ = GetLimit( "maxTransZLimitEnable", "maxTransZLimit", trans, 1e38 / fGeomScaleCoeff ) * fGeomScaleCoeff;
		}
		joints.push_back( joint );
	}
}
void FillConstraint( SSkeletonFormat::SNodeFormat::SConstraint *pConstraint, float fMin, float fMax, const CVec3 &axis, int type )
{
	if ( (fabs(fMin) < 1e30f) && (fabs(fMax) < 1e30f) )
	{
		pConstraint->fMin = fMin;
		pConstraint->fMax = fMax;
		pConstraint->axis = axis;
		pConstraint->type = type;
	}
}

void FillNode( SSkeletonFormat::SNodeFormat *pNode, const SJoint &joint )
{
	pNode->szName = joint.szName;
	pNode->bone = joint.bone;
	pNode->quat.Set( joint.rot.x, joint.rot.y, joint.rot.z, joint.rot.w );
	pNode->nIndex = joint.nIndex;

	pNode->constraint.fMin = pNode->constraint.fMax = 0;
	pNode->constraint.type = 0;
	pNode->constraint.axis = VNULL3;
	FillConstraint( &pNode->constraint, joint.fMinAngleX, joint.fMaxAngleX, V3_AXIS_X, SSkeletonFormat::SNodeFormat::SConstraint::ROT );
	FillConstraint( &pNode->constraint, joint.fMinAngleY, joint.fMaxAngleY, V3_AXIS_Y, SSkeletonFormat::SNodeFormat::SConstraint::ROT );
	FillConstraint( &pNode->constraint, joint.fMinAngleZ, joint.fMaxAngleZ, V3_AXIS_Z, SSkeletonFormat::SNodeFormat::SConstraint::ROT );

	FillConstraint( &pNode->constraint, joint.fMinTransX, joint.fMaxTransX, V3_AXIS_X, SSkeletonFormat::SNodeFormat::SConstraint::TRANS );
	FillConstraint( &pNode->constraint, joint.fMinTransY, joint.fMaxTransY, V3_AXIS_Y, SSkeletonFormat::SNodeFormat::SConstraint::TRANS );
	FillConstraint( &pNode->constraint, joint.fMinTransZ, joint.fMaxTransZ, V3_AXIS_Z, SSkeletonFormat::SNodeFormat::SConstraint::TRANS );
}
class CNodeNameAccumulate
{
	const std::string &szName;
	int nCounter;
public:
	explicit CNodeNameAccumulate( const std::string &_szName ) : szName( _szName ), nCounter( 0 ) {  }
	CNodeNameAccumulate( const CNodeNameAccumulate &acc ) : szName( acc.szName ), nCounter( acc.nCounter ) {  }
	int operator()( const SSkeletonFormat::SNodeFormat &node )
	{
		nCounter += ( node.szName == szName );
		return nCounter;
	}
	int GetCounter() const { return nCounter; }
};
MStatus CA7ExportModel::ProcessHierarchy()
{
	MTime time( 0, MTime::kMilliseconds );
	MGlobal::viewFrame( time );
	std::list<SJoint> joints;
	std::list<std::string> szTopNodes;
	ProcessJoint( oHierarchy, joints, szTopNodes, false );
	ProcessJoint( oLocatorsHierarchy, joints, szTopNodes, true );
	if ( szTopNodes.size() > 1 )
	{
		fprintf( stderr, "exported model have more then one top-level node:\n" );
		for ( std::list<std::string>::const_iterator it = szTopNodes.begin(); it != szTopNodes.end(); ++it )
			fprintf( stderr, "\t\"%s\"\n", it->c_str() );
		return MS::kFailure;
	}
	const std::string szTopNode = szTopNodes.front();
	NConverter::skeleton.locators.clear();
	NConverter::skeleton.nodes.clear();
	NConverter::skeleton.nodes.resize( joints.size() );
	for ( std::list<SJoint>::const_iterator joint = joints.begin(); joint != joints.end(); ++joint )
	{
		FillNode( &(NConverter::skeleton.nodes[joint->nIndex]), *joint );
		if ( joint->nParent != -1 )
			NConverter::skeleton.nodes[joint->nParent].children.push_back( joint->nIndex );
		if ( joint->bLocator )
			NConverter::skeleton.locators.push_back( joint->nIndex );
		if ( joint->szName == szTopNode )
			NConverter::skeleton.nTopNode = joint->nIndex;
	}
	for ( SSkeletonFormat::CNodesList::iterator node = NConverter::skeleton.nodes.begin(); node != NConverter::skeleton.nodes.end(); ++node )
	{
		CNodeNameAccumulate acc = std::for_each( NConverter::skeleton.nodes.begin(), NConverter::skeleton.nodes.end(), CNodeNameAccumulate(node->szName) );
		if ( acc.GetCounter() != 1 )
		{
			fprintf( stderr, "ERROR: node \"%s\" accounted %d times in the total nodes count\n", node->szName.c_str(), acc.GetCounter() );
			return MStatus::kFailure;
		}
	}

	std::sort( NConverter::skeleton.locators.begin(), NConverter::skeleton.locators.end() );
	NConverter::skeleton.locators.erase( std::unique( NConverter::skeleton.locators.begin(), NConverter::skeleton.locators.end() ),
		                                   NConverter::skeleton.locators.end() );
	for ( SSkeletonFormat::CNodesList::iterator node = NConverter::skeleton.nodes.begin(); node != NConverter::skeleton.nodes.end(); ++node )
	{
		std::sort( node->children.begin(), node->children.end() );
		node->children.erase( std::unique(node->children.begin(), node->children.end()), node->children.end() );
	}
	return MS::kSuccess;
}
MStatus GetAABBInfo( SAABBFormat *pAABB, MObject &oAABB )
{
	if ( oAABB.apiType() != MFn::kMesh )
		return MStatus::kFailure;
	MFnMesh mesh( oAABB );
	if ( mesh.numVertices() != 8 )
	{
		fprintf( stderr, "Wrong AABB \"%s\" with %d points (instead of 8)\n", mesh.name().asChar(), mesh.numVertices() );
		return MStatus::kFailure;
	}
	CVec3 vMin( 1e8f, 1e8f, 1e8f ), vMax( -1e8f, -1e8f, -1e8f );
	for ( int j = 0; j != mesh.numVertices(); ++j )
	{
		MPoint point;
		mesh.getPoint( j, point, MSpace::kObject );
		CVec3 vPoint( point.x, point.y, point.z );
		vPoint *= fGeomScaleCoeff;
		vMin.Minimize( vPoint );
		vMax.Maximize( vPoint );
	}

	pAABB->vCenter		= ( vMax + vMin ) / 2.0f;
	pAABB->vHalfSize	= ( vMax - vMin ) / 2.0f;

	return MStatus::kSuccess;
}
int ToIntLocal( const std::string &szInt )
{
	for ( int i = 0; i != szInt.size(); ++i )
	{
		if ( NStr::IsDecDigit(szInt[i]) == false )
			return -1;
	}
	if ( szInt[0] == '0' )
		return NStr::ToInt( szInt.c_str() + 1 );
	else
		return NStr::ToInt( szInt.c_str() );
}
MStatus CA7ExportModel::ProcessAABB()
{
	if ( GetAABBInfo(&NConverter::aabb, oAABB) != MStatus::kSuccess ) 
	{
		fprintf( stderr, "Object doesn't contain AABB\n" );
		return MStatus::kFailure;
	}
	for ( int i=0; i != oAABB_As.length(); ++i )
	{
		MFnDagNode node( oAABB_As[i] );
		const std::string szNodeName = node.name().asChar();
		const std::string szIndex = szNodeName.substr( 6, 2 );
		const int nIndex = ToIntLocal( szIndex );
		SAABBFormat aabb;
		if ( nIndex >= 0 && GetAABBInfo(&aabb, oAABB_As[i]) == MStatus::kSuccess ) 
			NConverter::AddAABB_A( aabb, nIndex );
		else
		{
			fprintf( stderr, "Wrong AABB_A \"%s\"\n", szNodeName.c_str() );
			return MStatus::kFailure;
		}
	}
	for ( int i=0; i != oAABB_Ds.length(); ++i )
	{
		MFnDagNode node( oAABB_Ds[i] );
		const std::string szNodeName = node.name().asChar();
		const std::string szIndex = szNodeName.substr( 6, 2 );
		const int nIndex = ToIntLocal( szIndex );
		SAABBFormat aabb;
		if ( nIndex >= 0 && GetAABBInfo(&aabb, oAABB_Ds[i]) == MStatus::kSuccess ) 
			NConverter::AddAABB_D( aabb, nIndex );
		else
		{
			fprintf( stderr, "Wrong AABB_A \"%s\"\n", szNodeName.c_str() );
			return MStatus::kFailure;
		}
	}
	return MStatus::kSuccess;
}
MStatus CA7ExportModel::ProcessMeshes()
{
	for ( int i=0; i != oMeshes.length(); ++i )
	{
		MFnMesh mesh( oMeshes[i] );
		NConverter::SetActiveMesh( mesh.name().asChar() );
		NConverter::SetMeshIndex( FindIndexInHierarchy(mesh.name()) );
		for ( int j = 0; j != mesh.numVertices(); ++j )
		{
			MPoint point;
			mesh.getPoint( j, point, MSpace::kObject );
			NConverter::AddPoint( CVec3(point.x, point.y, point.z) );
		}
		MFloatVectorArray normales;
		mesh.getNormals( normales, MSpace::kObject );
		for ( int j = 0; j != normales.length(); ++j )
			NConverter::AddNormale( CVec3(normales[j].x, normales[j].y, normales[j].z) );
		for ( int j = 0; j != mesh.numUVs(); ++j )
		{
			CVec2 uv;
			mesh.getUV( j, uv.u, uv.v );
			uv.v = 1.0f - uv.v;								// Maya считает за 0 левый нижний угол, а все нормальные пакеты - левый верхний
			NConverter::AddUV( uv );
		}
		MDagPath meshPath;
		MDagPath::getAPathTo( oMeshes[i], meshPath );
		MItMeshPolygon itPoly( meshPath.node() );
		int nIndices[9];
		int nPolyCounter = 0;
		for ( ; !itPoly.isDone(); itPoly.next(), ++nPolyCounter )
		{
			if ( itPoly.polygonVertexCount() > 3 )
			{
				fprintf( stderr, "Error: Mesh %s is needed to be triangulated because of polygon %d have %d vertices\n", mesh.name().asChar(), nPolyCounter, itPoly.polygonVertexCount() );
				return MS::kFailure;
			}
			else if ( itPoly.polygonVertexCount() < 3 )
			{
				fprintf( stderr, "Warning: Mesh %s have degenerated polygon %d (%d vertices)\n", mesh.name().asChar(), nPolyCounter, itPoly.polygonVertexCount() );
				continue;
			}
			for ( int vtx = 0; vtx != 3; ++vtx )
			{
				nIndices[ vtx * 3 ] = itPoly.vertexIndex( vtx );
				nIndices[ vtx * 3 + 1 ] = itPoly.normalIndex( vtx );
				if ( !itPoly.getUVIndex( vtx, nIndices[ vtx * 3 + 2 ] ) )
				{
					fprintf( stderr, "Error: Mesh %s is needed to be fully mapped!\n", mesh.name().asChar() );
					return MS::kFailure;
				}
			}
			NConverter::AddFace( nIndices );
		}
	}
	return MS::kSuccess;
}
struct SAnimDesc
{
	std::string szName;										// animation name
	int nStart;														// start frame
	int nNumKeys;													// number of keys...
	int nAction;													// action time (from nStart)
	int nAABBIndex;												// index of the AABBs for animation
};

MStatus CA7ExportModel::ProcessAnimations()
{
	const float fTimeStep = 62.5f;				// 16 fps
	std::list<SAnimDesc> animdescs;
	for ( int i=0; i<oAnimations.length(); ++i )
	{
		MFnDagNode node( oAnimations[i] );

		fprintf( stderr, "processing animation \"%s\"\n", node.name().asChar() );
		animdescs.push_back( SAnimDesc() );
		SAnimDesc &desc = animdescs.back();
		desc.szName = node.name().asChar();
		NStr::ToLower( desc.szName );
		desc.nStart = GetValue( "StartTime", node, 0.0 );
		int nEnd = GetValue( "EndTime", node, 0.0 );
		int nActionTime = GetValue( "ActionTime", node, 0.0 );
		const int nAABBIndex = GetValue( "AABBIndex", node, -1.0 );
		desc.nNumKeys = nEnd - desc.nStart + 1;
		desc.nAction = Max( 0, nActionTime - desc.nStart );
		desc.nAABBIndex = nAABBIndex;
		if ( desc.nStart >= nEnd )
		{
			fprintf( stderr, "wrong animation \"%s\" (%d >= %d). skipping\n", desc.szName.c_str(), desc.nStart, nEnd );
			animdescs.pop_back();
			continue;
		}
	}
	for ( std::list<SAnimDesc>::const_iterator anim = animdescs.begin(); anim != animdescs.end(); ++anim )
	{
		NConverter::animations.push_back( SAnimationFormat() );
		SAnimationFormat &animation = NConverter::animations.back();
		animation.szName = anim->szName;
		animation.nodes.SetSizes( anim->nNumKeys, NConverter::skeleton.GetNumNodes() );
		animation.nAction = anim->nAction;
		animation.nAABB_AIndex = NConverter::GetAABB_AIndex( anim->nAABBIndex );
		animation.nAABB_DIndex = NConverter::GetAABB_DIndex( anim->nAABBIndex );
		for ( int i=0; i<anim->nNumKeys; ++i )
		{
			double fFrame = anim->nStart + i;
			MGlobal::viewFrame( fFrame );
			std::list<SJoint> joints;
			std::list<std::string> szTopNodes;
			ProcessJoint( oHierarchy, joints, szTopNodes, false );
			ProcessJoint( oLocatorsHierarchy, joints, szTopNodes, true );
			for ( std::list<SJoint>::const_iterator joint = joints.begin(); joint != joints.end(); ++joint )
			{
				animation.nodes[joint->nIndex][i].vPos = joint->bone;
				animation.nodes[joint->nIndex][i].vRot.Set( joint->rot.x, joint->rot.y, joint->rot.z, joint->rot.w );
			}
		}
	}

	return MStatus::kSuccess;
}
void CA7ExportModel::OutputMatrix( const char *pszText, MMatrix &mx )
{
	fprintf( stderr, pszText );
	fprintf( stderr, "\n" );
	int i, j;
	for ( i = 0; i < 4; ++i )
	{
		for ( j = 0; j < 4; ++j )
		{
			fprintf( stderr, "%.3f ", mx.matrix[j][i] );
		}
		fprintf( stderr, "\n" );
	}
}
void CA7ExportModel::OutputVector( const char *pszText, MVector &v )
{
	fprintf( stderr, pszText );
	int i;
	for ( i = 0; i < 3; ++i )
		fprintf( stderr, " %.3f", v[i] );
	fprintf( stderr, "\n" );
}
void CA7ExportModel::OutputPoint( const char *pszText, MPoint &p )
{
	fprintf( stderr, pszText );
	int i;
	for ( i = 0; i < 3; ++i )
		fprintf( stderr, " %.3f", p[i] );
	fprintf( stderr, "\n" );
}
