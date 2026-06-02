#ifndef __DATA_H__
#define __DATA_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
const double pi = 3.141592535897932384624;
const float fGeomScaleCoeff = 38.0f * 2.936f / 6.880226f;
struct SPlot
{
	float x, y, z;
};
struct SQuat
{
	float x, y, z, w;
};
struct SJoint
{
	std::string szName;
	std::string szParentName;
	CVec3 bone;
	int nIndex;
	int nParent;
	bool bLocator;
	float fMinAngleX, fMaxAngleX;
	float fMinAngleY, fMaxAngleY;
	float fMinAngleZ, fMaxAngleZ;
	float fMinTransX, fMaxTransX;
	float fMinTransY, fMaxTransY;
	float fMinTransZ, fMaxTransZ;
	SPlot pos;
	SQuat rot;
	SPlot scale;
};
namespace NConverter
{
	extern std::vector<SJoint> joints;
	extern SSkeletonFormat skeleton;
	extern std::vector<SAnimationFormat> animations;
	extern SAABBFormat aabb;
	extern std::vector<SAABBFormat> aabb_as;
	extern std::vector<SAABBFormat> aabb_ds;

	void ClearAll();
	void AddFace( int nSet, int *pIndices );
	void SetActiveMesh( const char *pszMeshName );
	void SetMeshIndex( int nIndex );
	void AddPoint( const CVec3 &point );
	void AddNormale( const CVec3 &point );
	void AddUV( const CVec2 &uv );
	void AddFace( int *pIndices );
	bool SaveModel( std::string &szFileName );
	bool AddAABB_A( const SAABBFormat &aabb, int nIndex );
	bool AddAABB_D( const SAABBFormat &aabb, int nIndex );
	int GetAABB_AIndex( int nAABB );
	int GetAABB_DIndex( int nAABB );
};
#endif