#ifndef __UPDATABLE_OBJECT_H__
#define __UPDATABLE_OBJECT_H__

#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Common\Actions.h"
#include "RectTiles.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IUpdatableObj;
interface IUpdatableObj : public IRefCount
{
public:
	virtual bool IsSelectable()const{ NI_ASSERT_T( false, "Wrong call of IsSelectable" );return false;};
	virtual int GetMovingType() const { NI_ASSERT_T( false, "Wrong call of GetMovingType" ); return 0; }

	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff )	{ NI_ASSERT_T( false, "Wrong call of GetPlacement" ); }
	virtual void GetSpeed3( CVec3 *pSpeed ) const { NI_ASSERT_T( false, "Wrong call of GetSpeed3" ); }
	virtual const NTimer::STime GetTimeOfDeath() const { NI_ASSERT_T( false, "Wrong call of GetTimeOfDeath" ); return 0; }

	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats ) { NI_ASSERT_T( false, "Wrong call of GetRPGStats" ); }
	virtual bool IsAlive() const { NI_ASSERT_T( false, "Wrong call of IsAlive" ); return false; }
	virtual void GetHorTurretTurnInfo( struct SAINotifyTurretTurn *pTurretTurn ) { NI_ASSERT_T( false, "Wrong call of GetHorTurretTurnInfo" ); }
	virtual void GetVerTurretTurnInfo( struct SAINotifyTurretTurn *pTurretTurn ) { NI_ASSERT_T( false, "Wrong call of GetVerTurretTurnInfo" ); }
	virtual void GetHitInfo( struct SAINotifyHitInfo *pHitInfo ) const { NI_ASSERT_T( false, "Wrong call of GetHitInfo" ); }
	virtual void GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo ) { NI_ASSERT_T( false, "Wrong call of GetNewUnitInfo" ); }
	virtual void GetProjectileInfo( struct SAINotifyNewProjectile *pProjectileInfo ) { NI_ASSERT_T( false, "Wrong call of GetProjectileInfo" ); }
	virtual void GetDyingInfo( struct SAINotifyAction *pDyingInfo ) { NI_ASSERT_T( false, "Wrong call of GetDyingInfo" ); }
	
	virtual void GetMechShotInfo( struct SAINotifyMechShot *pMechShotInfo, const NTimer::STime &time ) const { NI_ASSERT_T( false, "Wrong call of GetMechShotInfo" ); }
	virtual void GetInfantryShotInfo( struct SAINotifyInfantryShot *pInfantryShotInfo, const NTimer::STime &time ) const { NI_ASSERT_T( false, "Wrong call of GetInfantryShotInfo" ); }
	virtual void GetEntranceStateInfo( struct SAINotifyEntranceState *pInfo ) const { NI_ASSERT_T( false, "Wrong call of GetEntranceStateInfo" ); }
	
	virtual void GetRevealCircle( CCircle *pCircle ) const { NI_ASSERT_T( false, "Wrong call of GetCircle" ); }
	virtual void GetShootAreas( struct SShootAreas *pShootAreas, int *pnAreas ) const { *pnAreas = 0; }
	virtual void GetRangeArea( struct SShootAreas *pRangeArea ) const { new (pRangeArea) SShootAreas(); }

	virtual const EActionNotify GetDieAction() const { NI_ASSERT_T( false, "Wrong call of GetDieAction" ); return ACTION_NOTIFY_NONE; }

	virtual const BYTE GetPlayer() const { NI_ASSERT_T( false, "Wrong call of GetPlayer" ); return 0xff; }
	virtual const int GetNAIGroup() const { return -2; }
	
	virtual float GetTerrainHeight( const float x, const float y, const NTimer::STime timeDiff ) const;

	virtual const int GetUnitState() const { return 0; }

	// обязательно должна быть константной, чтобы не было расхождений в multiplayerb
	virtual const bool IsVisible( const BYTE cParty ) const = 0;
	// виден ли игроком
	virtual const bool IsVisibleByPlayer();
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const = 0;
	virtual const bool IsVisibleForDiplomacyUpdate() { return true; }

	virtual IUpdatableObj* GetDieObject() const { NI_ASSERT_T( false, "Wrong call of GetDieObject" ); return 0; }

	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const = 0;
	// 
	virtual const int GetUniqueId() const = 0;
	
	virtual void SetScriptID( const int nScriptID ) { }
	
	virtual const int GetDBID() const { return -1; }
	
	virtual void AnimationSet( int nAnimation ) { }
	virtual bool IsFree() const { return true; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UPDATABLE_OBJECT_H__
