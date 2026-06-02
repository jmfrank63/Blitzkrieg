#include "stdafx.h"

#include "StandartPath.h"
#include "PathFinder.h"
#include "AIStaticMap.h"
#include "BasePathUnit.h"
#include "Formation.h"
#include "StandartPath.h"

#include "TimeCounter.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( IPath );
BASIC_REGISTER_CLASS( ISmoothPath );
BASIC_REGISTER_CLASS( IMemento );
BASIC_REGISTER_CLASS( IStaticPath );
BASIC_REGISTER_CLASS( CCommonStaticPath );
CCommonStaticPath::CCommonStaticPath( const interface IStaticPathFinder &staticPathFinder, const CVec2 &finishPoint )
{
	SetPath( staticPathFinder, staticPathFinder.GetPathLength(), finishPoint );
}
CCommonStaticPath::CCommonStaticPath( const interface IStaticPathFinder &staticPathFinder, const int nLen, const CVec2 &finishPoint )
{
	SetPath( staticPathFinder, nLen, finishPoint );
}
void CCommonStaticPath::SetPath( const interface IStaticPathFinder &staticPathFinder, const int _nLen, const CVec2 &_finishPoint )
{
	nLen = _nLen;

	NI_ASSERT_SLOW_T( nLen >= 0 && nLen <= staticPathFinder.GetPathLength(), "Wrong length" );
	
	if ( path.size() < nLen )
		path.resize( nLen * 1.5 );

	if ( nLen > 0 )
		staticPathFinder.GetStopTiles( &(path[0]), nLen );
	startTile = staticPathFinder.GetStartTile();
	finishTile = staticPathFinder.GetFinishTile();

	if ( !theStaticMap.IsPointInside( _finishPoint ) || finishTile != AICellsTiles::GetTile( _finishPoint ) )
		finishPoint = AICellsTiles::GetPointByTile( finishTile );
	else 
		finishPoint = _finishPoint;
}
void CCommonStaticPath::MoveStartTileTo( const int nStart )
{
	NI_ASSERT_T( nStart < nLen, "Wrong point to move start to" );

	nLen -= nStart;
	startTile = path[nStart];
	path.erase( path.begin(), path.begin() + nStart );
}
void CCommonStaticPath::MoveFinishTileTo( const int nFinish )
{
	NI_ASSERT_T( nFinish <= nLen && nFinish > 0, "Wrong point to move finish to" );

	nLen = nFinish;
	finishTile = path[nFinish-1];
	finishPoint = AICellsTiles::GetPointByTile( finishTile );
}
void CCommonStaticPath::MoveFinishPointBy( const CVec2 &vMove ) 
{ 
	if ( AICellsTiles::GetTile( finishPoint + vMove ) == finishTile )
		finishPoint += vMove;
}
void CStandartPath::SaveSmallPath( const int nToSave )
{
	if ( nToSave == -1 )
		return;

	if ( nCurPathPoint > 0 )
		--nCurPathPoint;

	pPathFinder->GetStopTiles( pathPoints+nCurPathPoint, nToSave );
	nCurPathPoint += nToSave;
	if ( nCurPathPoint >= ENPathPoints )
	{
		nCurPathPoint -= ENPathPoints;
		memcpy( pathPoints, pathPoints+ENPathPoints, nCurPathPoint * sizeof( SVector ) );
	}
}
void CStandartPath::CalculateSmallPath( const bool bLastStep )
{
	SVector nextTile;
	if ( curStPathTile == pStPath->GetFinishTile() )
		nextTile = AICellsTiles::GetTile( finishPoint );
	else
		nextTile = AICellsTiles::GetTile( AICellsTiles::GetPointByTile( curStPathTile ) + vShift );

	bool bFound = true;
	if ( theStaticMap.CanUnitGo( nBoundTileRadius, nextTile, aiClass ) )
	{
		theStaticMap.MemMode();
		theStaticMap.SetMode( ELM_STATIC );
		const SVector goodTile = 
			theStaticMap.CanUnitGo( nBoundTileRadius, pathPoints[nCurTile], aiClass ) ?
			pathPoints[nCurTile] : lastKnownGoodTile;
		theStaticMap.RestoreMode();

		pPathFinder->SetPathParameters
		(
			nBoundTileRadius, aiClass, 0, 
			AICellsTiles::GetPointByTile(pathPoints[nCurTile]), AICellsTiles::GetPointByTile(nextTile), 
			4 * mDistance( pathPoints[nCurTile], nextTile ), false, goodTile
		);
		pPathFinder->CalculatePathWOCycles();
		if ( pPathFinder->GetPathLength() == -1 )
			bFound = false;
	}
	else
		bFound = false;

	if ( !bFound )
	{
		theStaticMap.MemMode();
		theStaticMap.SetMode( ELM_STATIC );
		const SVector goodTile = 
			theStaticMap.CanUnitGo( nBoundTileRadius, pathPoints[nCurTile], aiClass ) ?
			pathPoints[nCurTile] : lastKnownGoodTile;
		theStaticMap.RestoreMode();
		
		
		pPathFinder->SetPathParameters
		(
			nBoundTileRadius, aiClass, 0, 
			AICellsTiles::GetPointByTile( pathPoints[nCurTile] ), AICellsTiles::GetPointByTile( curStPathTile ),
			SConsts::INFINITY_PATH_LIMIT, false, goodTile
		);
		pPathFinder->CalculatePathWOCycles();

	}
	pPathFinder->SmoothPath();

	if ( curStPathTile == pStPath->GetFinishTile() && nextTile != pPathFinder->GetFinishTile() )
		finishPoint = AICellsTiles::GetPointByTile( pPathFinder->GetFinishTile() );

	int len = pPathFinder->GetPathLength();
	if ( 2 * SConsts::MAX_LENGTH_OF_SMALL_PATH >= len )
	{
		if ( len < SConsts::MAX_LENGTH_OF_SMALL_PATH )
		{
			SaveSmallPath( len );
			bSmallPathTooLong = false;
		}
		else
		{
			SaveSmallPath( len/2 );
			bSmallPathTooLong = bLastStep;
		}
	}
	else
	{
		SaveSmallPath( SConsts::MAX_LENGTH_OF_SMALL_PATH );
		bSmallPathTooLong = true;
	}
}
bool CStandartPath::CalculateNewPath( const bool bShift )
{
	if ( bSmallPathTooLong )
		CalculateSmallPath( nCurStaticPoint == pStPath->GetLength() );
	else
		if ( nCurStaticPoint < pStPath->GetLength() || !bShift )
		{
			int shift;
			if ( bShift )
				shift = Min( nCurStaticPoint + SConsts::BIG_PATH_SHIFT, pStPath->GetLength() );
			else
				shift = nCurStaticPoint;

			while ( shift < pStPath->GetLength() && !theStaticMap.CanUnitGo( nBoundTileRadius, pStPath->GetTile( shift - 1 ), aiClass ) )
				++shift;

			if ( shift > 0 )
				curStPathTile = pStPath->GetTile( shift - 1 );

			nCurStaticPoint = shift;

			CalculateSmallPath( nCurStaticPoint == pStPath->GetLength() );
		}
		else
			return false;

	return true;
}
void CStandartPath::InitByStaticPath( IStaticPath *_pStPath, const CVec2 &_startPoint, const CVec2 &_finishPoint )
{
	NI_ASSERT_T( _pStPath != 0, "Null static path" );

	nInsertedTiles = 0;
	nCurInsertedTile = -1;

	NI_ASSERT_T( dynamic_cast<CCommonStaticPath*>(_pStPath) != 0, "Wrong path passed" );
	pStPath = static_cast<CCommonStaticPath*>(_pStPath);

	startPoint = _startPoint;
	finishPoint = _finishPoint;
	nCurStaticPoint = 0;
	bSmallPathTooLong = false;

	curStPathTile = pStPath->GetStartTile();
	
	nCurTile = 0; nCurPathPoint = 1;
	pathPoints[0] = AICellsTiles::GetTile( startPoint );
	vShift = finishPoint - pStPath->GetFinishPoint();

	CalculateNewPath( true );
}
CStandartPath::CStandartPath( const int _nBoundTileRadius, const BYTE _aiClass, IStaticPathFinder *_pPathFinder, interface IStaticPath *_pStPath, const CVec2 &startPoint, const CVec2 &finishPoint, const SVector &_lastKnownGoodTile )
: nBoundTileRadius( _nBoundTileRadius ), aiClass( _aiClass ), pPathFinder( _pPathFinder ), lastKnownGoodTile( _lastKnownGoodTile )
{
	InitByStaticPath( _pStPath, startPoint, finishPoint );
}
void CStandartPath::RecoverState( const CVec2 &point, const SVector &_lastKnownGoodTile )
{
	lastKnownGoodTile = _lastKnownGoodTile;
	pPathFinder->SetPathParameters( nBoundTileRadius, aiClass, 0, point, finishPoint, SConsts::INFINITY_PATH_LIMIT, true, lastKnownGoodTile );
	pPathFinder->CalculatePathWOCycles();
	pPathFinder->SmoothPath();

	InitByStaticPath( new CCommonStaticPath( *pPathFinder, finishPoint ), point, finishPoint );
}
void CStandartPath::Recalculate( const CVec2 &point, const SVector &_lastKnownGoodTile )
{
	lastKnownGoodTile = _lastKnownGoodTile;
	nCurTile = 0;
	nCurPathPoint = 1;
	startPoint = point;
	pathPoints[0] = AICellsTiles::GetTile( point );
	bSmallPathTooLong = false;

	CalculateNewPath( false );
}
const CVec2 CStandartPath::PeekPoint( int nShift )
{
	if ( nCurInsertedTile > -1 )
	{
		if ( nShift < nInsertedTiles - nCurInsertedTile )
			return AICellsTiles::GetPointByTile( insertedTiles[nCurInsertedTile + nShift] );
		else
			nShift -= nInsertedTiles - nCurInsertedTile;
	}

	int nPoint = nCurTile;
	int inc = 0;
	while ( inc != nShift && nPoint != nCurPathPoint )
	{
		nPoint = GetNextPos( nPoint );
		++inc;

		if ( nPoint == nCurPathPoint && !CalculateNewPath( true ) )
			break;
	}

	CVec2 result;
	if ( nPoint != nCurPathPoint )
		result = AICellsTiles::GetPointByTile( pathPoints[nPoint] );

	if ( nPoint == nCurPathPoint || GetNextPos( nPoint ) == nCurPathPoint && !CalculateNewPath( true ) )
		return GetFinishPoint();
	else
		return result;
}
void CStandartPath::Shift( int nShift )
{
	if ( nCurInsertedTile > -1 )
	{
		if ( nShift < nInsertedTiles - nCurInsertedTile )
		{
			nCurInsertedTile += nShift;
			return;
		}
		else
		{
			nShift -= nInsertedTiles - nCurInsertedTile;
			nCurInsertedTile = -1;
		}
	}

	int inc = 0;
	while ( inc != nShift && nCurTile != nCurPathPoint )
	{
		nCurTile = GetNextPos( nCurTile );
		++inc;

		if ( nCurTile == nCurPathPoint && !CalculateNewPath( true ) )
			break;
	}
}
void CStandartPath::InsertTiles( const std::list<SVector> &tiles )
{
	nInsertedTiles = 0;
	if ( insertedTiles.size() < tiles.size() )
		insertedTiles.resize( tiles.size() );

	nInsertedTiles = 0;
	for ( std::list<SVector>::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		insertedTiles[nInsertedTiles++] = *iter;

	nCurInsertedTile = 0;
}
const BYTE CStandartPath::GetNextPos( BYTE n )
{
	return ( ++n == ENPathPoints ) ? 0 : n;
}
bool CStandartPath::CanGoBackward( interface IBasePathUnit *pUnit )
{ 
	return 
		pUnit->CanGoBackward() && pStPath->GetLength() * float(SConsts::TILE_SIZE) <= pUnit->GetAABBHalfSize().y * 3.0f;
}
CStandartDirPath::CStandartDirPath( const CVec2 &_startPoint, const CVec2 &_dir, const CVec2 &_finishPoint )
: dir( _dir ), startPoint( _startPoint ), finishPoint( _finishPoint ), curPoint( _startPoint )
{
}
bool CStandartDirPath::IsFinished() const
{
	return mDistance( curPoint, finishPoint ) < 2;
}
const CVec2& CStandartDirPath::GetFinishPoint() const
{
	return finishPoint;
}
void CStandartDirPath::Recalculate( const CVec2 &point, const SVector &lastKnownGoodTile )
{
	startPoint = point;
	dir = finishPoint - point;
	Normalize( &dir );
}
void CStandartDirPath::RecoverState( const CVec2 &point, const SVector &lastKnownGoodTile )
{
	curPoint = point;
	dir = Norm( finishPoint - curPoint );
}
const CVec2 CStandartDirPath::PeekPoint( const int nShift )
{
	if ( !IsFinished() )
	{
		CVec2 res( curPoint );
		int inc = 0;
		
		
		while ( ( finishPoint - res ) * dir > 0  && inc < nShift )
		{
			res += dir * SConsts::TILE_SIZE;
			++inc;
		}

		if ( ( finishPoint - res ) * dir <= 0 )
			return finishPoint;
		else
			return res;
	}
	else 
		return finishPoint;
}
void CStandartDirPath::Shift( const int nShift )
{
	if ( !IsFinished() )
	{
		int inc = 0;

		while ( ( finishPoint - curPoint ) * dir > 0 && inc < nShift )
		{
			curPoint += dir * SConsts::TILE_SIZE;
			++inc;
		}

		if ( ( finishPoint - curPoint ) * dir <= 0 )
			curPoint = finishPoint;
	}
}
bool CStandartDirPath::CanGoBackward( interface IBasePathUnit *pUnit )
{
	return pUnit->CanGoBackward();
}
