#ifndef __SHOOT_ESTIMATOR_INTERNAL_H__
#define __SHOOT_ESTIMATOR_INTERNAL_H__

#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ShootEstimator.h"
#include "Obstacle.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
class CBasicGun;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTankShootEstimator : public IShootEstimator
{
	OBJECT_COMPLETE_METHODS( CTankShootEstimator );
	DECLARE_SERIALIZE;

	class CAIUnit *pOwner;
	CPtr<CAIUnit> pBestUnit;
	CPtr<CBasicGun> pBestGun;
	int nBestGun;
	CPtr<CAIUnit> pCurTarget;
	bool bDamageToCurTargetUpdated;

	float fBestRating;
	DWORD dwForbidden;
	DWORD dwDefaultForbidden;
	
	CGDBPtr<SUnitBaseRPGStats> pMosinStats;

	// �����, ���������, ����� ��������� pGun �� pEnemy
	//const float FindTimeToTurn( CAIUnit *pEnemy, CBasicGun *pGun ) const;
	// ������� gun ��� pEnemy
	void ChooseGun( CBasicGun **pBestGun, int *nBestGun, CAIUnit *pEnemy );

	const float GetRating( CAIUnit *pEnemy, CBasicGun *pGun ) const;
	const float GetRating( const SUnitBaseRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const;
public:
	CTankShootEstimator() : pOwner( 0 ) { }
	explicit CTankShootEstimator( class CAIUnit *pOwner );

	virtual void Reset( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden );
	virtual void AddUnit( class CAIUnit *pEnemy );
	virtual class CAIUnit* GetBestUnit() const;
	virtual class CBasicGun* GetBestGun() const;
	virtual const int GetNumberOfBestGun() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoldierShootEstimator : public IShootEstimator
{
	OBJECT_COMPLETE_METHODS( CSoldierShootEstimator );
	DECLARE_SERIALIZE;

	static const int N_GOOD_NUMBER_ATTACKING_GRENADES;

	class CAIUnit *pOwner;
	CPtr<CAIUnit> pBestUnit;
	CPtr<CBasicGun> pBestGun;
	int nBestGun;
	CPtr<CAIUnit> pCurTarget;
	bool bDamageToCurTargetUpdated;

	float fBestRating;

	bool bHasGrenades;
	// ������� �������, �� �������� ����� ������� ������ ���� �� ��������
	bool bThrowGrenade;

	DWORD dwForbidden;

	CGDBPtr<SUnitBaseRPGStats> pMosinStats;

	// ������� gun ��� pEnemy
	void ChooseGun( CBasicGun **pBestGun, int *nBestGun, CAIUnit *pEnemy );

	const float GetRating( CAIUnit *pEnemy, CBasicGun *pGun ) const;
	const float GetRating( const SUnitBaseRPGStats *pStats, const CVec2 &vCenter, CBasicGun *pGun ) const;
public:
	CSoldierShootEstimator() : pOwner( 0 ) { }
	explicit CSoldierShootEstimator( class CAIUnit *pOwner );

	virtual void Reset( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden );
	virtual void AddUnit( class CAIUnit *pEnemy );
	virtual class CAIUnit* GetBestUnit() const;
	virtual class CBasicGun* GetBestGun() const;
	virtual const int GetNumberOfBestGun() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// �������� �� �������� ���������� ����� ��� ���������
class CPlaneDeffensiveFireShootEstimator : public IShootEstimator
{
	OBJECT_COMPLETE_METHODS( CPlaneDeffensiveFireShootEstimator );
	DECLARE_SERIALIZE;

	class CAIUnit *pOwner;
	
	CPtr<CAIUnit> pBestUnit;
	CPtr<CAIUnit> pCurTarget;
	CPtr<CBasicGun> pGun;
	bool bDamageToCurTargetUpdated;
	float fBestRating;
	
	const float CalcTimeToOpenFire( class CAIUnit *pEnemy, CBasicGun *pGun ) const; // ����� ��� �������� ���� (�������� ������� ������ � �������� ��������� � ������)

	const float CalcRating( CAIUnit *pEnemy, CBasicGun *pGun ) const;
public:
	CPlaneDeffensiveFireShootEstimator() : pOwner( 0 ) { }
	explicit CPlaneDeffensiveFireShootEstimator( class CAIUnit *pOwner );

	void SetGun( CBasicGun *_pGun);

	virtual void Reset( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden );//dwFirbidden is ignored
	virtual void AddUnit( class CAIUnit *pEnemy );
	virtual class CAIUnit* GetBestUnit() const;
	virtual class CBasicGun* GetBestGun() const;
	virtual const int GetNumberOfBestGun() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for 
class CBuilding;
class CPlaneShturmovikShootEstimator : public IShootEstimator
{
	OBJECT_COMPLETE_METHODS( CPlaneShturmovikShootEstimator );
	DECLARE_SERIALIZE;

	struct STargetInfo
	{
		DECLARE_SERIALIZE;
	public:
		CPtr<CAIUnit> pTarget;
		bool bCanTargetShootToPlanes;
		bool bCanAttackerBreakTarget;
		WORD wSpeedDiff;
		WORD wDirToTarget;
		float fRating;
		//
		void Reset()
		{
			bCanTargetShootToPlanes = false;
			bCanAttackerBreakTarget = false;
			fRating = 0;
			wSpeedDiff = 65535;
			pTarget = 0;
		}
		STargetInfo() { Reset(); }
	};

	class CAIUnit *pOwner;
	CPtr<CAIUnit> pCurEnemy;
	CVec2 vCenter;
	
	STargetInfo bestForGuns;
	STargetInfo bestForBombs;
	STargetInfo bestAviation;
	typedef std::unordered_set< int/*unique id of building*/ >  CBuildings;
	CBuildings buildings;
	CPtr<CBuilding> pBestBuilding;

	const float CPlaneShturmovikShootEstimator::CalcTimeToOpenFire( CAIUnit *pEnemy ) const;
	void CollectTarget( CPlaneShturmovikShootEstimator::STargetInfo * pInfo, class CAIUnit *pTarget, const DWORD dwPossibleGuns );
	const float CalcRating( CAIUnit *pEnemy, const DWORD dwPossibleGuns ) const;
public:
	CPlaneShturmovikShootEstimator() : pOwner( 0 ) {  }
	CPlaneShturmovikShootEstimator( class CAIUnit    *pOwner );
	void SetCurCenter( const CVec2 &vNewCenter ) { vCenter = vNewCenter; }

	virtual void Reset( class CAIUnit *pCurEnemy, const bool bDamageUpdated, const DWORD dwForbidden );//dwFirbidden is ignored
	virtual void AddUnit( class CAIUnit *pEnemy );
	virtual class CAIUnit* GetBestUnit() const;

	void CalcBestBuilding();
	class CBuilding * GetBestBuilding() const { return pBestBuilding; }

	virtual class CBasicGun* GetBestGun() const { NI_ASSERT_T(false,"Wrong call"); return 0;} 
	virtual const int GetNumberOfBestGun() const{ NI_ASSERT_T(false,"Wrong call"); return 0;} 
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� �������� �� ������������. 
class CShootEstimatorForObstacles : public IObstacleEnumerator
{
	class CCommonUnit *pOwner;
	float fCurRating;
	CPtr<IObstacle> pBest;
public:
	CShootEstimatorForObstacles( class CCommonUnit *pOwner ) : pOwner( pOwner ), fCurRating( 0 ) {  }

	virtual bool AddObstacle( IObstacle *pObstacle );
	virtual interface IObstacle * GetBest() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif __SHOOT_ESTIMATOR_INTERNAL_H__
