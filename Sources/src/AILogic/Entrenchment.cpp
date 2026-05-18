#include "stdafx.h"

#include "Entrenchment.h"
#include "Soldier.h"
#include "Updater.h"
#include "StaticObjects.h"
#include "AIStaticMap.h"
#include "Diplomacy.h"
#include "Cheats.h"
#include "AIWarFog.h"
#include "UnitStates.h"
#include "Statistics.h"
#include "MultiplayerInfo.h"
#include "MPLog.h"

#include "..\Misc\Checker.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CUpdater updater;
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern SCheats theCheats;
extern CGlobalWarFog theWarFog;
extern CStatistics theStatistics;
extern CMultiplayerInfo theMPInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CEntrenchmentPart														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CEntrenchmentPart );
BASIC_REGISTER_CLASS( CEntrenchmentTankPit );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEntrenchmentPart::CEntrenchmentPart( const SEntrenchmentRPGStats *_pStats, const CVec2 &_center, const WORD _dir, const int nFrameIndex, const int dbID, float fHP )
:	CExistingObject( nFrameIndex, dbID, fHP ), pStats( _pStats ), center( _center ), dir( _dir )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::Init()
{
	boundRect = CalcBoundRect( GetCenter(), dir, GetSegmStats() );
	bVisible = false;
	GetTilesCoveredByRect( boundRect, &coveredTiles );

	nextSegmTime = 0;
	//CPtr<CStaticObject> p = this;
//	theStatObjs.RegisterSegment( p );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEntrenchmentPart::CanUnregister() const
{
	for ( CTilesSet::const_iterator iter = coveredTiles.begin(); iter != coveredTiles.end(); ++iter )
	{
		const SVector &tile = *iter;		
		if ( !theWarFog.IsTileVisible( tile, 0 ) || !theWarFog.IsTileVisible( tile, 1 ) )
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::Segment()
{
	// все действия здесь должны быть const, т.к. они различны на разных компах, для multiplayer
	if ( !bVisible )
	{
		for ( CTilesSet::const_iterator iter = coveredTiles.begin(); iter != coveredTiles.end(); ++iter )
		{
			if ( theWarFog.IsTileVisible( *iter, theDipl.GetMyParty() ) || theCheats.IsHistoryPlaying() )
			{
				pFullEntrenchment->SetVisible();
				break;
			}
		}

		if ( bVisible )
			pFullEntrenchment = 0;
	}
	else
		pFullEntrenchment = 0;

	// unregister только если виден всеми сторонами - для multiplayer	
	if ( CanUnregister() )
		theStatObjs.UnregisterSegment( this );

	// random вызывать всегда - для mutliplayer
	nextSegmTime = curTime + SConsts::AI_SEGMENT_DURATION * Random( 8, 15 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SRect CEntrenchmentPart::CalcBoundRect( const CVec2 & center, const WORD _dir, const SEntrenchmentRPGStats::SSegmentRPGStats& stats)
{
	SRect r;
	const CVec2 vDir( GetVectorByDirection( _dir ) );
	r.InitRect( center + GetShift( stats.vAABBCenter, vDir ), vDir, stats.vAABBHalfSize.x, stats.vAABBHalfSize.y );
	return r;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::SetNewPlaceWithoutMapUpdate( const CVec2 &_center, const WORD _dir ) 
{ 
	center = _center; 
	dir = _dir; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::GetRPGStats( SAINotifyRPGStats *pStats ) 
{ 
	pStats->pObj = this;
	pStats->fHitPoints = 1;
	pStats->time = curTime;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CEntrenchmentPart::GetShift( const CVec2 &vPoint, const CVec2 &vDir ) 
{
	const CVec2 dirPerp( vDir.y, -vDir.x );
	return CVec2( vDir * vPoint.y + dirPerp * vPoint.x );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::GetCoveredTiles( CTilesSet *pTiles ) const
{
	SRect boundRect;
	GetBoundRect( &boundRect );

	GetTilesCoveredByRect( boundRect, pTiles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( pOwner != 0 )
		pOwner->TakeDamage( fDamage, bFromExplosion, nPlayerOfShoot, pShotUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEntrenchmentPart::IsPointInside( const CVec2 &point ) const
{
	return boundRect.IsPointInside( point );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentPart::SetVisible()
{
	if ( !bVisible )
	{
		bVisible = true;

		updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, this );
		updater.Update( ACTION_NOTIFY_NEW_ENTRENCHMENT, this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CEntrenchment														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CEntrenchment );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEntrenchment::CEntrenchment( IRefCount** segments, const int nLen, CFullEntrenchment *pFullEntrenchment )
: nBusyFireplaces( 0 )
{
	NI_ASSERT_T( nLen != 0, "Окоп из нуля сегментов" );
	SetUniqueId();
	
	insiders.clear();
	fireplaces.reserve( 0 );

	float fLengthAhead = -1, fLengthBack = 0.0f, fWidth = 0.0f;
	CVec2 vDir( VNULL2 ), vDirPerp( VNULL2 ), center( VNULL2 );
	for ( int i = 0; i < nLen; ++i )
	{
		NI_ASSERT_T( dynamic_cast<const CEntrenchmentPart*>( segments[i] ) != 0, "Wrong part of entrenchment" );
		CEntrenchmentPart *pPart = static_cast<CEntrenchmentPart*>( segments[i] );
	//	NI_ASSERT_T( pPart->GetOwner() == 0, "Segment of entrenchment exists in two sections" );

		if ( pPart->GetType() == SEntrenchmentRPGStats::EST_FIREPLACE || pPart->GetType() == SEntrenchmentRPGStats::EST_LINE )
		{
			if ( fLengthAhead == -1 )
			{
				fLengthAhead = pPart->GetSegmStats().vAABBHalfSize.x;
				fLengthBack = pPart->GetSegmStats().vAABBHalfSize.x;
				fWidth = pPart->GetSegmStats().vAABBHalfSize.y;

				vDir = GetVectorByDirection( pPart->GetDir() );
				vDirPerp = CVec2( -vDir.y, vDir.x );

				center = pPart->GetCenter() + GetShift( pPart->GetSegmStats().vAABBCenter, vDir );
				z = pPart->GetSegmStats().vAABBHalfSize.z;
				pStats = static_cast<const SEntrenchmentRPGStats*>(pPart->GetStats());
			}
			else
			{
				const float fToCenter = ( pPart->GetCenter() + GetShift( pPart->GetSegmStats().vAABBCenter, vDir ) - center ) * vDirPerp;

				float fLength = fToCenter + pPart->GetSegmStats().vAABBHalfSize.y;
				if ( fLength > 0 && fLength > fLengthAhead )
					fLengthAhead = fLength;

				fLength = fToCenter - pPart->GetSegmStats().vAABBHalfSize.y;
				if ( fLength < 0 && -fLength > fLengthBack )
					fLengthBack = -fLength;
			}
			
			for ( int i = 0; i < pPart->GetSegmStats().fireplaces.size(); ++i )
				fireplaces.push_back( SFireplaceInfo( pPart->GetCenter() + GetShift( pPart->GetSegmStats().fireplaces[i], vDir ), 0, pPart->GetFrameIndex() ) );
		}

		pPart->SetOwner( this );
		pFullEntrenchment->AddEntrenchmentPart( pPart );
		pPart->SetFullEntrench( pFullEntrenchment );
		
		theStatObjs.RegisterSegment( pPart );
	}

	NI_ASSERT_T( fLengthAhead != -1, "Окоп без линейных сегментов и fireplaces" );

	// чтобы центр прямоульника был в середине
	const float fLength = ( fLengthAhead + fLengthBack ) / 2;
	rect.InitRect( center + vDirPerp * ( -fLengthBack + fLength ), vDirPerp, fLength, fWidth );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAIUnit* CEntrenchment::GetIteratedUnit() 
{ 
	NI_ASSERT_TF( !IsIterateFinished(), "Wrong fire unit to get", return 0 ); 
	return iter->pUnit; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CEntrenchment::GetShift( const CVec2 &vPoint, const CVec2 &vDir )
{
	const CVec2 dirPerp( vDir.y, -vDir.x );
	return CVec2( vDir * vPoint.y + dirPerp * vPoint.x );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::AddSoldier( CSoldier *pUnit )
{
	if ( nBusyFireplaces == 0 )
	{
		nextSegmTime = curTime + SConsts::AI_SEGMENT_DURATION - 1;		
		theStatObjs.RegisterSegment( this );
	}
	
	int nFireplace = -1;
	if ( nBusyFireplaces < fireplaces.size() )
	{
		++nBusyFireplaces;
		
		// найти ближайшее к центру юнита свободное fireplace 
		int i = 0;
		float fBestDist = 1e10;
		while ( i < fireplaces.size() )
		{
			if ( fireplaces[i].pUnit == 0 )
			{
				const float fDist = fabs2( pUnit->GetCenter() - fireplaces[i].center );
				if ( fDist < fBestDist )
				{
					fBestDist = fDist;
					nFireplace = i;
				}
			}

			++i;
		}

		NI_ASSERT_T( nFireplace < fireplaces.size(), "Wrong fireplace number" );

		fireplaces[nFireplace].pUnit = pUnit;
		pUnit->MoveToEntrenchFireplace( CVec3( fireplaces[nFireplace].center, -z ), fireplaces[nFireplace].nFrameIndex );
		insiders.push_front( SInsiderInfo( pUnit, nFireplace ) );
	}
	else
	{
		pUnit->SetNSlot( -1 );
		pUnit->SetToSolidPlace();
		insiders.push_back( SInsiderInfo( pUnit, nFireplace ) );
	}

	CRotatingFireplacesObject::AddUnit( pUnit, nFireplace );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::AddSoldierToFirePlace( CSoldier *pUnit, const int nFirePlace )
{
	if ( nBusyFireplaces == 0 )
	{
		nextSegmTime = curTime + SConsts::AI_SEGMENT_DURATION - 1;		
		theStatObjs.RegisterSegment( this );
	}

	NI_ASSERT_T( nBusyFireplaces < fireplaces.size(), "Not enough fireplaces" );
	
	++nBusyFireplaces;
	fireplaces[nFirePlace].pUnit = pUnit;
	pUnit->MoveToEntrenchFireplace( CVec3( fireplaces[nFirePlace].center, -z ), fireplaces[nFirePlace].nFrameIndex );
	insiders.push_front( SInsiderInfo( pUnit, nFirePlace ) );

	CRotatingFireplacesObject::AddUnit( pUnit, nFirePlace );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CEntrenchment::GetFirePlaceCoord( const int nFirePlace )
{
	CheckFixedRange( nFirePlace, GetNFirePlaces(), "entrehcment" );
	return fireplaces[nFirePlace].center;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::ProcessEmptyFireplace( const int nFireplace )
{
	// есть юниты в резерве, проверка на нахождение в окопе, т.к. он оттуда может уже собираться выходить
	if ( !insiders.empty() && insiders.back().nFireplace == -1 && insiders.back().pUnit->IsInEntrenchment() )
	{
		CSoldier *pUnit = insiders.back().pUnit;
		
		insiders.back().nFireplace = nFireplace;
		fireplaces[nFireplace].pUnit = pUnit;

		pUnit->MoveToEntrenchFireplace( CVec3( fireplaces[nFireplace].center, -z ), fireplaces[nFireplace].nFrameIndex );

		insiders.push_front( insiders.back() );
		insiders.pop_back();
	}
	else
	{
		NI_ASSERT_T( nBusyFireplaces > 0, "Wrong number of fireplaces" );
		--nBusyFireplaces;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::DelSoldier( CSoldier *pUnit, const bool bFillEmptyFireplace )
{
	std::list< SInsiderInfo >::iterator iter = insiders.begin();
	while ( iter != insiders.end() && iter->pUnit.GetPtr() != pUnit )
		++iter;

	if ( iter != insiders.end() )
	{
		int nFireplace = iter->nFireplace;
		// сидит в fireplace
		if ( nFireplace != -1 )
			fireplaces[nFireplace].pUnit = 0;

		insiders.erase( iter );

		if ( nFireplace != -1 && bFillEmptyFireplace )
			ProcessEmptyFireplace( nFireplace );
		if ( nFireplace != -1 && !bFillEmptyFireplace )
			--nBusyFireplaces;

		CRotatingFireplacesObject::DeleteUnit( pUnit );
		pUnit->SetNSlot( -1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	std::list< CPtr<CSoldier> > dead;	
	
	// все убиты
	if ( fDamage >= pStats->fMaxHP || theCheats.GetFirstShoot( theDipl.GetNParty( nPlayerOfShoot ) ) == 1 )
	{
		for ( std::list<SInsiderInfo>::iterator iter = insiders.begin(); iter != insiders.end(); ++iter )
			dead.push_back( iter->pUnit );
	}
	else
	{
		const float fProbability = fDamage / pStats->fMaxHP;

		for ( std::list<SInsiderInfo>::iterator iter = insiders.begin(); iter != insiders.end(); ++iter )
		{
			if ( Random( 0.0f, 1.0f ) < fProbability )
				dead.push_back( iter->pUnit );
		}
	}

	for ( std::list< CPtr<CSoldier> >::iterator iter = dead.begin(); iter != dead.end(); ++iter )
	{
		CSoldier *pSoldier = *iter;

		if ( pShotUnit )
			pShotUnit->EnemyKilled( pSoldier );
		
		theStatistics.UnitKilled( nPlayerOfShoot, pSoldier->GetPlayer(), 1, pSoldier->GetStats()->fPrice );
		theMPInfo.UnitsKilled( nPlayerOfShoot, pSoldier->GetStats()->fPrice, pSoldier->GetPlayer() );

		pSoldier->Die( false, 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::GetCoveredTiles( CTilesSet *pTiles ) const
{
	GetTilesCoveredByRect( rect, pTiles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::Iterate() 
{ 
	if ( iter != insiders.end() )
	{
		++iter;
		while ( iter != insiders.end() && iter->nFireplace == -1 ) 
			++iter; 
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CEntrenchment::GetNDefenders() const
{
	return insiders.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoldier* CEntrenchment::GetUnit( const int n ) const
{
	NI_ASSERT_T( n < GetNDefenders(), "Wrong unit to get from entrenchment" );

	std::list<SInsiderInfo>::const_iterator iter = insiders.begin();
	std::advance( iter, n );
	return iter->pUnit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const BYTE CEntrenchment::GetPlayer() const
{
	if ( GetNDefenders() ==0 || insiders.empty() )
		return theDipl.GetNeutralPlayer();
	else
		return insiders.begin()->pUnit->GetPlayer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::Segment() 
{ 
	nextSegmTime = curTime + Random( 2 * SConsts::AI_SEGMENT_DURATION - 1, 10 * SConsts::AI_SEGMENT_DURATION );

	if ( !IsUnitsInside() )
		theStatObjs.UnregisterSegment( this );
	else
	{
		if ( !CStormableObject::Segment() )
			CRotatingFireplacesObject::Segment();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::Delete()
{
	theStatObjs.DeleteInternalEntrenchmentInfo( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::GetClosestPoint( const CVec2 &vPoint, CVec2 *pvResult ) const
{
	const CSegment rectSegment( rect.center - rect.dir * rect.lengthBack, rect.center + rect.dir * rect.lengthAhead );
	rectSegment.GetClosestPoint( vPoint, pvResult );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEntrenchment::CanRotateSoldier( CSoldier *pSoldier ) const
{
	return pSoldier->GetState() && IsRestState( pSoldier->GetState()->GetName() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchment::ExchangeUnitToFireplace( CSoldier *pSoldier, int nFirePlace )
{
	NI_ASSERT_T( nFirePlace < GetNFirePlaces(), NStr::Format( "Wrong number of fireplace (%d), number of fireplaces (%d)", nFirePlace, GetNFirePlaces() ) );

	CSoldier *pDeletedSoldier = GetSoldierInFireplace( nFirePlace );

	if ( pDeletedSoldier )
		DelSoldier( pDeletedSoldier, false );
	DelSoldier( pSoldier, false );

	AddSoldierToFirePlace( pSoldier, nFirePlace );
	if ( pDeletedSoldier )
		AddSoldier( pDeletedSoldier );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CEntrenchment::GetNFirePlaces() const
{
	return fireplaces.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoldier* CEntrenchment::GetSoldierInFireplace( const int nFireplace) const
{
	return fireplaces[nFireplace].pUnit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											CEntrenchmentTankPit												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEntrenchmentTankPit::CEntrenchmentTankPit( const SMechUnitRPGStats *_pStats,
												const CVec2& center,
												const WORD dir,
												const int nFrameIndex, const int dbID, const class CVec2 &vHalfSize,
												const CTilesSet &_tilesToLock,
												class CAIUnit *_pOwner )
: wDir( dir ), vHalfSize( vHalfSize ), pStats( _pStats ), pOwner( _pOwner ),
	CGivenPassabilityStObject( center, dbID, 1.0f, nFrameIndex )
{
	//copy tiles
	for ( CTilesSet::const_iterator i = _tilesToLock.begin(); _tilesToLock.end() != i; ++i )
		tilesToLock.push_back( *i );
	
	boundRect.InitRect(center, GetVectorByDirection(dir), vHalfSize.y, vHalfSize.x );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::GetTilesForVisibility( CTilesSet *pTiles ) const
{
	GetCoveredTiles( pTiles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::GetCoveredTiles( CTilesSet *pTiles ) const
{
	pTiles->clear();
	for ( CTilesSet::const_iterator i = tilesToLock.begin(); tilesToLock.end() != i; ++i )
	{
		const SVector v = *i;
		if ( theStaticMap.IsTileInside( v ) )
			pTiles->push_back( v );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::GetNewUnitInfo( struct SNewUnitInfo *pNewUnitInfo )
{
	CGivenPassabilityStObject::GetNewUnitInfo( pNewUnitInfo );
	pNewUnitInfo->fResize = ( boundRect.width  ) / pStats->vAABBHalfSize.x;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::LockTiles( bool bInitialization )
{
	theStaticMap.UpdateMaxesByTiles( tilesToLock, AI_CLASS_ANY, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEntrenchmentTankPit::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return //eAction == ACTION_NOTIFY_NEW_ST_OBJ ||
		CGivenPassabilityStObject::ShouldSuspendAction( eAction );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::UnlockTiles(  bool bInitialization ) 
{
	if ( !bInitialization )
	{
		theStaticMap.UpdateMaxesByTiles( tilesToLock, AI_CLASS_ANY, false );
		//theStatObjs.UpdateAllPartiesStorages( false, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::Die( const float fDamage )
{
	Delete();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CEntrenchmentTankPit::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	//не разрушается
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												 CFullEntrenchment												*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CFullEntrenchment );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFullEntrenchment::AddEntrenchmentPart( class CEntrenchmentPart *pEntrenchmentPart )
{
	entrenchParts.push_back( pEntrenchmentPart );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFullEntrenchment::SetVisible()
{
	for ( std::list< CEntrenchmentPart* >::iterator iter = entrenchParts.begin(); iter != entrenchParts.end(); ++iter )
		(*iter)->SetVisible();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
