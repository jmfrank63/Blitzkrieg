#include "StdAfx.h"

#include "GeneralTasks.h"

#include "GeneralHelper.h"
#include "General.h"
#include "GeneralInternal.h"
#include "AIUnit.h"
#include "Formation.h"
#include "Diplomacy.h"
#include "UnitsIterators.h"
#include "GroupLogic.h"
#include "Guns.h"
#include "Soldier.h"
#include "UnitStates.h"
#include "GeneralConsts.h"
#include "StaticObjects.h"
#include "PathFinder.h"
#include "Path.h"
#include "General.h"
#include "..\SCene\Scene.h"

extern CSupremeBeing theSupremeBeing;
extern CStaticObjects theStaticObjects;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CGroupLogic theGroupLogic;
CGeneralTaskToDefendPatch::CGeneralTaskToDefendPatch() 
: nCurReinforcePoint( 0 ), nRequestForGunPlaneID( -1 ), pOwner( 0 ), bFinished( false )
{  
}
void CGeneralTaskToDefendPatch::InitTanks( CCommonUnit *pUnit )
{
	const CVec2 vTransReinforcePoint ( patchInfo.reinforcePoints[nCurReinforcePoint].vCenter.y, -patchInfo.reinforcePoints[nCurReinforcePoint].vCenter.x );
	const CVec2 vReinforcePoint( patchInfo.vCenter + ( vTransReinforcePoint ^ GetVectorByDirection(patchInfo.wDefenceDirection) ) );
	const CVec2 vLookPoint( vReinforcePoint + GetVectorByDirection( patchInfo.reinforcePoints[nCurReinforcePoint].wDir + patchInfo.wDefenceDirection )*100 );

	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_TO,vReinforcePoint ), pUnit, false );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_ROTATE_TO, vLookPoint ), pUnit, true );
	++nCurReinforcePoint;
	nCurReinforcePoint %= patchInfo.reinforcePoints.size();
}
void CGeneralTaskToDefendPatch::InitInfantryInTrenches( class CCommonUnit *pUnit )
{
	const CVec2 vSpyGlassPoint( patchInfo.vCenter + GetVectorByDirection( patchInfo.wDefenceDirection ) * 1000 );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_USE_SPYGLASS, vSpyGlassPoint), pUnit, true );
}
void CGeneralTaskToDefendPatch::Init( const SAIGeneralParcelInfo &_patchInfo, CGeneral *_pOwner )
{
	pOwner = _pOwner;
	patchInfo = _patchInfo;
	fSeverity = fEnemyForce = fFriendlyForce = fMaxSeverity = fFriendlyMobileForce = 0;
	bFinished = false;
	bWaitForFinish = false;
	timeLastUpdate = 0;
	
	pOwner->AddResistance( patchInfo.vCenter, patchInfo.fRadius );
}
ETaskName CGeneralTaskToDefendPatch::GetName() const 
{ 
	return ETN_DEFEND_PATCH; 
}
void CGeneralTaskToDefendPatch::AskForWorker( ICommander *pManager, const float _fMaxSeverity, const bool bInit )
{
	if ( bInit )
	{
		pManager->EnumWorkers( FT_INFANTRY_IN_TRENCHES, this );
		pManager->EnumWorkers( FT_STATIONARY_MECH_UNITS, this );
		pManager->EnumWorkers( FT_FREE_INFANTRY, this );
	}
	else 
	{
		fMaxSeverity = _fMaxSeverity;

		if ( _fMaxSeverity > fSeverity && fSeverity <= 0 )
			pManager->EnumWorkers( FT_MOBILE_TANKS, this );

		if ( fEnemyForce > 0 ) // можно позвать штурмовиков, если они еще не вызваны.
		{
			if ( -1 == nRequestForGunPlaneID )
				nRequestForGunPlaneID = pManager->RequestForSupport( patchInfo.vCenter, FT_AIR_GUNPLANE );
		}
		else if( -1 != nRequestForGunPlaneID ) // отменить вызов штурмовиков
		{
			pManager->CancelRequest( nRequestForGunPlaneID, FT_AIR_GUNPLANE );
			nRequestForGunPlaneID = -1;
		}
	}
}
void CGeneralTaskToDefendPatch::ReleaseWorker( ICommander *pManager, const float _fMinSeverity )
{
	if ( fEnemyForce != 0 && !bFinished )
	{
		while ( !tanksMobile.empty() && fSeverity > _fMinSeverity && fEnemyForce == 0 )
		{
			CCommonUnit *pTank = *tanksMobile.begin();
			tanksMobile.pop_front();

			if ( pTank->IsValid() && pTank->IsAlive() )
			{
				const float fFormerMobileForce = fFriendlyMobileForce;

				CalcSeverity( false, true );
				if ( fSeverity >= _fMinSeverity ) // юнит можно отдать без ущерба для ситуации
					pManager->Give( pTank );
				else
				{
					tanksMobile.push_front( pTank );
					fFriendlyMobileForce = fFormerMobileForce;
					CalcSeverity( false, false );
					break;
				}
			}
		}
	}
	else // отдать все резервы
	{
		while ( !tanksMobile.empty() )
		{
			CCommonUnit *pTank = *tanksMobile.begin();
			tanksMobile.pop_front();
			if ( pTank->IsValid() && pTank->IsAlive() )
				pManager->Give( pTank );
		}
	}
	if ( bFinished )
		pEnemyConatainer->RemoveResistance( patchInfo.vCenter );
}
float CGeneralTaskToDefendPatch::GetSeverity() const 
{ 
	return fSeverity; 
}
bool CGeneralTaskToDefendPatch::IsFinished() const 
{ 
	return bFinished; 
} 
void CGeneralTaskToDefendPatch::CalcSeverity( const bool bEnemyUpdated, const bool bFriendlyUpdated )
{
	if ( bEnemyUpdated )
	{
		fEnemyForce = 0;
		pEnemyConatainer->GiveEnemies( this );
		{
			IScene * pScene = GetSingleton<IScene>();
			IStatSystem *pStat = pScene->GetStatSystem();
			const int x = patchInfo.vCenter.x;
			const int y = patchInfo.vCenter.y;
			pStat->UpdateEntry( "General: enemy near defence (DEF):", 
				NStr::Format( "(%d,%d), enemy force = %f", x, y, fEnemyForce ) );
		}
	}
	
	if ( bFriendlyUpdated )
	{
		SGeneralHelper::SSeverityCountPredicate pr1;
		fFriendlyForce = 0;
		pr1 = std::for_each( infantryInTrenches.begin(), infantryInTrenches.end(), pr1 );
		pr1 = std::for_each( infantryFree.begin(), infantryFree.end(), pr1 );
		pr1 = std::for_each( stationaryUnits.begin(), stationaryUnits.end(), pr1 );
		fFriendlyForce += pr1.fCount;

		SGeneralHelper::SSeverityCountPredicate pr2;
		pr2 = std::for_each( tanksMobile.begin(), tanksMobile.end(), pr2 );
		fFriendlyMobileForce = pr2.fCount;
	}
	fSeverity = fFriendlyForce + fFriendlyMobileForce - SGeneralConsts::PLAYER_FORCE_COEFFICIENT * fEnemyForce;
}
void CGeneralTaskToDefendPatch::CancelTask( ICommander *pManager )
{
	pEnemyConatainer->RemoveResistance( patchInfo.vCenter );

	for ( CommonUnits::iterator it = infantryInTrenches.begin(); infantryInTrenches.end() != it; ++it )
		pManager->Give( *it );
	for ( CommonUnits::iterator it = infantryFree.begin(); it != infantryFree.end(); ++it )
		pManager->Give( *it );
	for ( CommonUnits::iterator it = tanksMobile.begin(); tanksMobile.end() != it; ++it )
		pManager->Give( *it );
	for ( CommonUnits::iterator it = stationaryUnits.begin(); stationaryUnits.end() != it; ++it )
		pManager->Give( *it );
}
void CGeneralTaskToDefendPatch::Segment()
{
	if ( bFinished ) 
		return;
	
	bool bNeedRecalc = false;
	
	bNeedRecalc |= SGeneralHelper::RemoveDead( &infantryFree );
	bNeedRecalc |= SGeneralHelper::RemoveDead( &infantryInTrenches );
	bNeedRecalc |= SGeneralHelper::RemoveDead( &tanksMobile );
	bNeedRecalc |= SGeneralHelper::RemoveDead( &stationaryUnits );

	CalcSeverity( true, bNeedRecalc );
	
	if ( fFriendlyForce != 0.0f )
	{
		IScene * pScene = GetSingleton<IScene>();
		IStatSystem *pStat = pScene->GetStatSystem();
		const int x = patchInfo.vCenter.x;
		const int y = patchInfo.vCenter.y;
		pStat->UpdateEntry( "General: mobile forces to patch(DEF):", 
												 NStr::Format("(%d,%d) with force %f", x, y, fFriendlyMobileForce )  );
	}

	if ( fFriendlyForce == 0.0f ) // friencdly force become 0
	{
		if ( !bWaitForFinish )							// if it is first time
		{
			bWaitForFinish = true;						// wait for repeat this situation
			timeLastUpdate = curTime;
		}
		else if ( curTime > timeLastUpdate + 1000 )	// ok, the same, finish task
		{
			bWaitForFinish = false;
			bFinished = true;
		}
	}
}
bool CGeneralTaskToDefendPatch::EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType )
{
	switch( eType )
	{
	case FT_FREE_INFANTRY:
		{
			NI_ASSERT_T( dynamic_cast<CFormation*>(pUnit) != 0, "not infantry passed" );
			infantryInTrenches.push_back( static_cast<CFormation*>(pUnit) );
			InitInfantryInTrenches( pUnit );
			SGeneralHelper::SSeverityCountPredicate pr1;
			pr1( pUnit );
			fFriendlyForce += pr1.fCount;
			CalcSeverity( false, false );		
		}
		return true;
	case FT_INFANTRY_IN_TRENCHES:
		{
			NI_ASSERT_T( dynamic_cast<CFormation*>(pUnit) != 0, "not infantry passed" );
			infantryInTrenches.push_back( static_cast<CFormation*>(pUnit) );
			InitInfantryInTrenches( pUnit );
			SGeneralHelper::SSeverityCountPredicate pr1;
			pr1( pUnit );
			fFriendlyForce += pr1.fCount;
		}			
		return true;
	case FT_STATIONARY_MECH_UNITS:
		{
			NI_ASSERT_T( dynamic_cast<CAIUnit*>(pUnit) != 0, "not mechUnit passed" );
			stationaryUnits.push_back( static_cast<CAIUnit*>(pUnit) );
			SGeneralHelper::SSeverityCountPredicate pr1;
			pr1( pUnit );
			fFriendlyForce += pr1.fCount;
		}
		
		return true;
	case FT_MOBILE_TANKS:
		NI_ASSERT_T( dynamic_cast<CAIUnit*>(pUnit) != 0, "not tank passed" );
		{
			tanksMobile.push_back( static_cast<CAIUnit*>(pUnit) );
			InitTanks( pUnit );
			SGeneralHelper::SSeverityCountPredicate pr1;
			pr1( pUnit );
			fFriendlyMobileForce += pr1.fCount;
			CalcSeverity( false, false );		
		}
		return fSeverity < fMaxSeverity && fSeverity < 0;
		break;
	}
	return false;
}
bool CGeneralTaskToDefendPatch::EvaluateWorker( CCommonUnit *pUnit, const enum EForceType eType ) const
{
	switch( eType )
	{
	case FT_INFANTRY_IN_TRENCHES:
	case FT_FREE_INFANTRY:
	case FT_STATIONARY_MECH_UNITS:
		return SGeneralHelper::IsUnitInParcel( pUnit, patchInfo );

	case FT_MOBILE_TANKS:
		if ( pUnit->GetFirstArtilleryGun() != 0 )
			return false;
		else for ( CommonUnits::const_iterator it = enemyForces.begin(); it != enemyForces.end(); ++it )
		{
			NI_ASSERT_T( dynamic_cast_ptr<CAIUnit*>(*it) != 0, "wrong tank" );
			CAIUnit * pEnemy = static_cast_ptr<CAIUnit*>( *it );
			if ( 0 != pUnit->GetKillSpeed( pEnemy ) ) // можем пробить
				return true;
		}
		return false; 
	}
	return false;
}
bool CGeneralTaskToDefendPatch::EnumEnemy( class CAIUnit *pEnemy )
{
	if ( SGeneralHelper::IsUnitNearParcel( pEnemy, patchInfo ) )
	{
		SGeneralHelper::SSeverityCountPredicate pr;
		pr( pEnemy );
		fEnemyForce += pr.fCount;
		enemyForces.push_back( pEnemy );
	}
	return true;
}
CGeneralTaskToHoldReinforcement::CGeneralTaskToHoldReinforcement()
: fSeverity( 0 ), nCurReinforcePoint( 0 )
{
}
void CGeneralTaskToHoldReinforcement::Init( const SAIGeneralParcelInfo &_patchInfo )
{
	patchInfo = _patchInfo;
	fSeverity = 0;
}
void CGeneralTaskToHoldReinforcement::AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit )
{
	if ( !bInit && fMaxSeverity >= 0.0f )
	{
		pManager->EnumWorkers( FT_MOBILE_TANKS, this );
	}
}
void CGeneralTaskToHoldReinforcement::ReleaseWorker( ICommander *pManager, const float fMinSeverity )
{
	while ( fMinSeverity < 0 && !tanksFree.empty() )
	{
		CCommonUnit *pTank = *tanksFree.begin();
		tanksFree.pop_front();

		if ( pTank->IsValid() && pTank->IsAlive() )
			pManager->Give( pTank );
	}
}
void CGeneralTaskToHoldReinforcement::CancelTask( ICommander *pManager )
{
	for ( CommonUnits::iterator it = tanksFree.begin(); tanksFree.end() != it; ++it )
		pManager->Give( *it );
}
void CGeneralTaskToHoldReinforcement::Segment()
{
	if ( SGeneralHelper::RemoveDead( &tanksFree ) )
	{
		std::list< CPtr<CCommonUnit> > removed;
		for ( UnitsPositions::iterator it = unitsPositions.begin(); it != unitsPositions.end(); ++it )
		{
			CCommonUnit *pUnit = GetObjectByUniqueIdSafe<CCommonUnit>( it->first );
			if ( !pUnit || !pUnit->IsValid() || !pUnit->IsAlive() )
				removed.push_back( pUnit );
		}
	}
	SGeneralHelper::SSeverityCountPredicate pr1;
	pr1 = std::for_each( tanksFree.begin(), tanksFree.end(), pr1 );
	fSeverity = pr1.fCount;
}
bool CGeneralTaskToHoldReinforcement::EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType )
{
	NI_ASSERT_T( FT_MOBILE_TANKS == eType, "not tank reinforcement" );
	
	tanksFree.push_back( static_cast<CAIUnit*>(pUnit) );
	if ( SAIGeneralParcelInfo::EPATCH_UNKNOWN == patchInfo.eType )
	{
		if ( unitsPositions.find( pUnit->GetUniqueId() ) == unitsPositions.end() )
			unitsPositions[pUnit->GetUniqueId()] = pUnit->GetCenter();
		else
			theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_TO, unitsPositions[pUnit->GetUniqueId()] ), pUnit, false );
	}
	else
	{
		const CVec2 vTransReinforcePoint ( patchInfo.reinforcePoints[nCurReinforcePoint].vCenter.y, -patchInfo.reinforcePoints[nCurReinforcePoint].vCenter.x );
		const CVec2 vReinforcePoint( patchInfo.vCenter + ( vTransReinforcePoint ^ GetVectorByDirection(patchInfo.wDefenceDirection) ) );
		const CVec2 vLookPoint( vReinforcePoint + GetVectorByDirection( patchInfo.reinforcePoints[nCurReinforcePoint].wDir + patchInfo.wDefenceDirection )*100 );

		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_TO, vReinforcePoint ), pUnit, false );
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_ROTATE_TO, vLookPoint), pUnit, true );
		
		++nCurReinforcePoint;
		nCurReinforcePoint %= patchInfo.reinforcePoints.size();
	}
	return false; // take 1 worker at a time
}
float CGeneralTaskToHoldReinforcement::GetSeverity() const 
{ 
	return /*fSeverity*/0.0f; 
}
bool CGeneralTaskToHoldReinforcement::EvaluateWorker( CCommonUnit *pUnit, const enum EForceType eType ) const
{
	if ( FT_MOBILE_TANKS == eType && pUnit->GetFirstArtilleryGun() == 0 )
	{
		return true;
	}
	return false;
}
CGeneralTaskRecaptureStorage::CGeneralTaskRecaptureStorage( const CVec2 & vReinforcePoint )
: vReinforcePoint( vReinforcePoint ), 
	fSeverity( -SGeneralConsts::RECAPTURE_ARTILLERY_TANKS_NUMBER ),
	bFinished( false )
{
}
void CGeneralTaskRecaptureStorage::AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit )
{
	if ( !bInit && fMaxSeverity > fSeverity )
		pManager->EnumWorkers( FT_MOBILE_TANKS, this );
}
void CGeneralTaskRecaptureStorage::ReleaseWorker( ICommander *pManager, const float fMinSeverity )
{
	if ( bFinished )
	{
		while( !tanksFree.empty() )
		{
			pManager->Give( *tanksFree.begin() );
			tanksFree.pop_front();
		}
	}
}
bool CGeneralTaskRecaptureStorage::IsFinished() const 
{
	return bFinished;
}
void CGeneralTaskRecaptureStorage::CancelTask( ICommander *pManager )
{
	bFinished = true;
	ReleaseWorker( pManager, 0 );
}
void CGeneralTaskRecaptureStorage::Segment()
{
	SGeneralHelper::RemoveDead( &tanksFree );
	
	if ( 0 == fSeverity && tanksFree.empty() )
		bFinished = true;
}
bool CGeneralTaskRecaptureStorage::EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType )
{
	if ( FT_MOBILE_TANKS == eType )
	{
		tanksFree.push_back( pUnit );
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_TO, vReinforcePoint), pUnit );
		return ++fSeverity == 0;
	}
	NI_ASSERT_T( false, "wrong type of worker given" );
	return false;
}
bool CGeneralTaskRecaptureStorage::EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const
{
	return fabs2( pUnit->GetCenter() - vReinforcePoint ) < sqr(SGeneralConsts::RECAPTURE_STORAGE_MAX_DISTANCE);
}
CGeneralTaskToSwarmToPoint::CGeneralTaskToSwarmToPoint( IEnemyContainer *_pEnemyConatainer, CGeneral *_pOwner )
: nAdditionalIterations( 0 ), fSeverity( 0 ), bFinished( false ), 
	pEnemyConatainer( _pEnemyConatainer ), bReleaseWorkers( false ), timeNextCheck( 0 ), 
	eState( ESS_REST ), pOwner( _pOwner ), bResistanesBusyByUs( false )
{
	ClearResistanceToAcceptNewTask();
}
CGeneralTaskToSwarmToPoint::CGeneralTaskToSwarmToPoint() : pOwner( 0 ), bResistanesBusyByUs( false )
{
}
bool CGeneralTaskToSwarmToPoint::IsTimeToRun() const
{
	if ( curTime > timeNextCheck )
		return true;

	if ( !swarmingTanks.empty() )
	{
		for ( CTanks::const_iterator it = swarmingTanks.begin(); it != swarmingTanks.end(); ++it )
		{
			CCommonUnit *pUnit = *it;
			if ( pUnit->IsValid() && pUnit->IsAlive() && !pUnit->IsIdle())
				return false;
		}
	}
	return true;
}
void CGeneralTaskToSwarmToPoint::Run()
{
	if ( !swarmingTanks.empty() )
	{
		const int nGroup = theGroupLogic.GenerateGroupNumber();
		IRefCount **arUnits = GetTempBuffer<IRefCount*>( swarmingTanks.size() );
		for ( int i = 0; i < swarmingTanks.size(); ++i )
			arUnits[i] = swarmingTanks[i];
		theGroupLogic.RegisterGroup( arUnits, swarmingTanks.size(), nGroup );
		theGroupLogic.GroupCommand( SAIUnitCmd(ACTION_COMMAND_SWARM_TO, curResistanceToAttack.GetResistanceCellCenter()), nGroup, false );
	
		{
			IScene * pScene = GetSingleton<IScene>();
			IStatSystem *pStat = pScene->GetStatSystem();
			const int x = curResistanceToAttack.GetResistanceCellCenter().x;
			const int y = curResistanceToAttack.GetResistanceCellCenter().y;
			pStat->UpdateEntry( "General: (ATTACK):", NStr::Format( "Attacking, (%d,%d)", x, y ) );
		}
		pOwner->SetCellInUse( curResistanceToAttack.GetCellNumber(), true );
		bResistanesBusyByUs = true;
	}
	timeNextCheck = curTime + 1000* (SGeneralConsts::TIME_SWARM_DURATION + Random( SGeneralConsts::TIME_SWARM_DURATION_RANDOM ));
	eState = ESS_ATTACKING;


}
void CGeneralTaskToSwarmToPoint::AskForWorker( ICommander *pManager, const float _fMaxSeverity, const bool bInit )
{
	if ( !bInit && eState == ESS_REST && curResistanceToAttack.IsInitted() && fSeverity < 0 && swarmingTanks.empty() )
	{
		fMaxSeverity = _fMaxSeverity;
		pManager->EnumWorkers( FT_SWARMING_TANKS, this ); // resieved some tanks.
		SendToGroupPoint();
	}
}
void CGeneralTaskToSwarmToPoint::SendToGroupPoint()
{
	if ( !swarmingTanks.empty() )
	{
		const int nGroup = theGroupLogic.GenerateGroupNumber();
		IRefCount **arUnits = GetTempBuffer<IRefCount*>( swarmingTanks.size() );
		
		for ( int i = 0; i < swarmingTanks.size(); ++i )
		{
			if ( swarmingTanks[i]->IsValid() && swarmingTanks[i]->IsAlive() )
			{
				vPrepearCenter = swarmingTanks[i]->GetCenter();
				break;
			}
		}

		for ( int i = 0; i < swarmingTanks.size(); ++i )
		{
			if ( swarmingTanks[i]->IsValid() && swarmingTanks[i]->IsAlive() )
				theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_TO_NOT_PRESIZE,vPrepearCenter, 32*SConsts::TILE_SIZE), swarmingTanks[i], true );
		}
		
		eState = ESS_PREPEARING;
		theSupremeBeing.RegisterDelayedTask( new CGeneralSwarmWaitForReady(this) );
		timeNextCheck = curTime + 1000*(SGeneralConsts::TIME_TO_WAIT_SWARM_READY + Random( SGeneralConsts::TIME_TO_WAIT_SWARM_READY_RANDOM ));

		{
			IScene * pScene = GetSingleton<IScene>();
			IStatSystem *pStat = pScene->GetStatSystem();
			const int x = vPrepearCenter.x;
			const int y = vPrepearCenter.y;
			pStat->UpdateEntry( "General: (ATTACK):", NStr::Format( "Prepearing, (%d,%d)", x, y ) );
		}

	}
}
void CGeneralTaskToSwarmToPoint::ReleaseWorker( ICommander *pManager, const float fMinSeverity )
{
	if ( (!curResistanceToAttack.IsInitted() || bReleaseWorkers ) && !swarmingTanks.empty() )
	{
		for ( int i = 0; i < swarmingTanks.size(); ++i )
		{
			pManager->Give( swarmingTanks[i] );
		}
		swarmingTanks.clear();
		fSeverity = 0;
	}
}
float CGeneralTaskToSwarmToPoint::GetSeverity() const 
{ 
	return fSeverity; 
}
bool CGeneralTaskToSwarmToPoint::IsFinished() const
{
	return bFinished;
}
void CGeneralTaskToSwarmToPoint::CancelTask( ICommander *pManager )
{
	bFinished = true;
	ReleaseWorker( pManager, 0 );
}
void CGeneralTaskToSwarmToPoint::Segment()
{

	switch( eState )
	{
	case ESS_REST:
		if ( timeNextCheck < curTime )
		{
			timeNextCheck = curTime + 3000;		//
			fSeverity = 0;
			ClearResistanceToAcceptNewTask();
			pEnemyConatainer->GiveResistances( this );

			if ( !curResistanceToAttack.IsInitted() )
				bReleaseWorkers = true;
			else
			{
				SendToGroupPoint();
				nAdditionalIterations = Random( 0, SGeneralConsts::SWARM_ADDITIONAL_ITERATIONS );
				bReleaseWorkers = false;
			}
		}

		break;
	case ESS_PREPEARING:
		for ( int i = 0; i < swarmingTanks.size(); ++i )
		{
			if ( IsValidObj( swarmingTanks[i] ) )
			{
				break;
			}
		}
		eState = ESS_REST;
	
		break;
	case ESS_ATTACKING:
		
		{
			int nAlive = 0;
			int nIdle = 0;
			for ( int i = 0; i < swarmingTanks.size(); ++i )
			{
				if ( IsValidObj( swarmingTanks[i] ) )
				{
					++nAlive;
					if ( swarmingTanks[i]->IsIdle() || !swarmingTanks[i]->CanMove() )
						++nIdle;
				}
			}
			if ( timeNextCheck < curTime || nIdle == nAlive || 0 == nAlive )
			{
				if ( ( 0 == nAlive || nIdle == nAlive ) && nAdditionalIterations == 0 ) // don't want to swarm anymore
				{
					eState = ESS_REST;

					if ( bResistanesBusyByUs )
					{
						pOwner->SetCellInUse( curResistanceToAttack.GetCellNumber(), false );
						bResistanesBusyByUs = false;
					}

					curResistanceToAttack.Clear();

					bReleaseWorkers = true;
				}
				else  // will swarm another time
				{
					--nAdditionalIterations;

					if ( bResistanesBusyByUs )
					{
						pOwner->SetCellInUse( curResistanceToAttack.GetCellNumber(), false );
						bResistanesBusyByUs = false;
					}

					ClearResistanceToAcceptNewTask();
					pEnemyConatainer->GiveResistances( this );
					if ( curResistanceToAttack.IsInitted() )
						SendToGroupPoint();
				}
			}
		}

		break;
	}
}
void CGeneralTaskToSwarmToPoint::ClearResistanceToAcceptNewTask()
{
	{
		IScene * pScene = GetSingleton<IScene>();
		IStatSystem *pStat = pScene->GetStatSystem();
		pStat->UpdateEntry( "General: (ATTACK):", "" );
	}

	if ( bResistanesBusyByUs )
	{
		pOwner->SetCellInUse( curResistanceToAttack.GetCellNumber(), false );
		bResistanesBusyByUs = false;
	}
	
	curResistanceToAttack.Clear();

	vTanksPosition = CVec2( -1,-1 );
	fCurDistance = -1;
	int nAlive = 0;
	for ( int i = 0; i < swarmingTanks.size(); ++i )
	{
		if ( IsValidObj( swarmingTanks[i] ) )
		{
			vTanksPosition += swarmingTanks[i]->GetCenter();
			++nAlive;
		}
	}
	if ( nAlive )
		vTanksPosition /= nAlive;
}
bool CGeneralTaskToSwarmToPoint::EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType )
{
	NI_ASSERT_T( curResistanceToAttack.GetWeight() != -1, "wrong weight" );
	swarmingTanks.push_back( pUnit );
	fSeverity += pUnit->GetPriceMax();
	return fSeverity < fMaxSeverity;
}
float CGeneralTaskToSwarmToPoint::EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType eType ) const
{
	const CVec2 vFinishPoint( curResistanceToAttack.GetResistanceCellCenter() );
	return 1.0f / ( fabs2( vFinishPoint - pUnit->GetCenter() ) + 1 );
}
bool CGeneralTaskToSwarmToPoint::EnumResistances( const SResistance &resistance )
{
	if ( resistance.GetWeight() > SGeneralConsts::MIN_WEIGHT_TO_SEND_SWARM )
	{
		if ( vTanksPosition.x == -1 || vTanksPosition.y == -1 )
		{
			fSeverity = - resistance.GetWeight() * SGeneralConsts::SWARM_WEIGHT_COEFFICIENT;
			curResistanceToAttack = resistance;

			return false;
		}
		const float fDistance = fabs2( vTanksPosition - resistance.GetResistanceCellCenter() );
		if ( fCurDistance == -1 || fDistance < fCurDistance )
		{
			fSeverity = - resistance.GetWeight() * SGeneralConsts::SWARM_WEIGHT_COEFFICIENT;
			curResistanceToAttack = resistance;
			fCurDistance = fDistance;
		}
	}
	return true;
}
bool CGeneralTaskToSwarmToPoint::EvaluateWorker( CCommonUnit *pUnit, const enum EForceType eType ) const
{
	if ( pUnit->GetFirstArtilleryGun() != 0 )
		return false;
	
	return true;
}
