#ifndef __PRESIZE_PATH_H__
#define __PRESIZE_PATH_H__
#pragma ONCE
#include "Path.h"
interface IPath;
class CPresizePath : public ISmoothPath
{
	OBJECT_COMPLETE_METHODS( CPresizePath );
	DECLARE_SERIALIZE;

	enum EPresizePathState
	{
		EPPS_WAIT_FOR_INIT,
		EPPS_APPROACH_BY_STANDART,
		EPPS_TURN_TO_DESIRED_POINT,
		EPPS_APPROACH_DESIRED_POINT,
		EPPS_TURN_TO_DESIRED_DIR,
		EPPS_FINISHED,
	};

	EPresizePathState eState;
	CVec2 vEndPoint;
	CVec2 vEndDir;
	WORD wDesiredDir;

	CPtr<ISmoothPath> pPathStandart; 
	CPtr<ISmoothPath> pPathCheat;

	float fSpeedLen;
	interface IBasePathUnit *pUnit;
public:
	CPresizePath() : pUnit( 0 ), fSpeedLen( 0.0f ), wDesiredDir( 0 ), vEndDir( VNULL2 ), vEndPoint( VNULL2 ), eState( EPPS_WAIT_FOR_INIT ) { }
	CPresizePath( interface IBasePathUnit *pUnit, const class CVec2 &vEndPoint, const class CVec2 &vEndDir );
	virtual const CVec2& GetFinishPoint() const { return vEndPoint; }

	virtual bool IsFinished() const;
	virtual const CVec3 GetPoint( NTimer::STime timeDiff );
	
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn = true, bool bCheckTurn = true );
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit );
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit );
	virtual void Stop();
	virtual float& GetSpeedLen();
	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist );
	virtual void SlowDown();
	virtual bool CanGoBackward() const;
	virtual bool CanGoForward() const;
	virtual void GetNextTiles( std::list<SVector> *pTiles );
	virtual CVec2 GetShift( const int nToShift ) const;
	virtual IMemento* GetMemento() const;

	virtual float GetCurvatureRadius() const { return 0.0f; }

	virtual bool IsWithFormation() const { return false; }
	void SetOwner( interface IBasePathUnit *_pUnit );
	virtual IBasePathUnit* GetOwner() const { return pUnit; }
};
#endif // __PRESIZE_PATH_H__
