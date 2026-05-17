#include "stdafx.h"

#include <float.h>

#include "AILogicInternal.h"
#include "Commands.h"
#include "Units.h"
#include "UnitsIterators.h"
#include "PathFinder.h"
#include "updater.h"
#include "StaticObjects.h"
#include "AIStaticMap.h"
#include "GroupLogic.h"
#include "Entrenchment.h"
#include "StaticObject.h"
#include "UnitStates.h"
#include "HitsStore.h"
#include "Diplomacy.h"
#include "CommonUnit.h"
#include "UnitCreation.h"
#include "EntrenchmentCreation.h"
#include "AntiArtilleryManager.h"
#include "Cheats.h"
#include "GlobalObjects.h"
#include "AIWarFog.h"
#include "AckManager.h"
#include "PathUnit.h"
#include "SuspendedUpdates.h"
#include "CombatEstimator.h"
#include "Statistics.h"
#include "RailroadGraph.h"
#include "TrainPathFinder.h"
#include "TrainPathUnit.h"
#include "General.h"
#include "MultiplayerInfo.h"
#include "Weather.h"
#include "DifficultyLevel.h"
#include "Graveyard.h"
#include "Trigonometry.h"
#include "GridCreation.h"
#include "Bridge.h"
#include "Artillery.h"
#include "Technics.h"
#include "Formation.h"
#include "Shell.h"

#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Main\CommandsHistoryInterface.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Main\GameStats.h"

#include "..\RandomMapGen\Resource_Types.h"
#include "..\RandomMapGen\RMG_Types.h"

#include "MPLog.h"
#include "..\Scene\Scene.h"

#include "Guns.h"

//CRAP{ for profiling
#ifdef _PROFILER
#include "VTuneAPI.h"
#endif // _PROFILER
//CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CWeather theWeather;
extern CSupremeBeing theSupremeBeing;
extern CCombatEstimator theCombatEstimator;
extern CSuspendedUpdates theSuspendedUpdates;
extern CAckManager theAckManager;
extern CStaticMap theStaticMap;
extern CGroupLogic theGroupLogic;
extern CUnits units;
extern CUpdater updater;
extern CStaticObjects theStatObjs;
CPtr<IStaticPathFinder> pThePathFinder;
CPtr<IStaticPathFinder> pThePlanePathFinder;
CPtr<IStaticPathFinder> pTheTrainPathFinder;
NTimer::STime curTime;
extern CHitsStore theHitsStore;
extern CDiplomacy theDipl;
extern CGlobalWarFog theWarFog;
extern CUnitCreation theUnitCreation;
extern CAntiArtilleryManager theAAManager;
extern SCheats theCheats;
extern CStatistics theStatistics;
extern CRailroadGraph theRailRoadGraph;
CAILogic *pAILogic;
extern CMultiplayerInfo theMPInfo;
extern CDifficultyLevel theDifficultyLevel;
extern CGraveyard theGraveyard;

// for debug
extern CShellsStore theShellsStore;
NTimer::STime timeToLogStart;
NTimer::STime timeToLogFinish;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 								   CAILogic																		*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogic::CAILogic()
{
	pAILogic = this;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::Clear()
{
	CQueueUnit::Clear();
	NGlobalObjects::Clear();
	CLinkObject::Clear();
	CAICommand::Clear();

	DestroyContents();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CAILogic::LockAvitaionAppearPoint()
{
	theUnitCreation.LockAppearPoint( theDipl.GetMyNumber(), true);
	return theUnitCreation.GetFirstIntercectWithMap( theDipl.GetMyNumber() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UnlockAviationAppearPoint()
{
	theUnitCreation.LockAppearPoint( theDipl.GetMyNumber(), false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LoadAvailableTrucks()
{
	//�������� ������ templates ��������, ������� �������� ��� settings ������� �������
	std::string szChapterName = GetGlobalVar( "Chapter.Current.Name" );

	NStr::ToLower( szChapterName );
	const SChapterStats *pChapterStats = NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER );

	if ( pChapterStats )
	{
		SRMContext context;
		if ( LoadDataResource( pChapterStats->szContextName, "", false, 0, RMGC_CONTEXT_NAME, context ) )
		{
			std::unordered_set<std::string> availiableUnits;
			if ( context.GetAvailiableUnits( 0, RPG_TYPE_TRN_CARRIER, &availiableUnits ) > 0 )
			{
				for ( std::unordered_set<std::string>::const_iterator unitIterator = availiableUnits.begin(); unitIterator != availiableUnits.end(); ++unitIterator )
				{
					availableTrucks.insert( ( NGDB::GetRPGStats<SMechUnitRPGStats>( unitIterator->c_str() ) ) );
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::CheckForScenarioTruck( const SMapObjectInfo &object, IObjectsDB *pIDB, const SGDBObjectDesc *pDesc, const int nDBIndex, LinkInfo *linksInfo, const SMechUnitRPGStats **pNewStats ) const
{
	if ( !theDipl.IsNetGame() )
	{
		NI_ASSERT_T( dynamic_cast<const SUnitBaseRPGStats*>(pIDB->GetRPGStats( pDesc )) != 0, NStr::Format( "Object (%s) was passed as SGVOGT_UNIT", pDesc->GetName() ) );
		CGDBPtr<SUnitBaseRPGStats> pStats = static_cast<const SUnitBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );

		// ��������� � � ���-�� ���������
		if ( pStats->IsTransport() && object.link.nLinkWith > 0 )
		{
			IRefCount *pObj = CLinkObject::GetObjectByLink( object.link.nLinkWith );
			// ������ ������, � ������� ���������
			if ( pObj )
			{
				CArtillery *pArtillery = dynamic_cast<CArtillery*>( pObj );
	
				// ��������� � �����������
				if ( pArtillery )
				{
					if ( !pArtillery->GetStats()->availExposures.GetData( ACTION_COMMAND_TAKE_ARTILLERY ) )
						return false;

					// ���������� �����������, ����� �������� �� ����������� ��������
					if ( pArtillery->GetScenarioUnit() )
					{
						*pNewStats = 0;
						const float fWeight = pArtillery->GetStats()->fWeight;

						for ( CAvailTrucks::iterator iter = availableTrucks.begin(); iter != availableTrucks.end(); ++iter )
						{
							const float fTowingForce = (*iter)->fTowingForce;
							if ( fTowingForce >= fWeight && ( *pNewStats == 0 || fTowingForce < (*pNewStats)->fTowingForce ) )
								*pNewStats = *iter;
						}

						NI_ASSERT_T( *pNewStats != 0, NStr::Format( "Truck for artillery %s not found", pArtillery->GetStats()->szKeyName.c_str() ) );
					}
				}
				else
					return false;
			}
			// ��������� � �������������� ��������
			else
				return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SendAcknowlegdementForced( IRefCount *pObj, const EUnitAckType eAck )
{
	if ( pObj && pObj->IsValid() )
	{
		CCommonUnit *pUnit = checked_cast<CCommonUnit*>( pObj );
		pUnit->SendAcknowledgement( eAck, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CAILogic::AddObject( const SMapObjectInfo &object, IObjectsDB *pIDB, LinkInfo *linksInfo, bool bInitialization, bool IsEditor, const SHPObjectRPGStats *pPassedStats )
{
	IUpdatableObj *pResult = 0;

	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( object.szName.c_str() );
	NI_ASSERT_TF( pDesc != 0, NStr::Format("Can't find DB object description for \"%s\"", object.szName.c_str()), return 0 );
	const int nDBIndex = pIDB->GetIndex( pPassedStats != 0 ? pPassedStats->szParentName.c_str() : object.szName.c_str() );

	switch ( pDesc->eGameType )
	{
		case SGVOGT_UNIT:
		{
			if ( theDipl.IsPlayerExist( object.nPlayer ) )
			{
				const SMechUnitRPGStats *pNewStats = 0;
				if ( IsEditor || CheckForScenarioTruck( object, pIDB, pDesc, nDBIndex, linksInfo, &pNewStats ) )
				{
					const int nPlayer = object.nPlayer;
					const WORD wDir = object.nDir;
					WORD id;
					if ( pNewStats != 0 )
					{
						id = theUnitCreation.AddNewUnit
						(
							pNewStats, object.fHP, object.vPos.x, object.vPos.y, object.vPos.z, 
							pIDB->GetIndex( pNewStats->szParentName.c_str() ), 
							wDir, nPlayer, pDesc->eVisType, bInitialization, true, IsEditor 
						);
					}
					else if ( pPassedStats != 0 )
					{
						NI_ASSERT_T( dynamic_cast<const SUnitBaseRPGStats*>(pPassedStats) != 0, NStr::Format( "Unit expected, passed object (%s)", pPassedStats->szKeyName.c_str() ) );
						const SUnitBaseRPGStats *pStats = static_cast<const SUnitBaseRPGStats*>(pPassedStats);
						id = theUnitCreation.AddNewUnit( pStats, object.fHP, object.vPos.x, object.vPos.y, object.vPos.z, nDBIndex, wDir, nPlayer, pDesc->eVisType, bInitialization, true, IsEditor );
					}
					else
						id = theUnitCreation.AddNewUnit( object.szName, pIDB, object.fHP, object.vPos.x, object.vPos.y, object.vPos.z, wDir, nPlayer, bInitialization, IsEditor );

					pResult = units[id];
					scripts.AddObjToScriptGroup( units[id], object.nScriptID );

					if ( !GetGlobalVar( "nogeneral", 0 ) &&
							 !theDipl.IsNetGame() &&
							 !bInitialization && theSupremeBeing.IsMobileReinforcement( theDipl.GetNParty(nPlayer), object.nScriptID ) )
						theSupremeBeing.AddReinforcement( units[id] );
				}
			}

			break;
		}

		case SGVOGT_SQUAD:
		{
			if ( theDipl.IsPlayerExist( object.nPlayer ) )
			{
				CGDBPtr<SSquadRPGStats> pStats = static_cast<const SSquadRPGStats*>(pPassedStats);
				if ( pStats == 0 )
					pStats = static_cast<const SSquadRPGStats*>( pIDB->GetRPGStats( pDesc ) );

				const int nPlayer = object.nPlayer;
				const WORD wDir = object.nDir;
				const int nFormation = object.nFrameIndex;

				pResult = theUnitCreation.AddNewFormation( pStats, nFormation, object.fHP, object.vPos.x, object.vPos.y, object.vPos.z, wDir, nPlayer, bInitialization );
				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}

		case SGVOGT_BUILDING:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SBuildingRPGStats> pStats = static_cast<const SBuildingRPGStats*>(pPassedStats);
				if ( pStats == 0 )
					pStats = static_cast<const SBuildingRPGStats*>( pIDB->GetRPGStats( pDesc ) );

				switch ( pStats->eType )
				{
					case SBuildingRPGStats::TYPE_MAIN_RU_STORAGE:
					case SBuildingRPGStats::TYPE_TEMP_RU_STORAGE:
						{
							int nPlayer = object.nPlayer;
							pResult = theStatObjs.AddNewStorage( pStats, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, nPlayer, bInitialization );
							scripts.AddObjToScriptGroup( pResult, object.nScriptID );
						}
						break;
					default: //case SBuildingRPGStats::TYPE_BULDING:
						pResult = theStatObjs.AddNewBuilding( pStats, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, bInitialization );
						scripts.AddObjToScriptGroup( pResult, object.nScriptID );
						break;
				}
			}
			break;
		}
		
		case SGVOGT_ENTRENCHMENT:
		{
			CGDBPtr<SEntrenchmentRPGStats> pStats = static_cast<const SEntrenchmentRPGStats*>( pPassedStats );
			if ( pStats == 0 )
				pStats = static_cast<const SEntrenchmentRPGStats*>( pIDB->GetRPGStats( pDesc ) );

			pResult = theStatObjs.AddNewEntrencmentPart( pStats, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nDir, object.nFrameIndex, bInitialization );
			scripts.AddObjToScriptGroup( pResult, object.nScriptID );

			break;
		}

		case SGVOGT_MINE:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SMineRPGStats> pStats = static_cast<const SMineRPGStats*>( pPassedStats );
				if ( pStats == 0 )
					pStats = static_cast<const SMineRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	  		pResult = theStatObjs.AddNewMine( pStats, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, object.nPlayer, bInitialization );
				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}
		
		case SGVOGT_TERRAOBJ:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SObjectBaseRPGStats> pObjDesc = static_cast<const SObjectBaseRPGStats*>( pPassedStats );
				if ( pObjDesc == 0 )
					pObjDesc = static_cast<const SObjectBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );
				
				if ( pDesc->eVisType == SGVOT_MESH )
					pResult = theStatObjs.AddNewTerraMeshObj( pObjDesc, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nDir, object.nFrameIndex, bInitialization );
				else
					pResult = theStatObjs.AddNewTerraObj( pObjDesc, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, bInitialization );

				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}
		
		case SGVOGT_BRIDGE:
		{
			CGDBPtr<SBridgeRPGStats> pObjDesc = static_cast<const SBridgeRPGStats*>( pPassedStats );
			if ( pObjDesc == 0 )
				pObjDesc = static_cast<const SBridgeRPGStats*>( pIDB->GetRPGStats( pDesc ) );

			pResult = theStatObjs.AddNewBridgeSpan( pObjDesc, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nDir, object.nFrameIndex, bInitialization );
			scripts.AddObjToScriptGroup( pResult, object.nScriptID );

			break;
		}
		
		case SGVOGT_FENCE:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SFenceRPGStats> pObjDesc = static_cast<const SFenceRPGStats*>( pPassedStats );
				if ( pObjDesc == 0 )
					pObjDesc = static_cast<const SFenceRPGStats*>( pIDB->GetRPGStats( pDesc ) );

				pResult = theStatObjs.AddNewFenceObject( pObjDesc, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, object.nPlayer, bInitialization, IsEditor );
				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}

		case SGVOGT_FLAG:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SStaticObjectRPGStats> pObjDesc = 
					pPassedStats ? 
						static_cast<const SStaticObjectRPGStats*>( pPassedStats ) : 
						static_cast<const SStaticObjectRPGStats*>( pIDB->GetRPGStats( pDesc ) );

				pResult = theStatObjs.AddNewFlag( pObjDesc, object.fHP, nDBIndex, CVec2( object.vPos.x, object.vPos.y ), object.nFrameIndex, object.nPlayer, bInitialization );
				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}
		case SGVOGT_SOUND:
			return 0;

		default:
		{
			if ( theCheats.GetLoadObjects() )
			{
				CGDBPtr<SObjectBaseRPGStats> pObjDesc = static_cast<const SObjectBaseRPGStats*>( pPassedStats );
				if ( pObjDesc == 0 )
					pObjDesc = static_cast<const SObjectBaseRPGStats*>( pIDB->GetRPGStats( pDesc ) );

				pResult = theStatObjs.AddNewStaticObject( pObjDesc, object.fHP, nDBIndex, CVec2(object.vPos.x, object.vPos.y), object.nFrameIndex, bInitialization );
				scripts.AddObjToScriptGroup( pResult, object.nScriptID );
			}

			break;
		}
	}

	if ( pResult != 0 )
	{
		NI_ASSERT_T( dynamic_cast<CLinkObject*>( pResult ) != 0, NStr::Format("Wrong object of type \"%s\" created - CLinkObject expected", typeid(*pResult).name()) );
		CLinkObject *pLinkResult = static_cast<CLinkObject*>( pResult );
		pLinkResult->SetLink( object.link.nLinkID );

		if ( linksInfo != 0 && pLinkResult->GetUniqueId() != 0 )
			(*linksInfo)[pLinkResult] = object.link;
	}

	return pResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LoadUnits( const SLoadMapInfo &mapInfo, LinkInfo *linksInfo )
{
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();

	std::list<int> transports;
	for ( int i = 0; i < mapInfo.objects.size(); ++i )
	{
		// � reinforcement
		const int nGroup = mapInfo.reinforcements.GetGroupById( mapInfo.objects[i].nScriptID );
		if ( nGroup != -1 )
			scripts.AddUnitToReinforcGroup( mapInfo.objects[i], nGroup, 0, 0 );
		else
		{
			const IGDBObject *pObject = NGDB::GetRPGStats<IGDBObject>( mapInfo.objects[i].szName.c_str() );
			const SUnitBaseRPGStats *pStats = dynamic_cast<const SUnitBaseRPGStats*>( pObject );

			if ( !pStats || !pStats->IsTransport() )
				AddObject( mapInfo.objects[i], pIDB, linksInfo, true, false, 0 );
			else
				transports.push_back( i );
		}
	}

	for ( std::list<int>::iterator iter = transports.begin(); iter != transports.end(); ++iter )
		AddObject( mapInfo.objects[*iter], pIDB, linksInfo, true, false, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitLinks( LinkInfo &linksInfo )
{
	std::set<int> locomotives;
	// �����, ������� ��������� � �����
	std::unordered_map<int, int> nextCarriages;
	
	for ( LinkInfo::iterator iter = linksInfo.begin(); iter != linksInfo.end(); ++iter )
	{
		bool bTrain = false;
		if ( CAIUnit *pTrain = dynamic_cast<CAIUnit*>(iter->first) )
		{
			if ( pTrain->GetStats()->IsTrain() )
			{
				bTrain = true;				
				// ���������
				if ( linksInfo[pTrain].nLinkWith <= 0 )
					locomotives.insert( pTrain->GetID() );
				else
				{
					IRefCount *pObject = CLinkObject::GetObjectByLink( linksInfo[pTrain].nLinkWith );
					NI_ASSERT_T( dynamic_cast<CAIUnit*>(pObject) != 0, NStr::Format( "Train (%s) links with non unit", pTrain->GetStats()->szKeyName.c_str() ) );
					CAIUnit *pUnit = static_cast<CAIUnit*>(pObject);
					nextCarriages[pUnit->GetID()] = pTrain->GetID();
				}
			}
		}

		if ( !bTrain )
		{
			if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(iter->first) )
			{
				const SMapObjectInfo::SLinkInfo &link = linksInfo[pUnit];
				// � ���-�� ��������� � ���� ���-�� ����������
				if ( link.nLinkWith > 0 )
				{
					IRefCount *pObj = CLinkObject::GetObjectByLink( link.nLinkWith );
					if ( pObj )
					{
						//�������� ���������� � ������
						if ( CArtillery *pArtillery = dynamic_cast<CArtillery*>( pObj ) )
						{
							if ( CAITransportUnit *pTransport = dynamic_cast<CAITransportUnit*>(pUnit) )
							{
								// ����������� ���������� � �����
								pTransport->SetTowedArtillery( pArtillery );
								pTransport->SetTowedArtilleryCrew( pArtillery->GetCrew() );
								pArtillery->UpdateDirection( 65535/2 + pTransport->GetFrontDir() );

								const CVec2 vTransportHookPoint = pTransport->GetHookPoint();
								const CVec2 vArtilleryHookPoint = pArtillery->GetHookPoint();
								const CVec2 vShiftFromHookPointToArtillery = pArtillery->GetCenter() - vArtilleryHookPoint;
								const CVec2 vHookPoint = vTransportHookPoint + vShiftFromHookPointToArtillery;

								// cannons can be outside map. need adjustment.
								//CRAP{ SIMPLEST ADJUSTMENT
								if ( theStaticMap.IsPointInside( vHookPoint ) )
									pArtillery->SetNewCoordinates( CVec3( vHookPoint.x, vHookPoint.y, theStaticMap.GetZ( AICellsTiles::GetTile( vHookPoint ) ) ) );
								else
									pArtillery->SetNewCoordinates( CVec3( vTransportHookPoint.x, vTransportHookPoint.y, theStaticMap.GetZ( AICellsTiles::GetTile( vTransportHookPoint ) ) ) );
								//CRAP}
								
								CCommonUnit* pCrew = pArtillery->GetCrew();
								if ( pCrew )
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_LOAD_NOW, pTransport ), pCrew, false );
								pArtillery->InstallAction( ACTION_NOTIFY_UNINSTALL_TRANSPORT, true );		

								theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_BEING_TOWED, pTransport ), pArtillery, false );
							}
						}
						// �������� � ������
						else if ( CCommonUnit *pWithUnit = dynamic_cast<CCommonUnit*>(pObj) )
						{
							// ��������� ���������
							if ( pUnit->IsFormation() )
								theGroupLogic.UnitCommand( SAIUnitCmd( link.bIntention ? ACTION_COMMAND_LOAD : ACTION_COMMAND_LOAD_NOW, pWithUnit ), pUnit, false );
						}
						// link with static object
						else if ( CStaticObject *pStaticObj = dynamic_cast<CStaticObject*>( pObj ) )
						{
							// �������� � �������
							if ( pStaticObj->GetObjectType() == ESOT_BUILDING )
							{
								if ( link.bIntention )
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pStaticObj, 0 ), pUnit, false );
								else
								{
									CCommonUnit *pLoadingUnit = checked_cast<CCommonUnit*>(iter->first);
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER_BUILDING_NOW, pStaticObj ), pLoadingUnit, false );
								}
							}
							// �������� � ������
							else if ( pStaticObj->GetObjectType() == ESOT_ENTR_PART )
							{
								if ( link.bIntention )
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER, pStaticObj, 2 ), pUnit, false );
								else
								{
									CCommonUnit *pLoadingUnit = checked_cast<CCommonUnit*>(iter->first);									
									theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_ENTER_ENTREHCMNENT_NOW, pStaticObj, 2 ), pLoadingUnit, false );
								}
							}
						}
						else
							NI_ASSERT_T( false, "Wrong link" );
					}
					else
						NI_ASSERT_T( false, "Wrong link" );
				}
			}
		}
	}

	for ( std::set<int>::iterator iter = locomotives.begin(); iter != locomotives.end(); ++iter )
	{
		CPathUnit *pPathUnit = units[*iter]->GetPathUnit();
		NI_ASSERT_T( dynamic_cast<CCarriagePathUnit*>(pPathUnit) != 0, "Wrong pathunit of locomotive" );
		CCarriagePathUnit* pCarriage = static_cast<CCarriagePathUnit*>(pPathUnit);

		pCarriage->SetOnRailroad();

		CCarriagePathUnit* pCurCarriage = pCarriage;
		while ( nextCarriages.find( pCurCarriage->GetOwner()->GetID() ) != nextCarriages.end() )
		{
			CPathUnit *pPathUnit = units[nextCarriages[pCurCarriage->GetOwner()->GetID()]]->GetPathUnit();
			NI_ASSERT_T( dynamic_cast<CCarriagePathUnit*>(pPathUnit) != 0, "Wrong pathunit of carriage" );
			CCarriagePathUnit *pCarriage = static_cast<CCarriagePathUnit*>(pPathUnit);

			pCarriage->HookTo( pCurCarriage );
			pCurCarriage = pCarriage;
		}
	}
} 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LoadEntrenchments( const std::vector<SEntrenchmentInfo> &entrenchments )
{
	// �� ������
	for ( int i = 0; i < entrenchments.size(); ++i )
	{
		// �� �������
		CPtr<CFullEntrenchment> pFullEntrenchment = new CFullEntrenchment();
		for ( int j = 0; j < entrenchments[i].sections.size(); ++j )
		{
			std::vector<IRefCount*> segments;
			for ( int k = 0; k < entrenchments[i].sections[j].size(); ++k )
			{
				const int nLink = entrenchments[i].sections[j][k];
				NI_ASSERT_T( CLinkObject::GetObjectByLink( nLink ) != 0, "Section of entrenchment doesn't exist" );
				segments.push_back( CLinkObject::GetObjectByLink( nLink ) );
			}

			theStatObjs.AddNewEntrencment( &(segments[0]), segments.size(), pFullEntrenchment, true );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LoadBridges( const std::vector< std::vector<int> > &bridgesInfo )
{
	// �� ������
	for ( int i = 0; i < bridgesInfo.size(); ++i )
	{
		std::list<CPtr<CBridgeSpan> > bridge;
		// �� �������
		CPtr<CFullBridge> pFullBridge = new CFullBridge;
		for ( int j = 0; j < bridgesInfo[i].size(); ++j )
		{
			NI_ASSERT_T( CLinkObject::GetObjectByLink( bridgesInfo[i][j] ) != 0, "Span of a bridge doesn't exist" );
			NI_ASSERT_T( dynamic_cast<CBridgeSpan*>( CLinkObject::GetObjectByLink( bridgesInfo[i][j] ) ) != 0, "Wrong type of bridge span" );

			CBridgeSpan* pSpan = static_cast<CBridgeSpan*>( CLinkObject::GetObjectByLink( bridgesInfo[i][j] ) );
			pSpan->SetFullBrige( pFullBridge );
			pFullBridge->AddSpan( pSpan );

			bridge.push_back( pSpan );
		}
		bridges.push_back( bridge );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LaunchStartCommand( const SAIStartCommand &startCommand, IRefCount **pUnitsBuffer, const int nSize )
{
	SAIUnitCmd cmd;
	cmd.cmdType = startCommand.cmdType;
	cmd.fNumber = startCommand.fNumber;
	cmd.fromExplosion = startCommand.fromExplosion;
	if ( startCommand.linkID > 0 )
		cmd.pObject = CLinkObject::GetObjectByLink( startCommand.linkID );
	else
		cmd.pObject = 0;
	cmd.vPos = startCommand.vPos;
	
	const	WORD wGroup = theGroupLogic.GenerateGroupNumber();
	theGroupLogic.RegisterGroup( pUnitsBuffer, nSize, wGroup );
	theGroupLogic.GroupCommand( cmd, wGroup, true );
	theGroupLogic.UnregisterGroup( wGroup );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitStartCommands()
{
	for ( SLoadMapInfo::TStartCommandsList::const_iterator iter = startCmds.begin(); iter != startCmds.end(); ++iter )
	{
		if ( !iter->unitLinkIDs.empty() )
		{
			const int nSize = iter->unitLinkIDs.size();
			std::vector<IRefCount*> unitsBuffer( nSize );
			for ( int i = 0; i < nSize; ++i )
				unitsBuffer[i] = CLinkObject::GetObjectByLink( iter->unitLinkIDs[i] );

			LaunchStartCommand( *iter, &(unitsBuffer[0]), nSize );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitStartCommands( const LinkInfo &linksInfo, std::unordered_map<int, int> &old2NewLinks )
{
	for ( SLoadMapInfo::TStartCommandsList::const_iterator iter = startCmds.begin(); iter != startCmds.end(); ++iter )
	{
		if ( !iter->unitLinkIDs.empty() )
		{
			int nActuallyUnits = 0;			
			const int nSize = iter->unitLinkIDs.size();
			std::vector<IRefCount*> unitsBuffer( nSize );
			for ( int i = 0; i < nSize; ++i )
			{
				const int nLink = iter->unitLinkIDs[i];
				if ( old2NewLinks.find( nLink ) != old2NewLinks.end() )
				{
					CLinkObject *pUnit = CLinkObject::GetObjectByLink( old2NewLinks[nLink] );
					if ( pUnit != 0 && linksInfo.find( pUnit ) != linksInfo.end() )
						unitsBuffer[nActuallyUnits++] = pUnit;
				}
			}

			if ( nActuallyUnits != 0 )
				LaunchStartCommand( *iter, &(unitsBuffer[0]), nSize );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitReservePositions()
{
	for ( SLoadMapInfo::TReservePositionsList::const_iterator iter = reservePositions.begin(); iter != reservePositions.end(); ++iter )
	{
		CLinkObject *pLinkObject = CLinkObject::GetObjectByLink( iter->nArtilleryLinkID );
		if ( pLinkObject && pLinkObject->IsValid() && pLinkObject->IsAlive() )
		{
			CAIUnit *pUnit = checked_cast<CAIUnit*>( pLinkObject );
			pUnit->SetBattlePos( iter->vPos );

			// ���� ����������, ������� �������
			CLinkObject *pLinkTruck = CLinkObject::GetObjectByLink( iter->nTruckLinkID );
			if ( pLinkTruck && pLinkTruck->IsValid() && pLinkTruck->IsAlive() )
			{
				CAITransportUnit *pTruck = checked_cast<CAITransportUnit*>( pLinkTruck );
				pTruck->SetMustTow( pUnit );
				pUnit->SetTruck( pTruck );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitReservePositions( std::unordered_map<int, int> &old2NewLinks )
{
	for ( SLoadMapInfo::TReservePositionsList::const_iterator iter = reservePositions.begin(); iter != reservePositions.end(); ++iter )
	{
		const int nArtilleryLink = iter->nArtilleryLinkID;
		if ( old2NewLinks.find( nArtilleryLink ) != old2NewLinks.end() )
		{
			CLinkObject *pLinkObject = CLinkObject::GetObjectByLink( old2NewLinks[nArtilleryLink] );
			if ( pLinkObject && pLinkObject->IsValid() && pLinkObject->IsAlive() )
			{
				CCommonUnit *pUnit = checked_cast<CCommonUnit*>( pLinkObject );
				pUnit->SetBattlePos( iter->vPos );

				// ���� ����������, ������� �������
				const int nTruckLink = iter->nTruckLinkID;
				if ( old2NewLinks.find( nTruckLink ) != old2NewLinks.end() )
				{
					CLinkObject *pLinkTruck = CLinkObject::GetObjectByLink( old2NewLinks[nTruckLink] );
					if ( pLinkTruck && pLinkTruck->IsValid() && pLinkTruck->IsAlive() )
					{
						CAIUnit *pTruck = checked_cast<CAIUnit*>( pLinkTruck );
						pUnit->SetTruck( pTruck );
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::CommonInit( const STerrainInfo &terrainInfo )
{
	// for debug
	timeToLogStart = 0;
	timeToLogFinish = 1000000;
	//
	
	NTrg::Init();
	theCheats.Init();
	theCheats.SetNPartyForWarFog( theDipl.GetMyParty(), true );

	bSuspended = true;
	bFirstTime = true;
	eTypeOfAreasToShow = ACTION_NOTIFY_NONE;
	
	SConsts::Load();

	pGameSegment = GetSingleton<IGameTimer>()->GetGameSegmentTimer();
	curTime = GetAIGetSegmTime( pGameSegment );

	pThePathFinder = CreateObject<IStaticPathFinder>( AI_STANDART_PATH_FINDER );
	pThePlanePathFinder = CreateObject<IStaticPathFinder>( AI_PLANE_PATH_FINDER );
	pTheTrainPathFinder = CreateObject<IStaticPathFinder>( AI_TRAIN_PATH_FINDER );

	theAAManager.Init();
	updater.Init();
	theWarFog.Init();
	units.Init();
	theGroupLogic.Init();

	theStaticMap.SetMode( ELM_ALL );

	theSuspendedUpdates.Init( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
	theStatistics.Init();

	CRailroadGraphConstructor railroadGraphConstructor;
	railroadGraphConstructor.Construct( terrainInfo, &theRailRoadGraph );
	
	theWeather.Init();

	checkSum = crc32( 0L, Z_NULL, 0 );	
	if ( theDipl.IsNetGame() )
	{
		periodToCheckSum = GetGlobalVar( "checksumperiod", 1000 );
		nextCheckSumTime = periodToCheckSum;
	}

	theDifficultyLevel.Init();

	theCheats.SetHistoryPlaying( GetGlobalVar( "History.Playing", 0 ) != 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::LoadScenarioUnits( const SLoadMapInfo &mapInfo, LinkInfo *linksInfo )
{
	IScenarioTracker *pScenarioTracker = GetSingleton<IScenarioTracker>();
	IPlayerScenarioInfo *pUserPlayer = pScenarioTracker->GetUserPlayer();
	// ���������� ������, ������� player ���� � ����� � ������
	const int nMissionUnits = pUserPlayer->GetNumUnits();

	// ��� ����������� ������� �� �����
	const std::vector<SMapObjectInfo> &scenarioObjects = mapInfo.scenarioObjects;	
	// �������� ��� ��� ����������� ������ �� ���� ������
	std::vector<bool> takenScenarioObjects( scenarioObjects.size(), false );

	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	// �� ���� ������ ������
	for ( int k = 0; k < nMissionUnits; ++k )
	{
		IScenarioUnit *pScenarioUnit = pUserPlayer->GetUnit( k );
		const std::string &szUnitStats = pScenarioUnit->GetRPGStats();
		const SUnitBaseRPGStats *pScenaroUnitStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( szUnitStats.c_str() );

		// ����� ���������� ����������� ������ ��� ����� k
		int i;
		for ( i = 0; i < scenarioObjects.size(); ++i )
		{
			if ( !takenScenarioObjects[i] )
			{
				const char *pszObjName = scenarioObjects[i].szName.c_str();
				const IGDBObject *pObjStats = pIDB->GetRPGStats( pIDB->GetDesc( pszObjName ) );
				NI_ASSERT_T( dynamic_cast<const SUnitBaseRPGStats*>( pObjStats ) != 0, NStr::Format( "Not unit in scenario objects (%s)", pszObjName ) );

				const SUnitBaseRPGStats *pStats = static_cast<const SUnitBaseRPGStats*>( pObjStats );
				if ( pStats->GetRPGClass() == pScenaroUnitStats->GetRPGClass() )
					break;
			}
		}
		// check, if we found place for this scenario unit
		if ( i >= scenarioObjects.size() ) 
			continue;

		// ���������� ����������� ������ ��� ����� k �� ������
		NI_ASSERT_T( i < scenarioObjects.size(), NStr::Format( "Slot for mission unit %d not found", k ) );
		takenScenarioObjects[i] = true;

		const int nGroup = mapInfo.reinforcements.GetGroupById( scenarioObjects[i].nScriptID );
		if ( nGroup != -1 )
			scripts.AddUnitToReinforcGroup( scenarioObjects[i], nGroup, pScenaroUnitStats, pScenarioUnit );
		else
		{
			IRefCount *pObj = AddObject( scenarioObjects[i], pIDB, linksInfo, true, false, pScenaroUnitStats );
			if ( pObj )
			{
				CAIUnit *pUnit = checked_cast<CAIUnit*>(pObj);
				pUnit->SetScenarioUnit( pScenarioUnit );
				pUnit->SetScenarioStats();
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::Init( const SLoadMapInfo &mapInfo, IProgressHook *pProgress )
{
	// set control word for FP co-processor
	// _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24
	// 0xa001f
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );

	theDipl.Load( mapInfo.diplomacies );	
	
	theCheats.SetWarFog( strlen( GetGlobalVar( "warfog" ) ) == 0 );
	theCheats.SetLoadObjects( strlen( GetGlobalVar( "noobjects" ) ) == 0 );

	theStaticMap.LoadMap( mapInfo.terrain, theCheats.GetLoadObjects() );
	theStatObjs.Init( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
	
	if ( pProgress ) //6
		pProgress->Step();

	theMPInfo.Init();

	theUnitCreation.Init( mapInfo.unitCreation );
	CommonInit( mapInfo.terrain );
	scripts.Init( mapInfo );
	if ( mapInfo.scriptAreas.size() > 0 )
		scripts.InitAreas( &(mapInfo.scriptAreas[0]), mapInfo.scriptAreas.size() );

	LinkInfo linksInfo;

	theUnitCreation.Init( mapInfo.unitCreation );
	// ����� ��������� ������ ���������, ��� �������� ��������� ��������
	// -- ������� �� ������
	LoadAvailableTrucks();
	LoadScenarioUnits( mapInfo, &linksInfo );
	if ( pProgress )
		pProgress->Step(); //7
	LoadUnits( mapInfo, &linksInfo );

	LoadEntrenchments( mapInfo.entrenchments );
	LoadBridges( mapInfo.bridges );
	
	if ( pProgress )
		pProgress->Step(); //8

	// ������������������� ��� maxes �� �����
	for ( int i = 1; i < 16; i *= 2 )
		theStaticMap.UpdateMaxesForAddedStObject( 0, theStaticMap.GetSizeX() - 1, 0, theStaticMap.GetSizeY() - 1, i );
	theStaticMap.UpdateMaxesForAddedStObject( 0, theStaticMap.GetSizeX() - 1, 0, theStaticMap.GetSizeY() - 1, AI_CLASS_ANY );

	//theStatObjs.UpdateAllPartiesStorages( true, false );
	//
	InitLinks( linksInfo );
	reservePositions = mapInfo.reservePositionsList;
	InitReservePositions();

	// init general
	if ( !theDipl.IsNetGame() )
	{
		std::list<CCommonUnit*> pUnits;
		for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
			pUnits.push_back( *iter );

		theSupremeBeing.Init( mapInfo.aiGeneralMapInfo );
		theSupremeBeing.GiveNewUnitsToGenerals( pUnits );
	}

	// -- ����� ������� �� ������
	startCmds = mapInfo.startCommandsList;
	InitStartCommands();

	scripts.Load( mapInfo.szScriptFile );
	theHitsStore.Init( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
	
	if ( pProgress )
		pProgress->Step(); //9

	theWarFog.ProcessAllNewUnits();
	
	if ( pProgress )
		pProgress->Step(); //10

	UpdateCheckSum( false );
	GetSingleton<ICommandsHistory>()->CheckStartMapCheckSum( checkSum );
/////////////////////////////////////
	bool bOldSuspended = bSuspended;
	bSuspended = false;
	Segment();
	bSuspended = bOldSuspended;
	
	bNetGameStarted = false;
	if ( theDipl.IsNetGame() )
		updater.AddFeedBack( SAIFeedBack( EFB_OBJECTIVE_CHANGED, 0xff<<8 ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::InitEditor( const STerrainInfo &terrainInfo )
{
	theCheats.SetWarFog( true );
	theCheats.SetNPartyForWarFog( 0, true );
	theCheats.SetLoadObjects( true );

	theDipl.InitForEditor();	
	theStaticMap.LoadMap( terrainInfo );
	theStatObjs.Init( theStaticMap.GetSizeX(), theStaticMap.GetSizeY() );
	theUnitCreation.Init();

	CommonInit( terrainInfo );
	
	for ( int i = 1; i < 16; i *= 2 )
		theStaticMap.UpdateMaxesForAddedStObject( 0, theStaticMap.GetSizeX() - 1, 0, theStaticMap.GetSizeY() - 1, i );
	theStaticMap.UpdateMaxesForAddedStObject( 0, theStaticMap.GetSizeX() - 1, 0, theStaticMap.GetSizeY() - 1, AI_CLASS_ANY );
	
	Resume();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateCheckSum( bool bSend )
{
	using namespace NCheckSums;
	
	static SCheckSumBufferStorage	checkSumBuf( 10000 );
	checkSumBuf.nCnt = 0;
	
	for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		const CVec2 vCenter = pUnit->GetCenter();
		const WORD wDir = pUnit->GetFrontDir();
		const float fHP = pUnit->GetHitPoints();
		const float fMorale = pUnit->GetMorale();

		CopyToBuf( &checkSumBuf, vCenter );
		CopyToBuf( &checkSumBuf, wDir );
		CopyToBuf( &checkSumBuf, fHP );
		CopyToBuf( &checkSumBuf, fMorale );
	}

	checkSum = crc32( checkSum, &(checkSumBuf.buf[0]), checkSumBuf.nCnt );
	
	GetSingleton<IConsoleBuffer>()->WriteASCII( 10, NStr::Format( "%ul", checkSum ), 0, true );

	theShellsStore.UpdateCheckSum( &checkSum );

	if ( bSend )
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_MULTIPLAYER_CHECK, NStr::Format( "%ul", checkSum ), 0, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::Segment()
{
	// set control word for FP co-processor
	// _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24
	// 0xa001f
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
	//
	if ( !bSuspended )
	{
//	for debug
/**
		if ( curTime >= timeToLogStart && curTime <= timeToLogFinish )
		{
			SetGlobalVar( "lograndom", 1 );
			GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "Log", 1.0f );
		}
		else
		{
			RemoveGlobalVar( "lograndom" );
			GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "Log", 0.0f );
		}
/**/
		bSegment = true;
		curTime = GetAIGetSegmTime( pGameSegment );

		theGroupLogic.Segment();

		if ( theCheats.GetWarFog() )
			theWarFog.Segment( bFirstTime );

		theGraveyard.Segment();
		theWeather.Segment();
		scripts.Segment();
		theHitsStore.Segment();
		CLinkObject::Segment();
		theStatObjs.Segment();
		theAAManager.Segment();
		garbage.clear();
		updater.ClearPlacementsUpdates();
		theUnitCreation.Segment();
		theSuspendedUpdates.Segment();
		theCombatEstimator.Segment();
		theSupremeBeing.Segment();
		theMPInfo.Segment();
		if ( curTime >= nextCheckSumTime && theDipl.IsNetGame() )
		{
			nextCheckSumTime = curTime + periodToCheckSum;
			UpdateCheckSum( true );
		}
		bFirstTime = false;

		bSegment = false;

		if ( curTime == SConsts::SHOW_ALL_TIME_COEFF * SConsts::AI_SEGMENT_DURATION )
			updater.AddFeedBack( SAIFeedBack( EFB_ASK_FOR_WARFOG ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetDeadUnits( SAINotifyDeadAtAll **pDeadUnitsBuffer, int *pnLen )
{
	theGraveyard.GetDeadUnits( pDeadUnitsBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UnitCommand( const SAIUnitCmd *pCommand, const WORD wGroupID, const int nPlayer )
{
	curTime = GetAIGetSegmTime( pGameSegment );
	
	bool bAviationCall = false;
	switch ( pCommand->cmdType )
	{
		case ACTION_COMMAND_CALL_BOMBERS: 
			theUnitCreation.CallBombers( *pCommand, wGroupID, nPlayer );
			bAviationCall = true;

			break;
		case ACTION_COMMAND_CALL_SHTURMOVIKS: 
			theUnitCreation.CallShturmoviks( *pCommand, wGroupID, nPlayer );
			bAviationCall = true;

			break;
		case ACTION_COMMAND_CALL_FIGHTERS: 
			theUnitCreation.CallFighters( *pCommand, wGroupID, nPlayer );
			bAviationCall = true;

			break;
		case ACTION_COMMAND_CALL_SCOUT: 
			theUnitCreation.CallScout( *pCommand, wGroupID, nPlayer );
			bAviationCall = true;

			break;
		case ACTION_COMMAND_PARADROP: 
			theUnitCreation.CallParadroppers( *pCommand, wGroupID, nPlayer );
			bAviationCall = true;

			break;
		case ACTION_COMMAND_PLACE_MARKER:
			if ( theDipl.GetNParty( nPlayer ) == theDipl.GetMyParty() ) 
			{
				updater.AddFeedBack( SAIFeedBack(EFB_PLACE_MARKER, MAKELONG(Max(0.0f,pCommand->vPos.x), Max(0.0f,pCommand->vPos.y)) ));
			}
			break;
		default: 
			NI_ASSERT_T( false, NStr::Format( "Unit command %d isn't permitted\n", pCommand->cmdType ) );
	}

	if ( bAviationCall )
		theStatistics.AviationCalled( nPlayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void GetWarfogVisibilities( const T &warFog, const CVec2 &upLeft, const CVec2 &downLeft, const CVec2 &downRight, const CVec2 &upRight, const int nPartyForWarFog,
											 SAIVisInfo **pVisBuffer, int *pnLen )
{
	const int minSum = upLeft.x + upLeft.y;
	const int maxSum = upRight.x + upRight.y;
	const int minDiff = upLeft.x - upLeft.y;
	const int maxDiff = downLeft.x - downLeft.y;

	*pVisBuffer = GetTempBuffer<SAIVisInfo>( ( maxSum - minSum + 1 )*( maxDiff - minDiff + 1 ) );
	*pnLen = 0;

	for ( int diff = ( minDiff + minDiff % 2 ) / 2; diff <= ( maxDiff - maxDiff % 2 ) / 2; ++diff )
	{
		for ( int sum = ( minSum + minSum % 2 ) / 2; sum <= ( maxSum - maxSum % 2 ) / 2; ++sum )
		{
			// ������� �� 2
			if ( ( ((sum + diff) & 1) == 0 ) && ( ((sum-diff) & 1) == 0 ) )
			{
				(*pVisBuffer)[*pnLen].x = (sum + diff) / 2;
				(*pVisBuffer)[*pnLen].y = (sum - diff) / 2;
				
				(*pVisBuffer)[*pnLen].vis = 
					(
						warFog.GetClientTileVis( SVector( sum + diff - 2, sum - diff - 2 ), nPartyForWarFog ) +
						warFog.GetClientTileVis( SVector( sum + diff - 2, sum - diff - 1 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff - 2, sum - diff     ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff - 2, sum - diff + 1 ), nPartyForWarFog ) +

  					warFog.GetClientTileVis( SVector( sum + diff - 1, sum - diff - 2 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff - 1, sum - diff - 1 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff - 1, sum - diff     ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff - 1, sum - diff + 1 ), nPartyForWarFog ) +

						warFog.GetClientTileVis( SVector( sum + diff    , sum - diff - 2 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff    , sum - diff - 1 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff    , sum - diff     ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff    , sum - diff + 1 ), nPartyForWarFog ) +

						warFog.GetClientTileVis( SVector( sum + diff + 1, sum - diff - 2 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff + 1, sum - diff - 1 ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff + 1, sum - diff     ), nPartyForWarFog ) + 
						warFog.GetClientTileVis( SVector( sum + diff + 1, sum - diff + 1 ), nPartyForWarFog ) 
					) / 16;

				++(*pnLen);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetVisibilities( const CVec2 &upLeft, const CVec2 &downLeft, 
																const CVec2 &downRight, const CVec2 &upRight,
															  struct SAIVisInfo **pVisBuffer, int *pnLen ) const
{
	if ( CanShowVisibilities() && theCheats.GetWarFog() && ( !theDipl.IsNetGame() || bNetGameStarted ) )
		GetWarfogVisibilities<CGlobalWarFog>( theWarFog, 2 * upLeft, 2 * downLeft, 2 * downRight, 2 * upRight, theCheats.GetNPartyForWarFog(), pVisBuffer, pnLen );
	else
		*pnLen = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD CAILogic::GenerateGroupNumber()
{
	return theGroupLogic.GenerateGroupNumber();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::RegisterGroup( IRefCount **pUnitsBuffer, const int nLen, const WORD wGroup )
{
	theGroupLogic.RegisterGroup( pUnitsBuffer, nLen, wGroup );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UnregisterGroup( const WORD wGroup )
{
	theGroupLogic.UnregisterGroup( wGroup );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue )
{
	theGroupLogic.GroupCommand( *pCommand, wGroup, bPlaceInQueue );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::CheckDiplomacy( const IRefCount **pUnitsBuffer, BYTE **pResults, const int nLen )
{
	*pResults = GetTempBuffer<BYTE>( nLen );
	
	for ( int i = 0; i < nLen; ++i )
		(*pResults)[i] = theDipl.GetDiplStatus( static_cast<const CAIUnit*>( pUnitsBuffer[i] )->GetPlayer(), theDipl.GetMyNumber() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetGlobalPassability( BYTE **pMapBuffer, int *pnLen )
{
	*pMapBuffer = GetTempBuffer<BYTE>( theStaticMap.GetSizeX() * theStaticMap.GetSizeY() );
	*pnLen = 0;

	for ( int i = 0; i < theStaticMap.GetSizeY(); ++i )
		for ( int j = 0; j < theStaticMap.GetSizeX(); ++j )
			*( (*pMapBuffer) + (*pnLen)++ ) = AI_CLASS_ANY			 * theStaticMap.IsLocked( j, i, AI_CLASS_ANY )			 |
																				AI_CLASS_HUMAN		 * theStaticMap.IsLocked( j, i, AI_CLASS_HUMAN )		 |
																				AI_CLASS_WHEEL		 * theStaticMap.IsLocked( j, i, AI_CLASS_WHEEL )		 |
																				AI_CLASS_HALFTRACK * theStaticMap.IsLocked( j, i, AI_CLASS_HALFTRACK ) |
																				AI_CLASS_TRACK		 * theStaticMap.IsLocked( j, i, AI_CLASS_TRACK );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetDisplayPassability( const class CVec2 &upLeft, const class CVec2 &downLeft, 
																			const class CVec2 &downRight, const class CVec2 &upRight,
																			SAIPassabilityInfo **pPassBuffer, int *pnLen )
{
	NI_ASSERT_SLOW_TF( upLeft.x - upLeft.y == upRight.x - upRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( downLeft.x - downLeft.y == downRight.x - downRight.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upLeft.x + upLeft.y == downLeft.x + downLeft.y, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( upRight.x + upRight.y == downRight.x + downRight.y, "Wrong points order", return );
	
	const int minSum = 2 * ( upLeft.x + upLeft.y ); 
	const int maxSum = 2 * ( upRight.x + upRight.y );
	const int minDiff = 2 * ( upLeft.x - upLeft.y );
	const int maxDiff = 2 * ( downLeft.x - downLeft.y );
	
	NI_ASSERT_SLOW_TF( minSum <= maxSum, "Wrong points order", return );
	NI_ASSERT_SLOW_TF( minDiff <= maxDiff, "Wrong points order", return );
	
	const int sizeSum = maxSum - minSum + 1;
	const int sizeDiff = maxDiff - minDiff + 1;
	
	*pPassBuffer = GetTempBuffer<SAIPassabilityInfo>( sizeSum * sizeDiff );
	*pnLen = 0;
	
	for ( int diff = minDiff; diff <= maxDiff; ++diff )
	{
		for ( int sum = minSum; sum <= maxSum; ++sum )
		{
			// ������� �� 2
			if ( ( ((sum + diff) & 1) == 0 ) && ( ((sum-diff) & 1) == 0 ) )
			{
				const int x = (sum + diff) / 2;
				const int y = (sum - diff) / 2;
				(*pPassBuffer)[*pnLen].x = x;
				(*pPassBuffer)[*pnLen].y = y;
				(*pPassBuffer)[*pnLen].pass = AI_CLASS_ANY			 * theStaticMap.IsLocked( x, y, AI_CLASS_ANY )			 |
																			AI_CLASS_HUMAN		 * theStaticMap.IsLocked( x, y, AI_CLASS_HUMAN )		 |
																			AI_CLASS_WHEEL		 * theStaticMap.IsLocked( x, y, AI_CLASS_WHEEL )		 |
																			AI_CLASS_HALFTRACK * theStaticMap.IsLocked( x, y, AI_CLASS_HALFTRACK ) |
																			AI_CLASS_TRACK		 * theStaticMap.IsLocked( x, y, AI_CLASS_TRACK );

				++(*pnLen);
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::CallScriptFunction( const char *pszCommand )
{
	scripts.CallScriptFunction( pszCommand );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateTurretTurn( struct SAINotifyTurretTurn **pTurretsBuffer, int *pnLen )
{
	updater.UpdateTurretTurn( pTurretsBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetDeletedStaticObjects( IRefCount ***pObjBuffer, int *pnLen )
{
	updater.GetDeletedStaticObjects( pObjBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetNewProjectiles( struct SAINotifyNewProjectile **pProjectiles, int *pnLen )
{
	updater.GetNewProjectiles( pProjectiles, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetDeadProjectiles( IRefCount ***pProjectilesBuf, int *pnLen )
{
	updater.GetDeadProjectiles( pProjectilesBuf, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetDisappearedUnits( IRefCount ***pUnitsBuffer, int *pnLen )
{
	updater.GetDisappearedUnits( pUnitsBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetNewStaticObjects( SNewUnitInfo **pObjects, int *pnLen )
{
	updater.GetNewStaticObjects( pObjects, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateRPGParams( SAINotifyRPGStats **pUnitRPGBuffer, int *pnLen )
{
	updater.UpdateRPGParams( pUnitRPGBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdatePlacements( SAINotifyPlacement **pObjPosBuffer, int *pnLen )
{
	updater.UpdatePlacements( pObjPosBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateStObjPlacements( SAINotifyPlacement **pObjPosBuffer, int *pnLen )
{
	updater.UpdateStObjPlacements( pObjPosBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetNewUnits( SNewUnitInfo **pNewUnitBuffer, int *pnLen )
{
	updater.GetNewUnits( pNewUnitBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateHits( SAINotifyHitInfo **pHits, int *pnLen )
{
	updater.UpdateHits( pHits, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateActions( SAINotifyAction **pActionsBuffer, int *pnLen )
{
	updater.UpdateActions( pActionsBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateShots( SAINotifyMechShot **pShots, int *pnLen )
{
	updater.UpdateShots( pShots, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateShots( SAINotifyInfantryShot **pShots, int *pnLen )
{
	updater.UpdateShots( pShots, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateFeedbacks( SAIFeedBack **pFeedBacksBuffer, int *pnLen )
{
	updater.UpdateFeedBacks( pFeedBacksBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::EndUpdates()
{
	updater.EndUpdates();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateEntranceStates( SAINotifyEntranceState **pUnits, int *pnLen )
{
	updater.UpdateEntranceStates( pUnits, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetEntrenchments( SSegment2Trench **pEntrenchemnts, int *pnLen )
{
	updater.GetEntrenchments( pEntrenchemnts, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetFormations( struct SSoldier2Formation **pFormations, int *pnLen )
{
	updater.GetFormations( pFormations, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetNewBridgeSpans( struct SNewUnitInfo **pObjects, int *pnLen )
{
	updater.GetNewBridgeSpans( pObjects, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::GetNewBridge( IRefCount ***pSpans, int *pnLen )
{
	*pnLen = 0;	
	if ( !bridges.empty() )
	{
		*pSpans = GetTempBuffer<IRefCount*>( bridges.front().size() );

		for ( std::list<CPtr<CBridgeSpan> >::iterator iter = bridges.front().begin(); iter != bridges.front().end(); ++iter )
			(*pSpans)[(*pnLen)++] = *iter;

		bridges.pop_front();
	}

	return !bridges.empty();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetRevealCircles( CCircle **pCircleBuffer, int *pnLen )
{
	updater.GetRevealCircles( pCircleBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateDiplomacies( struct SAINotifyDiplomacy **pDiplomaciesBuffer, int *pnLen )
{
	updater.UpdateDiplomacies( pDiplomaciesBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::CanShowVisibilities() const
{
	return curTime >= SConsts::AI_SEGMENT_DURATION * SConsts::SHOW_ALL_TIME_COEFF;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetMiniMapInfo( struct SMiniMapUnitInfo **pUnitsBuffer, int *pnLen )
{
	if ( CanShowVisibilities() && ( !theDipl.IsNetGame() || bNetGameStarted ) )
	{
		*pUnitsBuffer = GetTempBuffer<SMiniMapUnitInfo>( SConsts::MAX_NUMBER_OF_UNITS );
		*pnLen = 0;

		for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
		{
			CAIUnit *pUnit = *iter;
			if ( pUnit->IsAlive() )
			{
				const CVec2 vCenter( pUnit->GetCenter() );
				if ( theStaticMap.IsPointInside( vCenter ) && 
						 ( theCheats.IsHistoryPlaying() && !pUnit->IsInSolidPlace() || 
							 pUnit->GetParty() != theCheats.GetNPartyForWarFog() && pUnit->IsVisible( theCheats.GetNPartyForWarFog() ) ||
							 pUnit->IsVisibleByPlayer() 
							)
						)
				{
					float z = pUnit->GetZ();
					AI2VisZ( &z );

					const SVector tile( pUnit->GetTile() );
					(*pUnitsBuffer)[(*pnLen)++] = SMiniMapUnitInfo( tile.x / 2, tile.y / 2, z, pUnit->GetPlayer() );
				}
			}
		}
	}
	else
		*pnLen = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetMiniMapInfo( BYTE **pVisBuffer, int *pnLen )
{
	if ( CanShowVisibilities() && ( !theDipl.IsNetGame() || bNetGameStarted ) )
		theWarFog.GetMiniMapInfo( pVisBuffer, pnLen, theCheats.GetNPartyForWarFog(), bFirstTime );
	else
		*pnLen = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::ShowAreas( const int nGroup, const EActionNotify eType, bool bShow )
{
	if ( bShow )
	{
		eTypeOfAreasToShow = eType;

		updater.UpdateAreasGroup( nGroup );
		theGroupLogic.UpdateAllAreas( nGroup, eType );
	}
	else
		updater.UpdateAreasGroup( -1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateShootAreas( struct SShootAreas **pShootAreas, int *pnLen )
{
	switch ( eTypeOfAreasToShow )
	{
	case ACTION_NOTIFY_SHOOT_AREA:
		updater.UpdateShootAreas( pShootAreas, pnLen );

		break;
	case ACTION_NOTIFY_RANGE_AREA:
		updater.UpdateRangeAreas( pShootAreas, pnLen );

		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::IsCombatSituation()
{
	return theCombatEstimator.IsCombatSituation();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::ToGarbage( CCommonUnit *pUnit )
{
	garbage.push_back( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CAILogic::GetUniqueIDOfObject( IRefCount *pObj )
{
	NI_ASSERT_T( dynamic_cast<CLinkObject*>(pObj) != 0, NStr::Format("Wrong object of type \"%s\" - CLinkObject expected", typeid(*pObj).name()) );

	return static_cast<CLinkObject*>(pObj)->GetUniqueId();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CAILogic::GetObjByUniqueID( const int id )
{
	return GetObjectByUniqueIdSafe<IRefCount>( id );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SetMyInfo( const int nParty, const int nNumber )
{
	theDipl.SetMyNumber( nNumber );
	theDipl.SetParty( theDipl.GetMyNumber(), nParty );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SetNPlayers( const int nPlayers )
{
	theDipl.SetNPlayers( nPlayers );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SetNetGame( bool bNetGame )
{
	theDipl.SetNetGame( bNetGame );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::SubstituteUniqueIDs( IRefCount **pUnitsBuffer, const int nLen )
{
	bool bCorrect = true;
	
	for ( int i = 0 ; i < nLen; ++i )
	{
		if ( pUnitsBuffer[i] == 0 || dynamic_cast<CLinkObject*>(pUnitsBuffer[i]) == 0 )
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII(
				CONSOLE_STREAM_CONSOLE, 
				("Wrong object of type \"%s\" - CLinkObject expected", typeid(*pUnitsBuffer[i]).name()),
				0xffff0000, true );

			pUnitsBuffer[i] = 0;			
			bCorrect = false;
		}
		else
		{
			CLinkObject *pObj = static_cast<CLinkObject*>( pUnitsBuffer[i] );
			pUnitsBuffer[i] = reinterpret_cast<IRefCount*>( pObj->GetUniqueId() );
		}
	}

	return bCorrect;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateAcknowledgments( SAIAcknowledgment **pAckBuffer, int *pnLen )
{
	theAckManager.UpdateAcknowledgments( pAckBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::UpdateAcknowledgments( SAIBoredAcknowledgement **pAckBuffer, int *pnLen )
{
	theAckManager.UpdateAcknowledgments( pAckBuffer, pnLen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CAILogic::GetZ( const CVec2 &vPoint ) const
{
	return theStaticMap.GetVisZ( vPoint.x, vPoint.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD CAILogic::GetNormal( const CVec2 &vPoint ) const
{
	return theStaticMap.GetNormal( vPoint.x, vPoint.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CAILogic::GetIntersectionWithTerrain( CVec3 *pvResult, const CVec3 &vBegin, const CVec3 &vEnd ) const
{
	return theStaticMap.GetIntersectionWithTerrain( pvResult, vBegin, vEnd );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::ToggleShow( const int nShowType )
{
	theCheats.SetTurnOffWarFog( !theCheats.GetTurnOffWarFog() );
	return theCheats.GetTurnOffWarFog();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SetDifficultyLevel( const int nLevel )
{
	if ( !theDipl.IsNetGame() )
		theDifficultyLevel.SetLevel( nLevel );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::SetCheatDifficultyLevel( const int nCheatLevel )
{
	if ( !theDipl.IsNetGame() )
		theDifficultyLevel.SetCheatLevel( nCheatLevel );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::NetGameStarted()
{
	bNetGameStarted = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::IsNetGameStarted() const
{
	return bNetGameStarted;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CDifficultyLevel* CAILogic::GetDifficultyLevel() const
{
	return &theDifficultyLevel;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::NeutralizePlayer( const int nPlayer )
{
	if ( theDipl.IsPlayerExist( nPlayer ) )
	{
		int nBestPlayer = theDipl.GetNeutralPlayer();
		float fBestPrice = -1.0f;
		for ( int i = 0; i < theDipl.GetNPlayers(); ++i )
		{
			if ( i != nPlayer && theDipl.IsPlayerExist( i ) && theDipl.GetDiplStatus( i, nPlayer ) == EDI_FRIEND )
			{
				if ( IMissionStatistics *pPlayerStats = theStatistics.GetPlayerStats( nPlayer ) )
				{
					const float fLocalPrice = pPlayerStats->GetValue( STMT_ENEMY_KILLED_AI_PRICE );
					if ( fLocalPrice > fBestPrice )
					{
						fBestPrice = fLocalPrice;
						nBestPlayer = i;
					}
				}
			}
		}
		
		std::list<CCommonUnit*> playerUnits;
		for ( CGlobalIter iter( theDipl.GetNParty( nPlayer ), EDI_FRIEND ); !iter.IsFinished(); iter.Iterate() )
		{
			CCommonUnit *pUnit = *iter;
			if ( pUnit->GetPlayer() == nPlayer )
				playerUnits.push_back( pUnit );
		}

		for ( std::list<CCommonUnit*>::iterator iter = playerUnits.begin(); iter != playerUnits.end(); ++iter )
		{
			CCommonUnit *pUnit = *iter;
			theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), pUnit, false );

			pUnit->ChangePlayer( nBestPlayer );
		}

		theDipl.SetPlayerNotExist( nPlayer );

		if ( nBestPlayer != theDipl.GetNeutralPlayer() )
			updater.AddFeedBack( SAIFeedBack( EFB_TROOPS_PASSED, MAKELONG( nPlayer, nBestPlayer ) ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::NoWin()
{
	theMPInfo.NoWin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::IsNoWin() const
{
	return theMPInfo.IsNoWin();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CAILogic::GetUnitState( IRefCount *pObj )
{
	if ( CQueueUnit *pUnit = dynamic_cast<CQueueUnit*>(pObj) )
		return pUnit->GetState();
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::IsFrozen( IRefCount *pObj ) const
{
	if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj) )
		return pUnit->CanBeFrozen();
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::IsFrozenByState( IRefCount *pObj ) const
{
	if ( CCommonUnit *pUnit = dynamic_cast<CCommonUnit*>(pObj) )
		return pUnit->IsFrozenByState();
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::GetGridUnitsCoordinates( const int nGroup, const CVec2 &vGridCenter, CVec2 **pCoord, int *pnLen )
{
	CGrid grid( vGridCenter, nGroup, CVec2( 1.0f, 0.0f ) );

	*pnLen = grid.GetNUnitsInGrid();
	*pCoord = GetTempBuffer<CVec2>( *pnLen );
	for ( int i = 0; i < *pnLen; ++i )
		(*pCoord)[i] = grid.GetUnitCenter( i ) - vGridCenter;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::Suspend() 
{ 
	bSuspended = true; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::Resume() 
{ 
	bSuspended = false; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
