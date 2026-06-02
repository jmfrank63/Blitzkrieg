#include "stdafx.h"

#include "AIEditorInternal.h"
#include "AILogicInternal.h"
#include "AIUnit.h"
#include "StaticObjects.h"
#include "Entrenchment.h"
#include "UnitsIterators2.h"
#include "UnitsIterators.h"
#include "Updater.h"
#include "UnitStates.h"
#include "Commands.h"
#include "AIStaticMap.h"
#include "Soldier.h"
#include "Formation.h"
#include "AIUnit.h"
#include "Updater.h"
#include "Diplomacy.h"
#include "StaticObjectsIters.h"
extern CAILogic *pAILogic;
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CStaticMap theStaticMap;
extern CUpdater updater;
extern CDiplomacy theDipl;
void CAIEditor::Init( const struct STerrainInfo &terrainInfo )
{
	pGameSegment = GetSingleton<IGameTimer>()->GetGameSegmentTimer();
	pAILogic->InitEditor( terrainInfo );
}
bool CAIEditor::AddNewObject( const SMapObjectInfo &object, IRefCount **pObject )
{
	if ( !IsObjectInsideOfMap( object ) )
	{
		*pObject = 0;
		return false;
	}
	
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	curTime = GetAIGetSegmTime( pGameSegment );
	*pObject = pAILogic->AddObject( object, pIDB, 0, false, true, 0 );

	if ( pIDB->GetDesc( object.szName.c_str() )->eGameType == SGVOGT_ENTRENCHMENT )
		updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, static_cast<IUpdatableObj*>(*pObject) );

	return false;
}
void CAIEditor::SetPlayer( IRefCount *pObj, const int nPlayer )
{
	if ( CStaticObject *pStaticObj = dynamic_cast<CStaticObject*>(pObj) )
		pStaticObj->SetPlayerForEditor( nPlayer );
	else if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj) )
		pUnit->SetPlayerForEditor( nPlayer );
}
void CAIEditor::SetDiplomacies( const std::vector<BYTE> &playerParty )
{
	theDipl.SetDiplomaciesForEditor( playerParty );
}
bool CAIEditor::AddNewEntrencment( IRefCount** segments, const int nLen, IRefCount **pObject )
{
	CPtr<CFullEntrenchment> pFullEntrenchment = new CFullEntrenchment();
	*pObject = theStatObjs.AddNewEntrencment( segments, nLen, pFullEntrenchment );
	pFullEntrenchment->SetVisible();

	return false;
}
void CAIEditor::LoadEntrenchments( const std::vector<SEntrenchmentInfo> &entrenchments )
{
	pAILogic->LoadEntrenchments( entrenchments );
}
void CAIEditor::RecalcPassabilityForPlayer( CArray2D<BYTE> *array, const int nPlayer )
{
	theStatObjs.RecalcPassabilityForPlayer( array, nPlayer );
}
void GetUnitRectByStats( const SUnitBaseRPGStats *pStats, const CVec2 &vCenter, const WORD wDir, SRect *pRect )
{
	const float length = pStats->vAABBHalfSize.y;
	const float width = pStats->vAABBHalfSize.x;
	
	const CVec2 vDir( GetVectorByDirection( wDir ) );
	const CVec2 dirPerp( vDir.y, -vDir.x );
	const CVec2 vShift( vDir * pStats->vAABBCenter.y + dirPerp * pStats->vAABBCenter.x );

	pRect->InitRect( vCenter + vShift, vDir, length, width );
}
bool CAIEditor::IsRectInsideOfMap( const SRect &unitRect ) const
{
	return
			theStaticMap.IsPointInside( unitRect.v1 ) &&
			theStaticMap.IsPointInside( unitRect.v2 ) &&
			theStaticMap.IsPointInside( unitRect.v3 ) &&
			theStaticMap.IsPointInside( unitRect.v4 );
}
bool CAIEditor::MoveObject( IRefCount *pObject, short x, short y )
{
	if ( CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObject ) )
	{
		if ( pUnit->CanSetNewCoord( CVec3( x, y, 0.0f ) ) )
		{
			SRect rect;
			GetUnitRectByStats( pUnit->GetStats(), CVec2( x, y ), pUnit->GetDir(), &rect );
			if ( IsRectInsideOfMap( rect ) )
			{			
				pUnit->SetNewCoordinatesForEditor( CVec3( x, y, 0.0f ) );
				pUnit->LockTilesForEditor();
			}
		}
	}
	else if ( CFormation *pFormation = dynamic_cast<CFormation*>( pObject ) )
	{
		pFormation->SetNewCoordinates( CVec3( x, y, 0.0f ) );
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			CSoldier *pSoldier = (*pFormation)[i];
			MoveObject( pSoldier, pFormation->GetUnitCoord( i ).x, pFormation->GetUnitCoord( i ).y );
		}
	}
	else if ( CExistingObject *pStObject = dynamic_cast<CExistingObject*>( pObject ) )
	{
		if ( pStObject->CanBeMovedTo( CVec2( x, y ) ) )
			pStObject->SetNewPlacement( CVec2( x, y ), pStObject->GetDir() );
	}
	else
		NI_ASSERT_T( false, "Unknown object" );

	return false;
}
void CAIEditor::DeleteObject( IRefCount *pObject )
{
	if ( CCommonUnit *pUnit = dynamic_cast<CAIUnit*>( pObject ) )
		static_cast<CCommonUnit*>(pUnit)->Disappear();
	else if ( CExistingObject *pStObject = dynamic_cast<CExistingObject*>( pObject ) )
		pStObject->DeleteForEditor();
	else
		NI_ASSERT_T( false, "Unknown object" );
}
bool CAIEditor::TurnObject( IRefCount *pObject, const WORD wDir )
{
	if ( CFormation *pFormation = dynamic_cast<CFormation*>( pObject ) )
	{
		pFormation->UpdateDirection( GetVectorByDirection( wDir ) );
		for ( int i = 0; i < pFormation->Size(); ++i )
		{
			MoveObject( (*pFormation)[i], pFormation->GetUnitCoord( i ).x, pFormation->GetUnitCoord( i ).y );
			TurnObject( (*pFormation)[i], pFormation->GetUnitDir( i ) );
		}
	}
	else if ( CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObject ) )	
	{
		if ( pUnit->CanSetNewDir( GetVectorByDirection( wDir ) ) )
		{
			SRect rect;
			GetUnitRectByStats( pUnit->GetStats(), pUnit->GetCenter(), wDir, &rect );
			if ( IsRectInsideOfMap( rect ) )
				pUnit->UpdateDirectionForEditor( GetVectorByDirection( wDir ) );
		}
	}
	else if ( CExistingObject *pStObject = dynamic_cast<CExistingObject*>( pObject ) )
		pStObject->SetNewPlacement( pStObject->GetCenter(), wDir );

	return false;
}
void CAIEditor::DamageObject( IRefCount *pObject, const float fHP )
{
	if ( CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObject ) )
		pUnit->TakeEditorDamage( fHP );
	else if ( CFormation *pFormation = dynamic_cast<CFormation*>( pObject ) )
	{
		for ( int i = 0; i < pFormation->Size(); ++i )
			DamageObject( (*pFormation)[i], fHP );
	}
	else if ( CStaticObject *pStObject = dynamic_cast<CStaticObject*>( pObject ) )
		pStObject->TakeEditorDamage( fHP );
	else
		NI_ASSERT_T( false, "Can't damage something that isn't a unit" );
}
float CAIEditor::GetObjectHP( IRefCount *pObject )
{
	if ( CAIUnit *pUnit = dynamic_cast<CAIUnit*>( pObject ) )
		return pUnit->GetHitPoints();
	else if ( CStaticObject *pStObject = dynamic_cast<CStaticObject*>( pObject ) )
			return pStObject->GetHitPoints();
		else
			NI_ASSERT_T( false, "Unknown object" );

	return 0;
}
int STDCALL CAIEditor::GetObjectScriptID( IRefCount *pObject )
{
	return pAILogic->GetScriptID( dynamic_cast<IUpdatableObj*>( pObject ) );
}
void CAIEditor::HandOutLinks()
{
	CLinkObject::ClearLinks();
	
	int id = 1;
	for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
	{
		(*iter)->SetLink( id++ );

		if ( (*iter)->IsInFormation() )
			(*iter)->GetFormation()->SetLink( id++ );
	}
	
	for ( CStObjGlobalIter<false> iter; !iter.IsFinished(); iter.Iterate() )
		(*iter)->SetLink( id++ );
}
IRefCount* CAIEditor::LinkToAI( const int ID )
{
	IRefCount *pResult = CLinkObject::GetObjectByLink( ID );
	NI_ASSERT_T( pResult != 0, NStr::Format( "Wrong link passed (%d)\n", ID ) );

	return pResult;
}
int CAIEditor::AIToLink( IRefCount *pObj )
{
	NI_ASSERT_T( dynamic_cast<CLinkObject*>( pObj ) != 0, NStr::Format("Wrong object of type \"%s\" - CLinkObject expected", typeid(*pObj).name()) );
	return static_cast<CLinkObject*>(pObj)->GetLink();
}
IRefCount* CAIEditor::GetFormationOfUnit( IRefCount *pObject )
{
	if ( CAIUnit* pUnit = dynamic_cast<CAIUnit*>(pObject) )
		return pUnit->GetFormation();
	else
		return 0;
}
bool CAIEditor::IsFormation( IRefCount *pObject ) const
{
	return dynamic_cast<CFormation*>( pObject );
}
void CAIEditor::GetUnitsInFormation( IRefCount *pObject, IRefCount ***pUnits, int *pnLen )
{
	NI_ASSERT_T( dynamic_cast<CFormation*>(pObject) != 0, "Non formation passed to GetUnitsInFormation" );
	CFormation *pFormation = static_cast<CFormation*>(pObject);

	*pUnits = GetTempBuffer<IRefCount*>( pFormation->Size() );
	*pnLen = pFormation->Size();

	for ( int i = 0; i < *pnLen; ++i )
		(*pUnits)[i] = (*pFormation)[i];
}
const CVec2& CAIEditor::GetCenter( IRefCount *pObj ) const
{
	if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj) )
		return pUnit->GetCenter();
	else if ( CStaticObject *pObject = dynamic_cast<CStaticObject*>(pObj) )
		return pObject->GetCenter();
	else
		NI_ASSERT_T( false, "Wrong object passed" );
	
	return VNULL2;
}
const WORD CAIEditor::GetDir( IRefCount *pObj ) const
{
	if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj) )
		return pUnit->GetFrontDir();
	else if ( CStaticObject *pObject = dynamic_cast<CStaticObject*>(pObj) )
		return 0;
	else
		NI_ASSERT_T( false, "Wrong object passed" );

	return 0;
}
const int CAIEditor::GetUnitDBID( IRefCount *pObj ) const
{
	NI_ASSERT_T( dynamic_cast<CCommonUnit*>(pObj) != 0, "Wrong object" );

	return static_cast<CCommonUnit*>(pObj)->GetDBID();
}
void GetUnitRectByMapObject( const SMapObjectInfo &object, IObjectsDB *pIDB, const SGDBObjectDesc *pDesc, SRect *pRect )
{
	CGDBPtr<SUnitBaseRPGStats> pStats = static_cast<const SUnitBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );
	GetUnitRectByStats( pStats, CVec2( object.vPos.x, object.vPos.y ), object.nDir, pRect );
}
template<class T>
bool CheckStaticObject( const SMapObjectInfo &object, IObjectsDB *pIDB, const SGDBObjectDesc *pDesc, const T &checkFunc )
{
	CGDBPtr<SObjectBaseRPGStats> pStats = static_cast<const SObjectBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	const CVec2 vOrigin( pStats->GetOrigin( object.nFrameIndex ) );

	if ( object.vPos.x - vOrigin.x + SConsts::TILE_SIZE / 2 < 0.0f )
		return false;
	if ( object.vPos.y - vOrigin.y + SConsts::TILE_SIZE / 2 < 0.0f )
		return false;

	const int nDownX = Max( 0.0f, object.vPos.x - vOrigin.x + SConsts::TILE_SIZE / 2 );
	const int nDownY = Max( 0.0f, object.vPos.y - vOrigin.y + SConsts::TILE_SIZE / 2 );

	const CArray2D<BYTE> &passability = pStats->GetPassability( object.nFrameIndex );
	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			const SVector tile( AICellsTiles::GetTile( nDownX + SConsts::TILE_SIZE*x, nDownY + SConsts::TILE_SIZE*y ) );
			if ( !checkFunc( passability[y][x], tile ) )
				return false;
		}
	}

	return true;
}
class CCheckInside
{
public:
	bool operator()( const BYTE passYX, const SVector &tile ) const { return theStaticMap.IsTileInside( tile ); }
};
class CCheckCanAdd
{
public:
	bool operator()( const BYTE passYX, const SVector &tile ) const
	{
		if ( passYX == 1 )
		{
			return ( !theStaticMap.IsLocked( tile, AI_CLASS_ANY				) &&
							 !theStaticMap.IsLocked( tile, AI_CLASS_HUMAN			) &&
							 !theStaticMap.IsLocked( tile, AI_CLASS_WHEEL			) &&
							 !theStaticMap.IsLocked( tile, AI_CLASS_HALFTRACK ) &&
							 !theStaticMap.IsLocked( tile, AI_CLASS_TRACK		  ) );
		}
		else
			return true;
	}
};
bool CAIEditor::IsObjectInsideOfMap( const SMapObjectInfo &object ) const
{
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();	
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( object.szName.c_str() );
	NI_ASSERT_TF( pDesc != 0, NStr::Format("Can't find DB object description for \"%s\"", object.szName.c_str()), return 0 );
	
	if ( pDesc->eGameType == SGVOGT_UNIT )
	{
		SRect unitRect;
		GetUnitRectByMapObject( object, pIDB, pDesc, &unitRect );

		return IsRectInsideOfMap( unitRect );
	}
	else if ( pDesc->eGameType != SGVOGT_SQUAD && pDesc->eGameType != SGVOGT_ENTRENCHMENT )
	{
		const CCheckInside check;	
		return CheckStaticObject( object, pIDB, pDesc, check );
	}
	else
		return true;
}
bool CAIEditor::CanAddObject( const struct SMapObjectInfo &object ) const
{
	if ( !IsObjectInsideOfMap( object ) )
		return false;
	else
	{
		CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();	
		CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( object.szName.c_str() );
		NI_ASSERT_TF( pDesc != 0, NStr::Format("Can't find DB object description for \"%s\"", object.szName.c_str()), return 0 );
		
		if ( pDesc->eGameType == SGVOGT_UNIT )
		{
			SRect unitRect;
			GetUnitRectByMapObject( object, pIDB, pDesc, &unitRect );

			return 
				!IsRectOnLockedTiles( unitRect, AI_CLASS_ANY			 ) &&
				!IsRectOnLockedTiles( unitRect, AI_CLASS_HUMAN		 ) &&
				!IsRectOnLockedTiles( unitRect, AI_CLASS_WHEEL		 ) &&
				!IsRectOnLockedTiles( unitRect, AI_CLASS_TRACK		 ) &&
				!IsRectOnLockedTiles( unitRect, AI_CLASS_HALFTRACK );
		}
		else if ( pDesc->eGameType != SGVOGT_SQUAD && pDesc->eGameType != SGVOGT_ENTRENCHMENT )
		{
			const CCheckCanAdd check;
			return CheckStaticObject( object, pIDB, pDesc, check );
		}
		else
			return true;
	}
}
void CAIEditor::Clear()
{
	pAILogic->Clear();
}
void CAIEditor::ApplyPattern( const struct SVAPattern &rPattern )
{
	theStaticMap.ApplyPattern( rPattern );
}
void CAIEditor::UpdateAllHeights()
{
	theStaticMap.UpdateAllHeights();
}
bool CAIEditor::ToggleShow( const int nShowType )
{
	return pAILogic->ToggleShow( nShowType );
}
void CAIEditor::UpdateTerrain( const CTRect<int> &rect, const STerrainInfo &terrainInfo )
{
	int nMinX = 2 * rect.x1;
	int nMinY = theStaticMap.GetSizeY() - 2 * rect.y2;
	int nMaxX = 2 * rect.x2 - 1;
	int nMaxY = theStaticMap.GetSizeY() - 2 * rect.y1 - 1;

	const CVec2 vCenter( AICellsTiles::GetPointByTile( 0.5f * ( nMinX + nMaxX ), 0.5f * ( nMinY + nMaxY ) ) );
	const CVec2 vAABBHalfSize( 0.5f * ( nMaxX - nMinX + 1 ) * SConsts::TILE_SIZE, 0.5f * ( nMaxY - nMinY + 1 ) * SConsts::TILE_SIZE );
	
 	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vCenter, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		pUnit->UnlockTiles( false );
		const SVector tile = pUnit->GetTile();
		nMinX = Min( nMinX, (int)Max( tile.x - SConsts::MAX_UNIT_RADIUS - 1, 0 ) );
		nMinY = Min( nMinY, (int)Max( tile.y - SConsts::MAX_UNIT_RADIUS - 1, 0 ) );
		nMaxX = Max( nMaxX, (int)Min( tile.x + SConsts::MAX_UNIT_RADIUS + 1, theStaticMap.GetSizeX() - 1 ) );
		nMaxY = Max( nMaxY, (int)Min( tile.y + SConsts::MAX_UNIT_RADIUS + 1, theStaticMap.GetSizeY() - 1 ) );
	}

	for ( CStObjCircleIter<false> iter( vCenter, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;
		if ( pObj->GetObjectType() != ESOT_BRIDGE_SPAN )
		{
			pObj->UnlockTiles( true );

			CTilesSet tiles;
			pObj->GetCoveredTiles( &tiles );
			for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
			{
				const SVector tile = *iter;
				nMinX = Min( nMinX, tile.x );
				nMinY = Min( nMinY, tile.y );
				nMaxX = Max( nMaxX, tile.x );
				nMaxY = Max( nMaxY, tile.y );
			}
		}
	}

	for ( TVSOList::const_iterator iter = terrainInfo.rivers.begin(); iter != terrainInfo.rivers.end(); ++iter )
		theStaticMap.UpdateRiverPassability( *iter, false, false );

	for ( int x = nMinX; x <= nMaxX; ++x )
	{
		for ( int y = nMinY; y <= nMaxY; ++y )
			theStaticMap.RemoveTerrainPassability( x, y );
	}

	theStaticMap.UpdateMaxesForRemovedRect( nMinX, nMinY, nMaxX, nMaxY );
	
	for ( int x = nMinX; x <= nMaxX; ++x )
	{
		for ( int y = nMinY; y <= nMaxY; ++y )
		{
			const SVector rotatedTile( x, theStaticMap.GetSizeY() - y - 1 );
			theStaticMap.SetTerrainPassability( x, y, theStaticMap.GetTerrainPassTypeByTileNum( terrainInfo.tiles[rotatedTile.y/2][rotatedTile.x/2].tile ) );
		}
	}

	for ( TVSOList::const_iterator iter = terrainInfo.rivers.begin(); iter != terrainInfo.rivers.end(); ++iter )
		theStaticMap.UpdateRiverPassability( *iter, true, false );

	for ( CStObjCircleIter<false> iter( vCenter, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;		
		if ( pObj->GetObjectType() != ESOT_BRIDGE_SPAN )
			pObj->LockTiles( true );
	}

	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vCenter, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;

		pUnit->LockTiles( false );
		if ( !pUnit->GetStats()->IsInfantry() )
			pUnit->ForceLockingTiles( false );
	}

	theStaticMap.UpdateMaxesForAddedRect( nMinX, nMinY, nMaxX, nMaxY );
}
void CAIEditor::DeleteRiver( const SVectorStripeObject &river )
{
	theStaticMap.UpdateRiverPassability( river, false, true );
}
void CAIEditor::AddRiver( const SVectorStripeObject &river )
{
	theStaticMap.UpdateRiverPassability( river, true, true );
}
