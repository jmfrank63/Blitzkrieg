#ifndef __STANDART_PATH_H__
#define __STANDART_PATH_H__
#pragma ONCE
#include "Path.h"
class CCommonStaticPath : public IStaticPath
{
	OBJECT_COMPLETE_METHODS( CCommonStaticPath );
	DECLARE_SERIALIZE;

	std::vector<SVector> path;
	int nLen;
	SVector startTile, finishTile;
	CVec2 finishPoint;

	void SetPath( const interface IStaticPathFinder &staticPathFinder, const int nLen, const CVec2 &finishPoint );
public:
	CCommonStaticPath()	: path(0), nLen(-1)	{ }
	CCommonStaticPath( const interface IStaticPathFinder &staticPathFinder, const CVec2 &finishPoint );
	CCommonStaticPath( const interface IStaticPathFinder &staticPathFinder, const int nLen, const CVec2 &finishPoint );
	
	void MoveStartTileTo( const int nStart );
	void MoveFinishTileTo( const int nFinish );
	void MoveFinishPointBy( const CVec2 &vMove );
	
	const int GetLength() const	{ return nLen; }

	virtual const SVector GetTile( const int n ) const	{	NI_ASSERT_T( n >= 0 && n < nLen, "Wrong point number" ); return path[n]; }
	
	virtual const SVector GetStartTile() const	{ return startTile; }
	virtual const SVector GetFinishTile() const	{ return finishTile; }
	virtual const CVec2& GetFinishPoint() const { return finishPoint; }
};
class CStandartPath : public IPath
{
	OBJECT_COMPLETE_METHODS( CStandartPath );
	DECLARE_SERIALIZE;

	CPtr<IStaticPathFinder> pPathFinder;
	CPtr<CCommonStaticPath> pStPath;

	int nBoundTileRadius;
	BYTE aiClass;

	CVec2 startPoint, finishPoint;
	SVector curStPathTile;
	CVec2 vShift;
	int nCurTile;
	
	int nCurStaticPoint, nCurPathPoint;
	bool bSmallPathTooLong;

	enum { ENPathPoints = 2 * SAIConsts::MAX_LENGTH_OF_SMALL_PATH+1 };
	SVector pathPoints[ENPathPoints + 2 * SAIConsts::MAX_LENGTH_OF_SMALL_PATH];
	SVector lastKnownGoodTile;
	
	std::vector<SVector> insertedTiles;
	int nInsertedTiles;
	int nCurInsertedTile;

	bool CalculateNewPath( const bool bShift );
	void CalculateSmallPath( const bool bLastStep );
	void SaveSmallPath( const int nToSave );
	void MoveDistantAim( int nMove );
	void InitByStaticPath( interface IStaticPath *pStPath, const CVec2 &startPoint, const CVec2 &finishPoint );

	static const BYTE GetNextPos( BYTE n ); 
public:
	CStandartPath() : pStPath( 0 ), pPathFinder( 0 ) { }
	CStandartPath( const int nBoundTileRadius, const BYTE aiClass, interface IStaticPathFinder *pPathFinder, interface IStaticPath *pStaticPath, const CVec2 &startPoint, const CVec2 &finishPoint, const SVector &lastKnownGoodTile );

	virtual const CVec2& GetStartPoint() const { return startPoint; }
	virtual const CVec2& GetFinishPoint() const { return finishPoint; }	
	virtual bool IsFinished() const { return nCurTile == nCurPathPoint; }

	virtual const CVec2 PeekPoint( int nShift );
	virtual void Shift( int nShift );

	virtual void RecoverState( const CVec2 &point, const SVector &lastKnownGoodTile );
	virtual void Recalculate( const CVec2 &point, const SVector &lastKnownGoodTile );

	virtual void InsertTiles( const std::list<SVector> &tiles );

	virtual bool CanGoBackward( interface IBasePathUnit *pUnit );
	virtual bool ShouldCheckTurn() const { return true; }
};
class CStandartDirPath : public IPath
{
	OBJECT_COMPLETE_METHODS( CStandartDirPath );
	DECLARE_SERIALIZE;

	CVec2 dir;
	CVec2 startPoint, finishPoint;
	CVec2 curPoint;
	bool bFinished;
public:
	CStandartDirPath() : bFinished( true ) { }
	CStandartDirPath( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint );

	virtual bool IsFinished() const;

	virtual const CVec2& GetStartPoint() const { return startPoint; }
	virtual const CVec2& GetFinishPoint() const;

	virtual const CVec2 PeekPoint( int nShift );
	virtual void Shift( int nShift );

	virtual void RecoverState( const CVec2 &point, const SVector &lastKnownGoodTile );
	virtual void Recalculate( const CVec2 &point, const SVector &lastKnownGoodTile );

	virtual bool CanGoBackward( interface IBasePathUnit *pUnit );
	virtual bool ShouldCheckTurn() const { return false; }
};
class CStandartSmoothPathMemento : public IMemento
{
	OBJECT_COMPLETE_METHODS( CStandartSmoothPathMemento );
	DECLARE_SERIALIZE;

	CPtr<IPath> pPath;
	CPtr<CFormation> pFormation;
public:
	friend class CStandartSmoothMechPath;	
	friend class CStandartSmoothSoldierPath;
};
#endif // __STANDART_PATH_H__
