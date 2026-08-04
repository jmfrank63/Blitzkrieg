#ifndef __TRAIN_PATH_FINDER_H__
#define __TRAIN_PATH_FINDER_H__
#pragma ONCE
#include "PathFinder.h"
#include "RailroadGraph.h"
class CEdgePoint;
class CTrainPathUnit;
class CTrainPathFinder : public IStaticPathFinder
{
	OBJECT_COMPLETE_METHODS( CTrainPathFinder );
	DECLARE_SERIALIZE;

	CPtr<CTrainPathUnit> pTrain;
	CPtr<CEdgePoint> pStartEdgePoint;
	CPtr<CEdgePoint> pFinishEdgePoint;
	CVec2 finishPoint;

	std::list<int> bestPath;
	float fBestPathLen;
	std::list<int>::iterator iter;
	void AnalyzePath( const int v1, const int v2, const float fDistToV1, CEdgePoint *pPoint );
public:
	virtual void SetPathParameters( class CTrainPathUnit *pTrain, const CVec2 &finishPoint );

	virtual void SetPathParameters( const int nBoundTileRadius, const BYTE aiClass, interface IPointChecking *pChecking, const CVec2 &startPoint, const CVec2 &finishPoint, const int upperLimit, const bool longPath, const SVector &lastKnownGoodTile ) { }

	virtual bool CalculatePath();
	virtual void CalculatePathWOCycles() { }
	virtual void SmoothPath() { }
	
	virtual const int GetPathLength()	const;
	virtual const SVector GetStopTile( int n ) const { NI_ASSERT_T( false, "Not defined for train path" ); return SVector( 0, 0 ); }
	virtual const void GetStopTiles( void *buf, int len ) const { NI_ASSERT_T( false, "Not defined for train path" ); }
	
	virtual const SVector GetStartTile() const;
	virtual const SVector GetFinishTile() const;

	CEdgePoint* GetStartEdgePoint() const;
	CEdgePoint* GetFinishEdgePoint() const;

	void StartPathIterating();
	const int GetCurPathNode() const;
	void Iterate();

	virtual interface IPath* CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint, const int nBoundTileRadius ) { return 0; }
};
#endif //__TRAIN_PATH_FINDER_H__
