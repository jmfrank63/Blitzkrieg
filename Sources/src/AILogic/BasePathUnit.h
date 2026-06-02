#ifndef __BASE_PATH_UNIT_H__
#define __BASE_PATH_UNIT_H__
#pragma ONCE
interface IBasePathUnit
{
	DECLARE_SERIALIZE;
	NTimer::STime nextTimeToSearchPathToFormation;
public:
	IBasePathUnit() : nextTimeToSearchPathToFormation( 0 ) { }
	
	virtual void SetRightDir( bool _bRightDir ) { }
	virtual bool GetRightDir()const { return true; }

	virtual const WORD GetID() const = 0;
	virtual const CVec2& GetCenter() const = 0;
	virtual const float GetZ() const = 0;
	virtual const SVector GetTile() const { return AICellsTiles::GetTile( GetCenter() ); }
	virtual const float GetRotateSpeed() const = 0;
	virtual const float GetMaxSpeedHere( const CVec2 &point, bool bAdjust = true ) const = 0;
	virtual const float GetSpeedForFollowing();
	virtual const float GetMaxPossibleSpeed() const = 0;
	virtual const float GetPassability() const = 0;
	virtual bool CanMove() const = 0;
	virtual bool CanMovePathfinding() const = 0;
	virtual bool CanRotate() const { return true; }
	virtual const CVec2& GetSpeed() const = 0;
	bool IsMoving() const { return GetSpeed() != VNULL2; }
	virtual const int GetBoundTileRadius() const = 0;
	virtual const WORD GetDir() const = 0;
	virtual const WORD GetFrontDir() const = 0;
	virtual const CVec2& GetDirVector() const = 0;
	virtual const CVec2 GetAABBHalfSize() const = 0;
	virtual void SetCoordWOUpdate( const CVec3 &newCenter ) = 0;
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true ) = 0;

	virtual const SRect GetUnitRectForLock() const = 0;
	
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true ) = 0;
	virtual bool TurnToUnit( const CVec2 &targCenter );
	virtual void TurnAgainstUnit( const CVec2 &targCenter );
	virtual void UpdateDirection( const CVec2 &newDir ) = 0;
	virtual void UpdateDirection( const WORD newDir ) = 0;	
	virtual bool IsIdle() const = 0;
	virtual bool IsTurning() const = 0;
	virtual void StopUnit() = 0;
	virtual void StopTurning() = 0;
	virtual void ForceGoByRightDir() = 0;
	
	virtual interface IStaticPathFinder* GetPathFinder() const = 0;
	virtual interface ISmoothPath* GetCurPath() const = 0;
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true ) = 0;
	virtual bool SendAlongPath( interface IPath *pPath ) = 0;
	virtual bool CheckToTurn( const WORD wNewDir ) = 0;

	virtual void LockTiles( bool bUpdate = true ) = 0;
	virtual void LockTilesForEditor() = 0;
	virtual void UnlockTiles( const bool bUpdate = true ) = 0;
	virtual void FixUnlocking() = 0;
	virtual void UnfixUnlocking() = 0;
	virtual bool CanTurnToFrontDir( const WORD wDir ) = 0;

	virtual bool IsInFormation() const { return false; }
	virtual class CFormation* GetFormation() const { return 0; }
	virtual const CVec2 GetUnitPointInFormation() const { return VNULL2; }
	virtual const int GetFormationSlot() const { return 0; }

	IStaticPath* GetPathToBuilding( class CBuilding *pBuilding, int *pnEntrance );
	IStaticPath* GetPathToEntrenchment( class CEntrenchment *pEntrenchment );
	
	virtual bool CanGoToPoint( const CVec2 &point ) const = 0;
	virtual BYTE GetAIClass() const { return AI_CLASS_ANY; }
	
	virtual float GetSmoothTurnThreshold() const = 0;
	
	virtual void SetDesirableSpeed( const float fDesirableSpeed ) = 0;
	virtual void UnsetDesirableSpeed() = 0;
	virtual float GetDesirableSpeed() const = 0;
	virtual void AdjustWithDesirableSpeed( float *pfMaxSpeed ) const = 0;
	
	virtual const int CanGoBackward() const = 0;
	bool IsOnLockedTiles( const struct SRect &rect );
	
	virtual bool IsLockingTiles() const = 0;
	virtual bool HasSuspendedPoint() const = 0;

	virtual bool CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward = true ) const { return true; }
	virtual IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking ) = 0;
	
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const = 0;
	virtual bool IsTrain() const = 0;
	
	void CantFindPathToFormation();
	const NTimer::STime& GetTimeToNextSearchPathToFormation() const { return nextTimeToSearchPathToFormation; }
	
	virtual const SVector GetLastKnownGoodTile() const = 0;

	virtual bool IsDangerousDirExist() const = 0;
	virtual const WORD GetDangerousDir() const = 0;
};
#endif // __BASE_PATH_UNIT_H__
