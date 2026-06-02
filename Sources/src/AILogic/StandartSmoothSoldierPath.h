#ifndef __STANDART_SMOOTH_SOLDIER_PATH_H__
#define __STANDART_SMOOTH_SOLDIER_PATH_H__
#pragma ONCE
#include "Path.h"
class CStandartSmoothSoldierPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CStandartSmoothSoldierPath	);
	DECLARE_SERIALIZE;
	
	CPtr<IPath> pPath;
	CPtr<CFormation> pFormation;
	interface IBasePathUnit *pUnit;

	CBSpline spline;

	float speed;

	bool bFinished, bNotified, bMinSlowed, bMaxSlowed, bStopped, bWithFormation;

	CVec2 p0, p1, p2, p3;
	CVec2 predPoint;
	float nIter;
	float fRemain;
	int nPoints;

	void AddSmoothTurn();
	int InitSpline();
	bool ValidateCurPath( const CVec2 &center, const CVec2 &newPoint );

	void CutDriveToFormationPath( class CCommonStaticPath *pPath );
	bool CanGoToFormationPos( const CVec2 &newCenter, const CVec2 &vDesPos, const CVec2 &vFormPos );
	bool DriveToFormation( const CVec2 &newCenter, const bool bAnyPoint );
	void ValidateCurPathWithFormation( const CVec2 &newCenter );

	const CVec2 GetPointWithFormation( NTimer::STime timeDiff, const bool bFirstCall );
	const CVec2 GetPointWithoutFormation( NTimer::STime timeDiff );
	bool CheckTurn( const WORD wNewDir );
public:
	CStandartSmoothSoldierPath();
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn = true );
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit );
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit );
	
	virtual void SetOwner( interface IBasePathUnit *pUnit );
	virtual IBasePathUnit* GetOwner() const;

	virtual const CVec2& GetFinishPoint() const
	{
		if ( pPath.IsValid() )
			return pPath->GetFinishPoint();
		return VNULL2;
	}

 	virtual bool IsFinished() const;
	
	virtual void Stop();

	virtual const CVec3 GetPoint( NTimer::STime timeDiff );
	virtual float& GetSpeedLen() { return speed; }

	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pUnit, const float fDist );
	virtual void SlowDown();

	virtual bool CanGoBackward() const { return false; }
	virtual bool CanGoForward() const { return true; }

	virtual void GetNextTiles( std::list<SVector> *pTiles );
	virtual CVec2 GetShift( const int nToShift ) const;
	
	virtual IMemento* GetMemento() const;
	
	virtual bool IsWithFormation() const { return bWithFormation; }
};
#endif // __STANDART_SMOOTH_SOLDIER_PATH_H__
