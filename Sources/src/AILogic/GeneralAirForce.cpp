#include "StdAfx.h"

#include "EnemyRememberer.h"
#include "GeneralHelper.h"
#include "GeneralAirForce.h"
#include "GeneralInternalInterfaces.h"
#include "UnitCreation.h"
#include "AILogicInternal.h"
#include "Diplomacy.h"
#include "General.h"
#include "UnitsIterators2.h"
#include "UnitsIterators.h"
#include "Aviation.h"
#include "UnitStates.h"
#include "Guns.h"
#include "AIStaticMap.h"
#include "AIWarFog.h"
#include "GeneralConsts.h"

#include "../Formats/fmtMap.h"
extern CGlobalWarFog theWarFog;
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern CAILogic *pAILogic;
extern CUnitCreation theUnitCreation;
extern NTimer::STime curTime;
extern CSupremeBeing theSupremeBeing;
BASIC_REGISTER_CLASS(CGeneralAirForce);
CGeneralAirForce::CGeneralAirForce( const int nParty, IEnemyContainer *pEnemyContainer ) 
	: pEnemyContainer( pEnemyContainer ), 
	nParty( nParty ), bReservedByFighters( false ), 
	timeLastCheck( curTime ), timeLastFighterCheck( curTime )
{  
	for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
	{
		if ( theDipl.GetNParty(i) == nParty )
		{
			players.push_back( i );
		}
	}
	requests.clear();
	requests.resize( static_cast<int>(_FT_AIR_END - _FT_AIR_BEGIN + 1) );

	InitCheckPeriod();
	InitFighterCheckPeriod();
}
void CGeneralAirForce::InitCheckPeriod()
{
	checkPeriod = Random( SGeneralConsts::AVIATION_PERIOD_MIN, SGeneralConsts::AVIATION_PERIOD_MAX );

	if ( Random( 0.0f, 1.0f ) < SGeneralConsts::AVATION_LONG_PERIOD_PROBABILITY )
	{
		checkPeriod *= SGeneralConsts::AVATION_PERIOD_MULTIPLY;
	}
}
void CGeneralAirForce::InitFighterCheckPeriod()
{
	fighterCheckPeriod = Random( SGeneralConsts::FIGHTER_PERIOD_MIN, SGeneralConsts::FIGHTER_PERIOD_MAX );

	if ( Random( 0.0f, 1.0f ) < SGeneralConsts::AVATION_LONG_PERIOD_PROBABILITY )
	{
		fighterCheckPeriod *= SGeneralConsts::AVATION_PERIOD_MULTIPLY;
	}
}
void CGeneralAirForce::Segment()
{
	for ( int i = 0; i < players.size(); ++i )
	{
		if (  players[i] != theDipl.GetMyNumber() &&  !bReservedByFighters )
		{
			const int nPlayer = players[i];
			
			if ( curTime - timeLastFighterCheck > fighterCheckPeriod )
			{
				if ( theUnitCreation.IsAviaEnabled( nPlayer, SUCAviation::AT_FIGHTER ) )
					PrepeareFighters( nPlayer );
				InitFighterCheckPeriod();
				timeLastFighterCheck = curTime;
			}
			
			if ( curTime - timeLastCheck > checkPeriod )
			{
				timeLastCheck = curTime;
				InitCheckPeriod();
				
				
				if ( !bReservedByFighters && IsTimePossible( nPlayer, curTime ) ) // ждем времени, когда нужно выпустить истребителей
				{
					if ( theUnitCreation.IsAviaEnabled( nPlayer, SUCAviation::AT_BATTLEPLANE ) )
						LaunchByRequest( nPlayer, SUCAviation::AT_BATTLEPLANE, &requests[FT_AIR_GUNPLANE - _FT_AIR_BEGIN] );

					if ( theUnitCreation.IsAviaEnabled( nPlayer, SUCAviation::AT_BOMBER ) )
					{
						LaunchByRequest( nPlayer, SUCAviation::AT_BOMBER, &requests[FT_AIR_BOMBER- _FT_AIR_BEGIN] );
					}

					if ( theUnitCreation.IsAviaEnabled( nPlayer, SUCAviation::AT_SCOUT ) )
						LaunchScoutFree( nPlayer );
				}
			}
		}
	}
	std::list<int> deleted;
	for ( AntiAviation::iterator it = antiAviation.begin(); antiAviation.end() != it; ++it )
	{
		if ( it->second->IsTimeToForget() )
			deleted.push_back( it->first );
	}

	while( !deleted.empty() )
	{
		antiAviation.erase( deleted.front() );
		deleted.pop_front();
	}
}
bool CGeneralAirForce::IsTimePossible( const int nPlayer, const NTimer::STime timeToLaunch ) const
{
	const NTimer::STime regenerateTime = theUnitCreation.GetPlaneRegenerateTime( nPlayer );
	const NTimer::STime timeProspectiveRegenerate = timeToLaunch + timeToLaunch;

	for ( int i = 0; i < reservedTimes.size(); ++i )
	{
		const NTimer::STime reservedRegenerate = reservedTimes[i] + regenerateTime / 2;
		if ( 
					(
					reservedTimes[i] < timeProspectiveRegenerate &&
					reservedTimes[i] > timeToLaunch
					) ||
					(
					reservedRegenerate < timeProspectiveRegenerate && 
					reservedRegenerate > timeToLaunch
					)
				)
			return false;
	}
	return true;
}
void CGeneralAirForce::LaunchByRequest( const int nPlayer, const int nAvia, Requests *pRequest )
{
	if ( pRequest->empty() ) return;
	std::list<CVec2> vPoints;							// точки вызова штурмовиков
	for ( Requests::iterator it = pRequest->begin(); it != pRequest->end(); ++it )
		vPoints.push_back( it->second.vPoint );

	SSameEnemyPointPredicate pr1;
	std::list<CVec2>::iterator firstSame = std::unique( vPoints.begin(), vPoints.end(), pr1 );
	vPoints.erase( firstSame, vPoints.end() );

	const float fFlyHeight( theUnitCreation.GetPlaneFlyHeight( nPlayer, nAvia ) );

	theUnitCreation.LockAppearPoint( nPlayer, true );
	CVec2 vCurStartPoint = theUnitCreation.GetRandomAppearPoint( nPlayer );
	for ( std::list<CVec2>::iterator it = vPoints.begin(); it != vPoints.end();  )
	{
		if ( 0 == CheckLineForSafety( vCurStartPoint, *it, fFlyHeight ) )
		{
			vCurStartPoint = *it;
			++it;
		}
		else
			it = vPoints.erase( it );
	}

	LaunchPlane( nAvia, vPoints, nPlayer );

	theUnitCreation.LockAppearPoint( nPlayer, false );
	
	if ( nAvia == SUCAviation::AT_BOMBER )
	{
		for ( Requests::iterator it = pRequest->begin(); pRequest->end() != it; ++it )
		{
			requestsID.AddToFreeId( it->first );
		}
		pRequest->clear();
	}
}
void CGeneralAirForce::LaunchFighters( const int nPlayer )
{
	bReservedByFighters = false;
	LaunchPlane( SUCAviation::AT_FIGHTER, vFighterPoints, nPlayer );
	vFighterPoints.clear();
	theUnitCreation.LockAppearPoint( nPlayer, false );
}
void CGeneralAirForce::PrepeareFighters( const int nPlayer )
{
	theUnitCreation.LockAppearPoint( nPlayer, true );

	for ( CPlanesIter iter; !iter.IsFinished(); iter.Iterate() )
	{
		CAviation *pPlane = *iter;
		if ( pPlane->IsAlive() &&
				 RPG_TYPE_AVIA_FIGHTER != pPlane->GetStats()->type && 
				 EUSN_REST != pPlane->GetState()->GetName() &&
				 EDI_ENEMY == theDipl.GetDiplStatus( pPlane->GetPlayer(), nPlayer ) )
		{
			
			const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats *>( pPlane->GetStats() );
			const CVec2 vDirection = pPlane->GetDirVector();
			const CVec2 vSpeed = pPlane->GetSpeed();
			const float fSpeed = pPlane->GetSpeedLen();
			const CVec2 vPurposePoint = pPlane->GetState()->GetPurposePoint() + vDirection * pStats->fTurnRadius;
			const CVec2 vCurrentPoint = pPlane->GetCenter();
			CVec2 vSeenUnitPosition;
			bool bFound = false;

			for ( CIter<CLineIter> iter( nParty, EDI_FRIEND, CLineIter( vCurrentPoint, vPurposePoint ) );
						!iter.IsFinished(); iter.Iterate() )
			{
				CAIUnit * pUnit = *iter;
				if ( pUnit->IsAlive() && !pUnit->GetStats()->IsAviation() )
				{
					bFound = true;
					vSeenUnitPosition = pUnit->GetCenter();
					break;
				}
			}
			if ( bFound )
			{
				const float fFullPathLenght = fabs( vSeenUnitPosition - pPlane->GetCenter() );
				const float fAllowedPathLenght = Max( fFullPathLenght - SGeneralConsts::FIGHTER_INTERCEPT_OFFSET, 0.0f );
				const CVec2 vEnemyPath( fAllowedPathLenght * vDirection );
				const CVec2 vFirstEncunterPoint = pPlane->GetCenter() + vEnemyPath;
				
				for ( CUnitsIter<0,3> iter( nParty, EDI_FRIEND, vSeenUnitPosition, 500.0f );
							!iter.IsFinished(); iter.Iterate() )
				{
					CAIUnit *pUnit = *iter;
					if ( pUnit->IsAlive() )
					{
						const EUnitRPGType &type = pUnit->GetStats()->type;
						if ( type == RPG_TYPE_ART_AAGUN || type == RPG_TYPE_SPG_AAGUN )
							return ;
					}
				}
				const float fTimeToFly = fAllowedPathLenght / fSpeed;

				const SMechUnitRPGStats *pOurStats = theUnitCreation.GetPlaneStats( nPlayer, SUCAviation::AT_FIGHTER );
				
				const float fOurTimeToFly = 
					fabs( theUnitCreation.GetRandomAppearPoint( nPlayer, false ) - vFirstEncunterPoint ) / pOurStats->fSpeed;
				const float fTimeToStartAttack = SConsts::PLANE_GUARD_STATE_RADIUS / pOurStats->fSpeed +
																				 SConsts::PLANE_GUARD_STATE_RADIUS / fSpeed ;
				
				if ( fOurTimeToFly < fTimeToFly + fTimeToStartAttack )
				{
					theSupremeBeing.RegisterDelayedTask( new CGeneralAirForceLaunchFighters( this, curTime, nPlayer ) );
					vFighterPoints.push_back( vFirstEncunterPoint );
				}
			}
		}
	}
	theUnitCreation.LockAppearPoint( nPlayer, false );
}
float CGeneralAirForce::CheckLineForSafety( const CVec2 &vStart, const CVec2 &vFinish, const float fFlyHeight )
{
	CLine2 line( vStart, vFinish );
	const CVec2 vLineCenter( (vStart + vFinish)/2 );

	for ( AntiAviation::iterator it = antiAviation.begin(); it != antiAviation.end(); ++it )
	{
		CAIUnit *pUnit = GetObjectByUniqueIdSafe<CAIUnit>( it->first );
		
		if ( !pUnit || !pUnit->IsValid() || !pUnit->IsAlive() ) continue;

		const CVec2 vCenter = pUnit->GetCenter();
		const int nGuns = pUnit->GetNGuns();
		for ( int i = 0; i < nGuns; ++i )
		{
			CBasicGun *pGun = pUnit->GetGun( i );
			const SWeaponRPGStats *pStats = pGun->GetWeapon();
			if ( pStats->nCeiling > fFlyHeight )
			{
				const float fDistToCenter = fabs2( vCenter - vLineCenter );
				const float fDist1 = fabs2( vStart - vCenter );
				float fMinDist;

				if ( fDistToCenter > fDist1 ) // нормаль от точки не падает на отрезок
					fMinDist = fDist1;
				else 
				{
					const float fDist2 = fabs2( vCenter - vFinish );
					if ( fDistToCenter > fDist2 )
						fMinDist = fDist2;
					else
						fMinDist = sqr( fabs( line.DistToPoint( vCenter ) ) );

				}

				if ( sqr( pGun->GetFireRangeMax() ) > fMinDist )
					return 1;
			}
		}
	}
	return 0;
}
void CGeneralAirForce::LaunchPlane( const int /*SUCAviation::AIRCRAFT_TYPE*/ nType, const std::list<CVec2> &vPoints, const int nPlayer )
{
	if ( vPoints.empty() ) return;
	SAIUnitCmd cmd;
	std::list<CVec2>::const_iterator it = vPoints.begin();
	const int nGroupID = pAILogic->GenerateGroupNumber();
	cmd.fNumber = 0;	// allow unit creation disable planes.

	cmd.vPos = *it;
	++it;
	switch( nType )
	{
	case SUCAviation::AT_SCOUT:
		cmd.cmdType = ACTION_COMMAND_CALL_SCOUT;
		break;
	case SUCAviation::AT_FIGHTER:
		cmd.cmdType = ACTION_COMMAND_CALL_FIGHTERS;
		break;
	case SUCAviation::AT_PARADROPER:
		cmd.cmdType = ACTION_COMMAND_PARADROP;
		break;
	case SUCAviation::AT_BOMBER:
		cmd.cmdType = ACTION_COMMAND_CALL_BOMBERS;
		break;
	case SUCAviation::AT_BATTLEPLANE:
		cmd.cmdType = ACTION_COMMAND_CALL_SHTURMOVIKS;
		break;
	}

	pAILogic->UnitCommand( &cmd, nGroupID, nPlayer );
	
	for ( ; it != vPoints.end(); ++it )
	{

		SAIUnitCmd cmd;
		cmd.cmdType = ACTION_COMMAND_PLANE_ADD_POINT;
		cmd.vPos = *it;
		cmd.fNumber = 0;
		pAILogic->GroupCommand( &cmd, nGroupID, true );
	}

	cmd.cmdType = ACTION_COMMAND_PLANE_TAKEOFF_NOW;
	pAILogic->GroupCommand( &cmd, nGroupID, true );
}
void CGeneralAirForce::LaunchScoutFree( const int nPlayer )
{
	theUnitCreation.LockAppearPoint( nPlayer, true );
	
	const float fSizeX = theStaticMap.GetSizeX();
	const float fSizeY = theStaticMap.GetSizeY();
	const int nParty = theDipl.GetNParty( nPlayer );

	std::vector<CVec2> points;
	
	const int nStep = Max( 2.0f, fSizeX / SGeneralConsts::SCOUT_POINTS );

	for ( int x = nStep / 2; x < fSizeX; x += nStep )
	{
		for ( int y = nStep / 2; y < fSizeY; y += nStep )
		{
			if ( !theWarFog.IsTileVisible( SVector(x,y), nParty ) )
				points.push_back( CVec2( x, y ) );
		}
	}
	SGeneralHelper::SRandomFunctor pr;
	std::random_shuffle( points.begin(), points.end(), pr );

	const CVec2 vAppearPoint( theUnitCreation.GetRandomAppearPoint( nPlayer ) );
	const float fFlyHeight( theUnitCreation.GetPlaneFlyHeight( nPlayer, SUCAviation::AT_SCOUT ) );
	
	std::list<CVec2> vPointsToFly;
	const float fCheckRadius = Min( static_cast<int>( SGeneralConsts::SCOUT_FREE_POINT ),
																	static_cast<int>( nStep * SConsts::TILE_SIZE ) );
	
	CVec2 vCurStartPoint = vAppearPoint;
	
	for ( int i = 0; i < points.size(); ++i )
	{
		bool bCan = true;
		for ( CUnitsIter<0,3> iter( nParty, EDI_FRIEND, points[i], fCheckRadius );
					!iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit * pUnit = *iter;
			if ( pUnit->IsAlive() && sqr(fCheckRadius) > fabs2(points[i]-pUnit->GetCenter()) )
			{
				bCan = false;
				break;
			}
		}
		if ( bCan )
		{
			const CVec2 vCurPointToAdd = points[i] * SConsts::TILE_SIZE ;
			if ( 0 == CheckLineForSafety( vCurStartPoint, vCurPointToAdd, fFlyHeight ) )
			{
				vPointsToFly.push_back( vCurPointToAdd );
				vCurStartPoint = vCurPointToAdd;
			}
		}
	}
	if ( !vPointsToFly.empty() )
	{
		LaunchPlane( SUCAviation::AT_SCOUT, vPointsToFly, nPlayer );
	}
	theUnitCreation.LockAppearPoint( nPlayer, false );
}
int /*request ID*/CGeneralAirForce::RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType, int nResistanceCellNumber )
{
	if ( nResistanceCellNumber != -1 )
	{
		std::list<int> delRequest;
		for ( Requests::iterator iter = requests[eType-_FT_AIR_BEGIN].begin(); iter != requests[eType-_FT_AIR_BEGIN].end(); ++iter )
		{
			if ( iter->second.nResistanceCellNumber != -1 )
				delRequest.push_back( iter->first );
		}
		for ( std::list<int>::iterator iter = delRequest.begin(); iter != delRequest.end(); ++iter )
			CancelRequest( *iter, eType );
	}

	const int nID = requestsID.GetFreeId();
	SSupportInfo info;
	info.vPoint = vSupportCenter;
	info.nResistanceCellNumber = nResistanceCellNumber;
	
	requests[eType-_FT_AIR_BEGIN][nID] = info;
	return nID;
}
void CGeneralAirForce::CancelRequest( int nRequestID, enum EForceType eType )
{
	if ( 0 != nRequestID )
	{
		NI_ASSERT_T( requests[eType-_FT_AIR_BEGIN].find( nRequestID ) != requests[eType-_FT_AIR_BEGIN].end(), "wrong cancel request" );
		requests[eType-_FT_AIR_BEGIN].erase( nRequestID );
		requestsID.AddToFreeId( nRequestID );
	}
}
bool CGeneralAirForce::EnumEnemy( CAIUnit *pEnemy )
{
	return false;
}
void CGeneralAirForce::DeleteAA( CAIUnit *pUnit )
{
	antiAviation.erase( pUnit->GetUniqueId() );
}
void CGeneralAirForce::SetAAVisible( CAIUnit *pUnit, const bool bVisible )
{
	AntiAviation::iterator it = antiAviation.find( pUnit->GetUniqueId() );
	if (  it == antiAviation.end() )
	{
		CEnemyRememberer *pEnemy = new CEnemyRememberer( SGeneralConsts::TIME_SONT_SEE_AA_BEFORE_FORGET );
		antiAviation[pUnit->GetUniqueId()] = pEnemy;
	}

	antiAviation[pUnit->GetUniqueId()]->SetVisible( pUnit, bVisible );
}
void CGeneralAirForce::ReserveAviationForTimes( const std::vector<NTimer::STime> &times )
{
	reservedTimes = times;
	std::sort( reservedTimes.begin(), reservedTimes.end() );
}
CGeneralAirForceLaunchFighters::CGeneralAirForceLaunchFighters( class CGeneralAirForce *pAirForce, const NTimer::STime timeToRun, const int nPlayer ) 
: pAirForce( pAirForce ), timeToRun( timeToRun ), nPlayer( nPlayer ) 
{  
}
bool CGeneralAirForceLaunchFighters::IsTimeToRun() const 
{ 
	return curTime >= timeToRun; 
}
void CGeneralAirForceLaunchFighters::Run() 
{ 
	pAirForce->LaunchFighters( nPlayer );
}
