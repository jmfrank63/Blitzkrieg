#ifndef __FORMATION_H__
#define __FORMATION_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "CommonUnit.h"
#include "..\Misc\BitData.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFormationCenter : public CCommonUnit
{
	DECLARE_SERIALIZE;
	
	CVec2 speed;
	CVec2 center;
	CVec2 dir;
	CVec2 vAABBHalfSize;
	float z;

	int nBoundTileRadius;

	CPtr<ISmoothPath> pSmoothPath;
	CPtr<IStaticPath> pStaticPath;

	float maxDiff;
	float fSpeedCoeff;
	SVector lastKnownGoodTile;
protected:
	float maxSpeed;
public:
	CFormationCenter() : maxSpeed( 1000 ), dir( 0, 0 ), vAABBHalfSize( 0, 0 ), nBoundTileRadius( 0 ) { }
	void Init( const CVec2 &center, const int z, const WORD dir, const int dbID );

	virtual const CVec2& GetCenter() const { return center; }
	virtual const CVec2& GetSpeed() const { return speed; }
	virtual const float GetZ() const { return z; }
	virtual const float GetRotateSpeed() const { return 0; }
	virtual const float GetMaxPossibleSpeed() const;
	virtual const float GetSpeedForFollowing();
	virtual bool CanMove() const { return maxSpeed > 0; }
	virtual bool CanMovePathfinding() const { return CanMove(); }
	virtual bool CanRotate() const { return true; }
	virtual const int GetBoundTileRadius() const { return nBoundTileRadius; }
	virtual const WORD GetDir() const { return GetDirectionByVector( dir ); }
	virtual const WORD GetFrontDir() const { return GetDirectionByVector( dir ); }
	virtual const CVec2& GetDirVector() const { return dir; }
	virtual const CVec2 GetAABBHalfSize() const { return vAABBHalfSize; }
	virtual void SetCoordWOUpdate( const CVec3 &newCenter );
	virtual void SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit = true );
	virtual IStaticPath* CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking );

	virtual const SRect GetUnitRectForLock() const;
	
	virtual bool TurnToDir( const WORD &newDir, const bool bCanBackward = true, const bool bForward = true );
	virtual void UpdateDirection( const CVec2 &newDir );
	virtual void UpdateDirection( const WORD newDir );
	// stop only center of formation, not units
	void StopFormationCenter();

	//
	void SetMaxSpeed( const float &_maxSpeed ) { maxSpeed = _maxSpeed; }
	void SetAABBHalfSize( const CVec2 &_vAABBHalfSize ) { vAABBHalfSize = _vAABBHalfSize; }
	void SetBoundTileRadius( const int _nBoundTileRadius ) { nBoundTileRadius = _nBoundTileRadius; }

	void Segment();
	// возвращает - поехал или нет
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( IPath *pPath );

	virtual interface IStaticPathFinder* GetPathFinder() const;
	
	interface ISmoothPath* GetCurPath() const { return pSmoothPath; }
	interface IStaticPath* GetStaticPath() const { return pStaticPath; }

	virtual void LockTiles( bool bUpdate = true ) { }
	virtual void LockTilesForEditor() { }
	virtual void UnlockTiles( const bool bUpdate = true ) { }
	virtual void FixUnlocking() { }
	virtual void UnfixUnlocking() { }
	virtual bool CanTurnToFrontDir( const WORD wDir ) { return true; }	

	const CVec2 GetNearFormationPos() const;
	const CVec2 GetFarFormationPos() const;
	void GetNextTiles( std::list<SVector> *pTiles ) const;

	void NotifyDiff( const float fDiff );
	
	virtual float GetSmoothTurnThreshold() const;
	virtual const int CanGoBackward() const { return false; }

	virtual const float GetCurSpeedBonus() const = 0;
	virtual const float GetRadius() const = 0;
	virtual const int Size() const = 0;
	virtual bool IsLockingTiles() const { return false; }
	
	virtual bool CheckToTurn( const WORD wNewDir ) { return true; }
	virtual bool HasSuspendedPoint() const { return false; }
	
	virtual bool IsInOneTrain( interface IBasePathUnit *pUnit ) const;
	virtual bool IsTrain() const { return false; }

	// количество сегментнов, прошедшее с прошлого вызова SecondSegment
	virtual const float GetPathSegmentsPeriod() const;

	virtual const SVector GetLastKnownGoodTile() const;
	
	virtual bool IsDangerousDirExist() const { return false; }
	virtual const WORD GetDangerousDir() const { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFormation : public CFormationCenter
{
	OBJECT_NORMAL_METHODS( CFormation );
	DECLARE_SERIALIZE;
	
	struct SUnitInfo
	{
		DECLARE_SERIALIZE;

		struct SSquadGeometry
		{
			// поворот от направления формации к юниту
			CVec2 vForm2Unit;
			// проекция смещения юнита относительно центра формации на направление формации
			float fUnitProj;
			// собственное направление юнита, от центра формации
			WORD dir;

			SSquadGeometry() : vForm2Unit( VNULL2 ), fUnitProj( 0 ), dir( 0 ) { }
		};

	public:
		class CSoldier *pUnit;
		int nSlotInStats;
		std::vector<SSquadGeometry> geoms;

		SUnitInfo() : pUnit( 0 ), nSlotInStats( -1 ) { }
	};
	std::vector<SUnitInfo> units;
	int nUnits;

	struct SCommonGeometryInfo
	{
		float fMaxUnitProj;
		// радиус формации
		float fRadius;

		SCommonGeometryInfo() : fMaxUnitProj( 0 ), fRadius( 0 ) { }
	};
	std::vector<SCommonGeometryInfo> geomInfo;
	int nCurGeometry;

	float fPass;
	
	NTimer::STime timeToCamouflage;
	WORD id;

	CArray1Bit availCommands;

	//
	BYTE cPlayer;

	struct SGunInfo
	{
		int nUnit;
		int nUnitGun;

		SGunInfo() : nUnit( -1 ), nUnitGun( -1 ) { }
		SGunInfo( const int _nUnit, const int _nUnitGun ) : nUnit( _nUnit ), nUnitGun( _nUnitGun ) { }
	};
	std::vector<SGunInfo> guns;

	bool bWaiting;
	CGDBPtr<SSquadRPGStats> pStats;

	bool bDisabled;

	enum EObjectInsideOf { EOIO_NONE, EOIO_BUILDING, EOIO_TRANSPORT, EOIO_ENTRENCHMENT, EOIO_UNKNOWN };
	EObjectInsideOf eInsideType;
	IRefCount *pObjInside;
	
	float fMaxFireRange;

	struct SVirtualUnit
	{
		DECLARE_SERIALIZE;
	public:
		SVirtualUnit() : pSoldier( 0 ) { }

		CSoldier *pSoldier;
		int nSlotInStats;
	};
	std::vector<SVirtualUnit> virtualUnits;
	int nVirtualUnits;
	bool bCanBeResupplied;

	// для хранения данных о переносимом миномете
	class CCarryedMortar
	{
		DECLARE_SERIALIZE;
		bool bHasMortar;
		CGDBPtr<SUnitBaseRPGStats> pStats;
		float fHP;
		int nDBID;
	public:
		CCarryedMortar() : bHasMortar( false ), nDBID( -1 ) { }
		bool HasMortar() const { return bHasMortar; } 
		int CreateMortar( const class CFormation *pOwner );
		void Init( const class CAIUnit *pArt );
	};
	CCarryedMortar mortar;
	bool bBoredInMoveFormationSent;
	NTimer::STime lastBoredInMoveFormationCheck;

	bool bWithMoraleOfficer;

	//
	void InitGeometries();
	void PrepareToDelete();

	// проверка на посылка bored если в movement формации
	void CheckForMoveFormationBored();
	bool IsMemberResting( class CSoldier *pSoldier ) const;
	void ProcessLoadCommand( CAICommand *pCommand, bool bPlaceInQueue );
public:
	CFormation() : pObjInside( 0 ), bWithMoraleOfficer( false ) { }
	void Init( const SSquadRPGStats *pStats, const CVec2 &center, const int z, const WORD dir, const int dbID );
	// передвигает центр формации в её центр масс и инициализирует geomInfo
	void MoveGeometries2Center();
	void ChangeGeometry( const int nGeometry );
	const int GetNGeometries() const;
	const int GetCurGeometry() const { return nCurGeometry; }
	
	virtual const WORD GetID() const { return id; }
	const SSquadRPGStats* GetStats() const { return pStats; }

	// добавить новый юнит в формацию, порядковый номер его в статах - nSlot, местоположение юнита инициализируется
	void AddNewUnitToSlot( class CSoldier *pUnit, const int nSlot, const bool bSendToWorld = true );
	
	// добавить новый юнит на позицию nPos в списке юнитов объекта formation, причём местоположение юнита не инициализируется
	void AddUnit( class CSoldier *pUnit, const int nPos );
	void DelUnit( const BYTE cPos );
	void DelUnit( class CSoldier *pUnit );
	// для Save/Load
	void SetUnitToPos( const BYTE cPos, class CSoldier *pUnit );
	
	virtual BYTE GetAIClass() const;

	const CVec2 GetUnitCoord( const BYTE cSlot ) const;
	const float GetUnitLineShift( const BYTE cPos ) const;
	const float GetMaxProjection() const { return geomInfo[nCurGeometry].fMaxUnitProj; }
	// сдвиг юнита относительно центра формации, когда юнит стоит правильно
	const CVec2 GetUnitShift( const BYTE cSlot ) const;
	// каким должно быть собственное направление юнита
	const WORD GetUnitDir( const BYTE cSlot ) const;
	// возвращает позицию в статах формации для юнита с порядковым номером cSlot в массиве units 
	const int GetUnitSlotInStats( const BYTE cSlot ) const;
	virtual const float GetPassability() const { return fPass; }

	bool IsStopped() const;
	virtual const CVec2& GetCenter() const { return CFormationCenter::GetCenter(); }
	virtual const float GetRadius() const { return geomInfo[nCurGeometry].fRadius; }
	
	void Segment();

	void GetNextPositions( const BYTE cPos, std::list<SVector> *pTiles ) const;
	const CVec2 GetFarUnitPos( const BYTE cPos );
	virtual bool IsIdle() const;
	virtual bool IsTurning() const { return false; }
	// все ли юниты находятся в rest состоянии?
	bool IsEveryUnitResting() const;
	bool IsEveryUnitInTransport() const;
	virtual void StopUnit();
	virtual void StopTurning();
	virtual void ForceGoByRightDir() {}

	virtual interface IStatesFactory* GetStatesFactory() const;

	virtual const int Size() const { return nUnits; }
	class CSoldier* operator[]( const int n ) const { NI_ASSERT_T( n < nUnits, "Wrong unit number" ); return units[n].pUnit; }

	virtual bool CanCommandBeExecuted( class CAICommand *pCommand );
	virtual bool CanCommandBeExecutedByStats( class CAICommand *pCommand );
	virtual bool CanCommandBeExecutedByStats( int nCmd ) const;

	//
	virtual const bool CanShootToPlanes() const;
	virtual int GetNGuns() const { return guns.size(); }
	virtual class CBasicGun* GetGun( const int n ) const;

	virtual class CBasicGun* ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime );

	virtual const BYTE GetPlayer() const { return cPlayer; }
	virtual void SetPlayerForEditor( const int nPlayer );
	virtual void ChangePlayer( const BYTE cPlayer );

	virtual void SetSelectable	( bool bSelectable );

	virtual const float GetSightRadius() const;

	//
	void SetToWaitingState() { bWaiting = true; }
	void UnsetFromWaitingState() { bWaiting = false; }
	const bool IsInWaitingState() const { return bWaiting; }
	
	virtual const bool IsVisible( const BYTE party ) const;
	
	//
	void WasHitNearUnit();
	
	virtual void Fired( const float fGunRadius, const int nGun  ) { }
	
	virtual void SetAmbush();
	virtual void RemoveAmbush();
	
	virtual const NTimer::STime GetTimeToCamouflage() const;
	virtual void SetCamoulfage();
	virtual void RemoveCamouflage( ECamouflageRemoveReason eReason );
	
	virtual void UpdateArea( const EActionNotify eAction );
	
	virtual class CTurret* GetTurret( const int nTurret ) const { return 0; }
	virtual const int GetNTurrets() const { return 0; }
	virtual bool IsMech() const { return false; }
	
	virtual void Disappear();
	virtual void Die( const bool fromExplosion, const float fDamage );

	virtual bool IsAlive() const;
	// возвращает - поехал или нет
	virtual bool SendAlongPath( interface IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn = true );
	virtual bool SendAlongPath( IPath *pPath );
	
	virtual const float GetCurSpeedBonus() const;
	virtual const float GetDispersionBonus() const;
	virtual const float GetRelaxTimeBonus() const;
	virtual const float GetFireRateBonus() const;
	virtual const float GetCoverBonus() const;
	const float GetSightMultiplier() const;
	
	bool IsAllowedLieDown() const;
	bool IsAllowedStandUp() const;
	
	void Disable();
	void Enable();
	bool IsDisabled() const { return bDisabled; }
	
	virtual void UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand );

	bool IsFree() const { return eInsideType == EOIO_NONE; }
	bool IsInBuilding() const { return eInsideType == EOIO_BUILDING; }
	bool IsInEntrenchment() const { return eInsideType == EOIO_ENTRENCHMENT; }
	bool IsInTransport() const { return eInsideType == EOIO_TRANSPORT; }

	void SetFree();
	void SetInBuilding( class CBuilding *pBuilding );
	void SetInTransport(  class CMilitaryCar *pUnit );
	void SetInEntrenchment( class CEntrenchment *pEntrenchment );

	class CBuilding* GetBuilding() const;
	class CEntrenchment* GetEntrenchment() const;
	class CMilitaryCar* GetTransportUnit() const;
	
	virtual void GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo );

	virtual bool IsFormation() const { return true; }
	
	virtual void SendAcknowledgement( EUnitAckType ack, bool bForce = false );
	virtual void SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce = false );
	// установить центр формации в центр тяжести юнитов
	void BalanceCenter();
	
	virtual const int GetMinArmor() const { return 0; }
	virtual const int GetMaxArmor() const { return 0; }
	virtual const int GetMinPossibleArmor( const int nSide ) const { return 0; }
	virtual const int GetMaxPossibleArmor( const int nSide ) const { return 0; }
	virtual const int GetArmor( const int nSide ) const { return 0; }
	virtual const int GetRandomArmor( const int nSide ) const { return 0; }

	virtual float GetMaxFireRange() const { return fMaxFireRange; }
	void AddAvailCmd( const EActionCommand &eCmd ) { availCommands.SetData( eCmd ); }
	
	virtual EUnitAckType GetGunsRejectReason() const;

	// используется только для отложенных updates
	virtual const bool IsVisible( const int nParty ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }

	const int VirtualUnitsSize() const { return nVirtualUnits; }
	const int GetVirtualUnitSlotInStats( const int nVirtualUnit ) const;
	void AddVirtualUnit( class CSoldier *pSoldier, const int nSlotInStats );
	void MakeVirtualUnitReal( class CSoldier *pSoldier );
	void DelVirtualUnit( class CSoldier *pSoldier );

	// for bored condition
	virtual void UnRegisterAsBored( const enum EUnitAckType eBoredType );
	virtual void RegisterAsBored( const enum EUnitAckType eBoredType );

	// устанавливает солдату бонусы от формации
	void SetGeometryPropertiesToSoldier( class CSoldier *pSoldier, const bool bChangeWarFog );

	// для переноса миномета
	void SetCarryedMortar( class CAIUnit *pMortar );
	bool HasMortar() const ;							// true when formation carryes a mortar
		// returns ID of installed artillery
	int InstallCarryedMortar();
	//CRAP}

	virtual void ResetTargetScan();
	// просканировать, если пора; если нашли цель, то атаковать
	virtual BYTE AnalyzeTargetScan(	CAIUnit *pCurTarget, const bool bDamageUpdated, const bool bScanForObstacles, IRefCount *pCheckBuilding );
	virtual void LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, class CBasicGun **pGun );

	virtual bool CanMoveForGuard() const { return CanMove(); }
	virtual float GetPriceMax() const;
	virtual const NTimer::STime& GetBehUpdateDuration() const { return SConsts::BEH_UPDATE_DURATION; }
	//to protect from human resupply formation in some states
	virtual bool IsResupplyable() const { return bCanBeResupplied; }
	virtual void SetResupplyable( const bool _bCanBeResupplied ) { bCanBeResupplied = _bCanBeResupplied; }

	virtual const bool IsWithMoraleOfficer() const { return bWithMoraleOfficer; }

	virtual void FreezeByState( const bool bFreeze );
	
	// slow working
	virtual const float GetTargetScanRadius();
	
	virtual bool CanMoveAfterUserCommand() const { return CanMove(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FORMATION_H__
