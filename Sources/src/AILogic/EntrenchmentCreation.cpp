#include "StdAfx.h"

#include "EntrenchmentCreation.h"
#include "StaticObjects.h"
#include "AIStaticMap.h"
#include "Entrenchment.h"
#include "Fence.h"
#include "ObstacleInternal.h"
#include "UnitsIterators2.h"
#include "AIUnit.h"
#include "UnitStates.h"
#include "Bridge.h"
#include "PathFinder.h"
#include "Path.h"
#include "UnitCreation.h"
#include "AIStaticMap.h"
#include "Trigonometry.h"
#include "StaticObjectsIters.h"

#include "..\Formats\fmtTerrain.h"
#include "..\Formats\fmtMap.h"

#include "MPLog.h"
BASIC_REGISTER_CLASS(CEntrenchmentCreation);
BASIC_REGISTER_CLASS(CFenceCreation);
BASIC_REGISTER_CLASS(CLongObjectCreation);
BASIC_REGISTER_CLASS(CBridgeCreation);
extern CStaticMap theStaticMap;
const static float fNearToNormale = 0.05f; 
extern CStaticObjects theStatObjs;
extern CStaticMap theStaticMap;
extern CUnitCreation theUnitCreation;
int CLongObjectCreation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fWorkAccumulated );
	return 0;
}
int CEntrenchmentCreation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		InitConsts();
	}

	saver.Add( 1, &pFullEntrenchment );
	saver.Add( 2, &parts );
	saver.Add( 3, &pBeginTerminator );
	saver.Add( 4, &pEndTerminator );
	saver.Add( 5, &pNewEndTerminator );
	saver.Add( 6, &vPoints );
	saver.Add( 7, &nCurIndex );
	saver.Add( 8, &wAngle );
	saver.Add( 9, &nPlayer );
	saver.Add( 10, &tilesUnder );
	saver.Add( 11, &line );
	saver.Add( 12, &bCannot );
	saver.Add( 13, &bSayAck );
	saver.AddTypedSuper( 14, static_cast<CLongObjectCreation*>( this ) );
	return 0;
}
CEntrenchmentCreation::CEntrenchmentCreation( const int nPlayer )
: nPlayer( nPlayer )
{
	InitConsts();
}
void CEntrenchmentCreation::InitConsts()
{
}
bool CEntrenchmentCreation::SearchTrenches( const CVec2 &vCenter, const SRect &rectToTest )
{
	const float fMaxSize = Max( rectToTest.lengthAhead, Max(rectToTest.lengthBack, rectToTest.width ) ) + 2 * SConsts::TILE_SIZE;
	for ( CStObjCircleIter<false> iter( vCenter, fMaxSize ); !iter.IsFinished(); iter.Iterate() )
	{
		CStaticObject *pObj = *iter;
		if ( ESOT_ENTR_PART == pObj->GetObjectType() )
		{
			SRect objRect;
			pObj->GetBoundRect( &objRect );

			if ( rectToTest.IsIntersected( objRect ) )
				return true;
		}
	}
	return false;
}
bool CEntrenchmentCreation::PreCreate( const CVec2 &vFrom, const CVec2 &vTo )
{
	bCannot = false;
	bSayAck = false;
	nCurIndex = 0;
	line = CLine2( vFrom, vTo );
	NI_ASSERT_T( vPoints.empty() && parts.empty(), "repeative calls are not allowed" );

	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	CGDBPtr<SGDBObjectDesc> pDesc = pGDB->GetDesc( theUnitCreation.GetEntrenchmentName() );
  CGDBPtr<SEntrenchmentRPGStats> pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	int dbID = pGDB->GetIndex( theUnitCreation.GetEntrenchmentName() );
	int nRandomInd = Random( 0, 0xffff );
  int nTermInd = pRPG->GetTerminatorIndex( &nRandomInd );

	const float fTrenchWidth = GetTrenchWidth( 0 );
	SplitLineToSegrments( &vPoints, vFrom, vTo, fTrenchWidth );

	wAngle = GetLineAngle( vFrom, vTo );
	
	if ( vPoints.size() <= 1 ) return false;

	bool switcher=false;
	int nFrameIndex;
	for ( int i = 0; i < vPoints.size() - 1; ++i )
	{
		switcher = ((i+1)%3)==0;
		int nRandom = Random( 0, 0xffff );
		nFrameIndex = switcher ? pRPG->GetFirePlaceIndex( &nRandom ) : pRPG->GetLineIndex( &nRandom );
		const CVec2	pt = ( vPoints[i] + vPoints[i + 1] ) / 2.0f;
		if ( CanDig( pRPG, dbID, pt, wAngle, nFrameIndex ) )
			parts.push_back( AddElement( pRPG, dbID, pt, wAngle, nFrameIndex ) );
		else
		{
			bSayAck = true;
			break;
		}
	}
	if ( parts.empty() ) return false;

	if ( !CanDig( pRPG, dbID, vPoints[0], wAngle+ 65535/2, nTermInd ) ) return false;
	pBeginTerminator = AddElement( pRPG, dbID, vPoints[0], wAngle+ 65535/2, nTermInd );
	CreateNewEndTerminator();
	CalcTilesUnder();
	return true;
}
const int CEntrenchmentCreation::GetMaxIndex() const
{
	return parts.size();
}
const int CEntrenchmentCreation::GetCurIndex() const
{
	return nCurIndex;
}
const CVec2 CEntrenchmentCreation::GetNextPoint( const int nPlace, const int nMaxPlace ) const
{
	NI_ASSERT_T( nMaxPlace, "builders number = 0" );

	const CVec2 vDir = GetVectorByDirection( wAngle + 65535/4 );
	SRect rect;
	parts[nCurIndex]->GetBoundRect( &rect );
	return parts[nCurIndex]->GetCenter() + vDir * ( rect.lengthAhead+rect.lengthBack) * (nPlace -nMaxPlace/2) / nMaxPlace;

}
void CEntrenchmentCreation::BuildNext()
{
	CLongObjectCreation::BuildNext();
	if ( 0 == nCurIndex )
		theStatObjs.AddEntrencmentPart( pBeginTerminator, true, false );
	theStatObjs.AddEntrencmentPart( parts[nCurIndex], true, false );
	
	++nCurIndex;
	
	if ( pFullEntrenchment )
	{
		theStatObjs.DeleteInternalEntrenchmentInfo( pFullEntrenchment );
		pFullEntrenchment = 0;
	}
	if ( pEndTerminator )
	{
		theStatObjs.DeleteInternalObjectInfo( pEndTerminator );
		pEndTerminator = 0;
	}
	

	std::vector<IRefCount*> vEntr;
	
	for ( int i = 0; i < nCurIndex; ++i )
		vEntr.push_back( parts[i] );
	
	vEntr.push_back( pBeginTerminator );
	pEndTerminator = pNewEndTerminator;
	CreateNewEndTerminator();
	vEntr.push_back( pEndTerminator );
	pFullEntrenchment = static_cast<CEntrenchment*>(theStatObjs.AddNewEntrencment( &vEntr[0], vEntr.size(), new CFullEntrenchment(), false ));
	if ( GetCurIndex() < GetMaxIndex() )
		CalcTilesUnder();
}
void CEntrenchmentCreation::CalcTilesUnder()
{
	CTilesSet tilesUnder2, tilesUnder3;
	parts[nCurIndex]->GetCoveredTiles( &tilesUnder );
	pNewEndTerminator->GetCoveredTiles( &tilesUnder2 );
	if ( 0 == nCurIndex )
		pBeginTerminator->GetCoveredTiles( &tilesUnder3 );
	tilesUnder.splice( tilesUnder.begin(), tilesUnder2 );
	tilesUnder.splice( tilesUnder.begin(), tilesUnder3 );
}
void CEntrenchmentCreation::CreateNewEndTerminator()
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	CGDBPtr<SGDBObjectDesc> pDesc = pGDB->GetDesc( theUnitCreation.GetEntrenchmentName() );
  CGDBPtr<SEntrenchmentRPGStats> pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	int dbID = pGDB->GetIndex( theUnitCreation.GetEntrenchmentName() );
	int nRandom = Random( 0, 0xffff );
  int nTermInd = pRPG->GetTerminatorIndex( &nRandom );

	if ( nCurIndex + 1 < vPoints.size() )
		pNewEndTerminator = AddElement( pRPG, dbID, vPoints[nCurIndex+1], wAngle, nTermInd );
	else 
		pNewEndTerminator = 0;
}
void CEntrenchmentCreation::GetUnitsPreventing( std::list< CPtr<CAIUnit> > *units )
{
	SRect r1, r2, r3 ;
	parts[nCurIndex]->GetBoundRect( &r1 );
	pNewEndTerminator->GetBoundRect( &r2 );
	if ( 0 == nCurIndex ) 
		pBeginTerminator->GetBoundRect( &r3 );

	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + r2.lengthAhead + r2.lengthBack + r2.width + SConsts::TILE_SIZE * 5;
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		if ( !(*iter)->GetStats()->IsInfantry() )
		{
			const SRect rect = (*iter)->GetUnitRect();
			if ( r1.IsIntersected( rect ) || r2.IsIntersected( rect ) || 
				 ( 0 == nCurIndex ? r3.IsIntersected( rect ) : false ) 
				 )
				units->push_back( *iter );
		}
	}
}
bool CEntrenchmentCreation::IsAnyUnitPrevent() const
{
	SRect r1, r2, r3 ;
	parts[nCurIndex]->GetBoundRect( &r1 );
	pNewEndTerminator->GetBoundRect( &r2 );
	if ( 0 == nCurIndex ) 
		pBeginTerminator->GetBoundRect( &r3 );

	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + r2.lengthAhead + r2.lengthBack + r2.width + SConsts::TILE_SIZE * 5;
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		if ( !(*iter)->GetStats()->IsInfantry() )
		{
			const SRect rect = (*iter)->GetUnitRect();
			if ( r1.IsIntersected( rect ) || r2.IsIntersected( rect ) || 
					 ( 0 == nCurIndex ? r3.IsIntersected( rect ) : false ) 
				)
				return true;
		}
	}
	return false;
}
bool CEntrenchmentCreation::CanBuildNext() const
{
	if ( bCannot || !pNewEndTerminator ) return false;

	SRect r1, r2, r3 ;
	parts[nCurIndex]->GetBoundRect( &r1 );
	pNewEndTerminator->GetBoundRect( &r2 );
	if ( 0 == nCurIndex ) 
		pBeginTerminator->GetBoundRect( &r3 );

	if ( !theStaticMap.IsRectInside( r1 )  || !theStaticMap.IsRectInside( r2 ) ) return false;

	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + r2.lengthAhead + r2.lengthBack + r2.width + SConsts::TILE_SIZE * 5;
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit * pUnit = *iter;
		if ( !pUnit->GetStats()->IsInfantry() )
		{
			SRect rect = pUnit->GetUnitRect();
			rect.InitRect( rect.center, rect.dir, rect.lengthAhead + SConsts::TILE_SIZE, rect.lengthBack + SConsts::TILE_SIZE, rect.width + SConsts::TILE_SIZE );
			
			if ( r1.IsIntersected( rect ) || r2.IsIntersected( rect ) || 
					 ( 0 == nCurIndex ? r3.IsIntersected( rect ) : false ) 
				 )
			{
				if ( pUnit->GetPlayer() == nPlayer && 
						 EUSN_REST == pUnit->GetState()->GetName() &&
						 pUnit->CanMove() )
					(*iter)->UnlockTiles();
			}
		}
	}
	
	for ( CTilesSet::const_iterator it = tilesUnder.begin(); it != tilesUnder.end(); ++it )
	{
		if ( 0 != theStaticMap.GetTileLockInfo( *it ) )
			return false;
	}
	return true;
}
float CEntrenchmentCreation::GetPrice()
{
	return SConsts::ENTRENCHMENT_SEGMENT_RU_PRICE;
}
void CEntrenchmentCreation::LockNext()
{
	parts[nCurIndex]->LockTiles();
	pNewEndTerminator->LockTiles();
	if ( 0 == nCurIndex )
		pBeginTerminator->LockTiles();
}
CEntrenchmentPart * CEntrenchmentCreation::AddElement( const SEntrenchmentRPGStats *pRPG, int dbID, const CVec2 &pt, WORD angle, int nFrameIndex )
{
	CEntrenchmentPart *ptr = new CEntrenchmentPart( pRPG, pt, angle, nFrameIndex, dbID, pRPG->fMaxHP );
	ptr->Mem2UniqueIdObjs();
	ptr->Init();
	return ptr;
}
bool CEntrenchmentCreation::CanDig( const SEntrenchmentRPGStats *pRPG, int dbID, const CVec2 &pt, WORD angle, int nFrameIndex )
{
	const SRect rect = CEntrenchmentPart::CalcBoundRect( pt, angle, pRPG->segments[nFrameIndex] );
	if ( !theStaticMap.IsRectInside( rect ) ) return false;

	bool bPossible = true;
	
	const CVec3 vNormal =  DWORDToVec3( theStaticMap.GetNormal( pt ) );
	if ( fabs(vNormal.x) > fNearToNormale * fabs(vNormal.z) &&
			fabs(vNormal.y) > fNearToNormale * fabs(vNormal.z) ) return false;

	if ( !SearchTrenches( pt, rect ) ) 
	{
		CTilesSet tiles;
		GetTilesCoveredByRect( rect, &tiles );
		for ( CTilesSet::iterator i = tiles.begin(); i!= tiles.end(); ++i )
		{
			if(	!theStaticMap.IsTileInside( *i ) || !theStaticMap.CanDigEntrenchment( i->x, i->y ) )
			{
				bPossible = false;
				break;
			}
		}
	}
	else
	{
		bPossible = false;
	}
	return bPossible;
}
WORD CEntrenchmentCreation::GetLineAngle( const CVec2 &vBegin, const CVec2 &vEnd ) const
{
	CVec2 vTmp = vEnd - vBegin;  
	Normalize( &vTmp );
	float fAngle = NTrg::ACos( ( vTmp.x )/ fabs( vTmp.x, vTmp.y ) );
	if ( vTmp.y < 0 )
		fAngle = FP_2PI - fAngle;
	return WORD ( (fAngle/( 2.0f * PI ) ) * 65535 );
}

float CEntrenchmentCreation::GetTrenchWidth( int nType )// 0 - секция , 1 - поворот
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( "Entrenchment" );
	const SEntrenchmentRPGStats *pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	int nFrameIndex = 0;
	int nRandom = Random( 0, 0xffff );
	if ( nType == 0 )
		nFrameIndex = pRPG->GetLineIndex( &nRandom );
	if ( nType == 1 )
		nFrameIndex = pRPG->GetArcIndex( &nRandom );

	return pRPG->segments[nFrameIndex].vAABBHalfSize.x * 2;
}
void CEntrenchmentCreation::SplitLineToSegrments( std::vector<CVec2> *_vPoints, CVec2 vBegin, CVec2 vEnd, float TRENCHWIDTH )
{
	CVec2 currentPoint = vBegin;
	std::vector<CVec2> &vPoints = *_vPoints; 

	if ( vBegin == vEnd )
		return;
	
	float angle = (GetLineAngle( vBegin, vEnd ))*2.0f*PI/65535;
	float allLenght = fabs( vEnd.x - vBegin.x, vEnd.y - vBegin.y );
	CVec2 vAddSegment;
	vAddSegment.x = NTrg::Cos( angle ) * TRENCHWIDTH ;
	vAddSegment.y = NTrg::Sin( angle ) * TRENCHWIDTH ;
	vAddSegment.x += currentPoint.x;
	vAddSegment.y += currentPoint.y;
	if ( fabs( vAddSegment.x - currentPoint.x, vAddSegment.y - currentPoint.y ) < allLenght )
	{
	 	vPoints.push_back( CVec2( currentPoint.x, currentPoint.y ) );
	}

	while ( fabs( vBegin.x - vAddSegment.x, vBegin.y - vAddSegment.y ) < allLenght )
	{
		vPoints.push_back( CVec2( vAddSegment.x, vAddSegment.y ) );
		currentPoint = CVec2( vAddSegment.x, vAddSegment.y );
		vAddSegment.x = NTrg::Cos( angle ) * TRENCHWIDTH ;
		vAddSegment.y = NTrg::Sin( angle ) * TRENCHWIDTH ;
		vAddSegment.x += currentPoint.x;
		vAddSegment.y += currentPoint.y;
	}	
}
int CFenceCreation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
		InitConsts();
	
	saver.Add( 1, &fenceSegements );
	saver.Add( 2, &vPoints );
	saver.Add( 3, &nCurIndex );
	saver.Add( 4, &nPlayer );
	saver.Add( 5, &tilesUnder );
	saver.Add( 6, &isXConst );
	saver.Add( 7, &line );
	saver.Add( 8, &bCannot );
	saver.Add( 9, &bSayAck );
	saver.AddTypedSuper( 14, static_cast<CLongObjectCreation*>( this ) );
	return 0;
}
void CFenceCreation::InitConsts()
{
}
float CFenceCreation::GetPrice()
{
	return SConsts::FENCE_SEGMENT_RU_PRICE;
}
CFenceCreation::CFenceCreation( const int nPlayer ) 
: nPlayer( nPlayer )
{
	InitConsts();
}
void CFenceCreation::LockNext()
{
}
void CFenceCreation::BuildNext()
{
	CLongObjectCreation::BuildNext();
	theStatObjs.AddStaticObject( fenceSegements[nCurIndex], false, false );
	++nCurIndex;
}
bool CFenceCreation::PreCreate( const CVec2 &_vFrom, const CVec2 &_vTo )
{
	bCannot = false;
	bSayAck = false;
	nCurIndex = 0;
	line = CLine2( _vFrom, _vTo );
	const SVector vFrom( _vFrom / SConsts::TILE_SIZE );
	const SVector vTo( _vTo / SConsts::TILE_SIZE );

	APointHelper hlpFence;
	MakeLine2( vFrom.x, vFrom.y, vTo.x, vTo.y, hlpFence );

	isXConst = false;
	if ( vFrom.x == vTo.x ) isXConst = true;
	
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();

	const SGDBObjectDesc *pDesc = pGDB->GetDesc( theUnitCreation.GetWireFenceName() );
	const SFenceRPGStats *pStats = static_cast<const SFenceRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	const int nDBIndex = pGDB->GetIndex( theUnitCreation.GetWireFenceName() );

	int nFrameIndex;
	int nRandom = Random( 0, 0xffff );
	if ( !isXConst )
		nFrameIndex = vFrom.x > vTo.x ? pStats->GetCenterIndex( 1, &nRandom ) : pStats->GetCenterIndex( 3, &nRandom );
	else 
		nFrameIndex = vFrom.y > vTo.y ? pStats->GetCenterIndex( 0, &nRandom ) :  pStats->GetCenterIndex( 2, &nRandom ) ;
	const int nSegmentLenght = 2;//isXConst ? pStats->GetPassability( nFrameIndex ).GetSizeY() : pStats->GetPassability( nFrameIndex ).GetSizeX();
	const int nOffset = nSegmentLenght - 1;

	for ( std::vector<CVec2>::iterator it = hlpFence.m_points.begin(); it != hlpFence.m_points.end(); )
	{
		const CVec2 vPoint1 ( *it );
		bool bCanBuild = true;
		for ( int i = 0; i < nSegmentLenght && bCanBuild; ++i ) //check that every tile under the segment is inside map
		{
			if ( it == hlpFence.m_points.end() || !theStaticMap.IsTileInside( *it ) )
				bCanBuild = false;
			++it;
		}

		if ( !bCanBuild ) 
			return !fenceSegements.empty();

		CVec2 vFencePosition( vPoint1.x * SConsts::TILE_SIZE, vPoint1.y * SConsts::TILE_SIZE );
		CVec2 vCenterPosition( vPoint1 );
		if ( !isXConst )
		{
			if ( vFrom.x < vTo.x  )
			{
				vFencePosition = CVec2(  ( vPoint1.x + nSegmentLenght ) * SConsts::TILE_SIZE, ( vPoint1.y ) * SConsts::TILE_SIZE ); 
				vCenterPosition += CVec2( nOffset, 0.5 );
			}
			else
				vCenterPosition -= CVec2( nOffset, -0.5 );
		}
		else 
		{
			if ( vFrom.y > vTo.y  )
			{
				vFencePosition = CVec2(  vPoint1.x * SConsts::TILE_SIZE, ( vPoint1.y - nSegmentLenght ) * SConsts::TILE_SIZE ); 
				vCenterPosition -= CVec2( -0.5, nOffset );
			}
			else
				vCenterPosition += CVec2( 0.5, nOffset );
		}

		CVec3 vTmp( vFencePosition, 0 );
		FitAIOrigin2AIGrid( &vTmp, pStats->GetOrigin( nFrameIndex ) );	
		vFencePosition.x = vTmp.x;
		vFencePosition.y = vTmp.y;

		CPtr<CFence> pObj = new CFence( pStats, vFencePosition, nDBIndex, pStats->fMaxHP, nFrameIndex, nPlayer );
		pObj->Mem2UniqueIdObjs();
		pObj->Init();

		if ( /*строительство этого куска необходимо*/ IsCegmentToBeBuilt( pObj ) )
		{
			vPoints.push_back( vCenterPosition * SConsts::TILE_SIZE );
			fenceSegements.push_back( pObj.GetPtr() );
		}
	}
	return !fenceSegements.empty();
}
bool CFenceCreation::IsCegmentToBeBuilt( class CFence *pObj ) const
{
	const CVec3 vNormal =  DWORDToVec3( theStaticMap.GetNormal( pObj->GetCenter() ) );
	if ( fabs(vNormal.x) > fNearToNormale * fabs(vNormal.z) &&
			fabs(vNormal.y) > fNearToNormale * fabs(vNormal.z) ) return false;

	
	SRect r1;
	pObj->GetBoundRect( &r1 );
	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + SConsts::TILE_SIZE * 5;

	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, pObj->GetCenter(), fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( !pUnit->GetStats()->IsInfantry() )
		{
			SRect rect = pUnit->GetUnitRect();
			rect.InitRect( rect.center, rect.dir, rect.lengthAhead + SConsts::TILE_SIZE, rect.lengthBack + SConsts::TILE_SIZE, rect.width + SConsts::TILE_SIZE );
			
			if ( r1.IsIntersected( rect ) )
			{
				if ( pUnit->GetPlayer() == nPlayer && 
						 EUSN_REST == pUnit->GetState()->GetName() &&
						 pUnit->CanMove() )
				{
					(*iter)->UnlockTiles();
				}
			}
		}
	}
	CTilesSet tiles;
	pObj->GetCoveredTiles( &tiles );
	for ( CTilesSet::iterator it = tiles.begin(); it != tiles.end(); ++it )
	{
		const SStaticObjectRPGStats * pStats = static_cast<const SStaticObjectRPGStats *>(pObj->GetStats());
		if ( theStaticMap.IsLocked( *it, pStats->dwAIClasses ) )
			return false;
	}
	return true;
}
void CFenceCreation::GetUnitsPreventing( std::list< CPtr<CAIUnit> > *units )
{
	SRect r1;
	fenceSegements[nCurIndex]->GetBoundRect( &r1 );

	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + SConsts::TILE_SIZE * 5;
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		if ( !(*iter)->GetStats()->IsInfantry() )
		{
			const SRect rect = (*iter)->GetUnitRect();
			if ( r1.IsIntersected( rect ) ) 
				units->push_back( *iter );
		}
	}
}
bool CFenceCreation::IsAnyUnitPrevent() const
{
	SRect r1;
	fenceSegements[nCurIndex]->GetBoundRect( &r1 );

	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + SConsts::TILE_SIZE * 5;
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		if ( !(*iter)->GetStats()->IsInfantry() )
		{
			const SRect rect = (*iter)->GetUnitRect();
			if ( r1.IsIntersected( rect ) )
				return true;
		}
	}
	return false;
}
const int CFenceCreation::GetMaxIndex() const
{
	return fenceSegements.size();
}
const int CFenceCreation::GetCurIndex() const
{
	return nCurIndex;
}
bool CFenceCreation::CanBuildNext() const
{
	if ( bCannot ) return false;
	SRect r1;
	fenceSegements[nCurIndex]->GetBoundRect( &r1 );
	r1.Compress( 0.99f );	
	if ( !theStaticMap.IsRectInside( r1 ) ) return false;
	const float fRadius = r1.lengthAhead + r1.lengthBack + r1.width + SConsts::TILE_SIZE * 5;

	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, vPoints[nCurIndex], fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit * pUnit = *iter;
		if ( !pUnit->GetStats()->IsInfantry() )
		{
			SRect rect = pUnit->GetUnitRect();
			rect.InitRect( rect.center, rect.dir, rect.lengthAhead + SConsts::TILE_SIZE, rect.lengthBack + SConsts::TILE_SIZE, rect.width + SConsts::TILE_SIZE );
			
			if ( r1.IsIntersected( rect ) )
			{
				if ( pUnit->GetPlayer() == nPlayer && 
						 EUSN_REST == pUnit->GetState()->GetName() &&
						 pUnit->CanMove() )
				{
					(*iter)->UnlockTiles();
				}
			}
		}
	}
	
	for ( CTilesSet::const_iterator it = tilesUnder.begin(); it != tilesUnder.end(); ++it )
	{
		if ( 0 != theStaticMap.GetTileLockInfo( *it ) )
			return false;
	}
	return true;
}
void CFenceCreation::CalcTilesUnder()
{
	fenceSegements[nCurIndex]->GetCoveredTiles( &tilesUnder );
}
const CVec2 CFenceCreation::GetNextPoint( const int nPlace, const int nMaxPlace ) const
{
	NI_ASSERT_T( nMaxPlace, "builders number = 0" );

	const CVec2 vDir			= isXConst ? V2_AXIS_Y : V2_AXIS_X;
	const CVec2 vDirPerp	= - (isXConst ? V2_AXIS_X : V2_AXIS_Y);

	SRect rect;
	fenceSegements[nCurIndex]->GetBoundRect( &rect );

	return vPoints[nCurIndex]  +
		vDir * Max(rect.lengthAhead + rect.lengthBack,rect.width*2)/2 * (1 + nPlace - nMaxPlace/2) / nMaxPlace;
}
int CBridgeCreation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &spans );
	saver.Add( 2, &vStartPoint );
	saver.Add( 3, &line );
	saver.Add( 4, &nCurIndex );
	
	saver.Add( 5, &pFullBridge );
	saver.Add( 6, &wDir );
	saver.AddTypedSuper( 14, static_cast<CLongObjectCreation*>( this ) );
	return 0;
}
bool CBridgeCreation::SBridgeSpanSort::operator()( const CObj<CBridgeSpan> &s1, const CObj<CBridgeSpan> &s2 )
{
	const CVec2 &v1 = s1->GetCenter();
	const CVec2 &v2 = s2->GetCenter();

	if ( s1->GetBridgeStats()->direction == SBridgeRPGStats::VERTICAL )
		return v1.y > v2.y;
	else
		return v1.x > v2.x;
}
CVec2 CBridgeCreation::SortBridgeSpans( std::vector< CObj<CBridgeSpan> > *spans, class CCommonUnit *pUnit )
{
	if ( 1 >= spans->size() ) return VNULL2;

	SBridgeSpanSort pr;
	std::sort( spans->begin(), spans->end(), pr );
	const CBridgeSpan *s1 = *spans->begin();
	const CBridgeSpan *s2 = (*spans)[spans->size()-1];
	
	CVec2 vFrom1to2( s2->GetCenter() - s1->GetCenter() );
	Normalize( &vFrom1to2 );
	SRect r1, r2;
	s1->GetBoundRect( &r1 );
	s2->GetBoundRect( &r2 );
	const CVec2 v1( r1.center - Max(r1.lengthAhead,Max(r1.lengthBack,r1.width))*vFrom1to2 );
	const CVec2 v2( r2.center + Max(r2.lengthAhead,Max(r2.lengthBack,r2.width))*vFrom1to2 );


	CPtr<IStaticPath> pPath1 = CreateStaticPathToPoint( v1, VNULL2, pUnit, true );
	CPtr<IStaticPath> pPath2 = CreateStaticPathToPoint( v2, VNULL2, pUnit, true );

	const float fDiff1 = fabs2( pPath1->GetFinishPoint() - v1 );
	const float fDiff2 = fabs2( pPath2->GetFinishPoint() - v2 );



	if (	pPath1->GetLength() <= pPath2->GetLength() && fDiff1 < sqr( SConsts::TILE_SIZE * 10 ) )
	{
		return v1;
	}
	else if ( pPath1->GetLength() >= pPath2->GetLength() && fDiff2 < sqr( SConsts::TILE_SIZE * 10 ) )
	{
		std::reverse( spans->begin(), spans->end() );
		return v2;
	}
	else if ( fDiff1 < sqr( SConsts::TILE_SIZE * 10 ) ) // по первому пути хоть дойти можно
	{
		return v1;
	}
	else if ( fDiff2 < sqr( SConsts::TILE_SIZE * 10 ) )
	{
		std::reverse( spans->begin(), spans->end() );
		return v2;
	}
	else 
		return VNULL2;
}
bool CBridgeCreation::IsFirstSegmentBuilt() const
{
	return spans[0]->GetHitPoints() >= 0;
}
CBridgeCreation::CBridgeCreation( class CFullBridge *pBridge, class CCommonUnit *pUnit )
: pFullBridge( pBridge )
{
	pBridge->EnumSpans( &spans );
	NI_ASSERT_T( spans.size() >= 2, "bridge witout at least 2 spans" );
	vStartPoint = SortBridgeSpans( &spans, pUnit );

	for ( nCurIndex = 0; nCurIndex < spans.size(); ++nCurIndex )
	{
		if ( spans[nCurIndex]->GetHitPoints() < 0.0f )
			break;
	}
	line = CLine2( vStartPoint, spans[spans.size()-1]->GetCenter() );

	SRect r1;
	SRect r0;
	spans[1]->GetBoundRect( &r1 );
	spans[0]->GetBoundRect( &r0 );
	wDir = GetDirectionByVector( r1.center - r0.center );
}
const CVec2 & CBridgeCreation::GetStartPoint() const
{
	return vStartPoint;
}
CLine2 CBridgeCreation::GetCurLine() 
{ 
	return line; 
}
const int CBridgeCreation::GetMaxIndex() const
{
	return spans.size();
}
const int CBridgeCreation::GetCurIndex() const
{
	return nCurIndex;
}
const CVec2 CBridgeCreation::GetNextPoint( const int nPlace, const int nMaxPlace ) const
{
	SRect rect;
	spans[nCurIndex]->GetBoundRect( &rect );

	CVec2 vertexes[2];
	int nVertIndex = 0;
	for ( int i = 0; i < 4; ++i )
	{
		if ( DirsDifference( 65535/2+wDir, GetDirectionByVector( rect.v[i] - rect.center) ) < 65535/4 )
		{
			NI_ASSERT_T( nVertIndex < 2, "nakosyachil" );
			vertexes[nVertIndex] = rect.v[i];
			++nVertIndex;
		}
	}
	NI_ASSERT_T( nVertIndex == 2, "nakosyachil" );

	
	const CVec2 vOffset( (vertexes[0] - vertexes[1])/2 );
	return (vertexes[0] + vertexes[1])/2 + vOffset * ( 1.0f * nPlace / nMaxPlace ) - SConsts::TILE_SIZE * GetVectorByDirection( wDir );
}
void CBridgeCreation::BuildNext()
{
	CLongObjectCreation::BuildNext();
	if ( GetCurIndex() != 0 )
	{
		pFullBridge->UnlockSpan( spans[nCurIndex] );
	}
	spans[nCurIndex]->Build();
	const SHPObjectRPGStats * pStats = spans[nCurIndex]->GetStats();
	spans[nCurIndex]->SetHitPoints( pStats->fMaxHP );
	++nCurIndex;

	
	if ( GetCurIndex() < GetMaxIndex() )
	{
		const float fNextSpanHP = spans[nCurIndex]->GetHitPoints();
		if ( 0 ==  fNextSpanHP )
		{
			spans[nCurIndex]->SetHitPoints( spans[nCurIndex]->GetStats()->fMaxHP * 0.1 );
			spans.clear(); // строительство закончено
			pFullBridge->UnlockAllSpans();
		}
		else if ( 0 < fNextSpanHP )
		{
			spans.clear(); // строительство закончено
			pFullBridge->UnlockAllSpans();
		}
		else
		{
			pFullBridge->LockSpan( spans[nCurIndex], wDir );
		}
	}
	else
	{
		pFullBridge->UnlockAllSpans();
	}
}
float CBridgeCreation::GetPrice()
{
	const SHPObjectRPGStats *pStats = spans[nCurIndex]->GetStats();
	return pStats->fMaxHP * pStats->fRepairCost / pFullBridge->GetNSpans();
}
float CBridgeCreation::GetBuildSpeed()
{
	return SConsts::ENGINEER_REPEAR_HP_PER_QUANT;
}
