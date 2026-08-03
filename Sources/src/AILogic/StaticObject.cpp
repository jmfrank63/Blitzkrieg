#include "StdAfx.h"

#include "StaticObject.h"
#include "../Common/Actions.h"
#include "Updater.h"
#include "AIStaticMap.h"
#include "StaticObjects.h"
#include "Diplomacy.h"
#include "Shell.h"
#include "Cheats.h"
#include "AIWarFog.h"
extern CGlobalWarFog theWarFog;
extern CUpdater updater;
extern CStaticMap theStaticMap;
extern CStaticObjects theStatObjs;
extern CDiplomacy theDipl;
extern SCheats theCheats;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( CStaticObject );
bool CStaticObject::CheckStaticObject( const SObjectBaseRPGStats *pStats, const CVec2 &vPos, const int nFrameIndex )
{
	const CVec2 vOrigin( pStats->GetOrigin( nFrameIndex ) );
	const int nDownX = Max( 0.0f, vPos.x - vOrigin.x + SConsts::TILE_SIZE / 2 );
	const int nDownY = Max( 0.0f, vPos.y - vOrigin.y + SConsts::TILE_SIZE / 2 );

	const CArray2D<BYTE> &passability = pStats->GetPassability( nFrameIndex );
	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			const SVector tile( AICellsTiles::GetTile( nDownX + SConsts::TILE_SIZE*x, nDownY + SConsts::TILE_SIZE*y ) );

			if ( passability[y][x] == 1 && theStaticMap.IsLocked( tile, AI_CLASS_ANY) )
			{
				return false;
			}
		}
	}
	return true;
}
const BYTE CStaticObject::GetPlayer() const 
{ 
	return theDipl.GetNeutralPlayer();
}
const bool CStaticObject::IsVisible( const BYTE cParty ) const
{
	CTilesSet tiles;
	GetTilesForVisibility( &tiles );

	for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
	{
		if ( theWarFog.IsTileVisible( *iter, cParty ) )
			return true;
	}

	return false;
}
void CStaticObject::GetTilesForVisibility( CTilesSet *pTiles ) const
{
	SRect rect;
	GetBoundRect( &rect );
	GetTilesCoveredByRectSides( rect, pTiles );
}
bool CStaticObject::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return
		!theDipl.IsEditorMode() &&
		(	eAction == ACTION_NOTIFY_RPG_CHANGED || 
			eAction == ACTION_NOTIFY_UPDATE_DIPLOMACY ||
			eAction == ACTION_NOTIFY_CHANGE_FRAME_INDEX ||
			eAction == ACTION_NOTIFY_SILENT_DEATH );
}
BASIC_REGISTER_CLASS( CExistingObject );
unsigned long CExistingObject::globalMark = 0;
void CExistingObject::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )
{
	pPlacement->pObj = this;
	pPlacement->dir = GetDir();
	pPlacement->center = GetCenter();
	pPlacement->z = 0;
	pPlacement->fSpeed = 0;
}
void CExistingObject::GetNewUnitInfo( SNewUnitInfo *pNewUnitInfo )
{
	GetPlacement( pNewUnitInfo, 0 );

	pNewUnitInfo->dbID = GetDBID();
	pNewUnitInfo->eDipl = EDI_NEUTRAL;
	pNewUnitInfo->nFrameIndex = GetFrameIndex();
	pNewUnitInfo->fHitPoints = GetHitPoints();
	pNewUnitInfo->fMorale = 0;
	pNewUnitInfo->fResize = 1.0f;
	pNewUnitInfo->nPlayer = GetPlayer();
}
void CExistingObject::SetNewPlacement( const CVec2& center, const WORD dir )	
{ 
	UnlockTiles();	

	SetNewPlaceWithoutMapUpdate( center, dir );
	updater.Update( ACTION_NOTIFY_ST_OBJ_PLACEMENT, this );

	LockTiles();
}
void CExistingObject::TakeEditorDamage( const float fDamage )
{
	if ( fDamage > 0 && fHP > 0 || fDamage < 0 && fHP < GetStats()->fMaxHP )
	{
		fHP -= fDamage;
		if ( fHP < 0 )
			fHP = 0;
		if ( fHP > GetStats()->fMaxHP )
			fHP = GetStats()->fMaxHP;

		updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	}
}
void CExistingObject::Delete()
{
	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	theStatObjs.DeleteInternalObjectInfo( this );
}
void CExistingObject::DeleteForEditor()
{
	updater.Update( ACTION_NOTIFY_DELETED_ST_OBJ, this );
	this->UnlockTiles();
	theStatObjs.DeleteInternalObjectInfoForEditor( this );
}
const int CExistingObject::GetRandArmorByDir( const int nArmorDir, const WORD wAttackDir )
{
	switch ( nArmorDir )
	{
		case 0:
			{
				SRect boundRect;
				GetBoundRect( &boundRect );
				return GetStats()->GetRandomArmor( boundRect.GetSide( wAttackDir ) );
			}
		case 1: return GetStats()->GetRandomArmor( RPG_BOTTOM );
		case 2: return GetStats()->GetRandomArmor( RPG_TOP );
		default: NI_ASSERT_TF( false, "Wrong armor dir", 0 );
	}

	return 0;
}
bool CExistingObject::ProcessCumulativeExpl( CExplosion *pExpl, const int nArmorDir, const bool bFromExpl )
{
	if ( pExpl->GetExplZ() <= 0 && IsPointInside( pExpl->GetExplCoordinates() ) )
	{
		SRect targetRect; 
		GetBoundRect( &targetRect );

		if ( pExpl->GetRandomPiercing() >= GetRandArmorByDir( nArmorDir, pExpl->GetAttackDir() ) || theCheats.GetFirstShoot( pExpl->GetPartyOfShoot() ) == 1 )
		{
			TakeDamage( pExpl->GetRandomDamage(), bFromExpl, pExpl->GetPlayerOfShoot(), pExpl->GetWhoFire() );
			pExpl->AddHitToSend( new CHitInfo( pExpl, this, SAINotifyHitInfo::EHT_HIT, CVec3( pExpl->GetExplCoordinates(), pExpl->GetExplZ() ) ) );
		}
		else
			pExpl->AddHitToSend( new CHitInfo( pExpl, this, SAINotifyHitInfo::EHT_REFLECT, CVec3( pExpl->GetExplCoordinates(), pExpl->GetExplZ() ) ) );

		return true;
	}

	return false;
}
bool CExistingObject::ProcessBurstExpl( CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius )
{
	if ( !ProcessCumulativeExpl( pExpl, nArmorDir, true ) )
	{
		ProcessAreaDamage( pExpl, nArmorDir, fRadius, fSmallRadius );
		return false;
	}
	else
		return true;
}
void CExistingObject::BurnSegment()
{
	const float fDamage = GetStats()->fMaxHP * SConsts::BURNING_SPEED * 0.01f * SConsts::AI_SEGMENT_DURATION;
	fHP = Max( 0.0f, fHP - fDamage );
	if ( fHP <= 0 )
		Die( fDamage );

	updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );

	if ( curTime >= burningEnd )
		theStatObjs.EndBurning( this );
}
void CExistingObject::WasHit()
{
	if ( static_cast<const SStaticObjectRPGStats*>(GetStats())->bBurn && fHP <= GetStats()->fMaxHP / 2.0f )
	{
		burningEnd = curTime + 10000;
		theStatObjs.StartBurning( this );
	}
}
CGivenPassabilityStObject::CGivenPassabilityStObject( const CVec2 &_center, const int dbID, const float _fHP, const int nFrameIndex )
: CExistingObject( nFrameIndex, dbID, _fHP ), center( _center ), bTransparencySet( false )
{ 
}
void CGivenPassabilityStObject::Init()
{
	const CVec2 rectCenter( 0.5f * ( GetUpX() + GetDownX() ), 0.5f * ( GetUpY() + GetDownY() ) );
	const float fLength = 0.5f * ( GetUpX() - GetDownX() );
	const float fWidth = 0.5f * ( GetUpY() - GetDownY() );

	boundRect.InitRect( rectCenter, CVec2( 1, 0 ), fLength, fWidth );

	InitTransparenciesPossibility();
}
void CGivenPassabilityStObject::InitTransparenciesPossibility()
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	const CArray2D<BYTE> &visibility = pStats->GetVisibility( nFrameIndex );

	canSetTransparency.SetSizes( visibility.GetSizeX(), visibility.GetSizeY() );
	canSetTransparency.Set( 0xff );
}
void CGivenPassabilityStObject::GetRPGStats( SAINotifyRPGStats *pStats ) 
{ 
	pStats->pObj = this;
	pStats->fHitPoints = fHP;
	pStats->time = curTime;
}
const int CGivenPassabilityStObject::GetVisDownX() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Max( 0, int(center.x - pStats->GetVisOrigin( nFrameIndex ).x + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetVisUpX() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );	
	return Min( theStaticMap.GetSizeX() * SConsts::TILE_SIZE, int( center.x - pStats->GetVisOrigin( nFrameIndex ).x + SConsts::TILE_SIZE * pStats->GetPassability( nFrameIndex ).GetSizeX() + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetVisDownY() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Max( 0, int(center.y - pStats->GetVisOrigin( nFrameIndex ).y + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetVisUpY() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Min( theStaticMap.GetSizeY() * SConsts::TILE_SIZE, int(center.y - pStats->GetVisOrigin( nFrameIndex ).y + SConsts::TILE_SIZE * pStats->GetPassability( nFrameIndex ).GetSizeY() + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetDownX() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Max( 0, int(center.x - pStats->GetOrigin( nFrameIndex ).x + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetUpX() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );	
	return Min( theStaticMap.GetSizeX() * SConsts::TILE_SIZE, int( center.x - pStats->GetOrigin( nFrameIndex ).x + SConsts::TILE_SIZE * pStats->GetPassability( nFrameIndex ).GetSizeX() + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetDownY() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Max( 0, int(center.y - pStats->GetOrigin( nFrameIndex ).y + SConsts::TILE_SIZE / 2 ) );
}
const int CGivenPassabilityStObject::GetUpY() const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	return Min( theStaticMap.GetSizeY() * SConsts::TILE_SIZE, int(center.y - pStats->GetOrigin( nFrameIndex ).y + SConsts::TILE_SIZE * pStats->GetPassability( nFrameIndex ).GetSizeY() + SConsts::TILE_SIZE / 2 ) );
}
bool CGivenPassabilityStObject::CanBeMovedTo( const CVec2 &newCenter ) const
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	const int downX = GetDownX();
	const int downY = GetDownY();

	const CArray2D<BYTE>& passability = pStats->GetPassability( nFrameIndex );
	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			if ( !theStaticMap.IsPointInside( CVec2( downX + newCenter.x - GetCenter().x + SConsts::TILE_SIZE*x, downY + newCenter.y - GetCenter().y + SConsts::TILE_SIZE*y ) ) )
				return false;
		}
	}

	return true;
}
void CGivenPassabilityStObject::LockTiles( bool bInitialization )
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );	
	const int downX = GetDownX();
	const int downY = GetDownY();

	const CArray2D<BYTE>& passability = pStats->GetPassability( nFrameIndex );
	lockInfo.SetSizes( passability.GetSizeX(), passability.GetSizeY() );
	lockInfo.SetZero();
	
	lockTypes = 0;
	bPartially = false;
	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			if ( passability[y][x] == 1 )
			{
				const SVector tile( AICellsTiles::GetTile( downX + SConsts::TILE_SIZE*x, downY + SConsts::TILE_SIZE*y ) );				
				const BYTE tileInfo = theStaticMap.GetTileLockInfo( tile );
				lockInfo[y][x] = ~tileInfo & pStats->dwAIClasses & AI_CLASS_ANY;

				if ( ( lockInfo[y][x] & pStats->dwAIClasses ) != pStats->dwAIClasses )
					bPartially = true;
				lockTypes |= lockInfo[y][x];

				theStaticMap.LockTile( tile.x, tile.y, lockInfo[y][x] );
			}
		}
	}

	if ( !bInitialization )	
	{
		if ( bPartially )
		{
			for ( int i = 1; i < 16; i *= 2 )
			{
				theStaticMap.UpdateMaxesForAddedStObject
				(
					Max( GetDownX() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ), 
					Min( GetUpX() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeX() - 1 ),
					Max( GetDownY() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ),
					Min( GetUpY() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeY() - 1 ),
					i
				);
			}
		}
		
		theStaticMap.UpdateMaxesForAddedStObject
		(
			Max( GetDownX() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ), 
			Min( GetUpX() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeX() - 1 ),
			Max( GetDownY() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ),
			Min( GetUpY() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeY() - 1 ),
			lockTypes
		);
	}
}
void CGivenPassabilityStObject::UnlockTiles(  bool bInitialization ) 
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
	const int downX = GetDownX();
	const int downY = GetDownY();
	
	const CArray2D<BYTE> &passability = pStats->GetPassability( nFrameIndex );
	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			if ( passability[y][x] == 1 )
			{
				const SVector tile( AICellsTiles::GetTile( downX + SConsts::TILE_SIZE*x, downY + SConsts::TILE_SIZE*y ) );
				if ( y < lockInfo.GetSizeY() && x < lockInfo.GetSizeX() )
					theStaticMap.UnlockTile( tile.x, tile.y, lockInfo[y][x] );
			}
		}
	}

	NI_ASSERT_SLOW_T( dynamic_cast<const SStaticObjectRPGStats*>(GetStats()) != 0, NStr::Format( "Object \"%s\" of type \"%s\" can't lock tiles", GetStats()->szKeyName.c_str(), typeid(*GetStats()).name() ) );
	theStaticMap.UpdateTerrainPassabilityRect( downX, downY, downX + passability.GetSizeX(), downY + passability.GetSizeY(), false );

	if ( !bInitialization )
	{
		if ( bPartially )
		{
			for ( int i = 1; i < 16; i *= 2 )
			{
				theStaticMap.UpdateMaxesForRemovedStObject
				(
					Max( GetDownX() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ), 
					Min( GetUpX() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeX() - 1 ),
					Max( GetDownY() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ),
					Min( GetUpY() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeY() - 1 ),
					i
				);
			}
		}

		theStaticMap.UpdateMaxesForRemovedStObject
		(
			Max( GetDownX() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ), 
			Min( GetUpX() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeX() - 1 ),
			Max( GetDownY() / SConsts::TILE_SIZE - SConsts::MAX_UNIT_TILE_RADIUS - 1, 0 ),
			Min( GetUpY() / SConsts::TILE_SIZE + SConsts::MAX_UNIT_TILE_RADIUS + 1, theStaticMap.GetSizeY() - 1 ),
			lockTypes
		);

	}
}
void CGivenPassabilityStObject::SetTransparencies()
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );

	if ( pStats != 0 )
	{
		const int visDownX = GetVisDownX();
		const int visDownY = GetVisDownY();

		const CArray2D<BYTE> &visibility = pStats->GetVisibility( nFrameIndex );
		if ( canSetTransparency.IsEmpty() )
			canSetTransparency.SetSizes( visibility.GetSizeX(), visibility.GetSizeY() );
		for ( int y = 0; y < visibility.GetSizeY(); ++y )
		{
			for ( int x = 0; x < visibility.GetSizeX(); ++x )
			{
				const SVector tile( AICellsTiles::GetTile( visDownX + SConsts::TILE_SIZE*x, visDownY + SConsts::TILE_SIZE*y ) );

				if ( canSetTransparency[y][x] == 0xff || canSetTransparency[y][x] == 1 )
				{
					if ( theStaticMap.IsTileInside( tile ) )
					{
						if ( canSetTransparency[y][x] == 0xff )
						{
							canSetTransparency[y][x] =
								( theStaticMap.GetDissipation( tile ) != 0 || theStaticMap.IsOneWayTransp( tile ) ) ?
								0 : 1;
						}

						if ( canSetTransparency[y][x] == 1 )
						{
							theStaticMap.SetTransparency( tile, visibility[y][x] & 7 );

							if ( visibility[y][x] & 8 )
								theStaticMap.SetOneWayTransp( tile.x, tile.y, visibility[y][x] >> 4 );
							
							canSetTransparency[y][x] =
								( theStaticMap.GetDissipation( tile ) != 0 || theStaticMap.IsOneWayTransp( tile ) ) ?
								1 : 0;
						}
					}
				}
			}
		}

		bTransparencySet = true;
	}
}
void CGivenPassabilityStObject::RemoveTransparencies()
{
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );	
	const int visDownX = GetVisDownX();
	const int visDownY = GetVisDownY();

	const CArray2D<BYTE> &visibility = pStats->GetVisibility( nFrameIndex );
	if ( canSetTransparency.IsEmpty() )
		canSetTransparency.SetSizes( visibility.GetSizeX(), visibility.GetSizeY() );
	for ( int y = 0; y < visibility.GetSizeY(); ++y )
	{
		for ( int x = 0; x < visibility.GetSizeX(); ++x )
		{
			if ( canSetTransparency[y][x] == 1 )
			{
				const SVector tile( AICellsTiles::GetTile( visDownX + SConsts::TILE_SIZE*x, visDownY + SConsts::TILE_SIZE*y ) );
				if ( theStaticMap.IsTileInside( tile ) )
				{
					theStaticMap.RemoveTransparency( tile );

					if ( visibility[y][x] & 8 )
						theStaticMap.RemoveOneWayTransp( tile.x, tile.y );
				}
			}
		}
	}

	bTransparencySet = false;
}
void CGivenPassabilityStObject::RestoreTransparencies()
{
	if ( bTransparencySet )
		SetTransparencies();
}
bool CGivenPassabilityStObject::IsPointInside( const CVec2 &point ) const
{
	if ( boundRect.IsPointInside( point ) )
	{
		if ( static_cast<const SObjectBaseRPGStats*>(GetStats())->GetPassability( nFrameIndex ).IsEmpty() )
			return true;
		else
		{
			const SVector buildingTile = AICellsTiles::GetTile( point.x - GetDownX(), point.y - GetDownY() );
			const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );
						
			const CArray2D<BYTE>& pass = pStats->GetPassability( nFrameIndex );
			return 
				buildingTile.x < pass.GetSizeX() && buildingTile.y < pass.GetSizeY() &&
				point.x - GetDownX() >= 0 && point.y - GetDownY() >= 0 &&
				pass[buildingTile.y][buildingTile.x];
		}
	}
	else
		return false;
}
void CGivenPassabilityStObject::GetCoveredTiles( CTilesSet *pTiles ) const
{
	const int downX = GetDownX();
	const int downY = GetDownY();
	
	const SObjectBaseRPGStats* pStats = static_cast<const SObjectBaseRPGStats*>( GetStats() );	
	const CArray2D<BYTE>& passability = pStats->GetPassability( nFrameIndex );

	for ( int y = 0; y < passability.GetSizeY(); ++y )
	{
		for ( int x = 0; x < passability.GetSizeX(); ++x )
		{
			if ( passability[y][x] == 1 )
			{
				const SVector tile( AICellsTiles::GetTile( downX + SConsts::TILE_SIZE*x, downY + SConsts::TILE_SIZE*y ) );				
				if ( theStaticMap.IsTileInside( tile ) )
					pTiles->push_back( tile );
			}
		}
	}
}
const CVec2 CGivenPassabilityStObject::GetAttackCenter( const CVec2 &vPoint ) const
{
	CTilesSet tiles;
	GetCoveredTiles( &tiles );
	CVec2 vBestPoint;

	if ( tiles.empty() )
		vBestPoint = boundRect.center;
	else 
	{
		float fMinDist2 = -1.0f;
		for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		{
			const CVec2 vIteratingPoint = AICellsTiles::GetPointByTile( *iter );
			const float fDist2 = fabs2( vIteratingPoint - vPoint );
			if ( fMinDist2 == -1.0f || fDist2 < fMinDist2 )
			{
				fMinDist2 = fDist2;
				vBestPoint = vIteratingPoint;
			}
		}
	}

	return vBestPoint;
}
BASIC_REGISTER_CLASS( CCommonStaticObject );
void CCommonStaticObject::Die( const float fDamage )
{
	fHP = 0;
	Delete();
}
void CCommonStaticObject::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( bFromExplosion && IsAlive() && fHP > 0 )
	{
		fHP -= fDamage;
		if ( fHP <= 0 || theCheats.GetFirstShoot( theDipl.GetNParty( nPlayerOfShoot ) ) == 1 )
		{
			updater.Update( ACTION_NOTIFY_DIE, this );			
			Die( fDamage );
		}
		else
		{
			updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
			WasHit();
		}
	}
}
bool CCommonStaticObject::ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius )
{
	SRect boundRect;
	GetBoundRect( &boundRect );
	if ( boundRect.IsIntersectCircle( pExpl->GetExplCoordinates(), fRadius ) )
	{
		if ( GetRandArmorByDir( nArmorDir, pExpl->GetAttackDir() ) <= SConsts::ARMOR_FOR_AREA_DAMAGE )
		{
			TakeDamage( SConsts::AREA_DAMAGE_COEFF * pExpl->GetRandomDamage(), true, pExpl->GetPlayerOfShoot(), pExpl->GetWhoFire() );
			return true;
		}
	}

	return false;
}
bool CSimpleStaticObject::CanUnitGoThrough( const EAIClass &eClass ) const
{
	return ( pStats->dwAIClasses & eClass ) == 0;
}
bool CSimpleStaticObject::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return 
		!theDipl.IsEditorMode() &&	
		(	CCommonStaticObject::ShouldSuspendAction( eAction ) ||
			eAction == ACTION_NOTIFY_NEW_ST_OBJ && bDelayedUpdate );
}
bool CTerraMeshStaticObject::CanUnitGoThrough( const EAIClass &eClass ) const
{
	return ( pStats->dwAIClasses & eClass ) == 0;
}
void CTerraMeshStaticObject::SetNewPlacement( const CVec2 &center, const WORD dir )
{
	CCommonStaticObject::SetNewPlacement( center, dir );
	wDir = dir;
}
void CTerraMeshStaticObject::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )
{
	CCommonStaticObject::GetPlacement( pPlacement, timeDiff );
	pPlacement->dir = GetDir();
}
