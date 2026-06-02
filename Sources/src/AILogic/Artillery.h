#ifndef __ARTILLERY_H__
#define __ARTILLERY_H__
#pragma ONCE
#include "AIUnit.h"
class CUnitGuns;
class CTurret;
interface IPath;
class CFormation;
class CAIUnit;
class CArtilleryBulletStorage;
class CMechUnitGuns;
class CArtillery : public CAIUnit
{
	OBJECT_NORMAL_METHODS( CArtillery );
	DECLARE_SERIALIZE;

	CGDBPtr<SMechUnitRPGStats> pStats;

	int nInitialPlayer;

	CPtr<CMechUnitGuns> pGuns;

	std::vector< CObj<CTurret> > turrets;

	EActionNotify eCurInstallAction, eNextInstallAction;
	EActionNotify eCurrentStateOfInstall;

	bool bInstalled;
	NTimer::STime installActionTime;
	bool bInstallActionInstant;						// для того, чтобы артиллерию можно было создать в непроинсталлированном состоянии

	float fDispersionBonus;
	
	CPtr<IStaticPath> pStaticPathToSend;
	CVec2 vShift;
	CPtr<IPath> pIPathToSend;

	CPtr<CFormation> pCapturingUnit;			// взвод, который бежит захватывать пушку.
	CPtr<CFormation> pCrew;								// взвод, который пушку обслуживает
	float fOperable; // часть команды, которая обслуживает пушку

	CPtr<CAIUnit> pSlaveTransport; // транспорт, который работает на эту пушку
	CPtr<CAIUnit> pHookingTransport;	// transport, that is hooking this artillery.
	
	CObj<CArtilleryBulletStorage> pBulletStorage;
	bool bBulletStorageVisible;
	NTimer::STime lastCheckToInstall;

	NTimer::STime behUpdateDuration;

	void CreateAmmoBox();

	void ShowAmmoBox();
	void HideAmmoBox();

	bool IsInstallActionFinished();
	bool ShouldSendInstallAction( const EActionNotify &eAction ) const;
protected:
	virtual void InitGuns();
	virtual const CUnitGuns* GetGuns() const;
	virtual CUnitGuns* GetGuns();
public:
	CArtillery() : bBulletStorageVisible( false ) { }
	virtual ~CArtillery();
	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD _dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );	
	
	virtual const SUnitBaseRPGStats* GetStats() const { return pStats; }
	virtual IStatesFactory* GetStatesFactory() const;

	virtual void Segment();
	virtual void SetSelectable	( bool bSelectable );

	virtual void SetCamoulfage();
	virtual void RemoveCamouflage( ECamouflageRemoveReason eReason );

	virtual class CTurret* GetTurret( const int nTurret ) const { return turrets[nTurret]; }
	virtual const int GetNTurrets() const { return turrets.size(); }

	virtual void GetShotInfo( struct SAINotifyMechShot *pShotInfo ) const { pShotInfo->typeID = GetShootAction(); pShotInfo->pObj = const_cast<CArtillery*>(this); }	
	virtual const EActionNotify GetShootAction() const { return ACTION_NOTIFY_MECH_SHOOT; }
	virtual const EActionNotify GetAimAction() const { return ACTION_NOTIFY_AIM; }
	virtual const EActionNotify GetDieAction() const { return ACTION_NOTIFY_DIE; }
	virtual const EActionNotify GetIdleAction() const { return ACTION_NOTIFY_IDLE; }
	virtual const EActionNotify GetMovingAction() const { return ACTION_NOTIFY_MOVE; }

	virtual int GetNGuns() const;
	virtual class CBasicGun* GetGun( const int n ) const;

	virtual class CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime );

	virtual bool IsInstalled() const { return bInstalled && eCurInstallAction == ACTION_NOTIFY_NONE && eNextInstallAction == ACTION_NOTIFY_NONE; }
	virtual bool IsUninstalled() const { return !bInstalled && eCurInstallAction == ACTION_NOTIFY_NONE && eNextInstallAction == ACTION_NOTIFY_NONE; }
	bool IsInInstallAction() const { return eCurInstallAction != ACTION_NOTIFY_NONE || eNextInstallAction != ACTION_NOTIFY_NONE; }

	void InstallBack( bool bAlreadyDone ); // инсталлировать артиллерию обратно, если она не деинсталлирована - то ошибка.
	virtual void InstallAction( const EActionNotify eInstallAction, bool bAlreadyDone = false );
	void ForceInstallAction();

	virtual const bool NeedDeinstall() const;
	virtual const bool CanShoot() const { return IsInstalled(); }
	virtual class CBasicGun* GetFirstArtilleryGun() const;
	virtual void TakeDamage( const float fDamage, const SWeaponRPGStats::SShell *pShell, const int nPlayerOfShoot, CAIUnit *pShotUnit );

	bool IsLightGun() const;
	virtual const bool CanShootToPlanes() const;

	virtual bool IsIdle() const;
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( IPath *pPath );
	
	virtual float GetMaxFireRange() const;
	virtual void GetRangeArea( struct SShootAreas *pRangeArea ) const;
		
	virtual const float GetDispersionBonus() const;
	virtual const void SetDispersionBonus( const float fBonus ) { fDispersionBonus = fBonus; }

	virtual bool IsMech() const { return true; }
	
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true );
	virtual bool TurnToUnit( const CVec2 &targCenter );

	virtual void ChangePlayer( const BYTE cPlayer );
	virtual void SetCrew( class CFormation * _pCrew, const bool bCapture = true );
	virtual void DelCrew();
	virtual bool HasServeCrew() const;
	virtual bool MustHaveCrewToOperate() const;
	virtual class CFormation* GetCrew() const;
	virtual bool IsOperable() const { return fOperable != 0.0f; }
	virtual void SetOperable( float fOperable );

	virtual void Disappear();

	virtual CVec2 GetTowPoint();
	
	virtual void SetSlaveTransport( class CAIUnit* _pSlaveTransport ){ pSlaveTransport = _pSlaveTransport; }
	virtual bool HasSlaveTransport();
	virtual class CAIUnit* GetSlaveTransport() { return pSlaveTransport; }

	const CVec2 GetAmmoBoxCoordinates();

	virtual const float GetMaxSpeedHere( const CVec2 &point, bool bAdjust = true ) const;
	virtual const float GetRotateSpeed() const;

	virtual void DoAllowShoot( bool allow );

	virtual void ClearWaitForReload();
	
	virtual bool IsColliding() const;
	
	virtual const int CanGoBackward() const { return GetCrew() == 0; }

	virtual void StopUnit();
	virtual const DWORD GetNormale( const CVec2 &vCenter ) const;
	virtual const DWORD GetNormale() const;

	const CVec2 GetHookPoint() const;
	const CVec3 GetHookPoint3D() const;
	
	EActionNotify GetCurUninstallAction() const { return eCurrentStateOfInstall; }

	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );
	
	virtual void SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce = false );
	virtual void SendAcknowledgement( EUnitAckType ack, bool bForce = false );

	int GetInitialPlayer() const { return nInitialPlayer; }
	void SetInitialPlayer( const int nPlayer ) { nInitialPlayer = nPlayer; }
	virtual const NTimer::STime& GetBehUpdateDuration() const ;
	bool IsBeingCaptured() const ;
	void SetCapturingUnit( CFormation * pFormation ) ;
	CFormation * GetCapturedUnit() { return pCapturingUnit; }
	
	bool IsBeingHooked() const;
	void SetBeingHooked( class CAIUnit *pUnit );
	CAIUnit *GetHookingTransport();
	
	void UpdateAmmoBoxVisibility();
};
#endif // __ARTILLERY_H__
