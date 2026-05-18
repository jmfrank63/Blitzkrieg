#include "stdafx.h"

#include "AIWarFog.h"
#include "WarFogTracer.h"
#include "Diplomacy.h"
#include "StaticObject.h"
#include "StaticObject.h"
#include "Cheats.h"
#include "UnitsIterators2.h"
#include "AIUnit.h"
#include "Graveyard.h"

// for testing
#include "..\Scene\Scene.h"
#include "..\Scene\Terrain.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern SCheats theCheats;
extern CGraveyard theGraveyard;
CGlobalWarFog theWarFog;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CWarFog																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CWarFog::TVisFunc CWarFog::visFuncs[2] = { &CWarFog::VisFunc0, &CWarFog::VisFunc1 };
const CWarFog::TSegmTypes CWarFog::checkSegms[9][9] = 
{
//			0									1						2								3									4							5							6								7								8											
/*0*/{ &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSt, &CWarFog::CS1, &CWarFog::CSf, &CWarFog::CS1, &CWarFog::CS1 },
/*1*/{ &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CSt, &CWarFog::CS1, &CWarFog::CS2, &CWarFog::CSt, &CWarFog::CS1 },
/*2*/{ &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CSt, &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CS2, &CWarFog::CSf },
/*3*/{ &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CS2, &CWarFog::CSf, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSf, &CWarFog::CS1, &CWarFog::CS1 },
/*4*/{ &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSt },
/*5*/{ &CWarFog::CS1, &CWarFog::CS1, &CWarFog::CSf, &CWarFog::CSt, &CWarFog::CSt, &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CS2, &CWarFog::CSf },
/*6*/{ &CWarFog::CSf, &CWarFog::CS2, &CWarFog::CS2, &CWarFog::CSf, &CWarFog::CSt, &CWarFog::CS2, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf },
/*7*/{ &CWarFog::CS1, &CWarFog::CSt, &CWarFog::CS2, &CWarFog::CS1, &CWarFog::CSt, &CWarFog::CS2, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf },
/*8*/{ &CWarFog::CS1, &CWarFog::CS1, &CWarFog::CSf, &CWarFog::CS1, &CWarFog::CSt, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf, &CWarFog::CSf }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWarFog::Init( const struct SVector &upLeft, const struct SVector &downLeft, 
										const struct SVector &downRight, const struct SVector &upRight )
{
	NI_ASSERT_SLOW_TF( upLeft.x - upLeft.y == upRight.x - upRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( downLeft.x - downLeft.y == downRight.x - downRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upLeft.x + upLeft.y == downLeft.x + downLeft.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upRight.x + upRight.y == downRight.x + downRight.y, "Wrong points order", return );
	
	minSum = upLeft.x + upLeft.y; 
	maxSum = upRight.x + upRight.y;
	minDiff = upLeft.x - upLeft.y;
	maxDiff = downLeft.x - downLeft.y;
	
	NI_ASSERT_SLOW_TF( minSum <= maxSum, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( minDiff <= maxDiff, "Wrong points order", return );
	
	sizeSum = maxSum - minSum + 1;
	sizeDiff = maxDiff - minDiff + 1;

	if ( vis.size() < sizeSum*sizeDiff + 1 )
		vis.resize( sizeSum*sizeDiff + 1 );

	memset( &(vis[0]), 0, vis.size() );

	corners[0] = upLeft; 
	corners[1] = downLeft; 
	corners[2] = downRight; 
	corners[3] = upRight;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWarFog::InitVisCheck( const struct SVector &upLeft, const struct SVector &downLeft, 
														const struct SVector &downRight, const struct SVector &upRight )
{
	NI_ASSERT_SLOW_TF( upLeft.x - upLeft.y == upRight.x - upRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( downLeft.x - downLeft.y == downRight.x - downRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upLeft.x + upLeft.y == downLeft.x + downLeft.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upRight.x + upRight.y == downRight.x + downRight.y, "Wrong points order", return );
	
	minSum = upLeft.x + upLeft.y; 
	maxSum = upRight.x + upRight.y;
	minDiff = upLeft.x - upLeft.y;
	maxDiff = downLeft.x - downLeft.y;
	
	NI_ASSERT_SLOW_TF( minSum <= maxSum, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( minDiff <= maxDiff, "Wrong points order", return );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWarFog::CheckSegm( const SVector &p1, const SVector &p2, const int n ) const
{
  const int xSign1 = ( Sign( (center.x - r) - p1.x ) + Sign( ( center.x + r ) - p1.x ) ) / 2 + 1;
	const int ySign1 = ( Sign( p1.y - (center.y - r) ) + Sign( p1.y - ( center.y + r ) ) ) / 2 + 1;
  const int xSign2 = ( Sign( (center.x - r) - p2.x ) + Sign( ( center.x + r ) - p2.x ) ) / 2 + 1;
	const int ySign2 = ( Sign( p2.y - (center.y - r) ) + Sign( p2.y - ( center.y + r ) ) ) / 2 + 1;
	
	return ( this->*checkSegms[3*ySign1 + xSign1][3*ySign2 + xSign2] )( n ) ;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWarFog::IsInfluencedUnit( const struct SVector &center ) const 
{
	return 
		IsPointInside( center ) ||
		CheckSegm( corners[0], corners[1], minSum ) || CheckSegm( corners[1], corners[2], maxDiff ) ||
		CheckSegm( corners[2], corners[3], maxSum ) || CheckSegm( corners[3], corners[0], minDiff );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CWarFog::TraceToPointForScan( const SVector &center, const SVector &finishPoint )
{
	CBres bres;
	bres.InitPoint( center, finishPoint );
	int vis = SConsts::VIS_POWER;

	do
	{
		bres.MakePointStep();
		if ( !theStaticMap.IsTileInside( bres.GetDirection() ) )
			break;

		vis -= theStaticMap.GetDissipation( bres.GetDirection() );
	} while ( vis > 0  &&  bres.GetDirection() != finishPoint );

	return vis;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWarFog::DetermineRegion()
{
	if ( IsPointInside( center ) )
		isInside = true;
	else
	{
		isInside = false;
		const SLine dRays[4] = { SLine( center, corners[0] ), SLine( center, corners[1] ), 
														 SLine( center, corners[2] ), SLine( center, corners[3] ) };
		signed char signs[4];
		memset( signs, 0, sizeof( signs ) );

		for ( int i = 0; i < 4; ++i )
		{
			signs[0] += dRays[0].GetHPLineSign( corners[i] );
			signs[1] += dRays[1].GetHPLineSign( corners[i] );
			signs[2] += dRays[2].GetHPLineSign( corners[i] );
			signs[3] += dRays[3].GetHPLineSign( corners[i] );
		}

		signed char max = -10, min = 10;

		for ( int i = 0; i < 4; ++i )
		{
			if ( signs[i] > max )
			{
				plusRay = dRays[i];
				max = signs[i];
			}
			if ( signs[i] < min )
			{
				minusRay = dRays[i];
				min = signs[i];
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWarFog::AddUnit( const SFogInfo &fogInfo )
{
	if ( fogInfo.r <= 2 )
		return;

	// переменные center и r используются в IsInfluencedUnit
	center = fogInfo.center;
	r = fogInfo.r;

	if ( IsInfluencedUnit( center ) && fogInfo.wVisionAngle > 0 )
	{
		DetermineRegion();		

		if ( fogInfo.pObject.IsValid() )
			fogInfo.pObject->RemoveTransparencies();

		CWarFogTracer<CWarFog> warFogTracer( *this, center, r, fogInfo.wUnitDir, fogInfo.wVisionAngle, fogInfo.bAngleLimited, fogInfo.wMinAngle, fogInfo.wMaxAngle, fogInfo.bPlane, fogInfo.fSightPower );

		if ( fogInfo.pObject.IsValid() )
			fogInfo.pObject->SetTransparencies();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWarFog::GetVisibilities( SAIVisInfo **pVisBuffer, int *pnLen )
{
	*pVisBuffer = GetTempBuffer<SAIVisInfo>( sizeSum*sizeDiff );	
	*pnLen = 0;

	for ( int diff = ( minDiff + minDiff % 2 ) / 2; diff <= ( maxDiff - maxDiff % 2 ) / 2; ++diff )
	{
		for ( int sum = ( minSum + minSum % 2 ) / 2; sum <= ( maxSum - maxSum % 2 ) / 2; ++sum )
		{
			// делятся на 2
			if ( ( ((sum + diff) & 1) == 0 ) && ( ((sum-diff) & 1) == 0 ) )
			{
				(*pVisBuffer)[*pnLen].x = (sum + diff) / 2;
				(*pVisBuffer)[*pnLen].y = (sum - diff) / 2;
				
				(*pVisBuffer)[*pnLen].vis = 
					(
						Vis( SVector( sum + diff - 2, sum - diff - 2 ) ) + 
						Vis( SVector( sum + diff - 2, sum - diff - 1 ) ) + 
						Vis( SVector( sum + diff - 2, sum - diff     ) ) + 
						Vis( SVector( sum + diff - 2, sum - diff + 1 ) ) +

  					Vis( SVector( sum + diff - 1, sum - diff - 2 ) ) + 
						Vis( SVector( sum + diff - 1, sum - diff - 1 ) ) + 
						Vis( SVector( sum + diff - 1, sum - diff     ) ) + 
						Vis( SVector( sum + diff - 1, sum - diff + 1 ) ) +

						Vis( SVector( sum + diff    , sum - diff - 2 ) ) + 
						Vis( SVector( sum + diff    , sum - diff - 1 ) ) + 
						Vis( SVector( sum + diff    , sum - diff     ) ) + 
						Vis( SVector( sum + diff    , sum - diff + 1 ) ) +

						Vis( SVector( sum + diff + 1, sum - diff - 2 ) ) + 
						Vis( SVector( sum + diff + 1, sum - diff - 1 ) ) + 
						Vis( SVector( sum + diff + 1, sum - diff     ) ) + 
						Vis( SVector( sum + diff + 1, sum - diff + 1 ) ) 
					) / 16;

				++(*pnLen);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CGlobalWarFog													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::Init()
{
	fogCnts.resize( 2 );
	maxVis.resize( 2 );
	minCoeff2.resize( 2 );

	for ( int i = 0; i < fogCnts.size(); ++i )
	{
		fogCnts[i].SetSizes( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
		fogCnts[i].SetZero();

		maxVis[i].SetSizes( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );		
		maxVis[i].SetZero();

		minCoeff2[i].SetSizes( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
		for ( int k = 0; k < minCoeff2[i].GetSizeY(); ++k )
		{
			for ( int j = 0; j < minCoeff2[i].GetSizeX(); ++j )
				minCoeff2[i][k][j] = floatToByte( 1.0f );
		}
	}

	unitsInfo.resize( SConsts::AI_START_VECTOR_SIZE );
	newUnitsInfo.resize( SConsts::AI_START_VECTOR_SIZE );
	removedObjects4Units.resize( SConsts::AI_START_VECTOR_SIZE );
	addedObjects4Units.resize( SConsts::AI_START_VECTOR_SIZE );
	weights.Reserve( SConsts::AI_START_VECTOR_SIZE );

	nMiniMapY = 0;
	miniMapSums.resize( theStaticMap.GetSizeX() / 2 );
	memset( &(miniMapSums[0]), 0, miniMapSums.size() );

	areasOpenTiles.SetSizes( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
	areasOpenTiles.SetZero();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::Clear()
{
	unitsInfo.clear();
	newUnitsInfo.clear();
	removedObjects4Units.clear();
	addedObjects4Units.clear();
	weights.Clear();

	areasOpenTiles.Clear();
	areas.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::AddUnit( const int id, int nParty, const SFogInfo &fogInfo )
{
	if ( id >= unitsInfo.size() )
	{
		unitsInfo.resize( id * 1.5 );
		removedObjects4Units.resize( id * 1.5 );
		addedObjects4Units.resize( id * 1.5 );
		newUnitsInfo.resize( id * 1.5 );
		weights.Reserve( id * 1.5 );
	}

	newUnits.push_back( id );
	unitsInfo[id].fogInfo = fogInfo;
	unitsInfo[id].nParty = nParty;
	unitsInfo[id].nHeapPos = -2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::DeleteUnit( const int id )
{
	NI_ASSERT_SLOW_T( unitsInfo.size() > id, "out of bounds" );
	// не новый юнит
	if ( unitsInfo[id].nHeapPos != -2 )
	{
		deletedUnits.push_back( SDeletedUnitInfo() );
		deletedUnits.back().unitInfo = unitsInfo[id];
		deletedUnits.back().removedObjects = removedObjects4Units[id];
		deletedUnits.back().addedObjects = addedObjects4Units[id];

		if ( unitsInfo[id].nHeapPos >= 0 )
			weights.Erase( unitsInfo[id].nHeapPos );
		unitsInfo[id].nHeapPos = -1;

		removedObjects4Units[id].clear();
		addedObjects4Units[id].clear();
	}
	// новый, туман ещё не просчитан
	else
	{
		std::list<int>::iterator iter = newUnits.begin();
		while ( iter != newUnits.end() && (*iter) != id )
			++iter;

		NI_ASSERT_T( iter != newUnits.end(), "Wrong unit to delete" );
		newUnits.erase( iter );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CGlobalWarFog::GetWeight( const SFogInfo &oldFog, const SFogInfo &newFog )
{
	if ( ( oldFog.bAngleLimited != newFog.bAngleLimited ) || 
			 ( oldFog.values != newFog.values ) || 
			 ( oldFog.bAngleLimited && oldFog.wUnitDir != newFog.wUnitDir ) ||
			 ( newFog.wVisionAngle < 32768 ) )
		return 100;
	return SquareOfDistance( oldFog.center, newFog.center );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::UpdateUnit( const int id, SFogInfo &newFogInfo, const int weight )
{
	NI_ASSERT_SLOW_T( newUnitsInfo.size() > id, "out of bounds" );
	newUnitsInfo[id] = newFogInfo;

	if ( weight != 0 )
	{
		int &nHeapPos = unitsInfo[id].nHeapPos;
		// если лежит в куче
		if ( nHeapPos >= 0 ) 
		{
			if ( weight > weights[nHeapPos].nWeight )
			{
				weights[nHeapPos].nWeight = weight;
				weights.Increased( nHeapPos );
			}
		}
		else
		{
			nHeapPos = weights.Size();
			weights.Push( SWeightInfo( weight, id ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ChangeUnitCoord( const int id, SFogInfo &newFogInfo )
{
	// новый, туман ещё не считался
	if ( unitsInfo[id].nHeapPos == -2 )
		unitsInfo[id].fogInfo = newFogInfo;
	else
		UpdateUnit( id, newFogInfo, SquareOfDistance( unitsInfo[id].fogInfo.center, newFogInfo.center ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ChangeUnitState( const int id, SFogInfo &newFogInfo )
{
	// новый, туман ещё не считался
	if ( unitsInfo[id].nHeapPos == -2 )
		unitsInfo[id].fogInfo = newFogInfo;
	else
		UpdateUnit( id, newFogInfo, GetWeight( unitsInfo[id].fogInfo, newFogInfo ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ChangeUnitParty( const int id, const int nParty )
{
	// новый, туман ещё не считался
	if ( unitsInfo[id].nHeapPos == -2 )
		unitsInfo[id].nParty = nParty;
	else
	{
		const SFogInfo fogInfo = unitsInfo[id].fogInfo;
		DeleteUnit( id );

		AddUnit( id, nParty, fogInfo );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::RecalculateForRemovedObject( const int id, const float fDist, CExistingObject *pObject )
{
	// если туман для этого юнита уже рассчитан
	if ( unitsInfo[id].nHeapPos != -2 )
	{
		UpdateUnit( id, unitsInfo[id].fogInfo, Max( 1.0f, fDist / ( SConsts::TILE_SIZE * 5.0f ) ) );

		CObjectsList::iterator iter( std::find( addedObjects4Units[id].begin(), addedObjects4Units[id].end(), pObject ) );
		if ( iter == addedObjects4Units[id].end() )
			removedObjects4Units[id].push_back( pObject );
		else
			addedObjects4Units[id].erase( iter );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::RecalculateForAddedObject( const int id, const float fDist, CExistingObject *pObject )
{
	// если туман для этого юнита уже рассчитан
	if ( unitsInfo[id].nHeapPos != -2 )
	{
		UpdateUnit( id, unitsInfo[id].fogInfo, Max( 1.0f, fDist / ( SConsts::TILE_SIZE * 5.0f ) ) );

		CObjectsList::iterator iter( std::find( removedObjects4Units[id].begin(), removedObjects4Units[id].end(), pObject ) );
		if ( iter == removedObjects4Units[id].end() )
			addedObjects4Units[id].push_back( pObject );
		else
			removedObjects4Units[id].erase( iter );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ReclaculateFogAfterRemoveObject( CExistingObject *pObj )
{
	const CVec2 objCenter = pObj->GetCenter();
	for ( CUnitsIter<0,3> unitsIter( 0, ANY_PARTY, pObj->GetCenter(), SConsts::MAX_DIST_TO_RECALC_FOG ); !unitsIter.IsFinished(); unitsIter.Iterate() )
	{
		CAIUnit *pUnit = *unitsIter;
		if ( IsValidObj( pUnit ) )
			RecalculateForRemovedObject( pUnit->GetID(), fabs( pUnit->GetCenter() - objCenter ), pObj );
	}

	theGraveyard.UpdateFog4RemovedObject( pObj );

	for ( std::list<SDeletedUnitInfo>::iterator iter = deletedUnits.begin(); iter != deletedUnits.end(); ++iter )
		iter->removedObjects.push_back( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ReclaculateFogAfterAddObject( CExistingObject *pObj )
{
	const CVec2 vCenter( pObj->GetCenter() );
	for ( CUnitsIter<0,3> unitsIter( 0, ANY_PARTY, vCenter, SConsts::MAX_DIST_TO_RECALC_FOG ); !unitsIter.IsFinished(); unitsIter.Iterate() )
	{
		CAIUnit *pUnit = *unitsIter;
		if ( IsValidObj( pUnit ) )
			RecalculateForAddedObject( pUnit->GetID(), fabs( pUnit->GetCenter() - vCenter ), pObj );
	}

	theGraveyard.UpdateFog4AddedObject( pObj );

	for ( std::list<SDeletedUnitInfo>::iterator iter = deletedUnits.begin(); iter != deletedUnits.end(); ++iter )
		iter->addedObjects.push_back( pObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool operator < ( const SVector &a, const SVector &b )
{
	return a.x < b.x || a.x == b.x && a.y < b.y;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::RemoveUnitWarfog( SUnitInfo &unitInfo, const CObjectsList &removedObjects, const CObjectsList &addedObjects )
{
	// убрать прозрачности для тех объектов, которые были поставлены между расчётами тумана для этого юнита
	for ( CObjectsList::const_iterator iter = addedObjects.begin(); iter != addedObjects.end(); ++iter )
		(*iter)->RemoveTransparencies();

	// поставить прозрачности для тех объектов, которые были удалены между расчётами тумана для этого юнита
	for ( CObjectsList::const_iterator iter = removedObjects.begin(); iter != removedObjects.end(); ++iter )
	{
		if ( IsValidObj( *iter ) )
			(*iter)->SetTransparencies();
	}

	const SFogInfo &fogInfo = unitInfo.fogInfo;

	if ( fogInfo.pObject.IsValid() )
		fogInfo.pObject->RemoveTransparencies();
/*
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	unitInfo.deletedTiles.clear();
	CWarFogTracer<SDelWarFog> delTracer( SDelWarFog( unitInfo.nParty, &unitInfo.deletedTiles ), fogInfo );
	unitInfo.deletedTiles.sort();

	if ( !unitInfo.addedTiles.empty() )
	{
		if ( unitInfo.addedTiles.size() != unitInfo.deletedTiles.size() )
		{
			std::vector<SVector> diff( unitInfo.addedTiles.size() + unitInfo.deletedTiles.size() );
			std::set_symmetric_difference( unitInfo.addedTiles.begin(), unitInfo.addedTiles.end(), unitInfo.deletedTiles.begin(), unitInfo.deletedTiles.end(), diff.begin() );

			std::vector<SAIPassabilityInfo> info( diff.size() );
			int cnt = 0;
			for ( std::vector<SVector>::iterator iter = diff.begin(); iter != diff.end(); ++iter )
			{
				info[cnt].x = iter->x;
				info[cnt].y = iter->y;
				info[cnt].pass = 0xf;

				++cnt;
			}

			std::vector<SAIPassabilityInfo> info( unitInfo.addedTiles.size() + unitInfo.deletedTiles.size() );
			int cnt = 0;
			for ( std::list<SVector>::iterator iter = unitInfo.addedTiles.begin(); iter != unitInfo.addedTiles.end(); ++iter )
			{
				info[cnt].x = iter->x;
				info[cnt].y = iter->y;
				info[cnt].pass = 0xf;

				++cnt;
			}

			for ( std::list<SVector>::iterator iter = unitInfo.deletedTiles.begin(); iter != unitInfo.deletedTiles.end(); ++iter )
			{
				info[cnt].x = iter->x;
				info[cnt].y = iter->y;
				info[cnt].pass = 0x5;

				++cnt;
			}
			if ( cnt && !bShow )
				GetSingleton<IScene>()->GetTerrain()->SetAIMarker( &(info[0]), cnt );

			bShow =  true;
		}
//		NI_ASSERT_T( unitInfo.addedTiles.size() == unitInfo.deletedTiles.size(), "Wrong added/deleted tiles size" );
		else
		{
			std::list<SVector>::iterator addedTilesIter = unitInfo.addedTiles.begin();
			std::list<SVector>::iterator deletedTilesIter = unitInfo.deletedTiles.begin();

			while ( addedTilesIter != unitInfo.addedTiles.end() )
			{
				const SVector addedTile = *addedTilesIter;
				const SVector deletedTile = *deletedTilesIter;
				NI_ASSERT_T( addedTile == deletedTile, "Wrong added/deleted tiles" );
				++addedTilesIter;
				++deletedTilesIter;
			}

			while ( deletedTilesIter != unitInfo.deletedTiles.end() )
			{
				const SVector deletedTile = *deletedTilesIter;
				++deletedTilesIter;
			}
		}
	}
#else
*/
	CWarFogTracer<SDelWarFog> delTracer( SDelWarFog( unitInfo.nParty ), fogInfo );
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	if ( fogInfo.pObject.IsValid() )
		fogInfo.pObject->SetTransparencies();

	for ( CObjectsList::const_iterator iter = removedObjects.begin(); iter != removedObjects.end(); ++iter )
		(*iter)->RemoveTransparencies();

	for ( CObjectsList::const_iterator iter = addedObjects.begin(); iter != addedObjects.end(); ++iter )
		(*iter)->SetTransparencies();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::AddUnitWarfog( SUnitInfo &unitInfo )
{
	const SFogInfo &fogInfo = unitInfo.fogInfo;

	if ( fogInfo.pObject.IsValid() )
		fogInfo.pObject->RemoveTransparencies();
/*
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	unitInfo.addedTiles.clear();
	CWarFogTracer<SAddWarFog> addTracer( SAddWarFog( unitInfo.nParty, &unitInfo.addedTiles ), fogInfo );
	unitInfo.addedTiles.sort();
#else
*/
	CWarFogTracer<SAddWarFog> addTracer( SAddWarFog( unitInfo.nParty ), fogInfo );
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	if ( fogInfo.pObject.IsValid() )
		fogInfo.pObject->SetTransparencies();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ProcessDeletedUnits( bool bAllUnits )
{
	int cnt = 0;
	while ( ( cnt < 5 || bAllUnits ) && !deletedUnits.empty() )
	{
		if ( deletedUnits.front().unitInfo.nParty != theDipl.GetNeutralParty() )
		{
			SDeletedUnitInfo &info = deletedUnits.front();
			RemoveUnitWarfog( info.unitInfo, info.removedObjects, info.addedObjects );
		}

		deletedUnits.pop_front();
		++cnt;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ProcessNewUnits( bool bAllUnits )
{
	int cnt = 0;
	while ( ( bAllUnits || cnt < 20 ) && !newUnits.empty() )
	{
		const int id = newUnits.front();
		if ( unitsInfo[id].nParty != theDipl.GetNeutralParty() )
			AddUnitWarfog( unitsInfo[id] );

		newUnits.pop_front();
		unitsInfo[id].nHeapPos = -1;
		++cnt;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::Segment( bool bAllUnits )
{
	ProcessDeletedUnits( bAllUnits );
	ProcessNewUnits( bAllUnits );

	int cnt = 0;
	int nCalculatedUnits = 1;
	while ( ( cnt < nCalculatedUnits || bAllUnits ) && !weights.IsEmpty() )
	{
		const int id = weights.GetMaxEl().id;
		weights.Pop();

		if ( unitsInfo[id].nParty != theDipl.GetNeutralParty() )
			RemoveUnitWarfog( unitsInfo[id], removedObjects4Units[id], addedObjects4Units[id] );

		removedObjects4Units[id].clear();
		addedObjects4Units[id].clear();

		unitsInfo[id].fogInfo = newUnitsInfo[id];
		if ( unitsInfo[id].nParty != theDipl.GetNeutralParty() )
			AddUnitWarfog( unitsInfo[id] );

		unitsInfo[id].nHeapPos = -1;
		++cnt;

		if ( !weights.IsEmpty() )
		{
			if ( weights.GetMaxEl().nWeight > 410)
				nCalculatedUnits = Min( nCalculatedUnits + 1, 4 );
			else if ( weights.GetMaxEl().nWeight > 220 )
				nCalculatedUnits = Min( nCalculatedUnits + 1, 3 );
			else if ( weights.GetMaxEl().nWeight > 121 )
				nCalculatedUnits = Min( nCalculatedUnits + 1, 2 );
		}
	}
	GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "Warfog", NStr::Format( "%d", nCalculatedUnits ) );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	for ( int i = 0; i < weights.Size(); ++i )
		NI_ASSERT_T( i == unitsInfo[weights[i].id].nHeapPos, NStr::Format( "Wrong weights heap state, i = %d, nHeapPos = %d, heap size = %d\n", i, unitsInfo[weights[i].id].nHeapPos, weights.Size() ) );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalWarFog::IsTileVisible( const SVector &tile, const int nParty ) const
{ 
	if ( theCheats.GetTurnOffWarFog() )
		return true;
	if ( !theStaticMap.IsTileInside( tile ) || theDipl.GetNeutralParty() == nParty )
		return false;
	if ( !theDipl.IsNetGame() && IsOpenBySriptArea( tile ) && theDipl.GetMyParty() == nParty )
		return true;

	return fogCnts[nParty][tile.y][tile.x] != 0; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CGlobalWarFog::GetTileVis( const int tileX, const int tileY, const int nParty ) const
{ 
	if ( !theDipl.IsNetGame() && IsOpenBySriptArea( tileX, tileY ) && theDipl.GetMyParty() == nParty )
		return SConsts::VIS_POWER;
	if ( theCheats.GetTurnOffWarFog() )
		return SConsts::VIS_POWER;
	if ( !theStaticMap.IsTileInside( tileX, tileY ) || theDipl.GetNeutralParty() == nParty )
		return 0;
	else
		return maxVis[nParty].GetData( tileX, tileY ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CGlobalWarFog::GetClientTileVis( const int tileX, const int tileY, const int nParty ) const
{
	if ( theStaticMap.IsTileInside( tileX, tileY ) && areasOpenTiles.GetData( tileX, tileY ) > 0 )
		return SConsts::VIS_POWER;
	else if ( !theCheats.IsHistoryPlaying() )
		return CGlobalWarFog::GetTileVis( tileX, tileY, nParty );
	else
		return Max( GetTileVis( tileX, tileY, 0 ), GetTileVis( tileX, tileY, 1 ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::GetMiniMapInfo( BYTE **pVisBuffer, int *pnLen, const int nParty, bool bFirstTime )
{
	int nShift;
	if ( bFirstTime )
		nShift = theStaticMap.GetSizeY() / 2;
	else
		nShift = theStaticMap.GetSizeY() / 2 / ( 1000 / SConsts::AI_SEGMENT_DURATION ) + 1;

	const int nUpY = Min( nMiniMapY + nShift, theStaticMap.GetSizeY() / 2 );

	*pnLen = ( nUpY - nMiniMapY ) * theStaticMap.GetSizeX() / 2;
	*pVisBuffer = GetTempBuffer<BYTE>( *pnLen );

	int cnt = 0;
	for ( int i = nMiniMapY; i < nUpY; ++i )
	{
		if ( i == 0 )
		{
			miniMapSums[0] = GetClientTileVis( 0, 0, nParty ) + GetClientTileVis( 1, 0, nParty ) +
											 GetClientTileVis( 0, 1, nParty ) + GetClientTileVis( 1, 1, nParty );
			(*pVisBuffer)[cnt++] = miniMapSums[0] / 16;

			for ( int j = 1; j < theStaticMap.GetSizeX() / 2; ++j )
			{
				miniMapSums[j] = GetClientTileVis( 2 * j, 0, nParty ) + GetClientTileVis( 2 * j + 1, 0, nParty ) +
												 GetClientTileVis( 2 * j, 1, nParty ) + GetClientTileVis( 2 * j + 1, 1, nParty );
				(*pVisBuffer)[cnt++] = ( miniMapSums[j] + miniMapSums[j - 1] ) / 16;
			}
		}
		else 
		{
			int cornerSum;
			const int newSum = GetClientTileVis( 0, 2 * i		 , nParty )	+ GetClientTileVis( 1, 2 * i    , nParty ) +
												 GetClientTileVis( 0, 2 * i + 1, nParty ) + GetClientTileVis( 1, 2 * i + 1, nParty );

			(*pVisBuffer)[cnt++] = ( miniMapSums[0] + newSum ) / 16;

			cornerSum = miniMapSums[0];
			miniMapSums[0] = newSum;

			for ( int j = 1; j < theStaticMap.GetSizeX() / 2; ++j )
			{
				const int newSum = GetClientTileVis( 2 * j, 2 * i		 , nParty ) + GetClientTileVis( 2 * j + 1, 2 * i		, nParty ) +
								 					 GetClientTileVis( 2 * j, 2 * i + 1, nParty ) + GetClientTileVis( 2 * j + 1, 2 * i + 1, nParty );

				(*pVisBuffer)[cnt++] = ( cornerSum + miniMapSums[j] + miniMapSums[j-1] + newSum ) / 16;

				cornerSum = miniMapSums[j];
				miniMapSums[j] = newSum;
			}
		}
	}

	nMiniMapY = nUpY;
	if ( nMiniMapY == theStaticMap.GetSizeY() / 2 )
		nMiniMapY = 0;

	NI_ASSERT_T( nMiniMapY < theStaticMap.GetSizeY() / 2, "Wrong nMiniMapY" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalWarFog::IsUnitVisible( const int nParty, const SVector &tile, bool bCamouflated, const float fCamouflage ) const
{
	if ( theCheats.GetTurnOffWarFog() )
		return true;
	if ( !IsTileVisible( tile, nParty ) )
		return false;
	if ( !bCamouflated )
		return true;
	
	if ( !theDipl.IsNetGame() && IsOpenBySriptArea( tile ) && theDipl.GetMyParty() == nParty )
		return true;

	return GetMinCoeff2( tile, nParty ) <= sqr( fCamouflage * (float)GetTileVis( tile, nParty ) / float( SConsts::VIS_POWER ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalWarFog::ToggleOpen4ScriptAreaTiles( const SScriptArea &scriptArea, bool bOpen )
{
	if ( !(bOpen ^ (areas.find( scriptArea.szName ) == areas.end())) )
	{
		if ( bOpen )
			areas.insert( scriptArea.szName );
		else
			areas.erase( scriptArea.szName );

		SFogInfo fogInfo;
		fogInfo.values = 0;
		fogInfo.bAngleLimited = false;
		fogInfo.bPlane = true;
		fogInfo.center = AICellsTiles::GetTile( scriptArea.center );
		fogInfo.fSightPower = 0;
		fogInfo.wUnitDir = 0;
		fogInfo.wVisionAngle = 32768;
		fogInfo.r = scriptArea.fR / SConsts::TILE_SIZE;

		CWarFogTracer<SScriptAreaFog> tracer( SScriptAreaFog( bOpen ), fogInfo );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalWarFog::IsOpenBySriptArea( const int x, const int y ) const
{
	return theStaticMap.IsTileInside( x, y ) && areasOpenTiles.GetData( x, y ) > 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
