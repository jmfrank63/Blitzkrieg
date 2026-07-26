#ifndef __FMTMESH_H__
#define __FMTMESH_H__
#pragma ONCE
// No #pragma pack here: the POD structs (SAnimNodeFormat, SAABBFormat,
// SBSphereFormat, SVertexComponent) consist solely of 4-byte members, so
// their layout is identical packed or natural. The STL-bearing structs
// (SAnimationFormat, SMeshFormat, SSkeletonFormat) must NOT be packed —
// on x64 packing misaligns their 8-byte string/vector internals (UB).
static const float fAnimTimeStep = 62.5f;	// 16 fps
struct SAnimNodeFormat
{
	CVec3 vPos;														// position
	CVec4 vRot;														// rotation
};
struct SAnimationFormat
{
	std::string szName;										// animation name
	int nType;														// animation type
	CArray2D<SAnimNodeFormat> nodes;			// animation nodes for each transform node
	int nAction;													// action frame
	int nAABB_AIndex;											// AABB_for_fatality_animation index
	int nAABB_DIndex;											// AABB_for_death_state index
	SAnimationFormat() : nType( -1 ), nAction( 0 ), nAABB_AIndex( -1 ), nAABB_DIndex( -1 ) {  }
	float GetLength() const { return (nodes.GetSizeX() - 1) * fAnimTimeStep; }
	float GetActionTime() const { return nAction * fAnimTimeStep; }
	int operator&( interface IStructureSaver &ss );
};
struct SAABBFormat
{
	CVec3 vCenter;												// center of the box
	CVec3 vHalfSize;											// half size of the box
};
struct SBSphereFormat
{
	CVec3 vCenter;												// center of the sphere
	float fRadius;
};
struct SMeshFormat
{
	struct SVertexComponent
	{
		int geom;														// geometry index
		int norm;														// normale index
		int tex;														// texture coord index
		int operator&( interface IStructureSaver &ss );
	};
	std::string szName;										// mesh name
	int nIndex;														// mesh index
	std::vector<CVec3> geoms;							// geometry (position)
	std::vector<CVec3> norms;							// normales
	std::vector<CVec2> texes;							// primary texture mappings
	std::vector<SVertexComponent> components;	// vertex components
	std::vector<WORD> indices;						// indices from the 'components' array, which forms faces. each 3 indices forms 1 face
	SAABBFormat aabb;											// axis-aligned bounding box
	SBSphereFormat bsphere;								// miniball - smallest optimal bounding sphere
	int operator&( IStructureSaver &ss );
};
struct SSkeletonFormat
{
	struct SNodeFormat
	{
		struct SConstraint
		{
			enum { ROT = 1, TRANS = 2 };
			float fMin;													// min constraint value
			float fMax;													// max constraint value
			int type;														// constraint type
			CVec3 axis;													// constraint axis
			int operator&( IStructureSaver &ss );
		};
		std::string szName;										// node name
		int nIndex;														// node index (equal to mesh index)
		CVec3 bone;														// bone (from parent)
		CVec4 quat;														// default rotation

		SConstraint constraint;								// constraint info

		typedef std::vector<int> CChildrenList;
		CChildrenList children;								// child nodes
		SNodeFormat()
		{
			constraint.fMin = constraint.fMax = 0;
			constraint.axis = VNULL3;
			constraint.type = 0;
		}
		int operator&( IStructureSaver &ss );
	};
	typedef std::vector<SNodeFormat> CNodesList;
	CNodesList nodes;												// all nodes
	int nTopNode;														// top-level node index
	std::vector<int> locators;							// locator indices
	int operator&( IStructureSaver &ss );
	int GetNumNodes() const { return nodes.size(); }
	const SNodeFormat* GetNode( int nIndex ) const { return &( nodes[nIndex] ); }
};
#endif // __FMTMESH_H__
