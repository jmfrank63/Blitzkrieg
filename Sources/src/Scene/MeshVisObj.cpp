#include "StdAfx.h"

#include "MeshVisObj.h"

#include "../GFX/GFXHelper.h"
#include "../Common/Icons.h"
static const int aabbIndices[6][4] = 
{ { 0, 4, 6, 2 },
	{ 3, 7, 5, 1 },
	{ 0, 1, 5, 4 },
	{ 6, 7, 3, 2 },
	{ 0, 2, 3, 1 },
	{ 5, 7, 6, 4 } };
bool IsPointInConvexPolygon( const CVec3 *points, const int *indices, const int nNumIndices, const CVec2 &vPoint )
{
	bool bInside = false;
	
	for ( int i=0; i<nNumIndices; ++i )
	{
		const CVec3 &vBeg = points[ indices[i] ];
		const CVec3 &vEnd = points[ indices[(i + 1) % nNumIndices] ];
		if ( (vEnd.y <= vPoint.y) && (vPoint.y < vBeg.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) < (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
		else if ( (vBeg.y <= vPoint.y) && (vPoint.y < vEnd.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) > (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
	}
	
	return bInside;
}
void TransformAABB( CVec3 *corners, const SHMatrix &matrix, const SGFXAABB &aabb )
{
	matrix.RotateHVector( &corners[0], CVec3( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[1], CVec3( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[2], CVec3( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[3], CVec3( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[4], CVec3( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[5], CVec3( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[6], CVec3( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z ) );
	matrix.RotateHVector( &corners[7], CVec3( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z ) );
}
inline bool InPointInAABB( const SHMatrix &matrix, const SGFXAABB &aabb, const CVec2 &vPos )
{
	CVec3 corners[8];
	TransformAABB( corners, matrix, aabb );
	return IsPointInConvexPolygon( corners, aabbIndices[0], 4, vPos ) || 
		     IsPointInConvexPolygon( corners, aabbIndices[1], 4, vPos ) ||
				 IsPointInConvexPolygon( corners, aabbIndices[2], 4, vPos ) || 
				 IsPointInConvexPolygon( corners, aabbIndices[3], 4, vPos ) ||
				 IsPointInConvexPolygon( corners, aabbIndices[4], 4, vPos ) || 
				 IsPointInConvexPolygon( corners, aabbIndices[5], 4, vPos );
}
inline void GetMinMax( CVec2 *pRes, float x1, float x2, float x3, float x4 )
{
	pRes->x = Min( Min( x1, x2 ), Min( x3, x4 ) );
	pRes->y = Max( Max( x1, x2 ), Max( x3, x4 ) );
}
inline void GetMinMax( CVec2 *pMinMax1, CVec2 *pMinMax2, const CVec2 &vAxis, const CTRect<float> &rect, 
							         const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2, const CVec3 &pt3 )
{
	GetMinMax( pMinMax1, rect.x1*vAxis.x + rect.y1*vAxis.y, rect.x1*vAxis.x + rect.y2*vAxis.y,
			                 rect.x2*vAxis.x + rect.y1*vAxis.y, rect.x2*vAxis.x + rect.y2*vAxis.y );
	CVec2 vMinMax2;
	GetMinMax( pMinMax2, pt0.x*vAxis.x + pt0.y*vAxis.y, pt1.x*vAxis.x + pt1.y*vAxis.y,
			                 pt2.x*vAxis.x + pt2.y*vAxis.y, pt3.x*vAxis.x + pt3.y*vAxis.y );
}
bool IsIntersected( const CTRect<float> &rect, const CVec3 *points, const int *indices )
{
	const CVec3 &pt0 = points[ indices[0] ];
	const CVec3 &pt1 = points[ indices[1] ];
	const CVec3 &pt2 = points[ indices[2] ];
	const CVec3 &pt3 = points[ indices[3] ];
	{
		CVec2 vMinMax1( rect.x1, rect.x2 );
		CVec2 vMinMax2( Min( Min( pt0.x, pt1.x), Min( pt2.x, pt3.x ) ), 
			              Max( Max( pt0.x, pt1.x), Max( pt2.x, pt3.x ) ) );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	{
		CVec2 vMinMax1( rect.y1, rect.y2 );
		CVec2 vMinMax2( Min( Min( pt0.y, pt1.y), Min( pt2.y, pt3.y ) ), 
			              Max( Max( pt0.y, pt1.y), Max( pt2.y, pt3.y ) ) );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	{
		const CVec2 vAxis( -(pt1.y - pt0.y), pt1.x - pt0.x );
		CVec2 vMinMax1, vMinMax2;
		GetMinMax( &vMinMax1, &vMinMax2, vAxis, rect, pt0, pt1, pt2, pt3 );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	{
		const CVec2 vAxis( -(pt2.y - pt0.y), pt2.x - pt0.x );
		CVec2 vMinMax1, vMinMax2;
		GetMinMax( &vMinMax1, &vMinMax2, vAxis, rect, pt0, pt1, pt2, pt3 );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	{
		const CVec2 vAxis( -(pt1.y - pt3.y), pt1.x - pt3.x );
		CVec2 vMinMax1, vMinMax2;
		GetMinMax( &vMinMax1, &vMinMax2, vAxis, rect, pt0, pt1, pt2, pt3 );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	{
		const CVec2 vAxis( -(pt2.y - pt3.y), pt2.x - pt3.x );
		CVec2 vMinMax1, vMinMax2;
		GetMinMax( &vMinMax1, &vMinMax2, vAxis, rect, pt0, pt1, pt2, pt3 );
		if ( (vMinMax1.x >= vMinMax2.y) || (vMinMax2.x >= vMinMax1.y) )
			return false;
	}
	return true;
}
inline bool IsAABBIntersectRect( const SHMatrix &matrix, const SGFXAABB &aabb, const CTRect<float> &rect )
{
	CVec3 corners[8];
	TransformAABB( corners, matrix, aabb );
	return IsIntersected( rect, corners, aabbIndices[0] ) ||
		     IsIntersected( rect, corners, aabbIndices[1] ) ||
		     IsIntersected( rect, corners, aabbIndices[2] ) ||
		     IsIntersected( rect, corners, aabbIndices[3] ) ||
		     IsIntersected( rect, corners, aabbIndices[4] ) ||
		     IsIntersected( rect, corners, aabbIndices[5] );
}
bool CMeshVisObj::IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift )
{
	SHMatrix matrix;
	Multiply( &matrix, matTransform, matPlacement );
	const SGFXBoundSphere &sphere = pMesh->GetBS();
	CVec3 vScreenSphereCenter;
	matrix.RotateHVector( &vScreenSphereCenter, sphere.vCenter );
	if ( fabs2( vScreenSphereCenter.x - point.x, vScreenSphereCenter.y - point.y ) <= fabs2( sphere.fRadius ) )
	{
		if ( !InPointInAABB( matrix, pMesh->GetAABB(), point ) )
			return false;
		if ( pShift )
		{
			matrix.RotateHVector( &vScreenSphereCenter, VNULL3 );
			pShift->x = vScreenSphereCenter.x - point.x;
			pShift->y = vScreenSphereCenter.y - point.y;
		}
		return true;
	}
	return false;
}
bool CMeshVisObj::IsHit( const SHMatrix &matTransform, const RECT &rect )
{
	SHMatrix matrix;
	Multiply( &matrix, matTransform, matPlacement );
	const SGFXBoundSphere &sphere = pMesh->GetBS();
	CVec3 vScreenSphereCenter;
	matrix.RotateHVector( &vScreenSphereCenter, sphere.vCenter );
	CTRect<int> rcSrc( vScreenSphereCenter.x - sphere.fRadius, vScreenSphereCenter.y - sphere.fRadius, 
		                 vScreenSphereCenter.x + sphere.fRadius, vScreenSphereCenter.y + sphere.fRadius );
	CTRect<int> rcDst = rect;
	return rcDst.IsInside( vScreenSphereCenter.x, vScreenSphereCenter.y );
	/*
	rcSrc.Intersect( rcDst );
	if ( !rcSrc.IsEmpty() )
		return IsAABBIntersectRect( matrix, pMesh->GetAABB(), rect );
	else
		return false;
	*/
}
static CMatrixStack<128> mstack;
bool CMeshVisObj::Update( const NTimer::STime &time, bool bForced ) 
{  
	if ( dwLastUpdateTime != time || bForced )
	{
		matPlacement.Set( GetPos(), quat );
		if ( bHasScale ) 
			MultiplyScale( &matPlacement, matPlacement, vScale.x, vScale.y, vScale.z );
		dwLastUpdateTime = time;
	}
	if ( !effectors.empty() )
	{
		mstack.Push( matPlacement );
		for ( CEffectorsList::iterator it = effectors.begin(); it != effectors.end(); )
		{
			if ( it->pEffector->Update(time) == false )
				it = effectors.erase( it );
			else
			{
				if ( it->nPart == -1 )
					mstack.Push( it->GetMatrix() );
				++it;
			}
		}
		matPlacement1 = mstack();
		mstack.Clear();
	}
	else
		matPlacement1 = matPlacement;
	if ( pMaterialEffector != 0 )
	{
		if ( pMaterialEffector->Update( time ) )
		{
			SetOpacity( pMaterialEffector->GetAlpha() );
			SetSpecular( pMaterialEffector->GetSpecular() );
		}
		else
			RemoveMaterialEffector();
	}
	pAnim->SetTime( time );
	return true;
}
bool CMeshVisObj::Draw( IGFX *pGFX )
{
	pGFX->SetTexture( 0, pTexture );
	pGFX->EnableSpecular( material.vSpecular != VNULL4 );
	pGFX->SetMaterial( material );
	return pGFX->DrawMesh( pMesh, GetMatrices(), pAnim->GetNumNodes() );
}
bool CMeshVisObj::DrawShadow( IGFX *pGFX, const SHMatrix *pMatShadow, const CVec3 &vSunDir )
{
	if ( pMatShadow == 0 ) 
	{
		if ( IMatrixEffectorLeveling *pEffector = static_cast<IMatrixEffectorLeveling*>(pAnim->GetEffector(ANIM_EFFECTOR_LEVELING, -2)) )
		{
			CVec3 vNormal;
			matPlacement.RotateVector( &vNormal, pEffector->GetNormal() );
			const float fA = vNormal.x;
			const float fB = vNormal.y;
			const float fC = vNormal.z;
			const float fD = -( fA*GetPos().x + fB*GetPos().y + fC*GetPos().z );
			const float fXl = vSunDir.x;
			const float fYl = vSunDir.y;
			const float fZl = vSunDir.z;
			const float fRDenominator = 1.0f / ( fA*fXl + fB*fYl + fC*fZl );
			SHMatrix matShadow;
			matShadow._11 = +( fB*fYl + fC*fZl ) * fRDenominator;
			matShadow._12 = -( fB*fXl ) * fRDenominator;
			matShadow._13 = -( fC*fXl ) * fRDenominator;
			matShadow._14 = -( fD*fXl ) * fRDenominator;
			matShadow._21 = -( fA*fYl ) * fRDenominator;
			matShadow._22 = +( fA*fXl + fC*fZl ) * fRDenominator;
			matShadow._23 = -( fC*fYl ) * fRDenominator;
			matShadow._24 = -( fD*fYl ) * fRDenominator;
			matShadow._31 = -( fA*fZl ) * fRDenominator;
			matShadow._32 = -( fB*fZl ) * fRDenominator;
			matShadow._33 = +( fA*fXl + fB*fYl ) * fRDenominator;
			matShadow._34 = -( fD*fZl ) * fRDenominator;
			matShadow._41 = matShadow._42 = matShadow._43 = 0;
			matShadow._44 = 1;
			return pGFX->DrawMesh( pMesh, GetExtMatrices(matShadow), pAnim->GetNumNodes() );
		}
		return false;
	}
	else
		return pGFX->DrawMesh( pMesh, GetExtMatrices(*pMatShadow), pAnim->GetNumNodes() );
}
void CMeshVisObj::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitMeshObject( this, GetGameType(nType), GetPriority() );
	VisitIcons( pVisitor );
}
bool CMeshVisObj::DrawBB( IGFX *pGFX )
{
	{
		const SGFXAABB &aabb = pMesh->GetAABB();
		SGFXLineVertex *vertices = reinterpret_cast<SGFXLineVertex*>( pGFX->GetTempVertices( 8, SGFXLineVertex::format, GFXPT_LINELIST ) );
		DWORD color = 0xffff0000;
		vertices[0].Setup( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z, color );
		vertices[1].Setup( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z, color );
		vertices[2].Setup( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z, color );
		vertices[3].Setup( aabb.vCenter.x + aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z, color );
		vertices[4].Setup( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z, color );
		vertices[5].Setup( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y + aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z, color );
		vertices[6].Setup( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z + aabb.vHalfSize.z, color );
		vertices[7].Setup( aabb.vCenter.x - aabb.vHalfSize.x, aabb.vCenter.y - aabb.vHalfSize.y, aabb.vCenter.z - aabb.vHalfSize.z, color );
	}
	{
		WORD *indices = reinterpret_cast<WORD*>( pGFX->GetTempIndices( 24, GFXIF_INDEX16, GFXPT_LINELIST ) );
		indices[0]  = 0;		indices[1]  = 1;
		indices[2]  = 1;		indices[3]  = 3;
		indices[4]  = 3;		indices[5]  = 2;
		indices[6]  = 2;		indices[7]  = 0;
		indices[8]  = 4;		indices[9]  = 5;
		indices[10] = 5;		indices[11] = 7;
		indices[12] = 7;		indices[13] = 6;
		indices[14] = 6;		indices[15] = 4;
		indices[16] = 0;		indices[17] = 4;
		indices[18] = 1;		indices[19] = 5;
		indices[20] = 3;		indices[21] = 7;
		indices[22] = 2;		indices[23] = 6;
	}
	pGFX->SetWorldTransforms( 0, &matPlacement, 1 );
	pGFX->EnableSpecular( false );
	return pGFX->DrawTemp();
}
inline DWORD CheckLessZero( float fVal )
{
  return ( bit_cast<DWORD>( fVal ) & 0x80000000 );
}
inline DWORD CheckForViewVolume( const CVec3 &vCenter, float fRadius, const SPlane *pViewVolumePlanes )
{
	DWORD dwClipFlags = 0;
	float fDist = pViewVolumePlanes[GFXCP_LEFT].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
	dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_LEFT );
	fDist = pViewVolumePlanes[GFXCP_RIGHT].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
		dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_RIGHT );
	fDist = pViewVolumePlanes[GFXCP_TOP].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
		dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_TOP );
	fDist = pViewVolumePlanes[GFXCP_BOTTOM].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
		dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_BOTTOM );
	/*
	fDist = pViewVolumePlanes[GFXCP_NEAR].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
		dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_NEAR );
	fDist = pViewVolumePlanes[GFXCP_FAR].GetDistanceToPoint( vCenter );
	if ( fDist < -fRadius )
		return GFXCP_OUT;
	else
		dwClipFlags |= CheckLessZero( fDist - fRadius ) >> ( 31 - GFXCP_FAR );
		*/
	return dwClipFlags;
}
DWORD CMeshVisObj::CheckForViewVolume( const SPlane *pViewVolumePlanes )
{
	const SGFXBoundSphere &sphere = pMesh->GetBS();
	CVec3 vCenter;
	matPlacement.RotateHVector( &vCenter, sphere.vCenter );
	return ::CheckForViewVolume( vCenter, sphere.fRadius, pViewVolumePlanes );
}
void CMeshVisObj::RepositionIcons()
{
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
}
void CMeshVisObj::RepositionIconsLocal( DWORD placement )
{
	const SGFXAABB &aabb = pMesh->GetAABB();
	CTRect<float> rcRect( -13, aabb.vCenter.z + aabb.vHalfSize.z, 13, aabb.vCenter.z - aabb.vHalfSize.z );
	CObjVisObj::RepositionIconsLocal( placement, rcRect );
}
void CMeshVisObj::AddEffector( int nID, ISceneMatrixEffector *pEffector, int nPart )
{
	RemoveEffector( nID, nPart );
	if ( pEffector )
		effectors.push_back( SMatrixEffector(nID, pEffector, nPart) );
}
void CMeshVisObj::RemoveEffector( int nID, int nPart )
{
	for ( CEffectorsList::iterator it = effectors.begin(); it != effectors.end(); ++it )
	{
		if ( (it->nID == nID) && (it->nPart == nPart) )
		{
			effectors.erase( it );
			return;
		}
	}
}
void CMeshVisObj::AddMaterialEffector( ISceneMaterialEffector *pEffector )
{
	if ( pMaterialEffector == 0 )
	{
		pMaterialEffector = pEffector;
	}
}
void CMeshVisObj::RemoveMaterialEffector()
{
	pMaterialEffector = 0;
	SetSpecular( 0xFF000000 );
	SetOpacity( 0xFF );
}
const SHMatrix CMeshVisObj::GetBasePlacement()
{ 
	SHMatrix matResult; 
	pAnim->GetBaseMatrix( matPlacement1, &matResult ); 
	return matResult; 
}
const SHMatrix* CMeshVisObj::GetExtMatrices( const SHMatrix &matExternal )
{
	SHMatrix matrix;
	Multiply( &matrix, matExternal, matPlacement1 );
	return pAnim->GetMatrices( matrix );
}
int CMeshVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 20, static_cast<CObjVisObj*>(this) );
	saver.Add( 1, &pMesh );
	saver.Add( 2, &pAnim );
	saver.Add( 3, &pTexture );
	saver.Add( 4, &matPlacement );
	saver.Add( 5, &matPlacement1 );
	saver.Add( 6, &quat );
	saver.Add( 10, &effectors );
	saver.Add( 11, &vScale );
	saver.Add( 12, &bHasScale );
	saver.Add( 13, &pMaterialEffector );
	return 0;
}
