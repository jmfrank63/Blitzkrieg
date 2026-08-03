#include "StdAfx.h"

#include "TrainPathUnit.h"
#include "AIUnit.h"
#include "RailroadGraph.h"
#include "TrainPathFinder.h"
#include "TrainPath.h"
#include "AIStaticMap.h"
extern CPtr<IStaticPathFinder> pTheTrainPathFinder;
extern CRailroadGraph theRailRoadGraph;
extern CStaticMap theStaticMap;
BASIC_REGISTER_CLASS( CTrainPathUnit );
bool CTrainPathUnit::InitBy( CCarriagePathUnit *pUnit )
{
	if ( pUnit->GetOwner()->GetStats()->type == RPG_TYPE_TRAIN_LOCOMOTIVE )
		bCanMove = true;

	const CVec2 vFrontWheelPoint = pUnit->GetFrontWheel2D();
	const CVec2 vBackWheelPoint = pUnit->GetBackWheelPoint2DByFrontPoint( vFrontWheelPoint );

	std::list< CPtr<CEdgePoint> > edgePoints;
	float fMinDist;
	theRailRoadGraph.GetClosestPoints( vFrontWheelPoint, &edgePoints, &fMinDist, -1, 0 );

	vCenter = pUnit->GetCenter();
	vSpeed = VNULL2;
	vDir = pUnit->GetDirVector();
	fMaxPossibleSpeed = pUnit->GetMaxPossibleSpeed();
	fPassability = pUnit->GetOwner()->GetPassability();

	if ( edgePoints.empty() )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, "Can't put a train on railroad", 0xffff0000 );		
		bCanMove = false;
	}
	else
	{
		pCurEdgePoint = edgePoints.front();
		const CVec2 vNewFrontWheelPoint = pCurEdgePoint->Get2DPoint();

		CPtr<CEdgePoint> pBackEdgePoint = theRailRoadGraph.MakeIndent( -pUnit->GetFrontDirVec(), pCurEdgePoint, pUnit->GetDistanceBetweenWheels() );
		if ( pBackEdgePoint == 0 )
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, "Can't put a train on railroad", 0xffff0000 );
			return false;
		}

		pUnit->SetPlacementByWheels( pCurEdgePoint, pBackEdgePoint );

		pSmoothPath = new CTrainSmoothPath;
		pSmoothPath->SetOwner( this );
	}

	return true;
}
void CTrainPathUnit::AddCarriage( CCarriagePathUnit *pCarriage )
{
	if ( pCarriage->GetOwner()->GetStats()->type == RPG_TYPE_TRAIN_LOCOMOTIVE )
		bCanMove = true;
	
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( pCarriage->GetStats() );
	fTrainLength += fabs( pStats->vHookPoint.y ) + fabs( pStats->vTowPoint.y );

	carriages.push_back( pCarriage );
	nodesInside.resize( carriages.size() );
	intermNodes.resize( carriages.size() );
}
void CTrainPathUnit::SecondSegment( const bool bUpdate )
{
	if ( CanMovePathfinding() )
	{
		if ( pPathToMove )
		{
			bool bCanGo = true;
			std::vector< CPtr<CCarriagePathUnit> >::iterator iter = carriages.begin();
			while ( iter != carriages.end() && (*iter)->GetOwner()->IsUninstalled() )
				++iter;

			if ( iter == carriages.end() )
			{
				pSmoothPath->Init( pPathToMove );
				pPathToMove = 0;
			}
		}
		
		std::list<CVec2> oldCenters;
		for ( int i = 0; i < carriages.size(); ++i )
			oldCenters.push_back( carriages[i]->GetCenter() );
		
		const CVec3 newCenter = GetSmoothPath()->GetPoint( SConsts::AI_SEGMENT_DURATION );
		vCenter.x = newCenter.x;
		vCenter.y = newCenter.y;

		vSpeed = GetSmoothPath()->GetSpeedLen() * GetDirVector();

		std::list<CVec2>::iterator iter = oldCenters.begin();
		for ( int i = 0; i < carriages.size(); ++i )
		{
			carriages[i]->SetSpeed( fabs(*iter - carriages[i]->GetCenter()) / float( SConsts::AI_SEGMENT_DURATION ) * carriages[i]->GetDirVector() );
			++iter;
		}
	}
}
IStaticPath* CTrainPathUnit::CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, IPointChecking *pPointChecking )
{
	if ( !CanMovePathfinding() )
		return 0;
	else
	{
		static_cast_ptr<CTrainPathFinder*>(pTheTrainPathFinder)->SetPathParameters( this, vFinishPoint );
		pTheTrainPathFinder->CalculatePath();
		
		if ( pTheTrainPathFinder->GetPathLength() == -1.0f )
			return 0;
		else
			return new CTrainPath( pTheTrainPathFinder, this );
	}
}
bool CTrainPathUnit::SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn )
{
	pPathToMove = pStaticPath;
	if ( CanMovePathfinding() )
	{
		for ( std::vector< CPtr<CCarriagePathUnit> >::iterator iter = carriages.begin(); iter != carriages.end(); ++iter )
			(*iter)->GetOwner()->InstallAction( ACTION_NOTIFY_UNINSTALL_ROTATE );

		return true;
	}
	else
		pPathToMove = 0;

	return false;
}
bool CTrainPathUnit::SendAlongPath( IPath *pPath )
{
	NI_ASSERT_T( false, "A train can't be send by IPath" );
	return false;
}
CEdgePoint* CTrainPathUnit::GetCurEdgePoint()
{
	return pCurEdgePoint;
}
ISmoothPath* CTrainPathUnit::GetCurPath() const 
{ 
	return pSmoothPath; 
}
ISmoothPath* CTrainPathUnit::GetSmoothPath() const
{
	return pSmoothPath;
}
void CTrainPathUnit::SetCurEdgePoint( CEdgePoint *pEdgePoint )
{
	pCurEdgePoint = pEdgePoint;
}
const WORD CTrainPathUnit::GetID() const { return 0; }
const CVec2& CTrainPathUnit::GetCenter() const { return vCenter; }
const float CTrainPathUnit::GetZ() const { return 0.0f; }
void CTrainPathUnit::AdjustWithDesirableSpeed( float *pfMaxSpeed ) const
{
}
const float CTrainPathUnit::GetMaxSpeedHere( const CVec2 &point, bool bAdjust ) const
{
	const float fMapPass = theStaticMap.GetPass( point );
	const float fSpeed = GetMaxPossibleSpeed() * ( fMapPass + ( 1 - fMapPass ) * GetPassability() );

	return fSpeed;
}
const float CTrainPathUnit::GetMaxPossibleSpeed() const { return fMaxPossibleSpeed; }
const float CTrainPathUnit::GetPassability() const { return fPassability; }
const CVec2& CTrainPathUnit::GetSpeed() const { return vSpeed; }
const int CTrainPathUnit::GetBoundTileRadius() const { return 1; }
const WORD CTrainPathUnit::GetDir() const 
{ 
	return GetDirectionByVector( GetDirVector() );
}
const WORD CTrainPathUnit::GetFrontDir() const 
{ 
	return GetDirectionByVector( vDir );
}
const CVec2& CTrainPathUnit::GetDirVector() const 
{ 
	return vDir;
}
const CVec2 CTrainPathUnit::GetAABBHalfSize() const { return VNULL2; }
void CTrainPathUnit::SetCoordWOUpdate( const CVec3 &vNewCenter )
{
}
void CTrainPathUnit::SetNewCoordinates( const CVec3 &vNewCenter, bool bStopUnit )
{
	SetCoordWOUpdate( vNewCenter );
}
const SRect CTrainPathUnit::GetUnitRectForLock() const
{
	SRect unitRect;
	unitRect.InitRect( GetCenter(), GetDirVector(), 1.0f, 1.0f );

	return unitRect;
}
void CTrainPathUnit::UpdateDirection( const CVec2 &newDir ) { }
void CTrainPathUnit::UpdateDirection( const WORD newDir ) { }
bool CTrainPathUnit::IsIdle() const
{
	return GetCurPath() == 0 || GetCurPath()->IsFinished();
}
void CTrainPathUnit::StopUnit()
{
	pSmoothPath->Init( this, pTheTrainPathFinder->CreatePathByDirection( GetCenter(), CVec2( 1, 1 ), GetCenter(), 0 ), true );
	vSpeed = VNULL2;
}
IStaticPathFinder* CTrainPathUnit::GetPathFinder() const
{
	return pTheTrainPathFinder;
}
void CTrainPathUnit::SetDesirableSpeed( const float fDesirableSpeed )
{
}
void CTrainPathUnit::UnsetDesirableSpeed()
{
}
float CTrainPathUnit::GetDesirableSpeed() const
{
	return -1.0f;
}
CCarriagePathUnit* CTrainPathUnit::GetCarriage( const int n )
{
	NI_ASSERT_T( n < GetNCarriages(), NStr::Format( "Wrong number of carriage (%d)", n ) );
	return carriages[n];
}
float GetDistanceForWheels( float fWheel1, float fPoint1, float fWheel2, float fPoint2 )
{
	fWheel1 = fabs( fWheel1 );
	fPoint1 = fabs( fPoint1 );
	fWheel2 = fabs( fWheel2 );
	fPoint2 = fabs( fPoint2 );

	const float fDist1 = Max( fPoint1 - fWheel1, 0.0f );
	const float fDist2 = Max( fPoint2 - fWheel2, 0.0f );

	return fDist1 + fDist2;
}
const float CTrainPathUnit::GetDistFromBackToFrontWheel( const int n, const int m )
{
	const SMechUnitRPGStats *pCarrierStats = static_cast<const SMechUnitRPGStats*>( GetCarriage( n )->GetStats() );
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( GetCarriage( m )->GetStats() );

	const float fLen = GetDistanceForWheels(
												pCarrierStats->vBackWheel.y, pCarrierStats->vTowPoint.y,
												pStats->vFrontWheel.y, pStats->vHookPoint.y );

	return fLen;
}
const float CTrainPathUnit::GetDistFromFrontToBackWheel( const int n, const int m )
{
	const SMechUnitRPGStats *pCarrierStats = static_cast<const SMechUnitRPGStats*>( GetCarriage( n )->GetStats() );
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( GetCarriage( m )->GetStats() );

	const float fLen = GetDistanceForWheels(
												pCarrierStats->vFrontWheel.y, pCarrierStats->vHookPoint.y,
												pStats->vBackWheel.y, pStats->vTowPoint.y );

	return fLen;
}
void CTrainPathUnit::ChangeDirection( const bool bNewFrontDir )
{
	if ( bNewFrontDir != bFrontDir )
	{
		bFrontDir = bNewFrontDir;

		const int nCarriagesSize = carriages.size();
		for ( int i = 0; i < nCarriagesSize / 2; ++i )
		{
			std::swap( carriages[i], carriages[nCarriagesSize - 1 - i] );

			nodesInside[i].swap( nodesInside[nCarriagesSize - 1 - i] );
			intermNodes[i].swap( intermNodes[nCarriagesSize - 1 - i - 1] );

			nodesInside[i].reverse();
			if ( nCarriagesSize - 1 - i != i )
				nodesInside[nCarriagesSize - 1 - i].reverse();

			intermNodes[i].reverse();
			if ( nCarriagesSize - 1 - i - 1 != i )
				intermNodes[nCarriagesSize - 1 - i - 1].reverse();
		}

		if ( nCarriagesSize & 1 )
			nodesInside[nCarriagesSize >> 1].reverse();
	}

	if ( bFrontDir )
		pCurEdgePoint = carriages[0]->GetFrontWheelPoint();
	else
		pCurEdgePoint = carriages[0]->GetBackWheelPoint();		

	vCenter = pCurEdgePoint->Get2DPoint();

	for ( int i = 0; i < carriages.size(); ++i )
		carriages[i]->SetRightDir( bFrontDir );
}
void CTrainPathUnit::SetFrontWheel( const int n )
{
	NI_ASSERT_T( n > 0, "Can't set front wheel of the first train" );
	float fDist;
	if ( bFrontDir )
		fDist = GetDistFromBackToFrontWheel( n-1, n );
	else
		fDist = GetDistFromFrontToBackWheel( n-1, n );
		
	CPtr<CEdgePoint> pPoint;
	if ( !intermNodes[n-1].empty() )
	{
		IEdge *pBackWheelEdge = pSmoothPath->GetBackWheelPoint( n - 1 )->GetEdge();
		int nBackWheelV1 = pBackWheelEdge->GetFirstNode();
		int nBackWheelV2 = pBackWheelEdge->GetLastNode();

		NI_ASSERT_T( nBackWheelV1 == intermNodes[n-1].back() || nBackWheelV2 == intermNodes[n-1].back(), "Wrong edge" );

		if ( nBackWheelV2 == intermNodes[n-1].back() )
		{
			pPoint = new CEdgePoint( *(pSmoothPath->GetBackWheelPoint( n - 1 )) );
			pPoint->Reverse( theRailRoadGraph.GetEdge( nBackWheelV2, nBackWheelV1 ) );
		}
		else
			pPoint = pSmoothPath->GetBackWheelPoint( n - 1 );
	}
	else
		pPoint = pSmoothPath->GetBackWheelPoint( n - 1 );

	const CVec2 vPointToMeasureDist = pPoint->Get2DPoint();	
	CPtr<CEdgePoint> pNewFrontWheel;
	bool bFinished = false;
	std::list<int>::reverse_iterator iter = intermNodes[n-1].rbegin();
	while ( iter != intermNodes[n-1].rend() && !bFinished )
	{
		IEdge *pEdge = pPoint->GetEdge();
		CPtr<CEdgePoint> pFirstPoint = pEdge->CreateFirstEdgePoint();
		pNewFrontWheel = pEdge->MakeIndent( vPointToMeasureDist, pPoint, pFirstPoint, fDist );

		if ( !pNewFrontWheel->IsEqual( pFirstPoint ) )
			bFinished = true;
		else
		{
			std::list<int>::reverse_iterator iter1 = iter;
			std::advance( iter1, 1 );
			if ( iter1 != intermNodes[n-1].rend() )
			{
				IEdge *pNewEdge = theRailRoadGraph.GetEdge( *iter1, *iter );
				if ( pNewEdge != 0 )
					pPoint = pNewEdge->CreateLastEdgePoint();
			}
			else
			{
				IEdge *pNewEdge = pSmoothPath->GetFrontWheelPoint( n )->GetEdge();
				NI_ASSERT_T( pNewEdge->GetFirstNode() == *iter || pNewEdge->GetLastNode() == *iter, "Wrong last node" );
				int v = pNewEdge->GetFirstNode();
				if ( v == *iter )
					v = pNewEdge->GetLastNode();

				pPoint = theRailRoadGraph.GetEdge( v, *iter )->CreateLastEdgePoint();
			}

			++iter;
		}
	}

	if ( !bFinished )
	{
		IEdge *pEdge = pPoint->GetEdge();
		CPtr<CEdgePoint> pFirstPoint;
		if ( pSmoothPath->GetFrontWheelPoint( n )->GetEdge() != pEdge )
		{
			pFirstPoint = new CEdgePoint( *(pSmoothPath->GetFrontWheelPoint( n )) );
			pFirstPoint->Reverse( pEdge );
		}
		else
			pFirstPoint = pSmoothPath->GetFrontWheelPoint( n );
		pNewFrontWheel = pEdge->MakeIndent( vPointToMeasureDist, pPoint, pFirstPoint, fDist );
	}

	pSmoothPath->SetNewFrontWheel( n, pNewFrontWheel );

	for ( std::list<int>::iterator forwardIterator = intermNodes[n-1].begin(); forwardIterator != iter.base(); ++forwardIterator )
	{
		nodesInside[n].push_back( *forwardIterator );
	}

	intermNodes[n-1].erase( intermNodes[n-1].begin(), iter.base() );
}
void CTrainPathUnit::SetBackWheel( const int n )
{
	const float fDist = GetCarriage( n )->GetDistanceBetweenWheels();

	CPtr<CEdgePoint> pPoint;// = pSmoothPath->GetFrontWheelPoint( n );
	if ( !nodesInside[n].empty() )
	{
		IEdge *pFrontWheelEdge = pSmoothPath->GetFrontWheelPoint( n )->GetEdge();
		int nFrontWheelV1 = pFrontWheelEdge->GetFirstNode();
		int nFrontWheelV2 = pFrontWheelEdge->GetLastNode();

		NI_ASSERT_T( nFrontWheelV1 == nodesInside[n].back() || nFrontWheelV2 == nodesInside[n].back(), "Wrong edge" );

		if ( nFrontWheelV2 == nodesInside[n].back() )
		{
			pPoint = new CEdgePoint( *(pSmoothPath->GetFrontWheelPoint( n )) );
			pPoint->Reverse( theRailRoadGraph.GetEdge( nFrontWheelV2, nFrontWheelV1 ) );
		}
		else
			pPoint = pSmoothPath->GetFrontWheelPoint( n );
	}
	else
		pPoint = pSmoothPath->GetFrontWheelPoint( n );

	const CVec2 vPointToMeasureDist = pPoint->Get2DPoint();

	CPtr<CEdgePoint> pNewBackWheel;
	bool bFinished = false;
	std::list<int>::reverse_iterator iter = nodesInside[n].rbegin();
	while ( iter!= nodesInside[n].rend() && !bFinished )
	{
		IEdge *pEdge = pPoint->GetEdge();
		CPtr<CEdgePoint> pFirstPoint = pEdge->CreateFirstEdgePoint();
		pNewBackWheel = pEdge->MakeIndent( vPointToMeasureDist, pPoint, pFirstPoint, fDist );

		if ( !pNewBackWheel->IsEqual( pFirstPoint ) )
			bFinished = true;
		else
		{
			std::list<int>::reverse_iterator iter1 = iter;
			std::advance( iter1, 1 );
			if ( *iter != *iter1 )
			{
				if ( iter1 != nodesInside[n].rend() )
				{
					IEdge *pNewEdge = theRailRoadGraph.GetEdge( *iter1, *iter );
					pPoint = pNewEdge->CreateLastEdgePoint();
				}
				else
				{
					IEdge *pNewEdge = pSmoothPath->GetBackWheelPoint( n )->GetEdge();
					NI_ASSERT_T( pNewEdge->GetFirstNode() == *iter || pNewEdge->GetLastNode() == *iter, "Wrong last node" );
					int v = pNewEdge->GetFirstNode();
					if ( v == *iter )
						v = pNewEdge->GetLastNode();

					pPoint = theRailRoadGraph.GetEdge( v, *iter )->CreateLastEdgePoint();
				}
			}

			++iter;
		}
	}

	if ( !bFinished )
	{
		IEdge *pEdge = pPoint->GetEdge();
		CPtr<CEdgePoint> pFirstPoint;// = pSmoothPath->GetBackWheelPoint( n );
		if ( pSmoothPath->GetBackWheelPoint( n )->GetEdge() != pEdge )
		{
			pFirstPoint = new CEdgePoint( *(pSmoothPath->GetBackWheelPoint( n )) );
			pFirstPoint->Reverse( pEdge );
		}
		else
			pFirstPoint = pSmoothPath->GetBackWheelPoint( n );
			

		pNewBackWheel = pEdge->MakeIndent( vPointToMeasureDist, pPoint, pFirstPoint, fDist );
	}

	pSmoothPath->SetNewBackWheel( n, pNewBackWheel );

	if ( n < intermNodes.size() )
	{
		for ( std::list<int>::iterator forwardIterator = nodesInside[n].begin(); forwardIterator != iter.base(); ++forwardIterator )
		{
			intermNodes[n].push_back( *forwardIterator );
		}

		nodesInside[n].erase( nodesInside[n].begin(), iter.base() );
	}
}
void CTrainPathUnit::PushNodesToFrontCarriage( std::list<int> &newNodes )
{
	nodesInside[0].splice( nodesInside[0].end(), newNodes );
}
void CTrainPathUnit::GetTrainNodes( std::list<int> *pNodesOfTrain )
{
	pNodesOfTrain->clear();

	for ( int n = 0; n < nodesInside.size(); ++n )
	{
		for ( std::list<int>::reverse_iterator iter = nodesInside[n].rbegin(); iter != nodesInside[n].rend(); ++iter )
			pNodesOfTrain->push_back( *iter );

		if ( n < nodesInside.size() - 1 )
		{
			for ( std::list<int>::reverse_iterator iter = intermNodes[n].rbegin(); iter != intermNodes[n].rend(); ++iter )
				pNodesOfTrain->push_back( *iter );
		}
	}
}
bool CTrainPathUnit::IsInOneTrain( IBasePathUnit *pUnit ) const
{
	if ( !pUnit->IsTrain() )
		return false;
	else
	{
		NI_ASSERT_T( dynamic_cast<CTrainPathUnit*>(pUnit) != 0, "Wrong type of path unit" );
		return 
			this == static_cast<CTrainPathUnit*>(pUnit)->GetTrainOwner();
	}
}
void CTrainPathUnit::LocomotiveDead()
{
	bCanMove = false;
}
void CTrainPathUnit::CarriageTrackDamaged( const int nOwnerID, const bool bTrackDamagedState )
{
	if ( bTrackDamagedState )
		damagedTrackCarriages.insert( nOwnerID );
	else
		damagedTrackCarriages.erase( nOwnerID );
}
bool CTrainPathUnit::CanMove() const
{
	return damagedTrackCarriages.size() == 0 && bCanMove;
}
bool CTrainPathUnit::CanMovePathfinding() const
{
	return CanMove();
}
BASIC_REGISTER_CLASS( CCarriagePathUnit );
void CCarriagePathUnit::Init( class CAIUnit *pOwner, const CVec2 &center, const int z, const WORD dir, const WORD id )
{
	CPathUnit::Init( pOwner, center, z, dir, id );

	vOldDir = GetDirVector();
	vOldCenter = GetCenter();
}
ISmoothPath* CCarriagePathUnit::GetSmoothPath() const
{
	if ( pTrain )
		return pTrain->GetSmoothPath();
	else
		return 0;
}
ISmoothPath* CCarriagePathUnit::GetCurPath() const
{
	if ( pTrain )
		return pTrain->GetCurPath();
	else
		return 0;
}
IStaticPath* CCarriagePathUnit::CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, IPointChecking *pPointChecking )
{
	if ( pTrain )
		return pTrain->CreateBigStaticPath( vStartPoint, vFinishPoint, pPointChecking );
	else
		return 0;
}
bool CCarriagePathUnit::SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn )
{
	stayTime = 0;
	if ( pTrain )
		pTrain->SendAlongPath( pStaticPath, vShift, bSmoothTurn );
	
	CalculateIdle();

	return true;
}
bool CCarriagePathUnit::SendAlongPath( IPath *pPath )
{
	stayTime = 0;
	if ( pTrain )
		pTrain->SendAlongPath( pPath );

	CalculateIdle();

	return true;
}
void CCarriagePathUnit::SetOnRailroad()
{
	pTrain = new CTrainPathUnit( GetOwner() );
	pTrain->AddCarriage( this );
	if ( !pTrain->InitBy( this ) )
	{
		pTrain = 0;
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, "Can't put train on railroad", 0xffff0000, true );
	}
	else
	{
		vOldDir = GetDirVector();
		vOldCenter = GetCenter();
	}
}
void CCarriagePathUnit::HookTo( CCarriagePathUnit *pUnit )
{
	pTrain = pUnit->pTrain;
	if ( pTrain )
	{
		CEdgePoint *pCarrierBackWheelPoint = pUnit->GetBackWheelPoint();

		if ( pCarrierBackWheelPoint != 0 )
		{
			const SMechUnitRPGStats *pCarrierStats = static_cast<const SMechUnitRPGStats*>( pUnit->GetStats() );
			const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>(GetStats());

			const float fLen = GetDistanceForWheels(
														pCarrierStats->vBackWheel.y, pCarrierStats->vTowPoint.y,
														pStats->vFrontWheel.y, pStats->vHookPoint.y );

			CPtr<CEdgePoint> pFrontWheelPoint = theRailRoadGraph.MakeIndent( -pUnit->GetFrontDirVec(), pCarrierBackWheelPoint, fLen );

			CPtr<CEdgePoint> pBackWheelPoint;
			if ( pFrontWheelPoint )
				pBackWheelPoint = theRailRoadGraph.MakeIndent( -GetFrontDirVec(), pFrontWheelPoint, GetDistanceBetweenWheels() );
			
			if ( pFrontWheelPoint == 0 || pBackWheelPoint == 0 )
			{
				GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, "Can't hook a carriage", 0xffff0000 );
				pTrain = 0;
			}
			else
			{
				pTrain->AddCarriage( this );

				SetPlacementByWheels( pFrontWheelPoint, pBackWheelPoint );

				vOldDir = GetDirVector();
				vOldCenter = GetCenter();
			}
		}
	}
}
void CCarriagePathUnit::InitCenterAndDir3D( const CVec2 &vCenter, CVec3 *pvCenter3D, CVec3 *pvDir3D ) const
{
	const CVec3 vNormale = DWORDToVec3( GetOwner()->GetNormale( vCenter ) );
	const CVec2 vDir = GetVectorByDirection( GetFrontDir() );
	pvDir3D->x = vDir.x;
	pvDir3D->y = vDir.y;
	pvDir3D->z = ( -pvDir3D->x * vNormale.x - pvDir3D->y * vNormale.y ) / vNormale.z;
	Normalize( pvDir3D );

	*pvCenter3D =  CVec3( vCenter.x, vCenter.y, theStaticMap.GetVisZ( vCenter.x, vCenter.y ) );
}
const CVec3 CCarriagePathUnit::Get3DPointOfUnit( const CVec2 &vCenter, const float fLength ) const
{
	CVec3 vCenter3D, vDir3D;
	InitCenterAndDir3D( vCenter, &vCenter3D, &vDir3D );

	return vCenter3D + vDir3D * fLength;
}
const CVec2 CCarriagePathUnit::Get2DPointOfUnit( const CVec2 &vCenter, const float fLength ) const
{
	CVec3 v3DPointOfUnit( Get3DPointOfUnit( vCenter, fLength ) );
	return CVec2( v3DPointOfUnit.x, v3DPointOfUnit.y );
}
const CVec3 CCarriagePathUnit::GetBackHookPoint3D() const
{
	return Get3DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vTowPoint.y );
}
const CVec2 CCarriagePathUnit::GetBackHookPoint2D() const
{
	return Get2DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vTowPoint.y );	
}
const CVec3 CCarriagePathUnit::GetFrontHookPoint3D() const
{
	return Get3DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vHookPoint.y );	
}
const CVec2 CCarriagePathUnit::GetFronHookPoint2D() const
{
	return Get2DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vHookPoint.y );		
}
const CVec3 CCarriagePathUnit::GetFrontWheel3D() const
{
	return Get3DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vFrontWheel.y );	
}
const CVec2 CCarriagePathUnit::GetFrontWheel2D() const
{
	return Get2DPointOfUnit( GetCenter(), static_cast<const SMechUnitRPGStats*>( GetStats() )->vFrontWheel.y );	
}
const CVec3 CCarriagePathUnit::GetBackHookPoint3DByFrontPoint( const CVec2 &vFrontPoint ) const
{
	const float fFrontHookLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vHookPoint.y;
	const float fBackHookLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vTowPoint.y;
	
	return Get3DPointOfUnit( vFrontPoint, -fFrontHookLength + fBackHookLength );
}
const CVec2 CCarriagePathUnit::GetBackHookPoint2DByFrontPoint( const CVec2 &vFrontPoint ) const
{
	const float fFrontHookLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vHookPoint.y;
	const float fBackHookLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vTowPoint.y;
	
	return Get2DPointOfUnit( vFrontPoint, -fFrontHookLength + fBackHookLength );
}
const CVec3 CCarriagePathUnit::GetBackWheelPoint3DByFrontPoint( const CVec2 &vFrontPoint ) const
{
	const float fFrontWheelLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vFrontWheel.y;
	const float fBackWheelLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vBackWheel.y;

	return Get3DPointOfUnit( vFrontPoint, -fFrontWheelLength + fBackWheelLength );
}
const CVec2 CCarriagePathUnit::GetBackWheelPoint2DByFrontPoint( const CVec2 &vFrontPoint ) const
{
	const float fFrontWheelLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vFrontWheel.y;
	const float fBackWheelLength = static_cast<const SMechUnitRPGStats*>( GetStats() )->vBackWheel.y;

	return Get2DPointOfUnit( vFrontPoint, -fFrontWheelLength + fBackWheelLength );
}
const float CCarriagePathUnit::GetDistanceToBackWheel() const
{
	return fabs( static_cast<const SMechUnitRPGStats*>( GetStats() )->vBackWheel.y );
}
const float CCarriagePathUnit::GetDistanceBetweenWheels() const
{
	return
		fabs( static_cast<const SMechUnitRPGStats*>( GetStats() )->vBackWheel.y ) +
		fabs( static_cast<const SMechUnitRPGStats*>( GetStats() )->vFrontWheel.y );
}
void CCarriagePathUnit::SetPlacementByWheels( CEdgePoint *_pFrontWheelPoint, CEdgePoint *_pBackWheelPoint )
{
	pFrontWheelPoint = _pFrontWheelPoint;
	pBackWheelPoint = _pBackWheelPoint;

	const CVec2 vFrontWheelPoint = pFrontWheelPoint->Get2DPoint();
	const CVec2 vBackWheelPoint = pBackWheelPoint->Get2DPoint();
	
	const CVec2 vNewDir = vFrontWheelPoint - vBackWheelPoint;
	
	if ( vNewDir != VNULL2 )
	{
		SetNewCoordinates( CVec3( vBackWheelPoint + vNewDir * GetDistanceToBackWheel() / GetDistanceBetweenWheels(), 0 ), false );
		
		if ( GetRightDir() )
			UpdateDirection( vNewDir );
		else
			UpdateDirection( -vNewDir );
	}
}
void CCarriagePathUnit::FirstSegment()
{
	vOldDir = GetVectorByDirection( GetFrontDir() );
	vOldCenter = GetCenter();
	
	CPathUnit::FirstSegment();
}
void CCarriagePathUnit::SecondSegment( const bool bUpdate )
{
	if ( pTrain )
	{
		pTrain->SecondSegment( bUpdate );
		SetSpeed( pTrain->GetSpeed() );
		
		if ( vOldCenter != GetCenter() || vOldDir != GetFrontDirVec() )
			stayTime = 0;
		else
			stayTime += SConsts::AI_SEGMENT_DURATION;

		if ( pTrain->IsIdle() )
			LockTiles();

		CalculateIdle();
	}
}
CEdgePoint* CCarriagePathUnit::GetFrontWheelPoint() const
{
	return pFrontWheelPoint;
}
CEdgePoint* CCarriagePathUnit::GetBackWheelPoint() const
{
	return pBackWheelPoint;
}
void CCarriagePathUnit::GetPlacement( SAINotifyPlacement *pPlacement, const NTimer::STime timeDiff ) const 
{ 
	CVec2 vCenter = vOldCenter + ( GetCenter() - vOldCenter ) * ( 1.0f - float(timeDiff) / float( SConsts::AI_SEGMENT_DURATION ) );

	pPlacement->center.x = vCenter.x;
	pPlacement->center.y = vCenter.y;
	pPlacement->z = 0.0f;

	const WORD wOldDir = GetDirectionByVector( vOldDir );
	const WORD wTurn = DirsDifference( GetFrontDir(), wOldDir );
	if ( wOldDir + wTurn == GetDir() )
		pPlacement->dir = wOldDir + wTurn * ( 1.0f - float( timeDiff ) / float( SConsts::AI_SEGMENT_DURATION ) );
	else
		pPlacement->dir = wOldDir - wTurn * ( 1.0f - float( timeDiff ) / float( SConsts::AI_SEGMENT_DURATION ) );

	pPlacement->fSpeed = fabs( speed );
}
void CCarriagePathUnit::SetRightDir( bool bRightDir )
{
	if ( bRightDir != GetRightDir() )
		CPathUnit::SetRightDir( bRightDir );
}
void CCarriagePathUnit::SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit )
{
	CPathUnit::SetNewCoordinates( newCenter, bStopUnit );
}
void CCarriagePathUnit::SetNewCoordinatesForEditor( const CVec3 &newCenter )
{
	CPathUnit::SetNewCoordinates( newCenter, true );
	vOldCenter = GetCenter();
	vOldDir = GetDirVector();
}
void CCarriagePathUnit::SetCoordWOUpdate( const CVec3 &newCenter )
{
	CPathUnit::SetCoordWOUpdate( newCenter );
}
const CTrainPathUnit* CCarriagePathUnit::GetTrainOwner() const
{
	return pTrain;
}
bool CCarriagePathUnit::IsInOneTrain( IBasePathUnit *pUnit ) const
{
	if ( pTrain )
		return pUnit->IsInOneTrain( pTrain );
	else
		return false;
}
void CCarriagePathUnit::UpdateDirectionForEditor( const CVec2 &dirVec )
{
	CPathUnit::UpdateDirection( dirVec );
	vOldDir = GetVectorByDirection( GetFrontDir() );
}
bool CCarriagePathUnit::CanMove() const
{
	return pTrain && pTrain->CanMove();
}
bool CCarriagePathUnit::CanMovePathfinding() const
{
	return CanMove();
}
void CCarriagePathUnit::UnitDead()
{
	if ( pTrain && GetOwner()->GetStats()->type == RPG_TYPE_TRAIN_LOCOMOTIVE )
		pTrain->LocomotiveDead();
	if ( pTrain )
		pTrain->CarriageTrackDamaged( GetOwner()->GetID(), false );
}
void CCarriagePathUnit::TrackDamagedState( const bool bTrackDamaged )
{
	if ( pTrain )
		pTrain->CarriageTrackDamaged( GetOwner()->GetID(), bTrackDamaged );
}
