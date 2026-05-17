#ifndef __TRAIN_PATH_UNIT_H__
#define __TRAIN_PATH_UNIT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "PathUnit.h"
#include "BasePathUnit.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTrainSmoothPath;
class CEdgePoint;
class CCarriagePathUnit;
interface IEdge;
interface IStaticPath;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTrainPathUnit : public IRefCount, public IBasePathUnit
{
	OBJECT_COMPLETE_METHODS( CTrainPathUnit );
	DECLARE_SERIALIZE;

	std::vector< CPtr<CCarriagePathUnit> > carriages;
	std::vector< std::list<int> > nodesInside;
	std::vector< std::list<int> > intermNodes;

	CPtr<CTrainSmoothPath> pSmoothPath;
	CPtr<CEdgePoint> pCurEdgePoint;

	CVec2 vCenter;
	CVec2 vSpeed;
	CVec2 vDir;
	float fMaxPossibleSpeed;
	float fPassability;

	float fTrainLength;
	bool bFrontDir;
	bool bCanMove;

	std::unordered_set<int> damagedTrackCarriages;

	CPtr<IStaticPath> pPathToMove;
public:
	CTrainPathUnit() : fTrainLength( 0 ), bFrontDir( true ), bCanMove( false ) { }
	CTrainPathUnit( class CAIUnit *pOwner ) : fTrainLength( 0 ), bFrontDir( true ), bCanMove( false ) { }
	virtual void Init( const CVec2 &center, const int z, const WORD dir, const WORD id ) { }
	// проинициализировать локомотивом pUnit
	bool InitBy( class CCarriagePathUnit *pUnit );

	virtual ISmoothPath* GetSmoothPath() const;
	virtual const float GetRotateSpeed() const { return 0; }

	virtual interface ISmoothPath* GetCurPath() const;
	virtual void SetCurPath( interface ISmoothPath *pNewPath ) { NI_ASSERT_T( false, "Train can't change smooth path" ); }
	virtual void RestoreDefaultPath() { NI_ASSERT_T( false, "Train can't change smooth path" ); }

	virtual void SecondSegment( const bool bUpdate = true );
	virtual IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking );
	// возвращает - поехал или нет
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( interface IPath *pPath );

	CEdgePoint* GetCurEdgePoint();
	void SetCurEdgePoint( CEdgePoint *pEdgePoint );

	void AddCarriage( class CCarriagePathUnit *pCarriage );
	const float GetTrainLength() const { return fTrainLength; }

	const int GetNCarriages() const { return carriages.size(); }
	CCarriagePathUnit* GetCarriage( const int n );
	// расстояние от заднего колеса вагона n до переднего колеса вагона m, если m присоединён к m
	const float GetDistFromBackToFrontWheel( const int n, const int m );
	const float GetDistFromFrontToBackWheel( const int n, const int m );
	void PushNodesToFrontCarriage( std::list<int> &newNodes );

	void SetBackWheel( const int n );
	void SetFrontWheel( const int n );

	void ChangeDirection( const bool bNewFrontDir );
	bool IsFrontDir() const { return bFrontDir; }

	void GetTrainNodes( std::list<int> *pNodesOfTrain );

	// IBasePathUnit
	virtual void SetRightDir( bool _bRightDir ) { }
	virtual bool GetRightDir() const { return true; }

	virtual const WORD GetID() const;
	virtual const CVec2& GetCenter() const;
	virtual const float GetZ() const;
	virtual const SVector GetTile() const { return AICellsTiles::GetTile( GetCenter() ); }
	virtual const float GetMaxSpeedHere( const CVec2 &point, bool bAdjust = true ) const;
	virtual const float GetMaxPossibleSpeed() const;
	virtual const float GetPassability() const;
	virtual bool CanMove() const;
	virtual bool CanMovePathfinding() const;
	virtual const CVec2& GetSpeed() const;
	virtual const int GetBoundTileRadius() const;
	virtual const WORD GetDir() const;
	virtual const WORD GetFrontDir() const;
	virtual const CVec2& GetDirVector() const;
	virtual const CVec2 GetAABBHalfSize() const;
	virtual void SetCoordWOUpdate( const CVec3 &newCenter );
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true );

	virtual const SRect GetUnitRectForLock() const;
	
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true ) { return false; }
	virtual bool TurnToUnit( const CVec2 &targCenter ) { return false; }
	virtual void TurnAgainstUnit( const CVec2 &targCenter ) { }
	virtual void UpdateDirection( const CVec2 &newDir );
	virtual void UpdateDirection( const WORD newDir );
	virtual bool IsIdle() const;
	virtual bool IsTurning() const { return false; }
	virtual void StopUnit();
	virtual void StopTurning() { }
	virtual void ForceGoByRightDir() { }
	
	virtual interface IStaticPathFinder* GetPathFinder() const;
	// можно ли повернуться к направлению wNewDir, если нет - то попытаться проинициализировать путём в точку, где разворот возможен
	virtual bool CheckToTurn( const WORD wNewDir ) { return false; }

	virtual void LockTiles( bool bUpdate = true ) { }
	virtual void LockTilesForEditor() { }
	virtual void UnlockTiles( const bool bUpdate = true ) { }
	virtual void FixUnlocking() { }
	virtual void UnfixUnlocking() { }
	virtual bool IsLockingTiles() const { return false; }
	virtual bool CanTurnToFrontDir( const WORD wDir ) { return false; }

	virtual bool IsInFormation() const { return false; }
	virtual class CFormation* GetFormation() const { return 0; }
	virtual const CVec2 GetUnitPointInFormation() const { return VNULL2; }
	virtual const int GetFormationSlot() const { return 0; }

	virtual bool CanGoToPoint( const CVec2 &point ) const { return true; }
	virtual BYTE GetAIClass() const { return AI_CLASS_ANY; }
	
	virtual float GetSmoothTurnThreshold() const { return 1.0f; }
	
	virtual void SetDesirableSpeed( const float fDesirableSpeed );
	virtual void UnsetDesirableSpeed();
	virtual float GetDesirableSpeed() const;
	virtual void AdjustWithDesirableSpeed( float *pfMaxSpeed ) const;
	
	virtual const int CanGoBackward() const { return true; }
	virtual bool HasSuspendedPoint() const { return false; }
	virtual bool CanRotateTo( SRect smallRect, const CVec2 &vNewDir, bool bWithUnits, bool bCanGoBackward = true ) const { return false; }
	
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const;
	virtual bool IsTrain() const { return true; }
	const CTrainPathUnit* GetTrainOwner() const { return this; }
	
	virtual const SVector GetLastKnownGoodTile() const { return GetTile(); }

	void LocomotiveDead();
	
	virtual bool CanRotate() const { return false; }

	void CarriageTrackDamaged( const int nOwnerID, const bool bTrackDamagedState );

	virtual bool IsDangerousDirExist() const { return false; }
	virtual const WORD GetDangerousDir() const { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCarriagePathUnit : public CPathUnit
{
	OBJECT_COMPLETE_METHODS( CCarriagePathUnit );
	DECLARE_SERIALIZE;

	CObj<CTrainPathUnit> pTrain;

	CPtr<CEdgePoint> pFrontWheelPoint;
	CPtr<CEdgePoint> pBackWheelPoint;

	// рёбра, на которых стоит вагон
	std::list< CPtr<IEdge> > edges;
	CVec2 vOldDir;
	CVec2 vOldCenter;

	//
	void InitCenterAndDir3D( const CVec2 &vCenter, CVec3 *pvCenter3D, CVec3 *pvDir3D ) const;
	const CVec3 Get3DPointOfUnit( const CVec2 &vCenter, const float fLength ) const;
	const CVec2 Get2DPointOfUnit( const CVec2 &vCenter, const float fLength ) const;
public:
	CCarriagePathUnit() { }

	virtual void Init( class CAIUnit *pOwner, const CVec2 &center, const int z, const WORD dir, const WORD id );
	const CTrainPathUnit* GetTrainOwner() const;

	virtual ISmoothPath* GetSmoothPath() const;

	virtual interface ISmoothPath* GetCurPath() const;
	virtual void SetCurPath( interface ISmoothPath *pNewPath ) { NI_ASSERT_T( false, "Train can't change smooth path" ); }
	virtual void RestoreDefaultPath() { NI_ASSERT_T( false, "Train can't change smooth path" ); }

	virtual void FirstSegment();
	virtual void SecondSegment( const bool bUpdate = true );
	virtual IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking );
	// возвращает - поехал или нет
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( interface IPath *pPath );

	void SetOnRailroad();
	void HookTo( CCarriagePathUnit *pUnit );

	const CVec3 GetBackHookPoint3D() const;
	const CVec2 GetBackHookPoint2D() const;
	const CVec3 GetFrontHookPoint3D() const;
	const CVec2 GetFronHookPoint2D() const;
	const CVec3 GetBackHookPoint3DByFrontPoint( const CVec2 &vFrontPoint ) const;
	const CVec2 GetBackHookPoint2DByFrontPoint( const CVec2 &vFrontPoint ) const;
	const CVec3 GetFrontWheel3D() const;
	const CVec2 GetFrontWheel2D() const;

	const CVec3 GetBackWheelPoint3DByFrontPoint( const CVec2 &vFrontPoint ) const;
	const CVec2 GetBackWheelPoint2DByFrontPoint( const CVec2 &vFrontPoint ) const;
 
	const float GetDistanceToBackWheel() const;
	const float GetDistanceBetweenWheels() const;

	void SetPlacementByWheels( CEdgePoint *pFrontWheelPoint, CEdgePoint *pBackWheelPoint );

	CEdgePoint* GetFrontWheelPoint() const;
	CEdgePoint* GetBackWheelPoint() const;

	void SetSpeed( const CVec2 &_speed ) { speed = _speed; }

	virtual void GetPlacement(  struct SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff ) const;
	virtual void SetRightDir( bool bRightDir );
	
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true );
	virtual void SetNewCoordinatesForEditor( const CVec3 &newCenter );
	virtual void SetCoordWOUpdate( const CVec3 &newCenter );

	virtual bool CanTurnToFrontDir( const WORD wDir ) { return false; }
	virtual bool IsTrain() const { return true; }
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const;

	virtual void UpdateDirectionForEditor( const CVec2 &dirVec );
	virtual bool CanMove() const;
	virtual bool CanMovePathfinding() const;
	virtual bool CanRotate() const { return false; }

	void UnitDead();
	
	virtual void TrackDamagedState( const bool bTrackDamaged );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TRAIN_PATH_UNIT_H__
