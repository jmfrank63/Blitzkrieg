#ifndef __PATH_H__
#define __PATH_H__
#pragma ONCE
interface IMemento : public IRefCount
{
};
interface IStaticPath : public IRefCount
{
	virtual const SVector GetStartTile() const	= 0;
	virtual const SVector GetFinishTile() const= 0;
	virtual const CVec2& GetFinishPoint() const = 0;

	virtual const int GetLength() const	= 0;
	virtual void MoveFinishPointBy( const CVec2 &vMove ) = 0;
};
interface IPath : public IRefCount
{
	virtual bool IsFinished() const = 0;

	virtual const CVec2 PeekPoint( int nShift ) = 0;
	virtual void Shift( int nShift ) = 0;

	virtual const CVec2& GetFinishPoint() const = 0;
	virtual const CVec2& GetStartPoint() const = 0;

	virtual void RecoverState( const CVec2 &point, const SVector &lastKnownGoodTile ) = 0;
	virtual void Recalculate( const CVec2 &point, const SVector &lastKnownGoodTile ) = 0;

	virtual void InsertTiles( const std::list<SVector> &tiles ) { }
	
	virtual bool CanGoBackward( interface IBasePathUnit *pUnit ) = 0;
	
	virtual bool ShouldCheckTurn() const = 0;
};
interface ISmoothPath : public IRefCount
{
	virtual const CVec2& GetFinishPoint() const = 0;
	virtual bool Init( interface IBasePathUnit *pPathUnit, interface IAviationUnit *pAviationUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn = true ) { NI_ASSERT_T(false, "wrong call"); return false; }
	virtual bool Init( interface IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn = true ) = 0;
	virtual bool Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit ) = 0;
	virtual bool InitByFormationPath( class CFormation *pFormation, interface IBasePathUnit *pUnit ) = 0;

	virtual bool IsFinished() const = 0;
	
	virtual void Stop() = 0;

	virtual const CVec3 GetPoint( NTimer::STime timeDiff ) = 0;
	virtual float& GetSpeedLen() = 0;

	virtual void NotifyAboutClosestThreat( interface IBasePathUnit *pCollUnit, const float fDist ) = 0;
	virtual void SlowDown() = 0;

	virtual bool CanGoBackward() const = 0;
	virtual bool CanGoForward() const = 0;
	virtual void GetNextTiles( std::list<SVector> *pTiles ) = 0;
	virtual CVec2 GetShift( const int nToShift ) const = 0;
	
	virtual IMemento* GetMemento() const = 0;
	virtual float GetCurvatureRadius() const { return 0.0f; }
	virtual CVec2 GetCurvatureCenter() const { return CVec2( float(1e15), float(1e15) ); }
	
	virtual bool IsWithFormation() const = 0;

	virtual void GetSpeed3( CVec3 *vSpeed ) const
	{
	*vSpeed = VNULL3;
	}

	virtual void SetOwner( interface IBasePathUnit *pUnit ) = 0;
	virtual IBasePathUnit* GetOwner() const = 0;
};
#endif // __PATH_H__
