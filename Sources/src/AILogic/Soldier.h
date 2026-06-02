#ifndef __SOLDIER_H__
#define __SOLDIER_H__

#pragma ONCE
#include "AIUnit.h"
#include "AIWarFog.h"
#include "StaticObjectSlotInfo.h"
class CUnitGuns;
class CFormation;
class CSoldier : public CAIUnit
{
	DECLARE_SERIALIZE;

	enum EObjectInsideOf { EOIO_NONE, EOIO_BUILDING, EOIO_TRANSPORT, EOIO_ENTRENCHMENT, EOIO_UNKNOWN };
	EObjectInsideOf eInsideType;

	CGDBPtr<SInfantryRPGStats> pStats;
	CPtr<CUnitGuns> pGuns;
	
	IRefCount *pObjInside;

	SStaticObjectSlotInfo slotInfo;

	WORD wMinAngle, wMaxAngle;
	float fOwnSightRadius;

	bool bInFirePlace, bInSolidPlace;

	NTimer::STime lastHit, lastCheck;
	NTimer::STime lastMineCheck; // последн€€ проверка мин (дл€ инженеров)
	NTimer::STime lastDirUpdate;
	bool bLying;

	CPtr<CFormation> pFormation;
	CPtr<CFormation> pMemorizedFormation;
	CPtr<CFormation> pVirtualFormation;
	BYTE cFormSlot;
	bool bWait2Form;

	bool bAllowLieDown; // может ли солдат ложитьс€ под обстрелом (или стоит как олов€нный one.)
	NTimer::STime nextSegmTime;
	NTimer::STime timeBWSegments;
	NTimer::STime nextPathSegmTime;
	NTimer::STime nextLogicSegmTime;

	void UpdateLyingPosition();
protected:
	virtual void PrepareToDelete();
	
	virtual void InitGuns();
	virtual void RevealNearestMines( const bool bIncludingAP );

	virtual bool CalculateUnitVisibility4Party( const BYTE cParty ) const;
public:
	CSoldier() : pFormation( 0 ), pObjInside( 0 ), cFormSlot( 0 ), eInsideType( EOIO_NONE ), wMinAngle( 0 ), wMaxAngle( 0 ), bInFirePlace( false ), bInSolidPlace( false ), fOwnSightRadius( -1 ) { }

	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );
	
	virtual const SUnitBaseRPGStats* GetStats() const { return pStats; }
	virtual IStatesFactory* GetStatesFactory() const;
	
	virtual void UpdateDirection( const CVec2 &newDir );
	virtual void UpdateDirection( const WORD newDir );

	virtual void AllowLieDown( bool _bAllowLieDown );
	bool IsAllowedToLieDown() const { return bAllowLieDown; }
	void MoveToEntrenchFireplace( const CVec3 &coord, const int _nSlot  );
	
	class CBuilding* GetBuilding() const;
	class CEntrenchment* GetEntrenchment() const;
	class CMilitaryCar* GetTransportUnit() const;

	void SetInBuilding( class CBuilding *pBuilding );
	void SetInEntrenchment( class CEntrenchment *pEntrenchment );
	void SetInTransport( class CMilitaryCar *pUnit );
	void SetFree();
	
	virtual bool IsInSolidPlace() const;
	virtual bool IsInFirePlace() const;
	void SetToFirePlace();
	void SetToSolidPlace();
	virtual bool IsFree() const ;

	void SetNSlot( const int nSlot ) { slotInfo.nSlot = nSlot; }
	const int GetSlot() const { return slotInfo.nSlot; }
	const SStaticObjectSlotInfo& GetSlotInfo() const { return slotInfo; }
	SStaticObjectSlotInfo& GetSlotInfo() { return slotInfo; }
	void SetSlotInfo( const int nSlot, const int nType, const int nIndex ) { slotInfo.nSlot = nSlot; slotInfo.nType = nType; slotInfo.nIndex = nIndex; }
	void SetSlotIndex( const int nIndex ) { slotInfo.nIndex = nIndex; }
	const int GetSlotIndex() const { return slotInfo.nIndex; }

	void SetAngles( const WORD _wMinAngle, const WORD _wMaxAngle ) { wMinAngle = _wMinAngle; wMaxAngle = _wMaxAngle; }
	WORD GetMinAngle() const { return wMinAngle; }
	WORD GetMaxAngle() const { return wMaxAngle; }
	bool IsAngleLimited() const;
	
	bool IsInBuilding() const { return eInsideType == EOIO_BUILDING; }
	bool IsInEntrenchment() const { return eInsideType == EOIO_ENTRENCHMENT; }
	bool IsInTransport() const { return eInsideType == EOIO_TRANSPORT; }

	virtual void  GetShotInfo( struct SAINotifyInfantryShot *pShotInfo ) const;
	void GetThrowInfo( struct SAINotifyInfantryShot *pThrowInfo ) const;
	void GetEntranceStateInfo( struct SAINotifyEntranceState *pInfo ) const;

	virtual const EActionNotify GetAimAction() const;
	virtual const EActionNotify GetShootAction() const;
	virtual const EActionNotify GetThrowAction() const;
	virtual const EActionNotify GetDieAction() const;
	virtual const EActionNotify GetIdleAction() const;
	virtual const EActionNotify GetMovingAction() const;

	virtual bool CanMove() const { return IsFree() && GetBehaviour().moving != SBehaviour::EMHoldPos; }
	virtual bool CanMovePathfinding() const { return IsFree(); }

	virtual bool InVisCone( const CVec2 &point ) const;
	virtual const float GetSightRadius() const;
	void SetOwnSightRadius( const float _fOwnSightRadius ) { fOwnSightRadius = _fOwnSightRadius; }
	void RemoveOwnSightRadius() { fOwnSightRadius = -1; }
	virtual const float GetCover() const;
	bool IsLying() const { return bLying; }
	void LieDown();
	void StandUp();
	virtual float GetSightMultiplier() const;

	virtual void Segment();
	virtual void FreezeSegment();
	
	void SetFormation( class CFormation* pFormation, const BYTE cFormSlot );
	virtual bool IsInFormation() const { return pFormation != 0; }
	virtual class CFormation* GetFormation() const { return pFormation; }
	virtual const CVec2 GetUnitPointInFormation() const;
	virtual const int GetFormationSlot() const { return cFormSlot; }
	virtual const bool CanShootToPlanes() const;

	virtual int GetNGuns() const;
	virtual class CBasicGun* GetGun( const int n ) const;
	virtual class CTurret* GetTurret( const int nTurret ) const;
	virtual const int GetNTurrets() const;
	virtual int GetNAmmo( const int nCommonGun ) const;
	virtual void Fired( const float fGunRadius, const int nGun );
	virtual void ChangeAmmo( const int nCommonGun, const int nAmmo );
	virtual bool IsCommonGunFiring( const int nCommonGun ) const;

	virtual class CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime );
	
	virtual const float GetMaxPossibleSpeed() const;
	
	virtual void GetFogInfo( SFogInfo *pInfo ) const;
	void ChangeWarFogState();
	
	virtual void GetShootAreas( struct SShootAreas *pShootAreas, int *pnAreas ) const;
	virtual float GetMaxFireRange() const;
	
	virtual bool IsMech() const { return false; }

	virtual bool IsNoticableByUnit( class CCommonUnit *pUnit, const float fNoticeRadius );
	
	virtual bool ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius );
	
	virtual const float GetDispersionBonus() const;
	virtual const float GetRelaxTimeBonus() const;
	virtual const float GetFireRateBonus() const;
	
	virtual void MemCurFormation();
	class CFormation* GetMemFormation() { return pMemorizedFormation; }
	void SetVirtualFormation( class CFormation *pFormation );

	void SetWait2FormFlag( bool bNewValue ) { bWait2Form = bNewValue; }
	bool IsInWait2Form() const { return bWait2Form; }
	
	virtual bool IsColliding() const;

	void MemorizeFormation();
	
	virtual const int GetMinArmor() const;
	virtual const int GetMaxArmor() const;
	virtual const int GetMinPossibleArmor( const int nSide ) const;
	virtual const int GetMaxPossibleArmor( const int nSide ) const;
	virtual const int GetArmor( const int nSide ) const;
	virtual const int GetRandomArmor( const int nSide ) const;
	
	virtual const int CanGoBackward() const { return false; }
	
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
	virtual bool CanJoinToFormation() const;

	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );
	virtual void TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	
	virtual const NTimer::STime GetNextSegmTime() const { return nextSegmTime; }
	virtual void NullSegmTime() { timeBWSegments = 0; }

	virtual void FirstSegment();
	virtual const NTimer::STime GetNextPathSegmTime() const { return nextPathSegmTime; }

	virtual const float GetPathSegmentsPeriod() const;
	
	virtual const NTimer::STime& GetBehUpdateDuration() const { return SConsts::SOLDIER_BEH_UPDATE_DURATION; }

	virtual class CArtillery* GetArtilleryIfCrew() const;

	virtual const CUnitGuns* GetGuns() const { return pGuns; }
	virtual CUnitGuns* GetGuns() { return pGuns; }

	virtual void FreezeByState( const bool bFreeze );
};
class CInfantry : public CSoldier
{
	OBJECT_COMPLETE_METHODS( CInfantry );
	DECLARE_SERIALIZE;
public:
	CInfantry() { }
};
class CSniper : public CSoldier
{
	OBJECT_COMPLETE_METHODS( CSniper );
	DECLARE_SERIALIZE;

	NTimer::STime lastVisibilityCheck;
	bool bVisible;
	bool bSneak;
	float fCamouflageRemoveWhenShootProbability;

protected:
	virtual bool CalculateUnitVisibility4Party( const BYTE cParty ) const;
public:
	CSniper() : lastVisibilityCheck( 0 ), bVisible( false ), bSneak( false ) { }

	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );

	virtual void Segment();

	void SetVisible() { bVisible = true; }

	virtual void RemoveCamouflage( ECamouflageRemoveReason eReason );
	void SetSneak( const bool bSneakMode );

	virtual void Fired( const float fGunRadius, const int nGun  );
};

extern CGlobalWarFog theWarFog;
struct SSniperTrace
{
	const SVector centerTile;
	const bool bCamouflated;
	const float fCamouflage;
	const int nParty;
	CSniper *pSniper;

	SSniperTrace( CSniper *_pSniper ) 
		: centerTile( _pSniper->GetTile().x, _pSniper->GetTile().y ), 
			bCamouflated( _pSniper->IsCamoulflated() ), fCamouflage( _pSniper->GetCamouflage() ),
			nParty( _pSniper->GetParty() ),
			pSniper( _pSniper )
	{ }

	bool CanTraceRay( const SVector &point ) const { return true; }
	bool VisitPoint( const SVector &point, const int vis, const float fLen2, const float fR2, const float fSightPower2 )
	{
		if ( point.x == centerTile.x && point.y == centerTile.y )
		{
			if ( !bCamouflated )
			{
				pSniper->SetVisible();
				return false;
			}
			else
			{
				const float fRatio = fSightPower2 * fLen2 / fR2;
				if ( fRatio <= sqr( fCamouflage * (float)theWarFog.GetTileVis( point, nParty ) / float( SConsts::VIS_POWER ) ) )
				{
					pSniper->SetVisible();
					return false;
				}
			}
		}

		return true;
	}
};
#endif // __SOLDIER_H__
