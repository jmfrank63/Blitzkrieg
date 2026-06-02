#ifndef __AI_UNIT_H__
#define __AI_UNIT_H__
#pragma ONCE
#include "CommonUnit.h"
#include "AnimUnit.h"
class CPathUnit;
class CAntiArtillery;
class CAIUnitInfoForGeneral;
class CExistingObject;
class CAIUnit: public CCommonUnit
{
	DECLARE_SERIALIZE;
	
	WORD id;

	BYTE player;

	WORD wVisionAngle;
	float fSightMultiplier;
	
	bool bUnitUnderSupply;

	float fHitPoints;

	CObj<CPathUnit> pPathUnit;
	CObj<CAntiArtillery> pAntiArtillery;
	CObj<IAnimUnit> pAnimUnit;

	NTimer::STime timeLastmoraleUpdate;

	float fMorale;
	bool bHasMoraleSupport;								// рядом с этим юнитом находится юнит, оддерживающий мораль

	NTimer::STime lastTimeOfVis;

	CPtr<CExistingObject> pTankPit; 

	NTimer::STime camouflateTime;

	bool bVisibleByPlayer;
	WORD wWisibility;											// informatin about visibility for every player
	NTimer::STime lastAckTime;
	std::vector<int> visible4Party;

	float fTakenDamagePower;
	int nGrenades;
	NTimer::STime targetScanRandom;

	float fExperience;
	int nLevel;
	CGDBPtr<SAIExpLevel> pExpLevels;

	bool bFreeEnemySearch;
	NTimer::STime creationTime;
	
	bool bAlwaysVisible;
	
	bool bRevealed;
	bool bQueredToReveal;
	NTimer::STime nextRevealCheck;
	CVec2 vPlaceOfReveal;
	NTimer::STime timeOfReveal;
	int nVisIndexInUnits;

	const SAINotifyHitInfo::EHitType ProcessExactHit( const SRect &combatRect, const CVec2 &explCoord, const int nRandPiercing, const int nRandArmor ) const;
	void UpdateUnitsRequestsForResupply();
	void UpdateTankPitVisibility();
	void InitializeShootArea( struct SShootArea *pArea, CBasicGun *pGun, const float fRangeMin, const float fRangeMax ) const;
	void CheckForReveal();
protected:
	NTimer::STime timeToDeath;

	CObj<CAIUnitInfoForGeneral> pUnitInfoForGeneral;

	bool bAlive;
	float fCamouflage;
	virtual void PrepareToDelete();
	void DieTrain( const float fDamage );
	
	virtual void InitGuns() = 0;
	void Init( const CVec2 &center, const int z, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );

	const int GetRandArmorByDir( const int nArmorDir, const WORD wAttackDir, const SRect &unitRect );

	bool IsTimeToAnalyzeTargetScan() const;
	void SetMoraleForced( const float _fMorale );

	virtual bool CalculateUnitVisibility4Party( const BYTE cParty ) const;
public:
	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID ) = 0;
	void SetScenarioStats();

	void SetRightDir( bool _bRightDir);
	virtual bool IsAlive() const { return bAlive; }
	virtual void GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo );
	virtual void GetRPGStats( struct SAINotifyRPGStats *pStats );
	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );
	virtual void GetSpeed3( CVec3 *pSpeed ) const;
	virtual const NTimer::STime GetTimeOfDeath() const { return timeToDeath; }

	void SetInTankPit( CExistingObject *pTankPit );
	void SetOffTankPit();
	class CExistingObject* GetTankPit() const { return pTankPit; }
	bool IsInTankPit() const;

	void SetVisionAngle( const WORD wAngle ) { wVisionAngle = wAngle; }
	WORD GetVisionAngle() const { return wVisionAngle; }
	void SetSightMultiplier( const float _fSightMultiplier ) { fSightMultiplier = _fSightMultiplier; }
	virtual float GetSightMultiplier() const { return fSightMultiplier; }

	virtual const WORD GetID() const { return id; }
	virtual const SUnitBaseRPGStats* GetStats() const = 0;

	virtual bool IsInSolidPlace() const { return false; }
	virtual bool IsInFirePlace() const { return false; }
	virtual bool IsFree() const { return true; }

	virtual void Segment();
	virtual void FreezeSegment();
	bool IsPossibleChangeAction() const	{ return bAlive; }
	virtual void Die( const bool fromExplosion, const float fDamage );
	virtual void Disappear();

	virtual const bool IsVisible( const BYTE party ) const;
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const;
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const;
	virtual const DWORD GetNormale( const CVec2 &vCenter ) const;
	virtual const DWORD GetNormale() const;

	virtual const CVec2& GetCenter() const;
	virtual const SVector GetTile() const;
	virtual const float GetRotateSpeed() const;
	virtual const float GetMaxPossibleSpeed() const;
	virtual const float GetPassability() const;
	virtual const CVec2& GetSpeed() const;
	virtual float GetSpeedLen() const ;
	virtual const int GetBoundTileRadius() const;
	virtual const WORD GetDir() const;
	virtual const WORD GetFrontDir() const;
	virtual interface IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking );

	virtual const CVec2& GetDirVector() const;
	virtual const CVec2 GetAABBHalfSize() const;
	virtual void SetCoordWOUpdate( const CVec3 &newCenter );
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true );
	virtual void SetNewCoordinatesForEditor( const CVec3 &newCenter );
	bool CanSetNewCoord( const CVec3 &newCenter )	const;
	virtual const SRect GetUnitRectForLock() const;
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true );
	virtual void UpdateDirection( const CVec2 &newDir );
	virtual void UpdateDirectionForEditor( const CVec2 &newDir );
	virtual void UpdateDirection( const WORD newDir );
	bool CanSetNewDir( const CVec2 &newDir ) const;
	virtual bool IsIdle() const;
	virtual bool IsTurning() const;
	virtual void StopUnit();
	virtual void StopTurning();
	virtual void ForceGoByRightDir();
	virtual interface IStaticPathFinder* GetPathFinder() const;
	
	virtual interface ISmoothPath* GetCurPath() const;
	virtual void SetCurPath( interface ISmoothPath * pNewPath );
	virtual void RestoreDefaultPath();
	
	virtual bool SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( IPath *pPath );
	virtual void LockTiles( bool bUpdate = true );
	void ForceLockingTiles( bool bUpdate = true );
	virtual void LockTilesForEditor();
	virtual void UnlockTiles( const bool bUpdate = true );
	virtual const float GetZ() const;
	const WORD GetDirAtTheBeginning() const;

	const SRect GetUnitRect() const;
	virtual void FirstSegment();
	virtual void SecondSegment( const bool bUpdate = true );
	void FixUnlocking();
	void UnfixUnlocking();
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const;
	virtual bool IsTrain() const;

	const float GetHitPoints() const { return fHitPoints; }
	void IncreaseHitPoints( const float fInc = 1 );
	void TakeEditorDamage( const float fDamage );
	virtual void TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit );
	virtual bool ProcessCumulativeExpl( class CExplosion *pExpl, const int nArmorDir, const bool bFromExpl );
	virtual bool ProcessBurstExpl( class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius );
	virtual bool ProcessAreaDamage( const class CExplosion *pExpl, const int nArmorDir, const float fRadius, const float fSmallRadius );

	virtual const float GetCover() const;
	virtual bool IsSavedByCover() const;

	virtual class CTurret* GetTurret( const int nTurret ) const { NI_ASSERT_T( false, "Wrong call of get turret" ); return 0; }
	
	virtual void GetShotInfo( struct SAINotifyInfantryShot *pShotInfo ) const { NI_ASSERT_T( false, "Wrong call of GetShotInfo" ); }
	virtual void GetShotInfo( struct SAINotifyMechShot *pShotInfo ) const { NI_ASSERT_T( false, "Wrong call of GetShotInfo" ); }
	virtual const EActionNotify GetShootAction() const = 0;
	virtual const EActionNotify GetAimAction() const = 0;
	virtual const EActionNotify GetDieAction() const = 0;
	virtual const EActionNotify GetIdleAction() const = 0;
	virtual const EActionNotify GetMovingAction() const = 0;
	virtual int GetMovingType() const { return 0; }

	virtual bool CanMove() const;
	virtual bool CanMovePathfinding() const;
	virtual bool CanRotate() const;
	
	virtual void SetCamoulfage();
	const float GetCamouflage() const { return fCamouflage; }
	virtual void RemoveCamouflage( ECamouflageRemoveReason eReason );
	virtual bool IsCamoulflated() const { return fCamouflage < 1.0f; }

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	virtual bool CanCommandBeExecutedByStats( class CAICommand *pCommand );
	virtual bool CanCommandBeExecutedByStats( int nCmd ) const;

	virtual const BYTE GetPlayer() const { return player; }
	virtual void SetPlayerForEditor( const int nPlayer );
	virtual void ChangePlayer( const BYTE cPlayer );
	void SetPlayer( const BYTE cPlayer ) { player = cPlayer; }

	virtual bool InVisCone( const CVec2 &point ) const { return true; }
	virtual const float GetSightRadius() const;
	float GetRemissiveCoeff() const;
	virtual const int GetNAIGroup() const { return GetNGroup(); }
	
	class CPathUnit* GetPathUnit() const { return pPathUnit; }
	
	virtual bool CanTurnToFrontDir( const WORD wDir );
	
	virtual const NTimer::STime GetTimeToCamouflage() const;
	virtual void AnalyzeCamouflage();
	virtual void StartCamouflating();


	void CreateAntiArtillery( const float fMaxRevealRadius );
	virtual void Fired( const float fGunRadius, const int nGun );
	virtual NTimer::STime GetDisappearInterval() const { return SConsts::TIME_TO_DISAPPEAR; }

	virtual const float GetDispersionBonus() const { return 1.0f; }
	virtual const void SetDispersionBonus( const float fBonus ) {}
	virtual const float GetRelaxTimeBonus() const { return 1.0f; }
	virtual const float GetFireRateBonus() const { return 1.0f; }
	virtual const float GetAimTimeBonus() const { return 1.0f; }
	
	virtual void SetAmbush();
	virtual void RemoveAmbush();
	
	virtual void GetFogInfo( struct SFogInfo *pInfo ) const;
	virtual void GetShootAreas( struct SShootAreas *pShootAreas, int *pnAreas ) const;
	void WarFogChanged();

	virtual void UpdateArea( const EActionNotify eAction );
	
	virtual BYTE GetAIClass() const { return GetStats()->aiClass; }
	
	const int GetNCommonGuns() const;
	const SBaseGunRPGStats& GetCommonGunStats( const int nCommonGun ) const;
	virtual int GetNAmmo( const int nCommonGun ) const;
	virtual void ChangeAmmo( const int nCommonGun, const int nAmmo );
	virtual bool IsCommonGunFiring( const int nCommonGun ) const;

	
	void SetMorale( float _fMorale ) ;
	void SetMoraleSupport();
	const float GetMorale() const { return fMorale; }
	
	virtual float GetSmoothTurnThreshold() const;
	
	virtual bool IsNoticableByUnit( class CCommonUnit *pUnit, const float fNoticeRadius );
	
	const int ChooseFatality( const float fDamage );
	
	void NullCollisions();

	virtual void SendAcknowledgement( EUnitAckType ack, bool bForce = false );
	virtual void SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce = false );
	
	virtual bool IsColliding() const;
	
	virtual const int GetMinArmor() const;
	virtual const int GetMaxArmor() const;
	virtual const int GetMinPossibleArmor( const int nSide ) const;
	virtual const int GetMaxPossibleArmor( const int nSide ) const;
	virtual const int GetArmor( const int nSide ) const;
	virtual const int GetRandomArmor( const int nSide ) const;
	
	virtual bool IsLockingTiles() const;
	virtual bool CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward = true ) const;
	virtual bool CheckToTurn( const WORD wNewDir );
	virtual bool HasSuspendedPoint() const;

	virtual void UnRegisterAsBored( const enum EUnitAckType eBoredType );
	virtual void RegisterAsBored( const enum EUnitAckType eBoredType );
	
	virtual class CUnitGuns* GetGuns() = 0;	
	virtual const class CUnitGuns* GetGuns() const = 0;
	virtual EUnitAckType GetGunsRejectReason() const;
	bool DoesExistRejectGunsReason( const EUnitAckType &ackType ) const;
	
	const float GetKillSpeed( const SHPObjectRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const;
	const float GetKillSpeed( const SHPObjectRPGStats *pStats, const CVec2 &vCenter, const DWORD dwGuns ) const;
	virtual const float GetKillSpeed( class CAIUnit *pEnemy ) const;
	virtual const float GetKillSpeed( class CAIUnit *pEnemy, class CBasicGun *pGun ) const;
	virtual const float GetKillSpeed( CAIUnit *pEnemy, const DWORD dwGuns ) const;
	void UpdateTakenDamagePower( const float fUpdate );
	const float GetTakenDamagePower() const { return fTakenDamagePower; }
	
	virtual void ResetTargetScan();
	virtual BYTE AnalyzeTargetScan(	CAIUnit *pCurTarget, const bool bDamageUpdated, const bool bScanForObstacles, IRefCount *pCheckBuilding = 0 );
	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );
	virtual void LookForFarTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );
	void SetCircularAttack( const bool bCanAttack );
	virtual interface IObstacle *LookForObstacle();
	void UpdateNAttackingGrenages( const int nUpdate ) { nGrenades += nUpdate; NI_ASSERT_T( nGrenades >= 0, "Wrong number of grenades" ); }
	const int GetNAttackingGrenages() const { return nGrenades; }

	virtual void ResetGunChoosing();
	CBasicGun* AnalyzeGunChoose( CAIUnit *pEnemy );

	void EnemyKilled( CAIUnit *pEnemy );

	virtual bool CanMoveForGuard() const { return CanMove() && !GetStats()->IsTrain(); }
	virtual const float GetTimeToForget() const;
	CAIUnitInfoForGeneral* GetUnitInfoForGeneral() const;
	void SetLastVisibleTime( const NTimer::STime time );
	
	virtual const float GetTargetScanRadius();
	virtual bool IsFreeEnemySearch() const { return bFreeEnemySearch; }

	virtual const float GetPathSegmentsPeriod() const { return 1.0f; }
	virtual const NTimer::STime GetNextSecondPathSegmTime() const;
	
	virtual float GetPriceMax() const;
	friend class CStaticMembers;
	
	virtual const NTimer::STime& GetBehUpdateDuration() const { return SConsts::BEH_UPDATE_DURATION; }
	
	virtual void SendNTotalKilledUnits( const int nPlayerOfShoot );

	virtual const SVector GetLastKnownGoodTile() const;

	const SAIExpLevel::SLevel& GetExpLevel() const;
	
	virtual void UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand );

	void CalcVisibility();
	virtual const bool IsVisibleByPlayer();
	
	virtual void Lock( const CBasicGun *pGun );
	virtual void Unlock( const CBasicGun *pGun );
	virtual bool IsLocked( const CBasicGun *pGun ) const;
	
	void SetActiveShellType( const enum SWeaponRPGStats::SShell::EDamageType eShellType );

	void InitAviationPath();
	
	virtual void InstallAction( const EActionNotify eInstallAction, bool bAlreadyDone = false ) { }
	virtual bool IsUninstalled() const { return true; }
	virtual bool IsInstalled() const { return true; }

	virtual void AnimationSet( int nAnimation );
	virtual void AnimationSegment();
	void Moved();
	void Stopped();
	void StopCurAnimation();

	virtual class CArtillery* GetArtilleryIfCrew() const { return 0; }
	virtual void TrackDamagedState( const bool bTrackDamaged );

	void WantedToReveal( CAIUnit *pWhoRevealed );
	bool IsRevealed() const;
	
	virtual bool IsInfantry() const;
	virtual void Grazed( CAIUnit *pUnit ) { }
	
	void NullCreationTime() { creationTime = 0; }

	const int GetNVisIndexInUnits() const { return nVisIndexInUnits; }
	void SetNVisIndexInUnits( const int _nVisIndexInUnits ) { nVisIndexInUnits = _nVisIndexInUnits; }

	virtual bool CanMoveAfterUserCommand() const;
};
#endif // __AI_UNIT_H__
