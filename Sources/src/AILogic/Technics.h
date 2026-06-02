#ifndef __TECHNICS_H__
#define __TECHNICS_H__
#pragma ONCE
#include "AIUnit.h"
class CExistingObject;
class CTurret;
class CUnitGuns;
class CSoldier;
class CFormation;
class CArtillery;
class CEntrenchmentTankPit;
class CMilitaryCar : public CAIUnit
{
	DECLARE_SERIALIZE;

	CGDBPtr<SMechUnitRPGStats> pStats;

	CPtr<CUnitGuns> pGuns;

	std::vector< CObj<CTurret> > turrets;
	
	std::list<CPtr<CSoldier> > pass;
	
	CPtr<CFormation> pLockingUnit;
	float fDispersionBonus;
	NTimer::STime timeLastHeal;						// последнее время лечения
	NTimer::STime lastResupplyMorale;			// last morale addition time
	
	const CVec2 GetPassengerCoordinates( const int n );
protected:
	virtual void InitGuns();
	virtual const class CUnitGuns* GetGuns() const { return pGuns; }
	virtual class CUnitGuns* GetGuns() { return pGuns; }
	virtual void PrepareToDelete();
public:
	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );

	void Lock( class CFormation *_pLockingUnit );
	bool IsLocked() const { return pLockingUnit != 0; }
	void Unlock() { pLockingUnit = 0; }
	
	virtual const SUnitBaseRPGStats* GetStats() const { return pStats; }	
	virtual IStatesFactory* GetStatesFactory() const =0;

	virtual float GetDistanceToLandPoint() const;

	virtual BYTE GetNAvailableSeats() const { return static_cast<BYTE>( pStats->nPassangers - pass.size() ); }
	virtual BYTE GetNPassengers() const { return static_cast<BYTE>( pass.size() ); }
	virtual void AddPassenger( class CSoldier *pUnit );
	virtual class CSoldier* GetPassenger( const int n );

	const CVec2 GetEntrancePoint() const;

	virtual void ClearAllPassengers();
	virtual void DelPassenger( const int n );
	virtual void DelPassenger( class CSoldier *pSoldier );

	virtual void Segment();

	virtual CTurret* GetTurret( const int nTurret ) const { return turrets[nTurret]; }
	virtual const int GetNTurrets() const { return turrets.size(); }

	virtual void GetShotInfo( struct SAINotifyMechShot *pShotInfo ) const { pShotInfo->typeID = GetShootAction(); pShotInfo->pObj = const_cast<CMilitaryCar*>(this); }	
	virtual const EActionNotify GetShootAction() const { return ACTION_NOTIFY_MECH_SHOOT; }
	virtual const EActionNotify GetAimAction() const { return ACTION_NOTIFY_AIM; }
	virtual const EActionNotify GetDieAction() const { return ACTION_NOTIFY_DIE; }
	virtual const EActionNotify GetIdleAction() const { return ACTION_NOTIFY_IDLE; }
	virtual const EActionNotify GetMovingAction() const { return ACTION_NOTIFY_MOVE; }

	virtual const bool CanShootToPlanes() const;

	virtual int GetNGuns() const;
	virtual class CBasicGun* GetGun( const int n ) const;

	virtual class CBasicGun* GetFirstArtilleryGun() const;

	virtual class CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime );
	virtual float GetMaxFireRange() const;

	virtual bool IsMech() const { return true; }

	virtual const float GetDispersionBonus() const;
	virtual const void SetDispersionBonus( const float fBonus ) { fDispersionBonus = fBonus; }

	virtual void GetRangeArea( struct SShootAreas *pRangeArea ) const;
	virtual class CArtillery* GetTowedArtillery() const = 0;

	virtual const int CanGoBackward() const { return GetTowedArtillery() == 0; }
	
	const CVec2 GetHookPoint() const;
	const CVec3 GetHookPoint3D() const;
	
	virtual void SendNTotalKilledUnits( const int nPlayerOfShoot );
	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );

	virtual bool HasTowedArtilleryCrew() const { return false; }
	virtual void SetTowedArtilleryCrew( class CFormation *pFormation ) {  }
	virtual CFormation * GetTowedArtilleryCrew()  { return 0; }
};
class CTank : public CMilitaryCar
{
	OBJECT_COMPLETE_METHODS( CTank );
	DECLARE_SERIALIZE;

	bool bTrackDamaged; // true если у танка перебита гусеница
	
	WORD wDangerousDir;
	bool bDangerousDirSet;
	bool bDangerousDirSetInertia;
	NTimer::STime nextTimeOfDangerousDirScan;
	NTimer::STime lastTimeOfDangerousDirChanged;

	WORD wDangerousDirUnderFire;
	float fDangerousDamageUnderFire;

	void ScanForDangerousDir();
public:
	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );
	virtual IStatesFactory* GetStatesFactory() const;

	bool IsTrackDamaged() const { return bTrackDamaged; }
	void RepairTrack() ;// починили гусеницу

	virtual void TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual bool CanMove() const;
	virtual bool CanMovePathfinding() const;
	virtual bool CanRotate() const;
	virtual bool CanTurnToFrontDir( const WORD wDir );

	virtual const bool NeedDeinstall() const { return bTrackDamaged; }

	virtual class CArtillery* GetTowedArtillery() const { return 0; }

	virtual const NTimer::STime& GetBehUpdateDuration() const
	{
		if ( RPG_TYPE_SPG_AAGUN == GetStats()->type )
			return SConsts::AA_BEH_UPDATE_DURATION;
		return SConsts::BEH_UPDATE_DURATION;
	}

	virtual void Segment();

	virtual bool IsDangerousDirExist() const { return bDangerousDirSetInertia; }
	virtual const WORD GetDangerousDir() const { return wDangerousDir; }
	virtual void Grazed( CAIUnit *pUnit );

	virtual bool CanMoveAfterUserCommand() const;
};
class CAITransportUnit : public CMilitaryCar
{
	OBJECT_COMPLETE_METHODS( CAITransportUnit );
	DECLARE_SERIALIZE;

	float fResursUnits; // количество RU, которые есть у грузовичка
	CPtr<CArtillery> pTowedArtillery;
	CPtr<CAIUnit> pMustTow;			// artillery, that this truck must tow (for general intendant)

	typedef std::list< CPtr<CFormation> > CExternLoaders;
	CExternLoaders externLoaders; // дошоняющие гранспорт грузчики
	CPtr<CFormation> pTowedArtilleryCrew;	// when artillery is attached the crew.

	const int GetNUnitToTakeArtillery( bool bPlaceInQueue, CAIUnit *pUnitToTake );
public:
	void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );

	float GetResursUnitsLeft() const { return fResursUnits; }
	void SetResursUnitsLeft( float _fResursUnits );
	void DecResursUnitsLeft( float dRU );

	virtual bool IsTowing() const;
	virtual class CArtillery* GetTowedArtillery() const { return pTowedArtillery; }
	void SetTowedArtillery( class CArtillery *pTowedArtillery);

	virtual IStatesFactory* GetStatesFactory() const;
	bool CanCommandBeExecuted( class CAICommand *pCommand );
	
	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats );
	virtual const int GetUnitState() const;
	
	static void PrepareLoaders( CFormation *pLoaderSquad, CAITransportUnit * pTransport );
	static void FreeLoaders( CFormation *pLoaderSquad, CAITransportUnit * pTransport );
	virtual void Segment();

	void AddExternLoaders( CFormation *pLoaders );
	void Die( const bool fromExplosion, const float fDamage );

	virtual bool HasTowedArtilleryCrew() const ;
	virtual void SetTowedArtilleryCrew( class CFormation *pFormation ) ;
	virtual CFormation * GetTowedArtilleryCrew() ;
	
	void SetMustTow( class CAIUnit *_pUnit );
	bool IsMustTow() const;
	
	virtual void UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand );

	virtual bool CanHookUnit( class CAIUnit *pUnitToHook ) const;
};
#endif //__TECHNICS_H__
