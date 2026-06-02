#include "stdafx.h"

#include "TransportStates.h"
#include "SerializeOwner.h"
int CTransportWaitPassengerState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pTransport, &saver );
	saver.Add( 2, &formationsToWait );

	return 0;
}
int CTransportLoadRuState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );
	saver.Add( 2, &eState);
	saver.Add( 3, &pLoaderSquad);
	saver.Add( 4, &nEntrance );
	saver.Add( 5, &bSubState );
	return 0;
}
int CTransportLandState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	saver.Add( 2, &vLandPoint );
	SerializeOwner( 4, &pTransport, &saver );
	
	return 0;
}
int CTransportServeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );

	saver.Add( 2, &eState);
	saver.Add( 3, &pLoaderSquad);
	saver.Add( 4, &pResupplyUnit);
	saver.Add( 5, &timeLastUpdate );
	saver.Add( 6, &vServePoint );
	saver.Add( 8, &pStaticPath );
	saver.Add( 9, &bUpdatedActionsBegin );
	saver.Add( 10, &pPreferredUnit );
	saver.Add( 11, &bWaitForPath );
	return 0;
}
int CTransportResupplyState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );
	saver.AddTypedSuper( 2, static_cast<CTransportServeState*>(this) );
	return 0;
}
int CTransportRepairState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );
	saver.AddTypedSuper( 2, static_cast<CTransportServeState*>(this) );
	return 0;
}
int CTransportResupplyHumanResourcesState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );

	saver.Add( 2, &eState );
	saver.Add( 3, &vServePoint );
	saver.Add( 4, &notCompleteSquads );
	saver.Add( 5, &emptyArtillery );
	saver.Add( 6, &timeLastUpdate );
	saver.Add( 7, &pPreferredUnit );
	saver.Add( 8, &bWaitForPath );
	return 0;
}
int CTransportHookArtilleryState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &pArtillery );
	saver.Add( 5, &timeLast );
	saver.Add( 6, &wDesiredTransportDir );
	saver.Add( 7, &bInterrupted );
	saver.Add( 8, &vArtilleryPoint );
	return 0;
}
int CTransportUnhookArtilleryState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	SerializeOwner( 1, &pTransport, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &vDestPoint );
	saver.Add( 4, &bInterrupted );
	saver.Add( 5, &nAttempt );
	saver.Add( 6, &bNow ); 
	return 0;
}
int CTransportBuildState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pUnit, &saver );
	saver.Add( 2, &eState );
	saver.Add( 3, &pLoadRuSubState );
	saver.Add( 4, &vStartPoint );
	saver.Add( 5, &vEndPoint );
	saver.Add( 6, &pEngineers );
	return 0;
}
int CTransportBuildLongObjectState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &pCreation );
	return 0;
}
int CTransportBuildFenceState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildLongObjectState*>(this) );
	return 0;
}
int CTransportBuildEntrenchmentState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildLongObjectState*>(this) );
	return 0;
}
int CTransportClearMineState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &timeCheckPeriod );
	saver.Add( 3, &timeLastCheck );
	saver.Add( 4, &bWorkDone );
	return 0;
}
int CTransportPlaceMineState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &nNumber );
	saver.Add( 3, &bWorkDone );
	return 0;
}
int CTransportPlaceAntitankState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &bWorkFinished );
	saver.Add( 3, &bSent );
	return 0;
}
int CTransportRepairBridgeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &pBridgeToRepair );
	saver.Add( 3, &bSentToBuildPoint );

	return 0;
}
int CTransportBuildBridgeState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &pFullBridge );
	saver.Add( 3, &pCreation );
	saver.Add( 4, &bTransportSent );
	return 0;
}
int CTransportRepairBuildingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTransportBuildState*>(this) );
	saver.Add( 2, &pBuilding );
	saver.Add( 3, &bSentToBuildPoint );
	return 0;
}
int CMoveToPointNotPresize::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fRadius );
	saver.Add( 2, &vPurposePoint );
	SerializeOwner( 1, &pTransport, &saver );

	return 0;
}