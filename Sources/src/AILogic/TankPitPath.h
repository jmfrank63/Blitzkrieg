#ifndef __TANK_PIT_PATH_H__
#define __TANK_PIT_PATH_H__
#pragma ONCE
#include "Path.h"
class CTankPitPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CTankPitPath );
	DECLARE_SERIALIZE;

	CVec2 vCurPoint;
	CVec2 vEndPoint;
	float fSpeedLen;

	interface IBasePathUnit *pUnit;
public:
	CTankPitPath() : pUnit( 0 ) { }
	CTankPitPath( IBasePathUnit *pUnit, const class CVec2 &vStartPoint, const class CVec2 &vEndPoint );
	virtual bool IsFinished() const;
	virtual const CVec3 GetPoint( NTimer::STime timeDiff );
	
	virtual const CVec2& GetFinishPoint() const { return vEndPoint; }
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn = true, bool bCheckTurn = true )
	{
		CPtr<IPath> p = pPath;
		return true;
	}
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit  ) { return true; }
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit )
	{
		CPtr<IMemento> p = pMemento;
		return true;
	}
	virtual void Stop() {}
	virtual float& GetSpeedLen() { return fSpeedLen; }
	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist ) {}
	virtual void SlowDown() {}
	virtual bool CanGoBackward() const { return false; }
	virtual bool CanGoForward() const { return true; }
	virtual void GetNextTiles( std::list<SVector> *pTiles ) {}
	virtual CVec2 GetShift( const int nToShift ) const { return CVec2( 0, 0 ); };
	virtual IMemento* GetMemento() const { return 0; }
	virtual float GetCurvatureRadius() const { return 0.0f; }

	virtual bool IsWithFormation() const { return false; }

	void SetOwner( interface IBasePathUnit *_pUnit ) { pUnit = _pUnit; }
	virtual IBasePathUnit* GetOwner() const { return pUnit; }	
};
#endif // __TANK_PIT_PATH_H__
