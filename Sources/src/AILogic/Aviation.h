#ifndef __AVIATION_H__
#define __AVIATION_H__

#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "AIUnit.h"
#include "PlanePath.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitGuns;
class CTurret;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class to extend if unit to go by CPlaneSmoothPath
interface IAviationUnit
{
	virtual const CVec2 &GetSpeedHorVer() const = 0;
	virtual void SetSpeedHorVer( const class CVec2 &_vSpeedHorVer) = 0;
	virtual const WORD GetDivingAngle() const = 0;
	virtual const WORD GetClimbingAngle() const = 0;
	virtual const CVec3 & GetNewPoint() const = 0;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlanesFormation : public IRefCount, public IAviationUnit, public IBasePathUnit
{
	OBJECT_COMPLETE_METHODS( CPlanesFormation );
	DECLARE_SERIALIZE;

	CPtr<CPlaneSmoothPath> pPath;
	CVec2 vCenter2D;
	float fZ;
	
	float fMaxSpeed;
	WORD wDirection;
	CVec2 vDirection;
	CVec2 vSpeedHorVer;

	// for member counting
	int nProcessed;
	int nAlive;

	CVec3 vNewPos;
	WORD wNewDirection;
	CVec2 vNewDirection;
	float fBombPointOffset;

	typedef std::unordered_map<CVec2, CPlaneSmoothPath::SMemberInfo, SVec2Hash, SVec2Equ> CMemberCache;
	CMemberCache memberCache;

public:
	void SetNewPos( const CVec3 &vCenter );
	CVec2 GetPointByFormationOffset( const CVec2 &vFormationOffset );
	WORD GetDirByFormationOffset( const CVec2 &vFormationOffset );
	float GetCurvatureRadius( const CVec2 &vFormationOffset );
	const CVec2 & GetSpeedByFormationOffset( const CVec2 &vFormationOffset );
	const float GetBombPointOffset() const { return fBombPointOffset; }

	void AddProcessed();
	void AddAlive();
	bool IsAllProcessed() const;
	void SecondSegment();
	const CVec2 &GetNewDirVector() const { return vNewDirection; }

	void Init( const CVec2 &vCenter, const float _fZ, const float fTurnRadiusMin, const float fTurnRadiusMax, const WORD _wDirection, const float fMaxSpeed, const float _fBombPointOffset );

	virtual const CVec3 & GetNewPoint() const { return vNewPos; }
	virtual const CVec2 &GetSpeedHorVer() const { return vSpeedHorVer; }
	virtual void SetSpeedHorVer( const class CVec2 &_vSpeedHorVer){vSpeedHorVer=_vSpeedHorVer;}
	virtual const WORD GetDivingAngle() const { return 0; }
	virtual const WORD GetClimbingAngle() const { return 0; }
	
	
	virtual void UpdateDirection( const CVec2 &newDir );
	virtual void UpdateDirection( const WORD newDir );
	
	virtual bool SendAlongPath( interface IPath *pPath );
	virtual const WORD GetDir() const { return wDirection; }
	virtual const CVec2& GetDirVector() const { return vDirection; }
	virtual const WORD GetFrontDir() const { return wDirection; }
	virtual const CVec2& GetCenter() const { return vCenter2D; }
	virtual const float GetZ() const { return fZ; }
	virtual const CVec2& GetSpeed() const 
	{ 
		static CVec2 vSpeed;		
		return (vSpeed = vDirection * fMaxSpeed); 
	}
	virtual interface ISmoothPath* GetCurPath() const;

	// �� � ��������
	virtual const WORD GetID() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual const float GetRotateSpeed() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual const float GetMaxSpeedHere( const CVec2 &point, bool bAdjust = true ) const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual const float GetMaxPossibleSpeed() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual const float GetPassability() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual bool CanMove() const { NI_ASSERT_T( false, "WRONG CALL"); return true; }
	virtual bool CanMovePathfinding() const { NI_ASSERT_T( false, "WRONG CALL"); return true; }
	virtual const int GetBoundTileRadius() const { NI_ASSERT_T( false, "WRONG CALL"); return 1; }
	virtual const CVec2 GetAABBHalfSize() const { NI_ASSERT_T( false, "WRONG CALL"); return VNULL2; }
	virtual void SetCoordWOUpdate( const CVec3 &newCenter ) { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true ) { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual const SRect GetUnitRectForLock() const { NI_ASSERT_T( false, "WRONG CALL"); return SRect(); }
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true ) { NI_ASSERT_T( false, "WRONG CALL"); return true; }
	virtual bool TurnToUnit( const CVec2 &targCenter ) { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool IsIdle() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool IsTurning() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual void StopUnit() { }
	virtual void StopTurning() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void ForceGoByRightDir() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual interface IStaticPathFinder* GetPathFinder() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true ) { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool CheckToTurn( const WORD wNewDir ) { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual void LockTiles( bool bUpdate = true ) { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void LockTilesForEditor() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void UnlockTiles( const bool bUpdate = true ) { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void FixUnlocking() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual void UnfixUnlocking() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual bool CanTurnToFrontDir( const WORD wDir ) { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool IsInFormation() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual class CFormation* GetFormation() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual const CVec2 GetUnitPointInFormation() const { NI_ASSERT_T( false, "WRONG CALL"); return VNULL2; }
	virtual const int GetFormationSlot() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual bool CanGoToPoint( const CVec2 &point ) const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual float GetSmoothTurnThreshold() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual void SetDesirableSpeed( const float fDesirableSpeed ) { NI_ASSERT_T( false, "WRONG CALL");  }
	virtual void UnsetDesirableSpeed() { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual float GetDesirableSpeed() const { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual void AdjustWithDesirableSpeed( float *pfMaxSpeed ) const { NI_ASSERT_T( false, "WRONG CALL"); }
	virtual const int CanGoBackward() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool IsLockingTiles() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool HasSuspendedPoint() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward = true ) const { NI_ASSERT_T( false, "WRONG CALL"); return true; }
	virtual IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking ) { NI_ASSERT_T( false, "WRONG CALL"); return 0; }
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual bool IsTrain() const { NI_ASSERT_T( false, "WRONG CALL"); return false; }
	virtual const SVector GetLastKnownGoodTile() const { NI_ASSERT_T( false, "WRONG CALL"); return SVector(0,0); }

	virtual bool IsDangerousDirExist() const { return false; }
	virtual const WORD GetDangerousDir() const { return 0; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAviation : public CAIUnit, public IAviationUnit
{
	OBJECT_NORMAL_METHODS( CAviation );
	DECLARE_SERIALIZE;
	
	CGDBPtr<SMechUnitRPGStats> pStats;

	// ��������� ������
	CPtr<CUnitGuns> pGuns;

	// ����������� �����
	std::vector< CObj<CTurret> > turrets;

	// ��� �������� ���������
	CPtr<CPlanesFormation> pFormation;
	CVec2 vPlanesShift;										// shift in formation

	CVec3 lastPos;
	WORD wLastDir;
	CVec3 vNormal, vFormerNormal;
	float fFormerCurvatureSign;

	float fTiltAnge;											//���� ������� ��� ��������
	NTimer::STime timeLastTilt;						// �������� ������ ���� �������

	CVec2 vSpeedHorVer;										//���������� �������� �� �������������� � ������������ ������������
	CVec2 vFormerHorVerSpeed;							//��� ���������� ��������
	CVec2 vFormerDir;
	
	int /*SUCAviation::AIRCRAFT_TYPE*/ eAviationType;
protected:
	virtual void InitGuns();
	virtual const CUnitGuns* GetGuns() const { return pGuns; }
	virtual CUnitGuns* GetGuns() { return pGuns; }
public:
	virtual ~CAviation();

	virtual int GetMovingType() const ;
	virtual void Init( const CVec2 &center, const int z, const SUnitBaseRPGStats *pStats, const float fHP, const WORD dir, const BYTE player, const WORD id, EObjVisType eVisType, const int dbID );

	// IAviationUnit implementation
	// ��� ������������ ��������
	virtual const CVec3 &GetNewPoint() const { NI_ASSERT_T(false, "wrong call"); return lastPos; }
	virtual const CVec2 &GetSpeedHorVer() const { return vSpeedHorVer; }
	void SetSpeedHorVer( const class CVec2 &_vSpeedHorVer){vSpeedHorVer=_vSpeedHorVer;}
	virtual const WORD GetDivingAngle() const ;
	virtual const WORD GetClimbingAngle() const ;

	// end IAviationUnit implementation

	virtual void GetSpeed3( CVec3 *pSpeed ) const ;

	virtual void Segment();
	virtual void SecondSegment( const bool bUpdate = true );
	
	virtual const SUnitBaseRPGStats* GetStats() const { return pStats; }
	virtual IStatesFactory* GetStatesFactory() const;
	
	virtual class CTurret* GetTurret( const int nTurret ) const { return turrets[nTurret]; }
	virtual const int GetNTurrets() const { return turrets.size(); }
	
	// ��� ��������
	virtual void GetShotInfo( struct SAINotifyMechShot *pShotInfo ) const { pShotInfo->typeID = GetShootAction(); pShotInfo->pObj = const_cast<CAviation*>(this); }
	virtual const EActionNotify GetShootAction() const { return ACTION_NOTIFY_MECH_SHOOT; }
	virtual const EActionNotify GetAimAction() const { return ACTION_NOTIFY_AIM; }
	virtual const EActionNotify GetDieAction() const { return ACTION_NOTIFY_DIE; }
	virtual const EActionNotify GetIdleAction() const { return ACTION_NOTIFY_IDLE; }
	virtual const EActionNotify GetMovingAction() const { return ACTION_NOTIFY_MOVE; }

	virtual const bool CanShootToPlanes() const;

	//
	virtual int GetNGuns() const;
	virtual class CBasicGun* GetGun( const int n ) const;

	virtual class CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime );
	virtual float GetMaxFireRange() const;
	
	virtual const bool IsVisible( const BYTE party ) const { return true; }
	
	virtual NTimer::STime GetDisappearInterval() const { return 0; }

	// ��� ��������� ������� � ������������.
	virtual float GetTerrainHeight( const float x, const float y, const NTimer::STime timeDiff ) const { return 0; }

	virtual void GetPlacement( struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff );
	
	virtual bool IsMech() const { return true; }
	
	virtual bool IsColliding() const { return false; }
	
	virtual const int CanGoBackward() const { return false; }
	virtual const float GetSightRadius() const;
	virtual void Die( const bool fromExplosion, const float fDamage );

	virtual void StopUnit() { }
	virtual void Disappear();
	
	// �������� unit ( ���� ��� ��� ��������, �� ������ lock �������� )
	virtual void Lock( const CBasicGun *pGun ) { }
	// unlock unit ( ���� ������� ������ gun-��, �� ������ �� �������� )
	virtual void Unlock( const CBasicGun *pGun ) { }
	// ������� �� �����-���� gun-��, �� ������ pGun
	virtual bool IsLocked( const CBasicGun *pGun ) const { return true; }
	
	// plane's formation, to force planes keep parade during flight.
	void SetPlanesFormation( class CPlanesFormation *pFormation, const CVec2 &vShift );
	CPlanesFormation * GetPlanesFormation();
	const CVec2 GetPlaneShift() const { return vPlanesShift; }

	const SRect GetUnitRect() const;
	virtual const WORD GetDir() const;
	virtual const CVec2& GetDirVector() const;
	virtual const WORD GetFrontDir() const;
	virtual const float GetZ() const;
	virtual const CVec2& GetSpeed() const;
	float GetPathCurvatureRadius() const;

	void SetAviationType( const int _eAviationType ){ eAviationType = _eAviationType; }
	const int GetAviationType() const { return eAviationType; }

	const NTimer::STime GetNextSecondPathSegmTime() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AVIATION_H__
