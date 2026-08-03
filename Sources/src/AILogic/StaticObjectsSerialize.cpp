#include "StdAfx.h"

#include "StaticObjects.h"
#include "StaticObject.h"
#include "Entrenchment.h"
#include "Building.h"
#include "StormableObject.h"
#include "Mine.h"
#include "Bridge.h"
#include "ArtilleryBulletStorage.h"
#include "SerializeOwner.h"
#include "Fence.h"
#include "SmokeScreen.h"
#include "Flag.h"
#include "SaveDBID.h"
int CStormableObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &attackers );
	saver.Add( 2, &nAttackers );
	saver.Add( 3, &nActiveAttackers );
	saver.Add( 4, &bAttackers );
	saver.Add( 5, &startTimes );
	saver.Add( 7, &lastSegment );

	return 0;
}
int CGivenPassabilityStObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CExistingObject*>(this) ); 
	saver.Add( 2, &center );
	saver.Add( 3, &boundRect );
	saver.Add( 4, &lockInfo );
	saver.Add( 5, &lockTypes );
	saver.Add( 6, &bPartially );
	saver.Add( 7, &bTransparencySet );
	saver.Add( 8, &canSetTransparency );

	return 0;
}
int CCommonStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) ); 
	saver.Add( 3, &eType );

	return 0;
}
int CSimpleStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCommonStaticObject*>(this) );
	saver.Add( 2, &pStats );
	saver.Add( 3, &nPlayer );
	saver.Add( 4, &bDelayedUpdate );
	return 0;
}
int CBuilding::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) ); 
	saver.Add( 2, &pStats );
	saver.Add( 6, &medical );
	saver.Add( 7, &fire );
	saver.Add( 8, &rest );
	saver.Add( 9, &nOveralPlaces );
	saver.Add( 10, &startOfRest );
	saver.Add( 11, &bAlarm );
	saver.Add( 14, &nIterator );
	saver.AddTypedSuper( 16, static_cast<CStormableObject*>(this) );
	SerializeOwner( 17, &pLockingUnit, &saver );
	saver.Add( 21, &turrets );
	saver.Add( 22, &guns );
	saver.Add( 24, &nextSegmTime );
	saver.Add( 25, &lastDistibution );

	saver.Add( 26, &observationPlaces );
	saver.Add( 27, &sides );
	saver.Add( 29, &firePlace2Soldier );

	saver.Add( 30, &observationPlaces );
	saver.Add( 31, &sides );
	saver.Add( 32, &firePlace2Observation );
	saver.Add( 33, &firePlace2Soldier );
	saver.Add( 34, &nLastFreeFireSoldierChoice );
	
	saver.Add( 35, &nLastPlayer );
	saver.Add( 36, &nScriptID );

	saver.Add( 37, &bShouldEscape );
	saver.Add( 38, &bEscaped );
	saver.Add( 39, &timeOfDeath );
	saver.Add( 40, &lastLeave );

	if ( lastLeave.empty() )
		lastLeave.resize( 16, 0 );

	return 0;
}
int CBuildingStorage::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CBuilding*>(this) ); 
	saver.Add( 2, &nPlayer );
	saver.Add( 3, &bConnected );
	saver.Add( 4, &timeLastBuildingRepair );
	return 0;
}
int CBuildingSimple::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CBuilding*>(this) ); 
	return 0;
}
int CMineStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) );
	saver.Add( 2, &pStats );
	saver.Add( 3, &player );
	saver.Add( 4, &mVisibleStatus );
	saver.Add( 5, &bIfWillBeDeleted );
	saver.Add( 6, &bIfRegisteredInCWorld );
	saver.Add( 7, &nextSegmTime );
	saver.Add( 8, &bAlive );

	return 0;
}
int CEntrenchment::SFireplaceInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &center );
	saver.Add( 2, &pUnit );
	saver.Add( 3, &nFrameIndex );

	return 0;
}
int CEntrenchment::SInsiderInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pUnit );
	saver.Add( 2, &nFireplace );
	return 0;
}
int CEntrenchment::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CStaticObject*>(this) ); 
	saver.Add( 2, &rect );
	saver.Add( 3, &z );
	saver.Add( 4, &nBusyFireplaces );
	saver.Add( 5, &fireplaces );
	saver.Add( 6, &insiders );
	saver.Add( 7, &pStats );
	saver.AddTypedSuper( 10, static_cast<CStormableObject*>(this) );
	saver.Add( 11, &nextSegmTime );
	saver.AddTypedSuper( 12, static_cast<CRotatingFireplacesObject*>(this) );

	return 0;
}
int CEntrenchmentPart::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CExistingObject*>(this) ); 
	saver.Add( 2, &pStats );
	saver.Add( 3, &pOwner );
	saver.Add( 4, &center );
	saver.Add( 5, &dir );
	saver.Add( 6, &boundRect );
	saver.Add( 7, &bVisible );
	saver.Add( 8, &coveredTiles );
	saver.Add( 9, &pFullEntrenchment );
	saver.Add( 10, &nextSegmTime );

	return 0;
}
int CEntrenchmentTankPit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) ); 
	saver.Add( 2, &pStats );
	saver.Add( 4, &wDir );
	saver.Add( 5, &vHalfSize );
	saver.Add( 6, &boundRect );
	saver.Add( 7, &tilesToLock );
	saver.Add( 8, &pOwner );

	return 0;
}
/*
int CStaticObjects::CStoragesContainer2::CData::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &initials ); 
	saver.Add( 2, &connected );
	saver.Add( 3, &newlyFound );
	saver.Add( 4, &notConnected );
	saver.Add( 5, &bEnd );
	saver.Add( 6, &nParty );
	saver.Add( 7, &bFoundConnected );
	saver.Add( 8, &totalUnconnected );
	return 0;
}
int CStaticObjects::CStoragesContainer2::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &possibleConnected );
	saver.Add( 2, &mains );
	saver.Add( 3, &states );
	saver.Add( 4, &nextRecountStates );
	saver.Add( 5, &recountStates );
	saver.Add( 6, &pPointChecker );
	saver.Add( 7, &bNeedUpdate);															// recount is needed
	saver.Add( 8, &nPartyUpdated );														// if recount is performed then for that party
	saver.Add( 9, &bIgnoreUpdates );
	saver.Add( 10, &bPossibleConnectedMapInitted );
	saver.Add( 11, &bUpdateInProgress );												// update is performed
	saver.Add( 12, &timeNextUpdate );
	saver.Add( 13, &bInitialized );
	saver.Add( 14, &pData );
	return 0;
}*/
int CStaticObjects::CStoragesContainer::CPartyInfo::
operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &mains );
	saver.Add( 2, &secondary );
	return 0;
}
int CStaticObjects::CStoragesContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &storages );
	saver.Add( 4, &updated );
	saver.Add( 23, &storageSystem );

	if ( saver.IsReading() && storageSystem.empty() )
	{
		storageSystem.resize( 2 );
		bInitOnSegment = true;
	}
	else
		bInitOnSegment = false;

	return 0;
}
int CStaticObjects::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &entrenchments );
	saver.Add( 3, &areaMap );
	saver.Add( 4, &nObjs );
	saver.Add( 5, &segmObjects );
	saver.Add( 6, &deletedObjects );
	saver.Add( 7, &terraObjs );
	saver.Add( 8, &unregisteredObjects );
	saver.Add( 10, &burningObjects );
	saver.Add( 11, &storagesContainer );
	saver.Add( 12, &obstacles );
	saver.Add( 13, &obstacleObjects );
	saver.Add( 14, &containersAreaMap );
	saver.Add( 15, &bIterCreated );

	return 0;
}
int CExistingObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CStaticObject*>(this) ); 
	saver.Add( 2, &mark );
	saver.Add( 4, &nFrameIndex );
	saver.Add( 5, &fHP );
	saver.Add( 6, &burningEnd );

	if ( saver.IsReading() )
	{
		LoadDBID( &saver, 7, &dbID );

		if ( dbID == -1 )
			saver.Add( 3, &dbID );
	}
	else
		SaveDBID( &saver, 7, dbID );

	return 0;
}
int CStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CLinkObject*>(this) );

	return 0;
}
int CBridgeSpan::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) );
	saver.Add( 2, &pStats );
	saver.Add( 3, &unlockTypes );
	saver.Add( 4, &pFullBridge );
	saver.Add( 5, &bNewBuilt );
	saver.Add( 6, &bLocked );
	saver.Add( 7, &bDeletingAround );
	saver.Add( 8, &nScriptID );

	return 0;
}
int CArtilleryBulletStorage::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CGivenPassabilityStObject*>(this) );
	saver.Add( 2, &pStats );
	SerializeOwner( 3, &pOwner, &saver );

	return 0;
}
int CFullEntrenchment::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( !saver.IsReading() )
	{
		int nSize = entrenchParts.size();
		saver.Add( 1, &nSize );
		int cnt = 2;
		for ( std::list<CEntrenchmentPart*>::iterator iter = entrenchParts.begin(); iter != entrenchParts.end(); ++iter )
			SerializeOwner( cnt++, &(*iter), &saver );
	}
	else
	{
		int nSize;
		saver.Add( 1, &nSize );

		entrenchParts.clear();
		entrenchParts.resize( nSize );

		int cnt = 2;
		for ( std::list<CEntrenchmentPart*>::iterator iter = entrenchParts.begin(); iter != entrenchParts.end(); ++iter )
			SerializeOwner( cnt++, &(*iter), &saver );
	}

	return 0;
}
int CFullBridge::SSpanLock::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &formerTiles );
	SerializeOwner( 3, &pSpan, &saver );
	saver.Add( 4, &tiles );
	return 0;
}
int CFullBridge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	int cnt = 0;
	saver.Add( ++cnt, &bGivingDamage );

	if ( !saver.IsReading() )
	{
		int nSize = spans.size();
		saver.Add( ++cnt, &nSize );
		for ( std::list<CBridgeSpan*>::iterator iter = spans.begin(); iter != spans.end(); ++iter )
			SerializeOwner( ++cnt, &(*iter), &saver );
		
		nSize = projectedSpans.size();
		saver.Add( ++cnt, &nSize );
		for ( std::list<CBridgeSpan*>::iterator iter = projectedSpans.begin(); iter != projectedSpans.end(); ++iter )
			SerializeOwner( ++cnt, &(*iter), &saver );

	}
	else
	{
		int nSize;
		saver.Add( ++cnt, &nSize );
		spans.clear();
		spans.resize( nSize );
		for ( std::list<CBridgeSpan*>::iterator iter = spans.begin(); iter != spans.end(); ++iter )
			SerializeOwner( ++cnt, &(*iter), &saver );

		saver.Add( ++cnt, &nSize );
		projectedSpans.clear();
		projectedSpans.resize( nSize );
		for ( std::list<CBridgeSpan*>::iterator iter = projectedSpans.begin(); iter != projectedSpans.end(); ++iter )
			SerializeOwner( ++cnt, &(*iter), &saver );
	}

	saver.Add( ++cnt, &lockedSpans );
	saver.Add( ++cnt, &nSpans );
	return 0;
}
int CRotatingFireplacesObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &units );

	return 0;
}
int CRotatingFireplacesObject::SUnitInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pSoldier, &saver );
	saver.Add( 2, &nLastFireplace );
	saver.Add( 3, &lastFireplaceChange );

	return 0;
}
int CFence::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.AddTypedSuper( 1, static_cast<CCommonStaticObject*>(this) );
	saver.Add( 2, &pStats );
	saver.Add( 3, &nDir );
	saver.Add( 4, &leftTile );
	saver.Add( 5, &rightTile );
	saver.Add( 6, &eLifeType );
	saver.Add( 7, &neighFences );
	saver.Add( 8, &dirToBreak );
	saver.Add( 9, &nCreator );
	saver.Add( 10, &bSuspendAppear );

	return 0;
}
int CTerraMeshStaticObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CCommonStaticObject*>(this) );
	saver.Add( 2, &pStats );
	saver.Add( 3, &wDir );

	return 0;
}
int CSmokeScreen::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CExistingObject*>(this) );
	saver.Add( 2, &vCenter );
	saver.Add( 3, &tileCenter );
	saver.Add( 4, &fRadius );
	saver.Add( 5, &nTransparency );
	saver.Add( 6, &timeOfDissapear );

	saver.Add( 7, &nextSegmTime );
	saver.Add( 8, &bTransparencySet );

	return 0;
}
int CFlag::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 11, static_cast<CCommonStaticObject*>(this) );

	saver.Add( 1, &pStats );
	saver.Add( 2, &nParty );
	saver.Add( 3, &nextSegmentTime );
	saver.Add( 4, &bGoingToCapture );
	saver.Add( 5, &nPartyToCapture );
	saver.Add( 6, &timeOfStartCapturing );
	saver.Add( 7, &lastSegmentTime );
	saver.Add( 8, &nPlayerToCapture );
	saver.Add( 9, &timeOfStartNeutralPartyCapturing );
	saver.Add( 10, &bCapturingByNeutral );
	
	return 0;
}
