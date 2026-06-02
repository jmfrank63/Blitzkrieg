#include "stdafx.h"

#include "PathFinderInternal.h"
#include "AIStaticMap.h"
#include "StandartPath.h"
#include "PlanePath.h"
#include "TimeCounter.h"
BASIC_REGISTER_CLASS( IStaticPathFinder );
extern CStaticMap theStaticMap;
extern CTimeCounter timeCounter;
BYTE CStandartPathFinder::mapBuf[SConsts::MAX_MAP_SIZE][SConsts::MAX_MAP_SIZE];
SVector CStandartPathFinder::addPoints[SConsts::INFINITY_PATH_LIMIT + 1];
SVector CStandartPathFinder::stopPoints[SConsts::INFINITY_PATH_LIMIT + 1];
int CStandartPathFinder::cyclePoints[SConsts::INFINITY_PATH_LIMIT + 1];
int CStandartPathFinder::segmBegin[SConsts::INFINITY_PATH_LIMIT + 1];
void CStandartPathFinder::SetPathParameters( const int _nBoundTileRadius, const BYTE _aiClass, interface IPointChecking *_pChecking, const CVec2 &_startPoint, const CVec2 &_finishPoint, const int _upperLimit, const bool _longPath, const SVector &_lastKnownGoodTile )
{
	bFinished = false;
	nBoundTileRadius = _nBoundTileRadius;
	aiClass = _aiClass;
	pChecking = _pChecking;
	startPoint = AICellsTiles::GetTile( _startPoint );
	finishPoint = AICellsTiles::GetTile( _finishPoint );
	upperLimit = _upperLimit;
	longPath = _longPath;
	lastKnownGoodTile = _lastKnownGoodTile;
	
	NI_ASSERT_T( theStaticMap.IsTileInside( lastKnownGoodTile ), NStr::Format( "Wrong lastKnownGoodTile (%d, %d)", lastKnownGoodTile.x, lastKnownGoodTile.y ) );
}
void CStandartPathFinder::AnalyzePoint( const SVector &point, const int num )
{
	const int mDist = mDistance( point, finishPoint );
	if ( mDist < minDistance )
	{
		minDistance = mDist;
		minPointNum = num;
		if ( !bFinished && pChecking && pChecking->IsGoodTile( point ) )
			bFinished = true;
	}
}
const SVector CStandartPathFinder::CalculateHandPath( const SVector &blockPoint, const SVector &dir, const SVector &finish )
{
	const SLine blockLine( blockPoint, finish );
	const SLine perpLine( blockLine.GetPerpendicular( blockPoint ) );
	const SLine perpLine1( blockLine.GetPerpendicular( finish) );
	const int startLen = nLength;

	SVector dirLeft = dir, dirRight = dir;
	SVector curRightPoint = blockPoint, curLeftPoint = blockPoint;

	int cnt = 0;
	do
	{
		dirRight.TurnLeftUntil45();
		++cnt;
	} while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass ) );

	if ( cnt >= 10 )
	{
		nLength = startLen;
		return SVector( -1, -1 );
	}

	cnt = 0;
	do
	{
		dirLeft.TurnRightUntil45();
		++cnt;
	} while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) );

	if ( cnt >= 10 )
	{
		nLength = startLen;
		return SVector( -1, -1 );
	}

	if ( theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass )  && 
			 theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) )
	{
		stopPoints[nLength] = curRightPoint;
		curRightPoint += dirRight;

		addPoints[nLength++] = curLeftPoint;
		curLeftPoint += dirLeft;
	}

	while ( 1 )
	{
		SVector dirTemp = dirRight; 
		dirTemp.TurnRightUntil45();
		if ( theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirTemp, aiClass ) )
			dirRight = dirTemp;
		else
		{
			int cnt = 0;
			while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass ) )
			{
				++cnt;
				dirRight.TurnLeftUntil45();
			}

			if ( cnt >= 10 )
			{
				nLength = startLen;
				return SVector( -1, -1 );
			}
		}

		stopPoints[nLength] = curRightPoint;
		SVector nextPoint = curRightPoint + dirRight;
		
		if ( blockLine.IsSegmIntersectLine( curRightPoint, nextPoint ) )
		{
			if ( perpLine.GetHPLineSign( nextPoint ) * perpLine.GetHPLineSign( finish ) > 0  &&
					 perpLine1.GetHPLineSign( nextPoint ) * perpLine1.GetHPLineSign( blockPoint ) > 0 )
			{
				++nLength;				
				for ( int i = startLen; i < nLength; ++i )
				{
					if ( mapBuf[stopPoints[i].x][stopPoints[i].y] == 1 )
						cyclePoints[nCyclePoints++] = i;
					else
						mapBuf[stopPoints[i].x][stopPoints[i].y] = 1;
					AnalyzePoint( stopPoints[i], i );
				}

				return nextPoint;
			}
		}

		curRightPoint = nextPoint;


		dirTemp = dirLeft; 
		dirTemp.TurnLeftUntil45();

		if ( theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirTemp, aiClass ) )
			dirLeft = dirTemp;
		else
		{
			while ( !theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) )
					dirLeft.TurnRightUntil45();
		}
			
		addPoints[nLength] = curLeftPoint;
		nextPoint = curLeftPoint + dirLeft;
		
		if ( blockLine.IsSegmIntersectLine( curLeftPoint, nextPoint ) )
		{
			if ( perpLine.GetHPLineSign( nextPoint ) * perpLine.GetHPLineSign( finish ) > 0 &&
					 perpLine1.GetHPLineSign( nextPoint ) * perpLine1.GetHPLineSign( blockPoint ) > 0 )
			{
				++nLength;
				for ( int i = startLen; i < nLength; i++ )
				{
					if ( mapBuf[addPoints[i].x][addPoints[i].y] == 1 )
						cyclePoints[nCyclePoints++] = i;
					else
						mapBuf[addPoints[i].x][addPoints[i].y] = 1;
					AnalyzePoint( addPoints[i], i );
				}
				memcpy( stopPoints + startLen, addPoints + startLen, sizeof( SVector ) * ( nLength - startLen ) );

				return nextPoint;
			}
		}

		curLeftPoint = nextPoint;

		if ( nLength >= upperLimit )
		{
			nLength = startLen;
			return SVector( -1, -1 );
		}

		++nLength;
	}
}
bool CStandartPathFinder::CanGoTowardPoint( const SVector &start, const SVector &finish )
{
	CBres bres;
	bres.Init( start, finish );
	bres.MakeStep();

	return theStaticMap.CanUnitGo( nBoundTileRadius, start, bres.GetDirection(), aiClass );
}
const SVector CStandartPathFinder::CalculateSimplePath( const SVector &blockPoint, const SVector &dir, const SVector &finish )
{
	const int startLen = nLength; 

	SVector dirLeft = dir, dirRight = dir;
	SVector curRightPoint = blockPoint, curLeftPoint = blockPoint;

	int cnt = 0;
	do
	{
		dirRight.TurnLeftUntil45();
		++cnt;
	} while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass ) );
	if ( cnt >= 10 )
	{
		nLength = startLen;
		return SVector( -1, -1 );
	}

	cnt = 0;
	do
	{
		++cnt;
		dirLeft.TurnRightUntil45();
	} while ( cnt <10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) );
	if ( cnt >= 10 )
	{
		nLength = startLen;
		return SVector( -1, -1 );
	}
	
	if ( theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass ) &&
		   theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) )
	{
		stopPoints[nLength] = curRightPoint;
		curRightPoint += dirRight;
		
		addPoints[nLength++] = curLeftPoint;
		curLeftPoint += dirLeft;
	}
	
	while ( 1 )
	{

		SVector dirTemp = dirRight; 
		dirTemp.TurnRightUntil45();

		if ( theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirTemp, aiClass ) )
		{
			const SVector dir1 = finish-curRightPoint;
			if ( ( dirTemp * dir1 ) >= 0  &&  Sign( dirRight * dir1 ) >= 0  &&  
					 CanGoTowardPoint( curRightPoint, finish ) )
			{
				for ( int i = startLen; i < nLength; ++i )
				{
					if ( mapBuf[stopPoints[i].x][stopPoints[i].y] == 1 )
						cyclePoints[nCyclePoints++] = i;
					else
						mapBuf[stopPoints[i].x][stopPoints[i].y] = 1;
					AnalyzePoint( stopPoints[i], i );
				}

				return curRightPoint;
			}
			else
				dirRight = dirTemp;
		}
		else
		{	
			cnt = 0;
			while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curRightPoint, dirRight, aiClass ) )
			{
				++cnt;
				dirRight.TurnLeftUntil45(); 
			}
			if ( cnt >= 10 )
			{
				nLength = startLen;
				return SVector( -1, -1 );
			}

		}

		stopPoints[nLength] = curRightPoint;
		curRightPoint += dirRight;

		dirTemp = dirLeft; 
		dirTemp.TurnLeftUntil45();

		if ( theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirTemp, aiClass ) )
		{
			const SVector dir1 = finish-curLeftPoint;			
			if ( Sign( dirTemp * dir1 ) >= 0  &&  Sign( dirLeft * dir1 ) >= 0  &&  
					 CanGoTowardPoint( curLeftPoint, finish ) )
			{
				for ( int i = startLen; i < nLength; i++ )
				{
					if ( mapBuf[addPoints[i].x][addPoints[i].y] == 1 )
						cyclePoints[nCyclePoints++] = i;
					else
						mapBuf[addPoints[i].x][addPoints[i].y] = 1;
					AnalyzePoint( addPoints[i], i );
				}
				memcpy( stopPoints+startLen, addPoints+startLen, sizeof( SVector ) * ( nLength - startLen ) );

				return curLeftPoint;
			}
			else				
				dirLeft = dirTemp;
		}
		else
		{
			cnt = 0;
			while ( cnt < 10 && !theStaticMap.CanUnitGo( nBoundTileRadius, curLeftPoint, dirLeft, aiClass ) )
			{
				++cnt;
				dirLeft.TurnRightUntil45();
			}
			if ( cnt >= 10 )
			{
				nLength = startLen;
				return SVector( -1, -1 );
			}

		}
			
		addPoints[nLength] = curLeftPoint;
		curLeftPoint += dirLeft;

		if ( nLength > upperLimit )
		{
			nLength = startLen;
			return SVector( -1, -1 );
		}

		++nLength;
	}
}
bool CStandartPathFinder::CheckFakePath( const SVector point )
{
	if ( theStaticMap.CanUnitGo( nBoundTileRadius, point, aiClass ) )
	{
		CBres bres;
		bres.InitPoint( startPoint, point );

		do
		{
			stopPoints[nLength++] = bres.GetDirection();
			bres.MakePointStep();
		} while ( bres.GetDirection() != point );

		return true;
	}

	return false;
}
const float GetDist2ToTile( const CVec2 &vPoint, const SVector tile )
{
	const float fX1 = tile.x * SConsts::TILE_SIZE;
	const float fX2 = ( tile.x + 1 ) * SConsts::TILE_SIZE;
	const float fY1 = tile.y * SConsts::TILE_SIZE;
	const float fY2 = ( tile.y + 1 ) * SConsts::TILE_SIZE;

	const float fX = 
		( Sign( vPoint.x - fX1 ) != Sign( vPoint.x - fX2 ) ) ? 
		0.0f : Min( fabs( vPoint.x - fX1 ), fabs( vPoint.x - fX2 ) );

	const float fY = 
		( Sign( vPoint.y - fY1 ) != Sign( vPoint.y - fY2 ) ) ?
		0.0f : Min( fabs( vPoint.y - fY1 ), fabs( vPoint.y - fY2 ) );

	return sqr( fX ) + sqr( fY );
}
const int CStandartPathFinder::GetAdditionalPathLength( const SVector &pointFrom )
{
	if ( pointFrom == lastKnownGoodTile )
		return 0;
	else
	{
		const SVector oldStart = startPoint;
		const SVector oldFinish = finishPoint;
		IPointChecking *pOldPointChecking = pChecking;

		startPoint = pointFrom;
		finishPoint = lastKnownGoodTile;
		pChecking = 0;

		int nPathLength = -1;
		if ( CalculatePath() )
		{
			if ( GetPathLength() == 0 || GetStopTile( GetPathLength() - 1 ) != lastKnownGoodTile )
				nPathLength = -1;
			else
				nPathLength = GetPathLength();
		}

		startPoint = oldStart;
		finishPoint = oldFinish;
		pChecking = pOldPointChecking;

		return nPathLength;
	}
}
const SVector CStandartPathFinder::LookForFakePathBegin()
{
	const int nMaxFails = 5;
	int nFails = 0;
	
	int nBestPathLength = -1;
	SVector bestPoint( 0, 0 );
	SVector firstPointToGo( -1, -1 );
	for ( int i = 0; i < 6 && nBestPathLength == -1 && nFails < nMaxFails; ++i )
	{
		for ( int j = startPoint.y - i; j <= startPoint.y + i && nFails < nMaxFails; ++j )
		{
			const SVector point( startPoint.x - i, j );
			if ( theStaticMap.CanUnitGo( nBoundTileRadius, point, aiClass ) )
			{
				if ( firstPointToGo.x == -1 || mDistance( firstPointToGo, startPoint ) > mDistance( point, startPoint ) )
					firstPointToGo = point;

				const int nLocalPathLength = GetAdditionalPathLength( point );
				if ( nLocalPathLength != -1 && ( nBestPathLength == -1 || nLocalPathLength < nBestPathLength ) )
				{
					nBestPathLength = nLocalPathLength;
					bestPoint = point;
				}
				else if ( nLocalPathLength == -1 )
					++nFails;
			}
			
			const SVector point1( startPoint.x + i, j );
			if ( theStaticMap.CanUnitGo( nBoundTileRadius, point1, aiClass ) )
			{
				if ( firstPointToGo.x == -1 || mDistance( firstPointToGo, startPoint ) > mDistance( point, startPoint ) )
					firstPointToGo = point;
				
				const int nLocalPathLength = GetAdditionalPathLength( point1 );
				if ( nLocalPathLength != -1 && ( nBestPathLength == -1 || nLocalPathLength < nBestPathLength ) )
				{
					nBestPathLength = nLocalPathLength;
					bestPoint = point1;
				}
				else if ( nLocalPathLength == -1 )
					++nFails;
			}
		}

		for ( int j = startPoint.x - i; j < startPoint.x + i && nFails < nMaxFails; ++j )
		{
			const SVector point( j, startPoint.y - i );
			if ( theStaticMap.CanUnitGo( nBoundTileRadius, point, aiClass ) )
			{
				if ( firstPointToGo.x == -1 || mDistance( firstPointToGo, startPoint ) > mDistance( point, startPoint ) )
					firstPointToGo = point;
				
				const int nLocalPathLength = GetAdditionalPathLength( point );
				if ( nLocalPathLength != -1 && ( nBestPathLength == -1 || nLocalPathLength < nBestPathLength ) )
				{
					nBestPathLength = nLocalPathLength;
					bestPoint = point;
				}
				else if ( nLocalPathLength == -1 )
					++nFails;
			}

			const SVector point1( j, startPoint.y + i );
			if ( theStaticMap.CanUnitGo( nBoundTileRadius, point1, aiClass ) )
			{
				if ( firstPointToGo.x == -1 || mDistance( firstPointToGo, startPoint ) > mDistance( point, startPoint ) )
					firstPointToGo = point;
				
				const int nLocalPathLength = GetAdditionalPathLength( point1 );
				if ( nLocalPathLength != -1 && ( nBestPathLength == -1 || nLocalPathLength < nBestPathLength ) )
				{
					nBestPathLength = nLocalPathLength;
					bestPoint = point1;
				}
				else if ( nLocalPathLength == -1 )
					++nFails;
			}
		}
	}

	nLength = 0;
	if ( nBestPathLength == -1 )
	{
		if ( firstPointToGo.x != -1 )
			return firstPointToGo;
		else
			return startPoint;
	}
	else
	{
		CheckFakePath( bestPoint );
		return bestPoint;
	}
}
bool CStandartPathFinder::CalculatePath( )
{
	nLength = 0;
	nCyclePoints = 1;
	minDistance = mDistance( startPoint, finishPoint );
	minPointNum = 0;

	NI_ASSERT_T( finishPoint.x >= 0 && finishPoint.y >= 0, "Wrong finish point" );
	
	if ( startPoint == finishPoint || finishPoint.x < 0 || finishPoint.y < 0 )
		return false;
	
	SVector startSearchPoint;
	if ( !theStaticMap.CanUnitGo( nBoundTileRadius, startPoint, aiClass ) )
		startSearchPoint = LookForFakePathBegin();
	else
		startSearchPoint = startPoint;

	bool bCanGo = false;
	for ( int i = -1; i <= 1 && !bCanGo; ++i )
	{
		for ( int j = -1; j <= 1  && !bCanGo; ++j )
		{
			if ( i != 0 || j != 0 )
				bCanGo = theStaticMap.CanUnitGo( nBoundTileRadius, startSearchPoint, SVector( i, j ), aiClass );
		}
	}

	if ( !bCanGo )
		return false;

	SVector curPoint(startSearchPoint);

	CBres bres;
	bres.Init( startSearchPoint, finishPoint );

	while ( curPoint != finishPoint && upperLimit >= 0 )
	{
		bres.MakeStep();

		if ( !theStaticMap.CanUnitGo( nBoundTileRadius, curPoint, bres.GetDirection(), aiClass ) )
		{
			if ( curPoint + bres.GetDirection() == finishPoint )
			{
				finishPoint = curPoint;
				return true;
			}

			if ( mapBuf[curPoint.x][curPoint.y] == 0 )
			{
				SVector point( CalculateSimplePath( curPoint, bres.GetDirection(), finishPoint ) );
				if ( point.x == -1 )
				{
					point = CalculateHandPath( curPoint, bres.GetDirection(), finishPoint );
					if ( point.x == -1  ||  mDistance( point, curPoint ) > 1 )
						curPoint = point;
					else
					{
						finishPoint = curPoint;
						return true;
					} 
				}
				else
					curPoint = point;
			}
			else 
			{
				cyclePoints[nCyclePoints++] = nLength;				
				SVector point( CalculateHandPath( curPoint, bres.GetDirection(), finishPoint ) );
				if ( point.x == -1 || mDistance( point, curPoint ) > 1 )
					curPoint = point;
				else
				{
					finishPoint = curPoint;
					return true;
				}
			}

			if ( curPoint.x != -1 )
				bres.Init( curPoint, finishPoint );
		}
		else
		{
			mapBuf[curPoint.x][curPoint.y] = 1;
			AnalyzePoint( curPoint, nLength );
			stopPoints[nLength++] = curPoint;
			curPoint += bres.GetDirection();
		}

		if ( bFinished )
			return true;
		if ( curPoint.x == -1 ||  nLength >= upperLimit )
			return false;
	}

	return true;
}
void CStandartPathFinder::EraseCycles()
{
	if ( minDistance > 1 || !theStaticMap.CanUnitGo( nBoundTileRadius, finishPoint, aiClass ) )
	{
		while ( nLength > minPointNum )
		{
			--nLength;
			mapBuf[stopPoints[nLength].x][stopPoints[nLength].y] = 0;
		}
		
		finishPoint = stopPoints[nLength];
	}
	
	nStart = 0;

	int i = nLength - 1;
	int cycleNum = nCyclePoints - 1;

	while ( cycleNum > 0 && cyclePoints[cycleNum] > i - nStart )
		--cycleNum;

	while ( i - nStart >= 0  && cycleNum > 0 )
	{
		while ( i - nStart >= cyclePoints[cycleNum] )
		{
			stopPoints[i] = stopPoints[i - nStart];
			mapBuf[stopPoints[i - nStart].x][stopPoints[i - nStart].y] = 0;
			--i;
		}

		while ( i - nStart >= 0 && stopPoints[i + 1] != stopPoints[i - nStart] )
		{
			mapBuf[stopPoints[i - nStart].x][stopPoints[i - nStart].y] = 0;
			++nStart;
		}
		++nStart;

		while ( cycleNum > 0 && cyclePoints[cycleNum] > i - nStart )
			--cycleNum;
	}
	nLength -= nStart;

	while ( i - nStart  >= 0 )
	{
		stopPoints[i] = stopPoints[i - nStart];
		mapBuf[stopPoints[i - nStart].x][stopPoints[i - nStart].y] = 0;
		--i;
	}
/*
#ifdef _DEBUG
	for ( i = 0; i < theStaticMap.GetSizeY(); ++i )
		for ( int j = 0; j < theStaticMap.GetSizeX(); ++j )
				NI_ASSERT_T( mapBuf[i][j] == 0, "The mapBuf hasn't been cleared\n" );
#endif
*/
}
void CStandartPathFinder::CalculatePathWOCycles( )
{
	bFinished = false;
	CalculatePath();
 	EraseCycles();
}
bool CStandartPathFinder::Walkable( const SVector &start, const SVector &finish )
{
	CBres bres;
	bres.InitPoint( start, finish );

	while ( bres.GetDirection() != finish )
	{
		bres.MakePointStep();
		if (!theStaticMap.CanUnitGo( nBoundTileRadius, bres.GetDirection(), aiClass ) )
			return false;
	}

	return true;
}
const int CStandartPathFinder::SavePathThere( const SVector &start, const SVector &finish, SVector * const buf, const int nLen )
{
	CBres bres;
	bres.InitPoint( start, finish );

	int res = 0;
	do
	{
		buf[nLen+res++] = bres.GetDirection();		
		bres.MakePointStep();
	} while ( bres.GetDirection() != finish );

	return res;
}
const int CStandartPathFinder::SavePathBack( const SVector &start, const SVector &finish, SVector * const buf, const int nLen )
{
	CBres bres;
	bres.InitPoint( start, finish );

	int res = 0;
	do
	{
		bres.MakePointStep();		
		buf[nLen+res++] = bres.GetDirection();		
	} while ( bres.GetDirection() != finish );
	
	return res;
}
void CStandartPathFinder::LineSmoothing( const int STEP_LENGTH_THERE, const int MAX_NUM_OF_ATTEMPTS_THERE,
																			 const int STEP_LENGTH_BACK, const int MAX_NUM_OF_ATTEMPTS_BACK )
{
	if ( nLength < 0 ) 
		return;

	stopPoints[nStart+nLength++] = finishPoint;

	int curNum = 1, i = 1; 
	int checkNum = 0, numOfAttempts = 0, addLen = 0;

	while ( i < nLength-1 )
	{
		const int j = Min( i+STEP_LENGTH_THERE, nLength )-1;

		if ( numOfAttempts > MAX_NUM_OF_ATTEMPTS_THERE  ||  
				 !Walkable( stopPoints[checkNum + nStart], stopPoints[j + nStart] ) )
		{
			addLen += SavePathThere( stopPoints[checkNum + nStart], stopPoints[i + nStart], addPoints, addLen );

			checkNum = i;
			curNum = ++i;
			numOfAttempts = 0;
		}
		else
		{
			i = j;
			++numOfAttempts;
		}
	}

	addLen += SavePathThere( stopPoints[checkNum + nStart], stopPoints[nStart + nLength - 1], addPoints, addLen );
	addPoints[addLen] = finishPoint;
	nLength = addLen+1;

	i = nLength-2; 
	checkNum = nLength-1; 
	curNum = nLength-2; 
	numOfAttempts = addLen = 0;
	int nSegm = 0;
	while ( i > 0 )
	{
		const int j = Max( i-STEP_LENGTH_BACK, 0 );
		
		if ( numOfAttempts > MAX_NUM_OF_ATTEMPTS_BACK || !Walkable( addPoints[j], addPoints[checkNum] ) )
		{
			segmBegin[nSegm++] = addLen;			
			addLen += SavePathBack( addPoints[i], addPoints[checkNum], stopPoints, addLen );

			checkNum = i;
			curNum = --i;
			numOfAttempts = 0;
		}
		else
		{
			i = j;
			++numOfAttempts;
		}
	}

	segmBegin[nSegm++] = addLen;
	addLen += SavePathBack( addPoints[0], addPoints[checkNum], stopPoints, addLen );
	segmBegin[nSegm] = addLen;

	if ( nSegm == 1 )
	{
		addPoints[0] = startPoint;
		memcpy( addPoints + 1, stopPoints, nLength * sizeof( SVector ) );
		addPoints[addLen] = finishPoint;
		nLength = addLen + 1;
	}
	else
	{

		addPoints[0] = startPoint;	
		nLength = addLen = 1;
		i = nSegm-1;

		while ( i >= 0 )
		{



			
			
			if ( longPath )
			{
				int j = Min( i - 1, (int)TOLERANCE )+1;
				while ( j > 0  &&  !Walkable( stopPoints[segmBegin[i]], stopPoints[segmBegin[i-j+1]-1] ) )
					j >>= 1;

				if ( !j )
				{
					memcpy( addPoints+addLen, stopPoints+segmBegin[i], sizeof(SVector)*(segmBegin[i-j+1]-segmBegin[i]) );
					addLen += segmBegin[i-j+1]-segmBegin[i];
				}
				else
				{
					addLen += SavePathThere( stopPoints[segmBegin[i]], stopPoints[segmBegin[i-j+1]-1], addPoints, addLen );
					addPoints[addLen++] = stopPoints[segmBegin[i-j+1]-1];
				}

				i -= j+1;
			}
			else
			{
				int j = Max(1, i-TOLERANCE);
				while ( j <= i && !Walkable( stopPoints[segmBegin[i]], stopPoints[segmBegin[j]-1] ) )
					++j;

				addLen += SavePathThere( stopPoints[segmBegin[i]], stopPoints[segmBegin[j]-1], addPoints, addLen );
				addPoints[addLen++] = stopPoints[segmBegin[j]-1];
				i = j-2;
			}
		}

		nLength = addLen;



	}
}
IPath* CStandartPathFinder::CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint, const int nBoundTileRadius )
{ 
	return new CStandartDirPath( startPoint, dir, finishPoint );
}
void CPlanePathFinder::SetPathParameters( const int nBoundTileRadius, const BYTE aiClass, interface IPointChecking *_pChecking, const CVec2 &startPoint, const CVec2 &finishPoint, const int upperLimit, const bool longPath, const SVector &lastKnownGoodTile )
{
	startTile = AICellsTiles::GetTile( startPoint );
	finishTile = AICellsTiles::GetTile( finishPoint );
}
IPath* CPlanePathFinder::CreatePathByDirection( const CVec2 &startPoint, const CVec2 &dir, const CVec2 &finishPoint, const int nBoundTileRadius )
{
	CVec3 st( startPoint.x, startPoint.y, -1 );
	CVec3 en( finishPoint.x, finishPoint.y, -1 );
	return new CPlanePath( st, en );
}
