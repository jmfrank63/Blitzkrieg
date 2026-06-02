#ifndef __STANDART_SMOOTH_MECH_PATH__
#define __STANDART_SMOOTH_MECH_PATH__
#pragma ONCE
#include "Path.h"
class CStandartSmoothMechPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CStandartSmoothMechPath );
	DECLARE_SERIALIZE;
	
	CPtr<IPath> pPath;
	interface IBasePathUnit *pUnit;

	CBSpline spline;

	float speed;

	bool bFinished, bNotified, bMinSlowed, bMaxSlowed, bStopped, bSmoothTurn;

	CVec2 p0, p1, p2, p3;
	CVec2 predPoint;
	float nIter;
	float fRemain;
	int nPoints;
	CVec2 vLastValidatedPoint;

	bool bCanGoForward, bCanGoBackward;
	NTimer::STime lastCheckToRightTurn;

	void AddSmoothTurn();
	int InitSpline();
	bool ValidateCurPath( const CVec2 &center, const CVec2 &newPoint );

	const CVec2 GetPointWithoutFormation( NTimer::STime timeDiff );
	bool CheckTurn( const WORD wNewDir );
public:
	CStandartSmoothMechPath();
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn = true );
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit ) { return true; }
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

	virtual bool CanGoBackward() const;
	virtual bool CanGoForward() const { return bCanGoForward; }

	virtual void GetNextTiles( std::list<SVector> *pTiles );
	virtual CVec2 GetShift( const int nToShift ) const;
	
	virtual IMemento* GetMemento() const;
	
	virtual bool IsWithFormation() const { return false; }
};
#endif //__STANDART_SMOOTH_MECH_PATH__
