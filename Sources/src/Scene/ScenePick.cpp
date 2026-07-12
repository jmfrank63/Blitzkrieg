#include "StdAfx.h"

#include "SceneInternal.h"
typedef std::pair<IVisObj*, CVec2> CPickedObject;
typedef std::list<CPickedObject> CPickedObjectsList;
class CCheckObjectTypeFunctional
{
	const CVisObjDescMap &objdescs;				// all object descriptors
	const EObjGameType type;							// type to check with
public:
	CCheckObjectTypeFunctional( const CVisObjDescMap &_objdescs, const EObjGameType _type )
		: objdescs( _objdescs ), type( _type ) {  }
	bool operator()( const CPickedObject &obj ) const
	{
		CVisObjDescMap::const_iterator it = objdescs.find( obj.first );
		if ( it == objdescs.end() )
			return true;											// unknown object
		return it->second.gametype != type;
	}
};
class CCheckObjectVisibleFunctional
{
	const CScene *pScene;
public:
	explicit CCheckObjectVisibleFunctional( const CScene *_pScene ) : pScene( _pScene ) {  }
	bool operator()( const CPickedObject &obj ) const 
	{
		const SVisObjDesc *pDesc = pScene->GetDesc( obj.first );
		NI_ASSERT_TF( pDesc != 0, "wrong object - no descriptor", return true );
		return pDesc->gametype == SGVOGT_UNIT ? !pScene->IsVisible( static_cast<IObjVisObj*>(obj.first) ) : false;
	}
};
template <class TAreaMap>
void Pick( const CVec2 &pos2, int nCellX, int nCellY, const SHMatrix &matrix, 
					 CPickedObjectsList *pPickedObjects, const TAreaMap &area )
{
	typename TAreaMap::CDataList &data = area[nCellY][nCellX];
	for ( typename TAreaMap::CDataList::iterator it = data.begin(); it != data.end(); ++it )
	{
		CVec2 shift;
		if ( (*it)->IsHit( matrix, pos2, &shift ) )
			pPickedObjects->push_back( std::pair<IVisObj*, CVec2>( *it, shift ) );
	}
}
template <class TAreaMap>
void Pick( const CVec2 &pos2, const CVec3 &pos3, const SHMatrix &matrix, 
					 CPickedObjectsList *pPickedObjects, const TAreaMap &area )
{
	int nCellX = pos3.x / AREA_MAP_CELL_SIZE;
	int nCellY = pos3.y / AREA_MAP_CELL_SIZE;
	for ( int i = -1; i != 2; ++i )
	{
		if ( (nCellY + i < 0) || (nCellY + i >= area.GetSizeY()) )
			continue;
		for ( int j = -1; j != 2; ++j )
		{
			if ( (nCellX + j < 0) || (nCellX + j >= area.GetSizeX()) )
				continue;
			Pick( pos2, nCellX + j, nCellY + i, matrix, pPickedObjects, area );
		}
	}
}
template <class TAreaMap>
void Pick( const CTRect<float> &rect2, int nCellX, int nCellY, const SHMatrix &matrix, 
					 CPickedObjectsList *pPickedObjects, const TAreaMap &area )
{
	typename TAreaMap::CDataList &data = area[nCellY][nCellX];
	for ( typename TAreaMap::CDataList::iterator it = data.begin(); it != data.end(); ++it )
	{
		if ( (*it)->IsHit( matrix, rect2 ) )
			pPickedObjects->push_back( std::pair<IVisObj*, CVec2>( *it, VNULL2 ) );
	}
}
template <class TAreaMap>
void PickUpdated( const CTRect<float> &rect2, int nCellX, int nCellY, DWORD time, const SHMatrix &matrix, 
					        CPickedObjectsList *pPickedObjects, const TAreaMap &area )
{
	typename TAreaMap::CDataList &data = area[nCellY][nCellX];
	for ( typename TAreaMap::CDataList::iterator it = data.begin(); it != data.end(); ++it )
	{
		(*it)->Update( time );
		if ( (*it)->IsHit( matrix, rect2 ) )
			pPickedObjects->push_back( std::pair<IVisObj*, CVec2>( *it, VNULL2 ) );
	}
}
void Pick( const CVec2 &pos2, const SHMatrix &matrix, 
					 CPickedObjectsList *pPickedObjects, CMeshObjList &objects )
{
	for ( CMeshObjList::iterator it = objects.begin(); it != objects.end(); ++it )
	{
		CVec2 shift;
		if ( (*it)->IsHit(matrix, pos2, &shift) )
			pPickedObjects->push_back( std::pair<IVisObj*, CVec2>(*it, shift) );
	}
}
void PickUpdated( const CTRect<float> &rect2, DWORD time, const SHMatrix &matrix, 
					        CPickedObjectsList *pPickedObjects, CMeshObjList &objects )
{
	for ( CMeshObjList::iterator it = objects.begin(); it != objects.end(); ++it )
	{
		(*it)->Update( time );
		if ( (*it)->IsHit( matrix, rect2 ) )
			pPickedObjects->push_back( std::pair<IVisObj*, CVec2>( *it, VNULL2 ) );
	}
}
void CScene::Pick( const CVec2 &point, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible )
{
	UpdateTransformMatrix();
	CVec3 pos3;
	GetPos3( &pos3, point );
	CPickedObjectsList picked;
	switch ( type )
	{
		case SGVOGT_UNIT:
			::Pick( point, pos3, matTransform, &picked, areaUnits );
			::Pick( point, matTransform, &picked, outboundObjects2 );
			break;
		case SGVOGT_FORTIFICATION:
		case SGVOGT_BUILDING:
		case SGVOGT_OBJECT:
		case SGVOGT_MINE:
			::Pick( point, pos3, matTransform, &picked, spriteObjectsArea );
			break;
		case SGVOGT_TERRAOBJ:
			::Pick( point, pos3, matTransform, &picked, terraObjectsArea );
			break;
		case SGVOGT_EFFECT:
			::Pick( point, pos3, matTransform, &picked, effectsArea );
			break;
		case SGVOGT_ENTRENCHMENT:
			::Pick( point, pos3, matTransform, &picked, meshGraveyardArea );
			break;
		case SGVOGT_UNKNOWN:
			::Pick( point, pos3, matTransform, &picked, areaUnits );
			::Pick( point, pos3, matTransform, &picked, meshGraveyardArea );
			::Pick( point, pos3, matTransform, &picked, spriteObjectsArea );
			::Pick( point, pos3, matTransform, &picked, terraObjectsArea );
			::Pick( point, pos3, matTransform, &picked, effectsArea );
			::Pick( point, matTransform, &picked, outboundObjects2 );
			break;
	}
	if ( (type == SGVOGT_BUILDING) || (type == SGVOGT_OBJECT) || (type == SGVOGT_ENTRENCHMENT) || (type == SGVOGT_MINE) )
		picked.remove_if( CCheckObjectTypeFunctional(objdescs, type) );
	if ( (type == SGVOGT_UNKNOWN) && bVisible )
		picked.remove_if( CCheckObjectVisibleFunctional(this) );
	if ( picked.empty() )
	{
		*pnNumObjects = 0;
		*ppObjects = 0;
		return;
	}
	*pnNumObjects = picked.size();
	*ppObjects = GetTempBuffer< std::pair<IVisObj*, CVec2> >( *pnNumObjects );
	int i = 0;
	for ( CPickedObjectsList::const_iterator it = picked.begin(); it != picked.end(); ++it )
		(*ppObjects)[i++] = *it;
}
void CScene::Pick( const CTRect<float> &rcRect, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible )
{
	UpdateTransformMatrix();
	CVec3 vTL, vBR;
	{
		CTPoint<float> ptTL = rcRect.GetLeftTop();
		GetPos3( &vTL, CVec2(ptTL.x, ptTL.y) );
		CTPoint<float> ptBR = rcRect.GetRightBottom();
		GetPos3( &vBR, CVec2(ptBR.x, ptBR.y) );
	}
	CVec3 vCamera = ( vTL + vBR ) / 2.0f;
	const float fWidth = rcRect.Width();
	const float fHeight = rcRect.Height() * 2;
	CVec2 vCameraX( fWidth / FP_SQRT_2, fWidth / FP_SQRT_2 ), vCameraY( -fHeight / FP_SQRT_2, fHeight / FP_SQRT_2 );
	CPatchesList vispatches;
	SelectPatches2( vCamera, vCameraX, vCameraY, areaUnits.GetSizeX(), areaUnits.GetSizeY(), AREA_MAP_CELL_SIZE, &vispatches );
	DWORD time = pTimer->GetGameTime();
	CPickedObjectsList picked;
	for ( CPatchesList::const_iterator it = vispatches.begin(); it != vispatches.end(); ++it )
	{
		switch ( type )
		{
			case SGVOGT_UNIT:
				::PickUpdated( rcRect, it->first, it->second, time, matTransform, &picked, areaUnits );
				break;
			case SGVOGT_FORTIFICATION:
			case SGVOGT_BUILDING:
			case SGVOGT_OBJECT:
			case SGVOGT_MINE:
				::Pick( rcRect, it->first, it->second, matTransform, &picked, spriteObjectsArea );
				break;
			case SGVOGT_TERRAOBJ:
				::Pick( rcRect, it->first, it->second, matTransform, &picked, terraObjectsArea );
				break;
			case SGVOGT_EFFECT:
				::Pick( rcRect, it->first, it->second, matTransform, &picked, effectsArea );
				break;
			case SGVOGT_ENTRENCHMENT:
				::Pick( rcRect, it->first, it->second, matTransform, &picked, meshGraveyardArea );
				break;
			case SGVOGT_UNKNOWN:
				::PickUpdated( rcRect, it->first, it->second, time, matTransform, &picked, areaUnits );
				::PickUpdated( rcRect, it->first, it->second, time, matTransform, &picked, meshGraveyardArea );
				::Pick( rcRect, it->first, it->second, matTransform, &picked, spriteObjectsArea );
				::Pick( rcRect, it->first, it->second, matTransform, &picked, terraObjectsArea );
				::Pick( rcRect, it->first, it->second, matTransform, &picked, effectsArea );
				break;
		}
	}
	if ( type == SGVOGT_UNIT ) 
		::PickUpdated( rcRect, time, matTransform, &picked, outboundObjects2 );
	if ( (type == SGVOGT_BUILDING) || (type == SGVOGT_OBJECT) || (type == SGVOGT_ENTRENCHMENT) || (type == SGVOGT_MINE) )
		picked.remove_if( CCheckObjectTypeFunctional(objdescs, type) );
	if ( (type == SGVOGT_UNIT) && bVisible )
		picked.remove_if( CCheckObjectVisibleFunctional(this) );
	if ( picked.empty() )
	{
		*pnNumObjects = 0;
		*ppObjects = 0;
		return;
	}
	*pnNumObjects = picked.size();
	*ppObjects = GetTempBuffer< std::pair<IVisObj*, CVec2> >( *pnNumObjects );
	int i = 0;
	for ( CPickedObjectsList::const_iterator it = picked.begin(); it != picked.end(); ++it )
		(*ppObjects)[i++] = *it;
}
void CScene::SelectPatches2( const CVec3 &vCamera, const CVec2 &vCameraX, const CVec2 &vCameraY, 
																  float fPatchesX, float fPatchesY, float fPatchSize, CPatchesList *pPatches )
{
	const float fPatchHalfAxis = fPatchSize * FP_SQRT_2 / 2.0f; //fCellSizeX * STerrainPatchInfo::nSizeX;
	CVec3 vAxisX, vAxisY;
	GetLineEq( 0, 0, 1, 0, &vAxisX.x, &vAxisX.y, &vAxisX.z );
	GetLineEq( 0, 1, 0, 0, &vAxisY.x, &vAxisY.y, &vAxisY.z );
	const CVec2 vCameraO( vCamera.x, vCamera.y );
	CTRect<int> rcL0Rect;								// level 0 of roughness rect
	{
		const CVec2 point = vCameraO + vCameraY - vCameraX;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.minx = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesX ) );
	}
	{
		const CVec2 point = vCameraO + vCameraY + vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.miny = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesY ) );
	}
	{
		const CVec2 point = vCameraO + vCameraX - vCameraY;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.maxx = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesX ) );
	}
	{
		const CVec2 point = vCameraO - vCameraY - vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.maxy = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesY ) );
	}
	rcL0Rect.Normalize();
	CTRect<int> rcL1Rect;								// level 1 of roughness rect
	const CVec3 vTerraOX( 0, 0, 0 );
	const CVec3 vTerraOY( 0, fPatchesY * fPatchSize, 0 );
	{
		const CVec2 vO = vCameraO + vCameraY;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x + 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOY.x + vLine.y*vTerraOY.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.miny = nDist;
	}
	{
		const CVec2 vO = vCameraO - vCameraY;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x + 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOY.x + vLine.y*vTerraOY.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.maxy = nDist;
	}
	{
		const CVec2 vO = vCameraO - vCameraX;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x - 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOX.x + vLine.y*vTerraOX.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.minx = nDist;
	}
	{
		const CVec2 vO = vCameraO + vCameraX;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x - 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOX.x + vLine.y*vTerraOX.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.maxx = nDist;
	}
	rcL1Rect.Normalize();
	rcL0Rect.x1 = Clamp( rcL0Rect.x1, 0, int(fPatchesX) );
	rcL0Rect.y1 = Clamp( rcL0Rect.y1, 0, int(fPatchesY) );
	rcL0Rect.x2 = Clamp( rcL0Rect.x2 + 3, 0, int(fPatchesX) );
	rcL0Rect.y2 = Clamp( rcL0Rect.y2 + 3, 0, int(fPatchesY) );
	int nVertDist = fPatchesY;
	for ( int j=rcL0Rect.miny; j<rcL0Rect.maxy; ++j )
	{
		for ( int i=rcL0Rect.minx; i<rcL0Rect.maxx; ++i )
		{
			const int nSumY = i + ( nVertDist - j );
			if ( (nSumY < rcL1Rect.miny) || (nSumY > rcL1Rect.maxy + 2) )
				continue;
			const int nSumX = i + j;
			if ( (nSumX < rcL1Rect.minx - 2) || (nSumX > rcL1Rect.maxx + 2) )
				continue;
			pPatches->push_back( std::pair<int, int>( i, j ) );
		}
	}
}
