#include "StdAfx.h"

#include "TrainPath.h"
#include "TrainPathFinder.h"
#include "RailRoadGraph.h"
#include "TrainPathUnit.h"
#include "BasePathUnit.h"
#include "AIUnit.h"
extern CRailroadGraph theRailRoadGraph;
extern NTimer::STime curTime;
extern CPtr<IStaticPathFinder> pTheTrainPathFinder;
CTrainPath::CTrainPath( IStaticPathFinder *_pPathFinder, CTrainPathUnit *pTrain )
{
	NI_ASSERT_T( dynamic_cast<CTrainPathFinder*>(_pPathFinder) != 0, "Wrong pathfinder passed to CTrainPath" );
	CTrainPathFinder *pPathFinder = static_cast<CTrainPathFinder*>( _pPathFinder );

	int v1 = 0;
	pPathFinder->StartPathIterating();
	if ( pPathFinder->GetCurPathNode() != -1 )
	{
		v1 = pPathFinder->GetCurPathNode();
		IEdge *pEdge = pPathFinder->GetStartEdgePoint()->GetEdge();
		edges.push_back( SPathEdge( pPathFinder->GetStartEdgePoint(), pEdge->CreateLastEdgePoint() ) );
		
		pPathFinder->Iterate();
	}

	while ( pPathFinder->GetCurPathNode() != -1 )
	{
		const int v2 = pPathFinder->GetCurPathNode();
		IEdge *pCurEdge = theRailRoadGraph.GetEdge( v1, v2 );

		edges.push_back( SPathEdge( pCurEdge->CreateFirstEdgePoint(), pCurEdge->CreateLastEdgePoint() ) );

		pPathFinder->Iterate();
		v1 = v2;

		NI_ASSERT_T( edges.back().pFirstPoint != 0 && edges.back().pLastPoint != 0, "Wrong path edge" );
		NI_ASSERT_T( edges.back().pFirstPoint->GetEdge() != 0 && edges.back().pLastPoint->GetEdge() != 0, "Wrong path edge" );
	}

	if ( edges.empty() )
	{
		IEdge *pPathFinderEdge = pPathFinder->GetStartEdgePoint()->GetEdge();
		edges.push_back( SPathEdge( pPathFinder->GetStartEdgePoint(), pPathFinder->GetFinishEdgePoint() ) );

		NI_ASSERT_T( edges.back().pFirstPoint != 0 && edges.back().pLastPoint != 0, "Wrong path edge" );
		NI_ASSERT_T( edges.back().pFirstPoint->GetEdge() != 0 && edges.back().pLastPoint->GetEdge() != 0, "Wrong path edge" );
	}
	else
	{
		CPtr<IEdge> pLastEdge = pPathFinder->GetFinishEdgePoint()->GetEdge();
		edges.push_back( SPathEdge( pLastEdge->CreateFirstEdgePoint(), pPathFinder->GetFinishEdgePoint() ) );

		NI_ASSERT_T( edges.back().pFirstPoint != 0 && edges.back().pLastPoint != 0, "Wrong path edge" );
		NI_ASSERT_T( edges.back().pFirstPoint->GetEdge() != 0 && edges.back().pLastPoint->GetEdge() != 0, "Wrong path edge" );
	}
	
	vStartPoint = pPathFinder->GetStartEdgePoint()->Get2DPoint();
	if ( edges.empty() )
		vFinishPoint = vStartPoint;
	else
		vFinishPoint = edges.back().pLastPoint->Get2DPoint();
}
const SVector CTrainPath::GetStartTile() const
{
	return AICellsTiles::GetTile( vStartPoint );
}
const SVector CTrainPath::GetFinishTile() const
{
	return AICellsTiles::GetTile( vFinishPoint );
}
const CVec2& CTrainPath::GetFinishPoint() const
{
	return vFinishPoint;
}
CEdgePoint* CTrainPath::GetFirstPoint( std::list< SPathEdge >::iterator iter )
{
	return iter->pFirstPoint;
}
CEdgePoint* CTrainPath::GetLastPoint( std::list< SPathEdge >::iterator iter )
{
	return iter->pLastPoint;
}
const CVec2 CTrainPath::GetDirToGo()
{
	std::list<SPathEdge>::iterator iter = edges.begin();
	while ( iter != edges.end() && ( iter->pFirstPoint->GetEdge()->GetLength() == 0 ) )
		++iter;

	if ( iter == edges.end() )
		return edges.front().pFirstPoint->GetTangent();
	else
		return edges.front().pFirstPoint->GetTangent();
}
BASIC_REGISTER_CLASS( CTrainSmoothPath );
void CTrainSmoothPath::InitTrain()
{
	carriages.resize( pOwner->GetNCarriages() );
	
	for ( int i = 0; i < pOwner->GetNCarriages(); ++i )
	{
		if ( i == 0 )
		{
			carriages[i].frontWheel.pPoint = pTrainPath->GetFirstPoint( pTrainPath->GetStartEdgeIter() );
			carriages[i].frontWheel.iter = pTrainPath->GetStartEdgeIter();
		}
		else
		{
			if ( pOwner->IsFrontDir() )
				carriages[i].frontWheel.pPoint = pOwner->GetCarriage( i )->GetFrontWheelPoint();
			else
				carriages[i].frontWheel.pPoint = pOwner->GetCarriage( i )->GetBackWheelPoint();
		}

		if ( pOwner->IsFrontDir() )
			carriages[i].backWheel.pPoint = pOwner->GetCarriage( i )->GetBackWheelPoint();
		else
			carriages[i].backWheel.pPoint = pOwner->GetCarriage( i )->GetFrontWheelPoint();
	}
}
void CTrainSmoothPath::CheckPath()
{
	if ( pTrainPath->GetDirToGo() * pOwner->GetCarriage( 0 )->GetDirVector() < 0 )
	{
		if ( nRecalculating == 2 )
			bFinished = true;
		else if ( nRecalculating == 1 )
		{
			CVec2 vDirVector = pOwner->GetCarriage( 0 )->GetDirVector();

			std::list<int> trainNodes;
			pOwner->GetTrainNodes( &trainNodes );
			std::list<int>::iterator trainIter = trainNodes.begin();
			std::list< SPathEdge >::iterator pathIter = pTrainPath->GetStartEdgeIter();

			CPtr<CEdgePoint> pIndentPoint;
			float fLengthOfLastEdge = 10.0f;
			while ( pathIter != pTrainPath->GetEndEdgesIter() &&
							trainIter != trainNodes.end() &&
							pathIter->pLastPoint->GetEdge()->GetLastNode() == *trainIter )
			{
				if ( fLengthOfLastEdge != 0.0f )
					pIndentPoint = pathIter->pFirstPoint;
				fLengthOfLastEdge = pathIter->pFirstPoint->GetEdge()->GetLength();

				++pathIter;
				++trainIter;
			}

			if ( pathIter == pTrainPath->GetEndEdgesIter() || pIndentPoint == 0 )
				bFinished = true;
			else
			{
				while ( pathIter != pTrainPath->GetEndEdgesIter() && 
								pathIter->pFirstPoint->GetEdge()->GetLength() == 0 )
					++pathIter;

				if ( pathIter == pTrainPath->GetEndEdgesIter() )
					bFinished = true;
				else
				{
					CPtr<CEdgePoint> pPoint;
					if ( vDirVector * pathIter->pFirstPoint->GetTangent() <= 0 )
					{
						pPoint = theRailRoadGraph.MakeIndent( vDirVector, pIndentPoint, pOwner->GetTrainLength() + pOwner->GetCarriage(0)->GetDistanceBetweenWheels() / 2.0f );
						if ( pPoint == 0 )
						{
							pOwner->ChangeDirection( !pOwner->IsFrontDir() );
							vDirVector = pOwner->GetCarriage( 0 )->GetDirVector();
							pPoint = theRailRoadGraph.MakeIndent( vDirVector, pIndentPoint, pOwner->GetTrainLength() + pOwner->GetCarriage(0)->GetDistanceBetweenWheels() / 2.0f );
						}
					}
					else
					{
						pOwner->ChangeDirection( !pOwner->IsFrontDir() );
						vDirVector = pOwner->GetCarriage( 0 )->GetDirVector();
						pPoint = theRailRoadGraph.MakeIndent( vDirVector, pIndentPoint, pOwner->GetTrainLength() + pOwner->GetCarriage(0)->GetDistanceBetweenWheels() / 2.0f );

						if ( pPoint == 0 )
						{
							pOwner->ChangeDirection( !pOwner->IsFrontDir() );
							vDirVector = pOwner->GetCarriage( 0 )->GetDirVector();
							pPoint = theRailRoadGraph.MakeIndent( vDirVector, pIndentPoint, pOwner->GetTrainLength() + pOwner->GetCarriage(0)->GetDistanceBetweenWheels() / 2.0f );
						}
					}

					if ( !pPoint )
						bFinished = true;
					else
					{
						++nRecalculating;
						Init( pOwner->CreateBigStaticPath( pTrainPath->GetStartPoint(), pPoint->Get2DPoint(), 0 ) );
						--nRecalculating;
					}
				}
			}
		}
		else
		{
			pOwner->ChangeDirection( !pOwner->IsFrontDir() );
			NI_ASSERT_T( nRecalculating == 0, NStr::Format( "Wrong value of nRecalculating (%d)", nRecalculating ) );
			nRecalculating = 1;
			vRealFinishPoint = pTrainPath->GetFinishPoint();
			Init( pOwner->CreateBigStaticPath( pTrainPath->GetStartPoint(), pTrainPath->GetFinishPoint(), 0 ) );

			if ( bFinished )
				bRecalculatedPath = false;
			else
				bRecalculatedPath = true;

			nRecalculating = 0;
		}
	}
}
void CTrainSmoothPath::DelSharpAngles()
{
	std::list<SPathEdge>::iterator iter = pTrainPath->GetStartEdgeIter();
	CVec2 vDir = pOwner->GetCarriage( 0 )->GetDirVector();
	CPtr<CEdgePoint> pPoint = iter->pFirstPoint;

	bool bSharpAngle = false;
	while ( iter != pTrainPath->GetEndEdgesIter() && !bSharpAngle )
	{
		SPathEdge temp = *iter;
		CVec2 vTemp = iter->pFirstPoint->GetTangent();
		if ( iter->pFirstPoint->GetEdge()->GetLength() == 0 || temp.pFirstPoint->IsEqual( temp.pLastPoint ) )
		{
			pPoint = iter->pLastPoint;
			++iter;
		}
		else if ( iter->pFirstPoint->GetTangent() * vDir >= 0 )
		{
			vDir = iter->pLastPoint->GetTangent();
			pPoint = iter->pLastPoint;
			++iter;
		}
		else
			bSharpAngle = true;
	}

	if ( bSharpAngle )
	{
		CPtr<CEdgePoint> pFinishPoint = theRailRoadGraph.MakeIndent( vDir, pPoint, pOwner->GetTrainLength() + pOwner->GetCarriage(0)->GetDistanceBetweenWheels() / 2.0f );
		if ( pFinishPoint )
		{
			vRealFinishPoint = pTrainPath->GetFinishPoint();

			nRecalculating = 2;
			Init( pOwner->CreateBigStaticPath( pTrainPath->GetStartPoint(), pFinishPoint->Get2DPoint(), 0 ) );
			nRecalculating = 0;

			bRecalculatedPath = true;
		}
		else
			bFinished = true;
	}
}
bool CTrainSmoothPath::Init( IStaticPath *_pTrainPath )
{
	if ( _pTrainPath == 0 )
	{
		pTrainPath = 0;
		return false;
	}
	else
	{
		NI_ASSERT_T( dynamic_cast<CTrainPath*>(_pTrainPath) != 0, "Wrong path passed" );
		pTrainPath = static_cast<CTrainPath*>(_pTrainPath );
		lastUpdateTime = curTime - 1000;
		bFinished = false;
		bRecalculatedPath = false;

		const bool bFrontDir = pOwner->IsFrontDir();

		CheckPath();

		if ( !bFinished && nRecalculating == 0 )
		{
			DelSharpAngles();
			if ( !bFinished )
			{
				SRect locomotiveRect = pOwner->GetCarriage( 0 )->GetUnitRect();
				if ( pOwner->IsFrontDir() )
					locomotiveRect.InitRect( locomotiveRect.center, locomotiveRect.dir, locomotiveRect.lengthAhead, 0, locomotiveRect.width );
				else
					locomotiveRect.InitRect( locomotiveRect.center, locomotiveRect.dir, 0, locomotiveRect.lengthBack, locomotiveRect.width );
				if ( pOwner->GetCarriage( 0 )->GetOwner()->IsOnLockedTiles( locomotiveRect ) )
					FinishPath();
				else
					InitTrain();
			}
		}

		if ( bFrontDir != pOwner->IsFrontDir() )
			fSpeed = 0;
	}

	return true;
}
bool CTrainSmoothPath::IsFinished() const
{
	return bFinished;
}
void CTrainSmoothPath::MoveFrontWheel( const int n, float fDist )
{
	SPathPoint &wheel = carriages[n].frontWheel;
	CVec2 vPointToMeasureDist = wheel.pPoint->Get2DPoint();
	float fDistToNewEdgePoint = 0.0f;

	std::list<int> newNodes;
	do
	{
		if ( wheel.iter == pTrainPath->GetEndEdgesIter() )
			break;

		CPtr<CEdgePoint> pFirstEdgePoint = pTrainPath->GetFirstPoint( wheel.iter );
		CPtr<CEdgePoint> pLastEdgePoint = pTrainPath->GetLastPoint( wheel.iter );
		if ( pFirstEdgePoint && pFirstEdgePoint->IsValid() && pLastEdgePoint && pLastEdgePoint->IsValid() )
		{
			IEdge *pTrainEdge = pFirstEdgePoint->GetEdge();

			CPtr<CEdgePoint> pNewEdgePoint = pTrainEdge->MakeIndent( vPointToMeasureDist, wheel.pPoint, pLastEdgePoint, fDist );

			fDistToNewEdgePoint = fabs( vPointToMeasureDist - pNewEdgePoint->Get2DPoint() );
			if ( pNewEdgePoint->IsEqual( pLastEdgePoint ) )
			{
				if ( ++wheel.iter != pTrainPath->GetEndEdgesIter() )
				{
					CPtr<CEdgePoint> pLastPointOfTrainEdge = pTrainEdge->CreateLastEdgePoint();
					if ( pNewEdgePoint->IsEqual( pLastPointOfTrainEdge ) )
					{
						newNodes.push_back( pTrainEdge->GetLastNode() );
					}

					wheel.pPoint = pTrainPath->GetFirstPoint( wheel.iter );
				}
			}
			else
				wheel.pPoint = pNewEdgePoint;

			if ( !pTrainPath|| wheel.iter == pTrainPath->GetEndEdgesIter() )
				bFinished = true;
		}
		else
			bFinished = true;
	} while ( !bFinished && fDist - 0.001f > fDistToNewEdgePoint );

	pOwner->PushNodesToFrontCarriage( newNodes );
}
void CTrainSmoothPath::LoadIterators()
{
	bJustLoaded = false;
	if ( pTrainPath )
	{
		carriages[0].frontWheel.iter = pTrainPath->GetStartEdgeIter();
		std::advance( carriages[0].frontWheel.iter, iteratorShift );
	}
}
const CVec3 CTrainSmoothPath::GetPoint( NTimer::STime timeDiff )
{
	if ( bJustLoaded )
		LoadIterators();
	
	if ( lastUpdateTime != curTime && !IsFinished() )
	{
		lastUpdateTime = curTime;

		const float minSpeed = fSpeed - pOwner->GetMaxPossibleSpeed() / 100;
		const float maxSpeed = fSpeed + pOwner->GetMaxPossibleSpeed() / 100;
		float maxSpeedHere = pOwner->GetMaxSpeedHere( pOwner->GetCenter() );

		if ( maxSpeed < maxSpeedHere )
			fSpeed = maxSpeed;
		else if ( minSpeed > maxSpeedHere )
			fSpeed = minSpeed;
		else
			fSpeed = maxSpeedHere;

		float fDist = fSpeed * timeDiff;

		const CVec2 vOldPos = carriages[0].frontWheel.pPoint->Get2DPoint();
		MoveFrontWheel( 0, fDist );

		SRect locomotiveRect = pOwner->GetCarriage( 0 )->GetUnitRect();
		locomotiveRect.Compress( 1.3f );
		if ( pOwner->IsFrontDir() )
			locomotiveRect.InitRect( locomotiveRect.center, locomotiveRect.dir, locomotiveRect.lengthAhead, 0, locomotiveRect.width );
		else
			locomotiveRect.InitRect( locomotiveRect.center, locomotiveRect.dir, 0, locomotiveRect.lengthBack, locomotiveRect.width );
		if ( pOwner->GetCarriage( 0 )->GetOwner()->IsLockingTiles() )
			pOwner->GetCarriage( 0 )->GetOwner()->UnlockTiles();

		if ( pOwner->GetCarriage( 0 )->GetOwner()->IsOnLockedTiles( locomotiveRect ) )
		{
			FinishPath();
			return CVec3( pOwner->GetCenter(), pOwner->GetZ() );
		}
		else
		{
			fSpeed = fabs( vOldPos - carriages[0].frontWheel.pPoint->Get2DPoint() ) / float( timeDiff );
			pOwner->SetBackWheel( 0 );

			for ( int i = 1; i < carriages.size(); ++i )
			{
				pOwner->SetFrontWheel( i );
				pOwner->SetBackWheel( i );
			}

			pOwner->SetCurEdgePoint( carriages[0].frontWheel.pPoint );

			for ( int i = 0; i < carriages.size(); ++i )
			{
				if ( pOwner->IsFrontDir() )
					pOwner->GetCarriage( i )->SetPlacementByWheels( carriages[i].frontWheel.pPoint, carriages[i].backWheel.pPoint );
				else
					pOwner->GetCarriage( i )->SetPlacementByWheels( carriages[i].backWheel.pPoint, carriages[i].frontWheel.pPoint );
			}

			const CVec3 vResult = CVec3( carriages[0].frontWheel.pPoint->Get2DPoint(), 0.0f );

			if ( bRecalculatedPath && bFinished )
			{
				bFinished = false;
				bRecalculatedPath = false;
				Init( pOwner->CreateBigStaticPath( carriages[0].frontWheel.pPoint->Get2DPoint(), vRealFinishPoint, 0 ) );
			}

			return vResult;
		}
	}
	return 
		CVec3( pOwner->GetCenter(), pOwner->GetZ() );
}
float& CTrainSmoothPath::GetSpeedLen()
{
	return fSpeed;	
}
void CTrainSmoothPath::SetOwner( IBasePathUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CTrainPathUnit*>(pUnit) != 0, "Wrong unit passed to TrainSmoothPath" );
	pOwner = static_cast<CTrainPathUnit*>(pUnit);
}
IBasePathUnit* CTrainSmoothPath::GetOwner() const
{
	return pOwner;
}
CEdgePoint* CTrainSmoothPath::GetBackWheelPoint( const int n ) const
{
	return carriages[n].backWheel.pPoint;
}
CEdgePoint* CTrainSmoothPath::GetFrontWheelPoint( const int n ) const
{
	return carriages[n].frontWheel.pPoint;
}
void CTrainSmoothPath::SetNewFrontWheel( const int n, CEdgePoint *pNewPoint )
{
	carriages[n].frontWheel.pPoint = pNewPoint;
}
void CTrainSmoothPath::SetNewBackWheel( const int n, CEdgePoint *pNewPoint )
{
	carriages[n].backWheel.pPoint = pNewPoint;
}
void CTrainSmoothPath::NotifyAboutClosestThreat( IBasePathUnit *pCollUnit, const float fDist )
{
	if ( !pCollUnit->IsInOneTrain( pOwner ) )
		fSpeed = 0;
}
bool CTrainSmoothPath::Init( IBasePathUnit *pUnit, IPath *pPath, bool bSmoothTurn, bool bCheckTurn )
{
	FinishPath();
	return true;
}
bool CTrainSmoothPath::Init( IMemento *pMemento, IBasePathUnit *pUnit )
{
	NI_ASSERT_T( dynamic_cast<CTrainSmoothPathMemento*>(pMemento) != 0, "Wrong memento passed" );
	pTrainPath = static_cast<CTrainSmoothPathMemento*>(pMemento)->pPath;

	return true;
}
bool CTrainSmoothPath::InitByFormationPath( CFormation *pFormation, IBasePathUnit *pUnit )
{
	FinishPath();
	return true;
}
void CTrainSmoothPath::FinishPath()
{
	bFinished = true;
	fSpeed = 0;
}
IMemento* CTrainSmoothPath::GetMemento() const 
{ 
	return 
		new CTrainSmoothPathMemento( pTrainPath );
}
CTrainSmoothPathMemento::CTrainSmoothPathMemento( CTrainPath *_pPath ) 
: pPath( _pPath ) 
{ 
}
