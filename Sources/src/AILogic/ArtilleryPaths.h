#ifndef __ARTILLERY_PATHS_H__
#define __ARTILLERY_PATHS_H__
#pragma ONCE
#include "Path.h"
class CArtilleryCrewPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CArtilleryCrewPath );
	DECLARE_SERIALIZE;

	interface IBasePathUnit *pUnit;

	CVec2 vCurPoint;
	CVec2 vEndPoint;
	float fSpeedLen;
	bool bSelfSpeed;
	bool bNotInitialized;
	CVec3 vSpeed3;
public:
	CArtilleryCrewPath()
		: pUnit( 0 ), vCurPoint( VNULL2 ), vEndPoint( VNULL2 ), fSpeedLen( 0.0f ), bSelfSpeed( false ), bNotInitialized( true ), vSpeed3( VNULL3 ) { }
	CArtilleryCrewPath( interface IBasePathUnit *pUnit, const CVec2 &vStartPoint, const CVec2 &vEndPoint, const float fMaxSpeed = 0.0f );

	void SetParams( const CVec2 &vEndPoint, const float fMaxSpeed );
	void SetParams( const CVec2 &_vEndPoint, const float fMaxSpeed, const CVec2 &_vSpeed2 );
	
	virtual bool IsFinished() const;
	virtual const CVec3 GetPoint( NTimer::STime timeDiff );

	virtual const CVec2& GetFinishPoint() const { return vEndPoint; }
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn = true, bool bCheckTurn = true );
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit  ) { return true; }
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit );
	virtual void Stop() {}
	virtual float& GetSpeedLen() { return fSpeedLen; }
	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist ) { }
	virtual void SlowDown() {}
	virtual bool CanGoBackward() const { return false; }
	virtual bool CanGoForward() const { return true; }
	virtual void GetNextTiles( std::list<SVector> *pTiles ) {}
	virtual CVec2 GetShift( const int nToShift ) const { return VNULL2; };
	virtual IMemento* GetMemento() const { return 0; }
	virtual float GetCurvatureRadius() const { return 0.0f; }
	virtual bool IsWithFormation() const { return true; }

	virtual void SetOwner( interface IBasePathUnit *_pUnit ) { pUnit = _pUnit; }
	virtual IBasePathUnit* GetOwner() const { return pUnit; }
};
class CArtilleryBeingTowedPath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CArtilleryBeingTowedPath );
	DECLARE_SERIALIZE;

	float fSpeedLen;
	CVec3 vCurPoint;
	CVec2 vCurPoint2D;
	CVec2 vSpeed;
public:
	CArtilleryBeingTowedPath() : fSpeedLen( 0.0f ), vCurPoint( VNULL3	), vCurPoint2D( VNULL2 ), vSpeed( VNULL2 ) { }
	CArtilleryBeingTowedPath( const float fSpeedLen, const CVec2 &vCurPoint, const CVec2 &vSpeed );
	bool Init( float fSpeedLen, const class CVec2 &vCurPoint, const CVec2 &vSpeed );

	virtual const CVec2& GetFinishPoint() const { return vCurPoint2D; }
	virtual bool IsFinished() const { return false; }
	virtual const CVec3 GetPoint( NTimer::STime timeDiff ) { return vCurPoint; }

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
	virtual void Stop() { }
	virtual float& GetSpeedLen() { return fSpeedLen; }
	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist ) { }
	virtual void SlowDown() { }
	virtual bool CanGoBackward() const { return true; }
	virtual bool CanGoForward() const { return true; }
	virtual void GetNextTiles( std::list<SVector> *pTiles ) { }
	virtual CVec2 GetShift( const int nToShift ) const { return VNULL2; };
	virtual IMemento* GetMemento() const { return 0; }
	virtual float GetCurvatureRadius() const { return 0.0f; }

	virtual bool IsWithFormation() const { return false; }
	
	virtual void GetSpeed3( CVec3 *pvSpeed ) const { *pvSpeed = CVec3( vSpeed, 0.0f ); }

	void SetOwner( interface IBasePathUnit *pUnit ) { }
	virtual IBasePathUnit* GetOwner() const { return 0; }
};
#endif // __ARTILLERY_PATHS_H__
