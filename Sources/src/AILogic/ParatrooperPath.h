#ifndef __PARATROOPER_PATH_H__
#define __PARATROOPER_PATH_H__
#pragma ONCE
#include "Path.h"
class CParatrooperPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CParatrooperPath );
	DECLARE_SERIALIZE;

	NTimer::STime lastPathUpdateTime;

	CVec3 vStartPoint;
	CVec3 vFinishPoint;
	CVec2 vFinishPoint2D;

	CVec3 vCurPoint;
	float fSpeedLen;
	
	CVec3 vHorSpeed;//horisontal speed of parachute
	
	void FindFreeTile();
	void Init();

public:
	CParatrooperPath() { };
	CParatrooperPath( const CVec3 &startPoint );
	virtual bool IsFinished() const;
	virtual const CVec3 GetPoint( NTimer::STime timeDiff );

	virtual void GetSpeed3( CVec3 *vSpeed ) const;

	virtual const CVec2& GetFinishPoint() const { return vFinishPoint2D; }
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn = true, bool bCheckTurn = true ) { CPtr<IPath> p = pPath; return true; }
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit ) { return true; }
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit ) { CPtr<IMemento> p = pMemento; return true; }

	virtual void Stop() {}
	virtual float& GetSpeedLen() { return fSpeedLen; }
	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist ) {}
	virtual void SlowDown() {}
	virtual bool CanGoBackward() const { return false; }
	virtual bool CanGoForward() const { return true; }
	virtual void GetNextTiles( std::list<SVector> *pTiles ) {}
	virtual CVec2 GetShift( const int nToShift ) const { return CVec2( 0, 0 ); }
	virtual IMemento* GetMemento() const { return 0; }
	virtual float GetCurvatureRadius() const { return 0.0f; }

	virtual bool IsWithFormation() const { return true; }

	virtual void SetOwner( interface IBasePathUnit *pUnit ) { }
	virtual IBasePathUnit* GetOwner() const { return 0; }
	
	static float CalcFallTime( const float fZ );
};
#endif // __PARATROOPER_PATH_H__