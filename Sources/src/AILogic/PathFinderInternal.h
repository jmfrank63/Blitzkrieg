#ifndef __PATH_FINDER_INTERNAL_H__
#define __PATH_FINDER_INTERNAL_H__

#pragma ONCE
#include "PathFinder.h"
class CStandartPathFinder : public IStaticPathFinder
{
	OBJECT_NORMAL_METHODS( CStandartPathFinder );
	DECLARE_SERIALIZE;

	interface IPointChecking *pChecking;

	int nBoundTileRadius;
	int upperLimit;
	BYTE aiClass;
	bool longPath;
	SVector startPoint, finishPoint;
	SVector lastKnownGoodTile;
	
	int nLength, nStart;

	int minDistance, minPointNum;
	bool bFinished;

	enum { COEFFICIENT = 7 };
	enum { MAX_STOP_POINTS = SAIConsts::MAX_MAP_SIZE * COEFFICIENT };
	enum 
	{ 
		STEP_LENGTH_THERE = 30, MAX_NUM_OF_ATTEMPTS_THERE = 4,
		STEP_LENGTH_BACK = 10, MAX_NUM_OF_ATTEMPTS_BACK = 12,
		STEP_LENGTH_THERE_SHORT = 5, MAX_NUM_OF_ATTEMPTS_THERE_SHORT = 4,
		STEP_LENGTH_BACK_SHORT = 2, MAX_NUM_OF_ATTEMPTS_BACK_SHORT = 12
	};
	enum { TOLERANCE = 64, TOLERANCE_SHORT = 16 };
	
	static SVector stopPoints[SAIConsts::INFINITY_PATH_LIMIT + 1];
	static SVector addPoints[SAIConsts::INFINITY_PATH_LIMIT + 1];
	static BYTE mapBuf[SAIConsts::MAX_MAP_SIZE][SAIConsts::MAX_MAP_SIZE];
	int nCyclePoints;
	static int cyclePoints[SAIConsts::INFINITY_PATH_LIMIT + 1];
	static int segmBegin[SAIConsts::INFINITY_PATH_LIMIT + 1];

	void LineSmoothing( const int STEP_LENGTH_THERE, const int MAX_NUM_OF_ATTEMPTS_THERE,
											const int STEP_LENGTH_BACK, const int MAX_NUM_OF_ATTEMPTS_BACK );
	
	const SVector CalculateHandPath( const SVector &blockPoint, const SVector &dir, const SVector &finish );
	const SVector CalculateSimplePath( const SVector &blockPoint, const SVector &dir, const SVector &finish );
	bool CanGoTowardPoint( const SVector &start, const SVector &finish );
	
	bool Walkable( const SVector &start, const SVector &finish );
  const int SavePathThere( const SVector &start, const SVector &finish, SVector * const buf, const int nLen );
  const int SavePathBack( const SVector& start, const SVector& finish, SVector * const buf, const int nLen );
	
	bool CheckFakePath( const SVector point );
	const SVector LookForFakePathBegin();
	
	void EraseCycles();
	void AnalyzePoint( const SVector &point, const int num );
	const int GetAdditionalPathLength( const SVector &pointFrom );
public:
	CStandartPathFinder() : nBoundTileRadius(0), upperLimit( 0 ), nLength( -1 ), pChecking( 0 ), aiClass( AI_CLASS_HUMAN )
	{ 
		memset(*mapBuf, 0, sizeof(mapBuf) ); 
		cyclePoints[0] = 0;
	}
	virtual void SetPathParameters( const int nBoundTileRadius, const BYTE aiClass, interface IPointChecking *pChecking, const CVec2 &startPoint, const CVec2 &finishPoint, const int upperLimit, const bool longPath, const SVector &lastKnownGoodTile );

	virtual bool CalculatePath();
	virtual void CalculatePathWOCycles();

	virtual void SmoothPath()
	{
		if ( longPath ) 
			LineSmoothing( STEP_LENGTH_THERE, MAX_NUM_OF_ATTEMPTS_THERE, STEP_LENGTH_BACK, MAX_NUM_OF_ATTEMPTS_BACK );
		else 
			LineSmoothing( STEP_LENGTH_THERE_SHORT, MAX_NUM_OF_ATTEMPTS_THERE_SHORT, STEP_LENGTH_BACK_SHORT, MAX_NUM_OF_ATTEMPTS_BACK_SHORT );
	}
	
	virtual const int GetPathLength()	const { return nLength; }
	virtual const SVector GetStopTile( int n ) const
		{ NI_ASSERT_T( n >= 0 && n < nLength, "Wrong number of stop point" ); return addPoints[n]; }
	virtual const void GetStopTiles( void *buf, int len ) const
		{ NI_ASSERT_T( nLength > 0 && len > 0 && len <= nLength, "Wrong number of stop points" ); memcpy( buf, addPoints, len * sizeof( SVector ) ); }
	
	virtual const SVector GetStartTile() const {  return startPoint; }
	virtual const SVector GetFinishTile() const { return finishPoint; }
	
	virtual IPath* CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint, const int nBoundTileRadius );
};
class CPlanePathFinder : public IStaticPathFinder
{
	OBJECT_NORMAL_METHODS( CPlanePathFinder );
	DECLARE_SERIALIZE;

	interface IPointChecking *pChecking;

	SVector startTile, finishTile;
public:
	virtual void SetPathParameters( const int nBoundTileRadius, const BYTE aiClass, interface IPointChecking *pChecking, const CVec2 &startPoint, const CVec2 &finishPoint, const int upperLimit, const bool longPath, const SVector &lastKnownGoodTile );

	virtual bool CalculatePath() { return true; }	
	virtual void CalculatePathWOCycles() { }
	virtual void SmoothPath() { }
	
	virtual const int GetPathLength()	const { return 0; }
	virtual const SVector GetStopTile( int n ) const { return startTile; }
	virtual const void GetStopTiles( void *buf, int len ) const { }
	
	virtual const SVector GetStartTile() const { return startTile; }
	virtual const SVector GetFinishTile() const { return finishTile; }
	
	virtual interface IPath* CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint, const int nBoundTileRadius );
};
#endif // __PATH_FINDER_INTERNAL_H__
