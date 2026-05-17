#include "StdAfx.h"

#include "WorldClient.h"

#include "..\Formats\fmtTerrain.h"
#include "..\AILogic\AITypes.h"
#include "..\Common\Actions.h"
#include "..\Common\Icons.h"
#include "iMission.h"
#include "..\Main\TextSystem.h"
#include "..\Main\GameStats.h"
#include "..\Common\UISquadElement.h"
#include "..\Common\PauseGame.h"
#include "..\Main\iMainCommands.h"

#include "SelectorVisitors.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Main\ScenarioTracker.h"
#include "..\UI\UI.h"
#include "..\UI\UIMessages.h"

#include "..\StreamIO\OptionSystem.h"
#include "..\Misc\VSHelper.h"
#include "..\RandomMapGen\MapInfo_Types.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const NInput::SRegisterCommandEntry worldClientCommands[] = 
{
	{ "assign_group_0"	,	WCC_ASSIGN_GROUP_0	},
	{ "assign_group_1"	,	WCC_ASSIGN_GROUP_1	},
	{ "assign_group_2"	,	WCC_ASSIGN_GROUP_2	},
	{ "assign_group_3"	,	WCC_ASSIGN_GROUP_3	},
	{ "assign_group_4"	,	WCC_ASSIGN_GROUP_4	},
	{ "assign_group_5"	,	WCC_ASSIGN_GROUP_5	},
	{ "assign_group_6"	,	WCC_ASSIGN_GROUP_6	},
	{ "assign_group_7"	,	WCC_ASSIGN_GROUP_7	},
	{ "assign_group_8"	,	WCC_ASSIGN_GROUP_8	},
	{ "assign_group_9"	,	WCC_ASSIGN_GROUP_9	},
	//
	{ "select_group_0"	, WCC_SELECT_GROUP_0	},
	{ "select_group_1"	, WCC_SELECT_GROUP_1	},
	{ "select_group_2"	, WCC_SELECT_GROUP_2	},
	{ "select_group_3"	, WCC_SELECT_GROUP_3	},
	{ "select_group_4"	, WCC_SELECT_GROUP_4	},
	{ "select_group_5"	, WCC_SELECT_GROUP_5	},
	{ "select_group_6"	, WCC_SELECT_GROUP_6	},
	{ "select_group_7"	, WCC_SELECT_GROUP_7	},
	{ "select_group_8"	, WCC_SELECT_GROUP_8	},
	{ "select_group_9"	, WCC_SELECT_GROUP_9	},
	//
	{ "center_group_0"	, WCC_CENTER_GROUP_0	},
	{ "center_group_1"	, WCC_CENTER_GROUP_1	},
	{ "center_group_2"	, WCC_CENTER_GROUP_2	},
	{ "center_group_3"	, WCC_CENTER_GROUP_3	},
	{ "center_group_4"	, WCC_CENTER_GROUP_4	},
	{ "center_group_5"	, WCC_CENTER_GROUP_5	},
	{ "center_group_6"	, WCC_CENTER_GROUP_6	},
	{ "center_group_7"	, WCC_CENTER_GROUP_7	},
	{ "center_group_8"	, WCC_CENTER_GROUP_8	},
	{ "center_group_9"	, WCC_CENTER_GROUP_9	},
	//
	{ "assign_terrain_0", WCC_ASSIGN_TERRAIN_0},
	{ "assign_terrain_1", WCC_ASSIGN_TERRAIN_1},
	{ "assign_terrain_2", WCC_ASSIGN_TERRAIN_2},
	{ "assign_terrain_3", WCC_ASSIGN_TERRAIN_3},
	{ "assign_terrain_4", WCC_ASSIGN_TERRAIN_4},
	{ "assign_terrain_5", WCC_ASSIGN_TERRAIN_5},
	{ "assign_terrain_6", WCC_ASSIGN_TERRAIN_6},
	{ "assign_terrain_7", WCC_ASSIGN_TERRAIN_7},
	{ "assign_terrain_8", WCC_ASSIGN_TERRAIN_8},
	{ "assign_terrain_9", WCC_ASSIGN_TERRAIN_9},
	//
	{ "center_terrain_0", WCC_CENTER_TERRAIN_0},
	{ "center_terrain_1", WCC_CENTER_TERRAIN_1},
	{ "center_terrain_2", WCC_CENTER_TERRAIN_2},
	{ "center_terrain_3", WCC_CENTER_TERRAIN_3},
	{ "center_terrain_4", WCC_CENTER_TERRAIN_4},
	{ "center_terrain_5", WCC_CENTER_TERRAIN_5},
	{ "center_terrain_6", WCC_CENTER_TERRAIN_6},
	{ "center_terrain_7", WCC_CENTER_TERRAIN_7},
	{ "center_terrain_8", WCC_CENTER_TERRAIN_8},
	{ "center_terrain_9", WCC_CENTER_TERRAIN_9},
	//
#ifndef _FINALRELEASE
	{ "force_rotation"	, WCC_FORCE_ROTATION	},
	{ "place_effect"		, WCC_PLACE_EFFECT		},
	{ "show_ai_info"		, WCC_SHOW_AI_INFO		},
	{ "show_hp_info"		, WCC_SHOW_HP_INFO		},
	{ "show_haze"				, WCC_SHOW_HAZE				},
	{ "show_noise"			, WCC_SHOW_NOISE			},
	{ "test_animations"	, WCC_TEST_ANIMATIONS	},
#endif // _FINALRELEASE
	//
	{ "show_fire_ranges_on"	, WCC_SHOW_FIRE_RANGES_ON	},
	{ "show_fire_ranges_off", WCC_SHOW_FIRE_RANGES_OFF},
	{ "show_zero_areas_on"	, WCC_SHOW_ZERO_AREAS_ON	},
	{ "show_zero_areas_off"	, WCC_SHOW_ZERO_AREAS_OFF	},
	{ "show_storage_ranges_on" , WCC_SHOW_STORAGE_RANGES_ON	},
	{ "show_storage_ranges_off", WCC_SHOW_STORAGE_RANGES_OFF},
	//
	{ "select_by_type"	, WCC_SELECT_BY_TYPE	},
	//
	{ "action_stop"			, USER_ACTION_STOP		},
	{ "action_attack"		, USER_ACTION_ATTACK	},
	{ "action_move"			, USER_ACTION_MOVE		},
	{ "action_swarm"		, USER_ACTION_SWARM		},
	{ "action_follow"		, USER_ACTION_FOLLOW	},
	{ "action_board"		, USER_ACTION_BOARD		},
	{ "action_leave"		, USER_ACTION_LEAVE		},
	{ "action_rotate"		, USER_ACTION_ROTATE	},
	{ "action_formation",	USER_ACTION_FORMATION	},
	{ "action_install"	, USER_ACTION_INSTALL },
	{ "action_uninstall", USER_ACTION_UNINSTALL },
	{ "action_guard"		,	USER_ACTION_GUARD		},
	{ "action_ambush"		, USER_ACTION_AMBUSH	},
	{ "action_ranging"	,	USER_ACTION_RANGING	},
	{ "action_suppress"	,	USER_ACTION_SUPPRESS},
	{ "action_stand_ground"						,USER_ACTION_STAND_GROUND							},
	{ "action_hook_artillery"					, USER_ACTION_HOOK_ARTILLERY					},
	{ "action_deploy_artillery"				, USER_ACTION_DEPLOY_ARTILLERY				},
	{ "action_entrench_self"					, USER_ACTION_ENTRENCH_SELF						},
	{ "action_officer_call_bombers"		, USER_ACTION_OFFICER_CALL_BOMBERS		},
	{ "action_officer_call_fighters"	, USER_ACTION_OFFICER_CALL_FIGHTERS		},
	{ "action_officer_call_spy"				, USER_ACTION_OFFICER_CALL_SPY				},
	{ "action_officer_call_paradropers",USER_ACTION_OFFICER_CALL_PARADROPERS},
	{ "action_officer_call_gunplanes"	, USER_ACTION_OFFICER_CALL_GUNPLANES	},
	{ "action_officer_use_spyglass"		, USER_ACTION_OFFICER_BINOCULARS			},
	{ "action_engineer_place_mine_ap"	, USER_ACTION_ENGINEER_PLACE_MINE_AP	},
	{ "action_engineer_place_mine_at"	, USER_ACTION_ENGINEER_PLACE_MINE_AT	},
	{ "action_engineer_clear_mines"		, USER_ACTION_ENGINEER_CLEAR_MINES		},
	{ "action_engineer_build_fence"		,	USER_ACTION_ENGINEER_BUILD_FENCE		},
	{ "action_engineer_build_entrenchment", USER_ACTION_ENGINEER_BUILD_ENTRENCHMENT },
//	{ "action_engineer_build_tank_pit", USER_ACTION_ENGINEER_ENTRENCH_TANK	},
	{ "action_engineer_build_anti_tank", USER_ACTION_ENGINEER_BUILD_ANTITANK},
	{ "action_engineer_repair"				, USER_ACTION_ENGINEER_REPAIR					},
	{ "action_support_resupply"				, USER_ACTION_SUPPORT_RESUPPLY				},
	{ "action_support_resupply_hr"		, USER_ACTION_HUMAN_RESUPPLY					},
	{ "action_engineer_repair_object"	, USER_ACTION_ENGINEER_REPAIR_BUILDING	},
	{ "action_engineer_build_bridge"	, USER_ACTION_ENGINEER_BUILD_BRIDGE		},
//	{ "action_support_build_ru_storage",USER_ACTION_SUPPORT_BUILD_RU_STORAGE },
	{ "action_formation_0"	, USER_ACTION_FORMATION_0		},
	{ "action_formation_1"	, USER_ACTION_FORMATION_1		},
	{ "action_formation_2"	, USER_ACTION_FORMATION_2		},
	{ "action_formation_3"	, USER_ACTION_FORMATION_3		},
	{ "action_formation_4"	, USER_ACTION_FORMATION_4		},
	{ "action_form_squad"		, USER_ACTION_FORM_SQUAD		},
	{ "action_disband_squad", USER_ACTION_DISBAND_SQUAD },
	{ "action_ui_button_1_1", WCC_UI_BUTTON1_1	},
	{ "action_ui_button_1_2", WCC_UI_BUTTON1_2	},
	{ "action_ui_button_1_3", WCC_UI_BUTTON1_3	},
	{ "action_ui_button_1_4", WCC_UI_BUTTON1_4	},
	{ "action_ui_button_1_5", WCC_UI_BUTTON1_5	},
	{ "action_ui_button_1_6", WCC_UI_BUTTON1_6	},
	{ "action_ui_button_1_7", WCC_UI_BUTTON1_7	},
	{ "action_ui_button_1_8", WCC_UI_BUTTON1_8	},
	{ "action_ui_button_1_9", WCC_UI_BUTTON1_9	},
	{ "action_ui_button_1_10",WCC_UI_BUTTON1_10	},
	{ "action_ui_button_1_11",WCC_UI_BUTTON1_11	},
	{ "action_ui_button_1_12",WCC_UI_BUTTON1_12	},
	/*
	{ "action_sneak_on"			, USER_ACTION_SNEAK_ON			},
	{ "action_sneak_off"		, USER_ACTION_SNEAK_OFF			},
	*/
	{ "action_call_hq"			, USER_ACTION_CALL_HQ				},
	{ "action_place_marker",  USER_ACTION_PLACE_MARKER  },
	{ 0											,	0													}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static SMissionStatusObject statusObject;
const SMissionStatusObject* GetMissionStatusObject()
{
	return &statusObject;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetPassangers( const IMOContainer *pContainer, std::vector<IMOUnit*> &passangers, const bool bCanSelectOnly )
{
	const int nNumPassangers = pContainer->GetPassangers( 0, bCanSelectOnly );
	if ( nNumPassangers == 0 ) 
		return 0;
	const int nOldNumPassangers = passangers.size();
	passangers.resize( nOldNumPassangers + nNumPassangers );
	pContainer->GetPassangers( &(passangers[nOldNumPassangers]), bCanSelectOnly );
	return nNumPassangers;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetPassangers( const CMapObjectsList &containers, std::vector<IMOUnit*> &passangers, const bool bCanSelectOnly )
{
	for ( CMapObjectsList::const_iterator it = containers.begin(); it != containers.end(); ++it )
		GetPassangers( static_cast_ptr<const IMOContainer*>(*it), passangers, bCanSelectOnly );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** CSelector
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::SendAcknowledgement( interface IAILogic *pAILogic )
{
	if ( !addedObjects.empty() )
	{
		SMapObject * pMO = *addedObjects.rbegin();
		if ( pMO->IsAlive() )
			pAILogic->SendAcknowlegdementForced( pMO->GetAIObj(), ACK_SELECTED );
		//pMO->
		/*IMOSelectable *pSelMO = static_cast_ptr<IMOSelectable*>( *addedObjects.begin() );
		pSelMO->SendAcknowledgement( pAckManager, ACK_SELECTED );
		addedObjects.clear();*/
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// select single map object
bool CSelector::Select( struct SMapObject *pMO, bool bSelect, bool bSelectSuper )
{
	if ( bSelect && pMO && (!pMO->IsValid() || !pMO->IsAlive()) ) 
		return false;
	if ( bSelect )
	{
		IMOSelectable *pSelMO = static_cast<IMOSelectable*>( pMO );
		if ( bSelectSuper ) 
		{
			pSelMO->Select( this, bSelect, true );
			return true;
		}
		else if ( objset.find(pMO) == objset.end() )
		{
			addedObjects.push_back( pMO );
			objects.push_back( pMO );
			objset.insert( pMO );
			bValid = false;
			pSelMO->Select( this, bSelect, false );
			return true;
		}
	}
	else
	{
		if ( pMO )
		{
			CMapObjectsSet::iterator pos = objset.find( pMO );
			if ( pos != objset.end() )
			{
				static_cast<IMOSelectable*>(pMO)->Select( this, bSelect, false );
				addedObjects.remove( pMO );
				objects.remove( pMO );
				objset.erase( pos );
				bValid = false;
				if ( IsEmpty() ) 
					UnRegister();
				return true;
			}
		}
		else
		{
			for ( CMapObjectsList::iterator it = objects.begin(); it != objects.end(); ++it )
			{
				IMOSelectable *pMO = static_cast_ptr<IMOSelectable*>( *it );
				pMO->Select( this, bSelect, false );
			}
			//
			addedObjects.clear();
			objects.clear();
			objset.clear();
			bValid = false;
			UnRegister();
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelector::Register()
{
	if ( bValid ) 
	{
		NI_ASSERT_T( ((nSelectionGroupID != -1) && !objects.empty()) || ((nSelectionGroupID == -1) && objects.empty()), "ERROR: valid selector, but invalid selection group!" );
		return nSelectionGroupID;
	}
	// unregister previous selection group
	if ( nSelectionGroupID != -1 ) 
		UnRegister();
	if ( pTransceiver == 0 ) 
		return -1;
	// make new selection group
	if ( int nNumSelectedObjects = objects.size() )
	{
		IRefCount **ppAIObjects = GetTempBuffer<IRefCount*>( nNumSelectedObjects );
		IRefCount **ppTempObjects = ppAIObjects;
		for ( CMapObjectsList::iterator it = objects.begin(); it != objects.end(); )
		{
			if ( !(*it)->IsValid() ) 
			{
				objset.erase( *it );
				it = objects.erase( it );
				--nNumSelectedObjects;
			}
			else
			{
				*ppTempObjects++ = (*it)->GetAIObj();
				++it;
			}
		}
		// register
		if ( nNumSelectedObjects > 0 ) 
			nSelectionGroupID = pTransceiver->CommandRegisterGroup( ppAIObjects, nNumSelectedObjects );
		else
			nSelectionGroupID = -1;
		bValid = true;
	}
//	NStr::DebugTrace( "%s selector with group ID %d\n", bValid ? "Valid" : "Invalid", nSelectionGroupID );
	return nSelectionGroupID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::UnRegister()
{
	if ( pTransceiver && (nSelectionGroupID != -1) )
		pTransceiver->CommandUnregisterGroup( nSelectionGroupID );
	nSelectionGroupID = -1;
	bValid = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::UpdateSelection( IMOContainer *pContainer )
{
	if ( objset.find(pContainer) == objset.end() ) 
		return;
	DoneSelection();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::DoneSelection()
{
	// in the case of one object selected, initiate 'who-in-container' interface
	if ( objects.size() == 1 ) 
	{
		const int nNumPassangers = static_cast_ptr<IMOContainer*>( objects.back() )->GetPassangers( 0 );
		if ( nNumPassangers > 0 ) 
		{
			IUIScreen *pUIScreen = GetSingleton<IScene>()->GetMissionScreen();
			// remove old interfaces
			ClearWhoInContainer();
			typedef std::pair<IRefCount*, CUISquadElement*> CSquadAIPair;
			typedef std::list<CSquadAIPair> CSquadsAIList;
			CSquadsAIList squadsAI;
			// create new one(s)
			if ( pUIScreen ) 
			{
				CTRect<float> rcScreen;
				pUIScreen->GetWindowPlacement( 0, 0, &rcScreen );
				//
				std::vector<IMOUnit*> passangers( nNumPassangers );
				static_cast_ptr<IMOContainer*>( objects.back() )->GetPassangers( &(passangers[0]) );
				std::list<CUISquadElement*> squadElements;
				for ( std::vector<IMOUnit*>::iterator it = passangers.begin(); it != passangers.end(); ++it )
				{
					IRefCount *pAISquad = (*it)->GetSquad();
					//
					CUISquadElement *pSquad = 0;
					for ( CSquadsAIList::iterator squad = squadsAI.begin(); squad != squadsAI.end(); ++squad )
					{
						if ( squad->first == pAISquad ) 
						{
							pSquad = squad->second;
							break;
						}
					}
					if ( pSquad == 0 ) 
					{
						pSquad = new CUISquadElement;
						squads.push_back( pSquad );
						squadElements.push_back( pSquad );
						squadsAI.push_back( CSquadAIPair(pAISquad, pSquad) );
					}
					CUIUnitObserver *pObserver = new CUIUnitObserver(*it);
					pObserver->SetSquad( pSquad );
					(*it)->SetObserver( pObserver );
					pSquad->AddPassanger( pObserver );
				}
				CVec2 vPos( 5.0f, 5.0f ), vSize( 200, 32 );
				for ( std::list<CUISquadElement*>::iterator it = squadElements.begin(); it != squadElements.end(); ++it )
				{
					if ( (*it)->GetPassangerCount() > 1 )
					{
						(*it)->SetWindowPlacement( &vPos, &vSize );
						vPos.y += 32 + 2;
						(*it)->Reposition( rcScreen );
						pUIScreen->AddChild( *it );
						pUIScreen->MoveWindowDown( *it );
					}
				}
				for ( std::list<CUISquadElement*>::iterator it = squadElements.begin(); it != squadElements.end(); ++it )
				{
					if ( (*it)->GetPassangerCount() <= 1 )
					{
						(*it)->SetWindowPlacement( &vPos, &vSize );
						(*it)->Reposition( rcScreen );
						pUIScreen->AddChild( *it );
						pUIScreen->MoveWindowDown( *it );
						vPos.x += 32;
						if ( vPos.x > 32 * 9 + 5 )
						{
							vPos.y += 32 + 2;
							vPos.x = 5;
						}
					}
				}
			}
		}
		else
			ClearWhoInContainer();
	}
	else
		ClearWhoInContainer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::ClearWhoInContainer()
{
	//fLastSquadPosY = 5;
	IUIScreen *pUIScreen = GetSingleton<IScene>()->GetMissionScreen();
	// remove old interfaces
	if ( !squads.empty() && pUIScreen ) 
	{
		for ( CUIElementsList::iterator it = squads.begin(); it != squads.end(); ++it )
			pUIScreen->RemoveChild( *it );
		squads.clear();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelector::Visit( ISelectorVisitor *pVisitor ) const
{
	NI_ASSERT_TF( pVisitor != 0, "Can't visit with NULL visitor", return );
	for ( CMapObjectsList::const_iterator it = objects.begin(); it != objects.end(); ++it )
		pVisitor->VisitMapObject( const_cast_ptr<SMapObject*>(*it) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &objects );
	saver.Add( 2, &nSelectionGroupID );
	saver.Add( 3, &bValid );
	saver.Add( 4, &addedObjects );
	saver.Add( 5, &groups );
	//saver.Add( 6, &fLastSquadPosY );
	saver.Add( 7, &squads );
	if ( saver.IsReading() ) 
	{
		objset.clear();
		for ( CMapObjectsList::const_iterator it = objects.begin(); it != objects.end(); ++it )
			objset.insert( *it );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** CWorldClient
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CWorldClient::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CWorldBase*>(this) );
	// selection groups
	saver.Add( 3, &selunits );
	saver.Add( 4, &selbuildings );
	saver.Add( 5, &bCheckDiplomacy );
	//
	saver.Add( 6, &nForcedAction );
	saver.Add( 11, &bShowHPs );
	saver.Add( 12, &aviationPoints );
	saver.Add( 13, &fencePoints );
	saver.Add( 14, &pBoldLine );
	//
	saver.Add( 15, &availActions );
	saver.Add( 16, &lastActions );
	saver.Add( 17, &availPlanes );
	if ( saver.IsReading() )
	{
		nAutoAction = USER_ACTION_UNKNOWN;
		bActionModifierForcedMove = false;
		bActionModifierForcedAttack = false;
		bActionModifierAdd = false;
		framePick.clear();
		timeLastPick = 0;
		vLastPickPos = VNULL2;
		ResetPreSelection();
		statusObject.Clear();
	}
	saver.Add( 18, &aviationCircles );
	saver.Add( 19, &availCrossActions );
	saver.Add( 22, &bCanShowNextObjective );
	saver.Add( 23, &hiddenObjects );
	saver.Add( 24, &objectives );
	saver.Add( 25, &lastObjective );
	saver.Add( 26, &nCurrObjective );
	saver.Add( 27, &terrainPoints );
	saver.Add( 28, &buildPoints );
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CWorldClient::CWorldClient()
{
	bShowHPs = true;
	bActionModifierAdd = false;
	bActionModifierForcedMove = false;
	bActionModifierForcedAttack = false;
	//
	bCheckDiplomacy = true;
	nLastAvailableActionsSet = 0;
	timeLastPick = 0;
	vLastPickPos = VNULL2;
	//
	nForcedAction = USER_ACTION_UNKNOWN;
	nAutoAction = USER_ACTION_UNKNOWN;
	//
	availActions.Clear();
	lastActions.Clear();
	availPlanes.Clear();
	// all planes initially enabled
	availPlanes.SetAction( USER_ACTION_OFFICER_CALL_BOMBERS );
	availPlanes.SetAction( USER_ACTION_OFFICER_CALL_FIGHTERS );
	availPlanes.SetAction( USER_ACTION_OFFICER_CALL_SPY );
	availPlanes.SetAction( USER_ACTION_OFFICER_CALL_PARADROPERS );
	availPlanes.SetAction( USER_ACTION_OFFICER_CALL_GUNPLANES );
	//
	bCanShowNextObjective = true;
	bAviationLocked = false;
	nMultiplayer = -1;
	bShowMultipleMarkers = GetGlobalVar( "Options.NoMultipleMarkers", 0 ) == 0;
	bCombineRotation = false;
	bGotMoveCommand = false;
	bGotGridCommand = false;
	nCurrObjective = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillActionsPriority( const char *pszRow, const char *pszEntry, CTableAccessor &table, std::vector<int> &dst, bool bAddOther = true )
{
	std::vector<int> desired;
	table.GetArray( pszRow, pszEntry, desired, ',' );
	dst = desired;
	// add other actions
	if ( bAddOther ) 
	{
		for ( int i = 1; i < 64; ++i )
		{
			if ( std::find(desired.begin(), desired.end(), i) == desired.end() )
				dst.push_back( i );
		}
		// add 'UNKNOWN' action
		if ( std::find(desired.begin(), desired.end(), 0) == desired.end() )
			dst.push_back( 0 );
	}
	// remove all actions >= 64
	dst.erase( std::remove_if(dst.begin(), dst.end(), [](int val) { return val >= 64; }), dst.end() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Init( ISingleton *pSingleton )
{
	CWorldBase::Init( pSingleton );
	worldClientMsgs.Init( pInput, worldClientCommands );
	//
	RegisterAction( USER_ACTION_MOVE, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionMoveMsg );
	RegisterAction( USER_ACTION_MOVE_TO_GRID, SActionDesc::FORCED, &CWorldClient::ActionMoveToGridMsg );
	RegisterAction( USER_ACTION_SWARM, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionSwarmMsg );
	RegisterAction( USER_ACTION_ROTATE, SActionDesc::FORCED, &CWorldClient::ActionRotateMsg );
	RegisterAction( USER_ACTION_ATTACK, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionAttackMsg );
	RegisterAction( USER_ACTION_FOLLOW, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionFollowMsg );
	RegisterAction( USER_ACTION_STAND_GROUND, SActionDesc::INSTANT, &CWorldClient::ActionStandGroundMsg );

	RegisterAction( USER_ACTION_FORMATION_0, SActionDesc::INSTANT, &CWorldClient::ActionChangeFormationMsg );
	RegisterAction( USER_ACTION_FORMATION_1, SActionDesc::INSTANT, &CWorldClient::ActionChangeFormationMsg );
	RegisterAction( USER_ACTION_FORMATION_2, SActionDesc::INSTANT, &CWorldClient::ActionChangeFormationMsg );
	RegisterAction( USER_ACTION_FORMATION_3, SActionDesc::INSTANT, &CWorldClient::ActionChangeFormationMsg );
	RegisterAction( USER_ACTION_FORMATION_4, SActionDesc::INSTANT, &CWorldClient::ActionChangeFormationMsg );

	RegisterAction( USER_ACTION_USE_SHELL_DAMAGE, SActionDesc::INSTANT, &CWorldClient::ActionChangeShellTypeMsg );
	RegisterAction( USER_ACTION_USE_SHELL_AGIT, SActionDesc::INSTANT, &CWorldClient::ActionChangeShellTypeMsg );
	RegisterAction( USER_ACTION_USE_SHELL_SMOKE, SActionDesc::INSTANT, &CWorldClient::ActionChangeShellTypeMsg );
	
	RegisterAction( USER_ACTION_BOARD, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionBoardMsg );
	RegisterAction( USER_ACTION_LEAVE, SActionDesc::FORCED, &CWorldClient::ActionLeaveMsg );

	RegisterAction( USER_ACTION_INSTALL, SActionDesc::INSTANT, &CWorldClient::ActionInstallMsg );
	RegisterAction( USER_ACTION_UNINSTALL, SActionDesc::INSTANT, &CWorldClient::ActionUnInstallMsg );

	RegisterAction( USER_ACTION_CAPTURE_ARTILLERY, SActionDesc::AUTO, ActionCaptureArtilleryMsg );
	RegisterAction( USER_ACTION_HOOK_ARTILLERY, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionHookArtilleryMsg );
	RegisterAction( USER_ACTION_DEPLOY_ARTILLERY, SActionDesc::FORCED, &CWorldClient::ActionDeployArtilleryMsg );

	RegisterAction( USER_ACTION_ENTRENCH_SELF, SActionDesc::INSTANT, &CWorldClient::ActionEntrenchSelfMsg );

	RegisterAction( USER_ACTION_ENGINEER_PLACE_MINE_AP, SActionDesc::FORCED, &CWorldClient::ActionPlaceMineAPMsg );
	RegisterAction( USER_ACTION_ENGINEER_PLACE_MINE_AT, SActionDesc::FORCED, &CWorldClient::ActionPlaceMineATMsg );
	RegisterAction( USER_ACTION_ENGINEER_CLEAR_MINES, SActionDesc::FORCED, &CWorldClient::ActionClearMineMsg );
	RegisterAction( USER_ACTION_ENGINEER_BUILD_FENCE, SActionDesc::FORCED, &CWorldClient::ActionBuildFenceMsg );
	RegisterAction( USER_ACTION_ENGINEER_BUILD_ENTRENCHMENT, SActionDesc::FORCED, &CWorldClient::ActionBuildEntrenchmentMsg );
	RegisterAction( USER_ACTION_ENGINEER_BUILD_ANTITANK, SActionDesc::FORCED, &CWorldClient::ActionBuildAntiTank );
	RegisterAction( USER_ACTION_ENGINEER_BUILD_BRIDGE, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionBuildBridgeMsg );
	RegisterAction( USER_ACTION_ENGINEER_REPAIR, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionRepairMsg );
	RegisterAction( USER_ACTION_ENGINEER_REPAIR_BUILDING, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionRepairBuildingMsg );

	RegisterAction( USER_ACTION_SUPPORT_RESUPPLY, SActionDesc::AUTO | SActionDesc::FORCED, &CWorldClient::ActionResupplyMsg );
	RegisterAction( USER_ACTION_HUMAN_RESUPPLY, SActionDesc::FORCED, &CWorldClient::ActionResupplyHRMsg );
	RegisterAction( USER_ACTION_FILL_RU, SActionDesc::AUTO, &CWorldClient::ActionFillRU );
	//RegisterAction( USER_ACTION_SUPPORT_BUILD_RU_STORAGE, SActionDesc::FORCED, &CWorldClient::ActionBuildRUStorageMsg );


	RegisterAction( USER_ACTION_GUARD, SActionDesc::FORCED, &CWorldClient::ActionGuardMsg );
	RegisterAction( USER_ACTION_AMBUSH, SActionDesc::INSTANT, &CWorldClient::ActionAmbush );

	RegisterAction( USER_ACTION_RANGING, SActionDesc::FORCED, &CWorldClient::ActionRangingMsg );
	RegisterAction( USER_ACTION_SUPPRESS, SActionDesc::FORCED, &CWorldClient::ActionSuppressMsg );

	RegisterAction( USER_ACTION_OFFICER_CALL_BOMBERS, SActionDesc::FORCED, &CWorldClient::ActionCallForBombersMsg );
	RegisterAction( USER_ACTION_OFFICER_CALL_FIGHTERS, SActionDesc::FORCED, &CWorldClient::ActionCallForFightersMsg );
	RegisterAction( USER_ACTION_OFFICER_CALL_SPY, SActionDesc::FORCED, &CWorldClient::ActionCallForSpyMsg );
	RegisterAction( USER_ACTION_OFFICER_CALL_PARADROPERS, SActionDesc::FORCED, &CWorldClient::ActionCallForParadropersMsg );
	RegisterAction( USER_ACTION_OFFICER_CALL_GUNPLANES, SActionDesc::FORCED, &CWorldClient::ActionCallForGunplanesMsg );
	RegisterAction( USER_ACTION_OFFICER_BINOCULARS, SActionDesc::FORCED, &CWorldClient::ActionLookAtBinocularsMsg );

	///RegisterAction( USER_ACTION_GUNNER_ASSIGN_TO_GUN, SActionDesc::AUTO, &CWorldClient::ActionAssignToGunMsg );

	RegisterAction( USER_ACTION_FORM_SQUAD, SActionDesc::INSTANT, &CWorldClient::ActionFormSquadMsg );
	RegisterAction( USER_ACTION_DISBAND_SQUAD, SActionDesc::INSTANT, &CWorldClient::ActionDisbandSquadMsg );

	RegisterAction( USER_ACTION_CALL_HQ, SActionDesc::INSTANT, &CWorldClient::ActionCallHQMsg );
	
	RegisterAction( USER_ACTION_STOP, SActionDesc::INSTANT, &CWorldClient::ActionStopMsg );
	RegisterAction( USER_ACTION_PLACE_MARKER, SActionDesc::FORCED, &CWorldClient::ActionPlaceMarkerMsg );
	//
	RegisterAction( USER_ACTION_DO_SELFACTION, SActionDesc::AUTO, &CWorldClient::ActionDoSelfActionMsg );
	// load action sequences
	CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
	FillActionsPriority( "World", "Actions.User.Friendly", table, userPriorityFriendly );
	FillActionsPriority( "World", "Actions.User.Neutral", table, userPriorityNeutral );
	FillActionsPriority( "World", "Actions.User.Enemy", table, userPriorityEnemy );
	FillActionsPriority( "World", "Actions.User.Self", table, userPrioritySelf, false );
	FillActionsPriority( "World", "Actions.Exclude.Friendly", table, excludeFriendly, false );
	FillActionsPriority( "World", "Actions.Exclude.Neutral", table, excludeNeutral, false );
	FillActionsPriority( "World", "Actions.Exclude.Enemy", table, excludeEnemy, false );
	// copy self actions to CUserActions class
	for ( std::vector<int>::const_iterator it = userPrioritySelf.begin(); it != userPrioritySelf.end(); ++it )
		selfActions.SetAction( *it );
	//
	selunits.SetTransceiver( pTransceiver );
	fMinRotShiftSq = fabs2( GetGlobalVar( "World.MinRotateRadius", 30.0f ) );
	bSetPlayerTooltip = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::NewObjectAdded( SMapObject *pMO )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Update( const NTimer::STime &currTime )
{
	CWorldBase::Update( currTime );
	while ( const char *pszString = GetSingleton<IConsoleBuffer>()->ReadASCII(CONSOLE_STREAM_WORLD) )
	{
		NStr::DebugTrace( "Got command: %s\n", pszString );
		std::vector<std::string> szParts;
		NStr::SplitString( pszString, szParts, '(' );
		if ( szParts.size() < 2 || szParts[1].length() == 0)
		{
			NStr::DebugTrace( "Skipping command: incorrect syntax\n" );
			continue;
		}
		szParts[1] = szParts[1].substr( 0, szParts[1].length() - 1 );
		std::vector<std::string> szOperands;
		NStr::SplitString( szParts[1], szOperands, ',' );
		if ( szParts[0].compare( "SetCamera" ) == 0 )
		{
			if ( !NStr::IsDecNumber(szOperands[0]) || !NStr::IsDecNumber(szOperands[1]) )
			{
				NStr::DebugTrace( "Skipping command: wrong operands to SetCamera\n" );
				continue;
			}
			const CVec3 anchor = CVec3( NStr::ToFloat(szOperands[0]), NStr::ToFloat(szOperands[1]), 0 );
			pCamera->SetAnchor( anchor );
		}
		else if ( szParts[0].compare( "HighlightMinimap" ) == 0 )
		{
			if ( !NStr::IsDecNumber(szOperands[0]) || !NStr::IsDecNumber(szOperands[1]) )
			{
				NStr::DebugTrace( "Skipping command: wrong operands to HighlightMinimap\n" );
				continue;
			}
			if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() )
			{
				if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) ) )
				{
					CVec2 objectivePosition( NStr::ToFloat(szOperands[0]), NStr::ToFloat(szOperands[1]) );
					if ( IGameTimer* pGameTimer = GetSingleton<IGameTimer>() )
					{
						NTimer::STime currentAbsTime = pGameTimer->GetAbsTime();
						pUIMiniMap->AddMarker( "ObjectiveReceived", objectivePosition, true, 1, currentAbsTime, 5000, false );
						pUIMiniMap->AddCircle( objectivePosition, fWorldCellSize * 8.0f, MMC_STYLE_MIXED, 0xF00F, currentAbsTime, 3000, false, 0 );
					}
				}
			}
		}
		else if ( szParts[0].compare( "HighlightControl" ) == 0 )
		{
			if ( !NStr::IsDecNumber(szOperands[0]) )
			{
				NStr::DebugTrace( "Skipping command: wrong operand to HighlightControl\n" );
				continue;
			}
			if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() )
			{
				if ( IUIContainer *pDialog = checked_cast<IUIContainer *>( pUIScreen->GetChildByID( 100 ) ) )
				{
					SUIMessage msg;
					msg.nMessageCode = UI_BLINK_WINDOW;
					msg.nFirst = NStr::ToFloat( szOperands[0] );        
					msg.nSecond = 1<<16;
					pDialog->ProcessMessage( msg );
				}
			}
		}
		else if ( szParts[0].compare( "HighlightIndicator" ) == 0 )
		{
			if ( !NStr::IsDecNumber(szOperands[0]) )
			{
				NStr::DebugTrace( "Skipping command: wrong operand to HighlightIndicator\n" );
				continue;
			}
			if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() )
			{
				if ( IUIContainer *pDialog = checked_cast<IUIContainer *>( pUIScreen->GetChildByID( 5000 ) ) )
				{
					SUIMessage msg;
					msg.nMessageCode = UI_BLINK_WINDOW;
					msg.nFirst = NStr::ToFloat(szOperands[0]);        
					msg.nSecond = 1<<16;
					pDialog->ProcessMessage( msg );
				}
			}
		}
		else if ( szParts[0].compare( "GetSelectedUnits" ) == 0 )
		{
			CMapObjectsList selList = selunits.GetObjects();
			if ( selList.size() != 0 )
			{
				std::string szCmd = "ReturnScriptIDs( ";

				for ( CMapObjectsList::const_iterator it = selList.begin(); it != selList.end(); ++it )
					szCmd += NStr::Format( "%d,", reinterpret_cast<int>((*it)->pAIObj.GetPtr()) );
				szCmd.resize( szCmd.size() - 1 );
				szCmd += " )";

				GetSingleton<IAILogic>()->CallScriptFunction( szCmd.c_str() );
			}
		}
		else
		{
			NStr::DebugTrace( "Skipping command: unknown function\n" );
			continue;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** selection and pre-selection operations
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SelectType( const CVec2 &vPos )
{
	if ( pScene->GetMissionScreen()->PickElement(vPos, 1000000) != 0 )
		return;
	//
	UpdatePick( vPos, pTimer->GetGameTime(), false );
	//
	if ( bActionModifierForcedAttack ) 
	{
		for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
		{
			if ( ((*it)->pDesc->eGameType == SGVOGT_UNIT) && (*it)->IsAlive() && IsFriend(*it) && (*it)->CanSelect() )
			{
				CMapObjectsPtrList mapobjects;
				GetAllObjectsByMatch( mapobjects, (*it)->pDesc, true );
				if ( !mapobjects.empty() ) 
				{
					Select( mapobjects, bActionModifierAdd );
					ResetPreSelection( 0 );
				}
			}
			break;
		}
	}
	else
	{
		for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
		{
			if ( ((*it)->pDesc->eGameType == SGVOGT_UNIT) && (*it)->IsAlive() && IsFriend(*it) && (*it)->CanSelect() )
			{
				std::vector<IVisObj*> objects;
				SelectType( *it, objects );
				if ( !objects.empty() ) 
					Select( &(objects[0]), objects.size() );
				return;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SelectType( SMapObject *pMO, std::vector<IVisObj*> &objects )
{
	std::pair<IVisObj*, CVec2> *ppObjects = 0;
	int nNumObjects = 0;
	pScene->Pick( pGFX->GetScreenRect(), &ppObjects, &nNumObjects, SGVOGT_UNIT, true );
	objects.reserve( nNumObjects );
	for ( int i = 0; i != nNumObjects; ++i )
	{
		IVisObj *pVO = ppObjects[i].first;
		SMapObject *pMO2 = FindByVis( pVO );
		if ( pMO2 && (pMO2->pDesc == pMO->pDesc) && pMO2->IsAlive() ) 
			objects.push_back( pVO );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Select( const CVec2 &vPos )
{
	// reset non-unit forced actions (call for planes)
	if ( IsSelectionEmpty() && (nForcedAction != USER_ACTION_UNKNOWN) ) 
	{
		ResetForcedAction();
		return;
	}
	//
	if ( bActionModifierForcedAttack )
	{
		SelectType( vPos );
		return;
	}
	EObjGameType eSelType = SGVOGT_UNKNOWN;
	if ( IsBuildingsSelected() )
		eSelType = SGVOGT_BUILDING;
	else if ( IsUnitsSelected() )
		eSelType = SGVOGT_UNIT;
	//
	UpdatePick( vPos, pTimer->GetGameTime(), false );
	switch ( eSelType )
	{
		case SGVOGT_UNKNOWN:
			for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
			{
				if ( ((*it)->pDesc->eGameType == SGVOGT_UNIT) && (*it)->CanSelect() )
				{
					IVisObj *pObj = (*it)->pVisObj.GetPtr();
					Select( &pObj, 1 );
					return;
				}
				else if ( ((*it)->pDesc->eGameType == SGVOGT_BUILDING) && IsFriend(*it) )
				{
					IVisObj *pObj = (*it)->pVisObj.GetPtr();
					SelectBuilding( pObj, bActionModifierAdd );
					return;
				}
			}
			break;
		case SGVOGT_UNIT:
			{
				CMapObjectsPtrList tounselect;
				int nNumUnselect = 0;
				for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
				{
					if ( ((*it)->pDesc->eGameType == eSelType) && (*it)->CanSelect() )
					{
						if ( (!bActionModifierAdd && selunits.IsSelected(*it)) || !selunits.IsSelected(*it) ) 
						{
							IVisObj *pObj = (*it)->pVisObj.GetPtr();
							Select( &pObj, 1 );
							return;
						}
						else if ( bActionModifierAdd && selunits.IsSelected(*it) ) 
						{
							tounselect.push_back( *it );
							++nNumUnselect;
						}
					}
				}
				//
				if ( nNumUnselect == 1 ) 
				{
					IVisObj *pObj = tounselect.front()->pVisObj.GetPtr();
					Select( &pObj, 1 );
					return;
				}
			}
			break;
		case SGVOGT_BUILDING:
			for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
			{
				if ( ((*it)->pDesc->eGameType == eSelType) && IsFriend(*it) )
				{
					IVisObj *pObj = (*it)->pVisObj.GetPtr();
					SelectBuilding( pObj, bActionModifierAdd );
					return;
				}
			}
			break;
	}
	// reset selection in the case of empty selection
	ResetSelection();
	if ( nForcedAction != USER_ACTION_UNKNOWN ) 
		ResetForcedAction();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SelectBuilding( IVisObj *pVisObj, bool bMerge )
{
	if ( !bMerge )
		ResetSelection();

	SMapObject *pMO = FindByVis( pVisObj );
	if ( (pMO == 0) || !pMO->CanSelect() || (pMO->pDesc->eGameType != SGVOGT_BUILDING) )
		return;
	if ( !selbuildings.IsSelected(pMO) )
	{
		selbuildings.Select( pMO, true, false );
		selbuildings.DoneSelection();
	}
	else if ( bMerge )
		ResetSelection( pMO->pVisObj );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CWorldClient::Select( IVisObj **newObjects, int nNumObjects )
{
	if ( nNumObjects == 0 )
		return selunits.GetAIGroup();
	// AI objects temp buffer
	CMapObjectsPtrList mapObjects;
	// form AI objects vector
	for ( int i=0; i<nNumObjects; ++i )
	{
		SMapObject *pMO = FindByVis( newObjects[i] );
		if ( pMO && pMO->IsAlive() ) 
			mapObjects.push_back( pMO );
	}
	// check diplomacy
	int nNumSelectedObjects = nNumObjects;
	if ( bCheckDiplomacy )
	{
		// select only allied objects
		for ( CMapObjectsPtrList::iterator it = mapObjects.begin(); it != mapObjects.end(); )
		{
			if ( !(*it)->CanSelect() )
				it = mapObjects.erase( it );
			else
				++it;
		}
		nNumSelectedObjects = mapObjects.size();
	}
	// check for non-empty selection
	if ( nNumSelectedObjects == 0 )
		return selunits.GetAIGroup();
	//
	Select( mapObjects, bActionModifierAdd );
	// reset pre-selection list
	//preselectedObjects.clear();
	ResetPreSelection( 0 );
	//
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Select( CMapObjectsPtrList &mapObjects, bool bMerge )
{
	// reset all previous selection
	if ( !bMerge )
		ResetSelection();
	// form selected objects list
	const int nNumSelectedObjects = mapObjects.size();
	CMapObjectsList objects;
	for ( CMapObjectsPtrList::iterator it = mapObjects.begin(); it != mapObjects.end(); ++it )
	{
		SMapObject *pMO = *it;
		if ( !pMO->IsAlive() ) 
			continue;
		//
		if ( !selunits.IsSelected(pMO) ) 
			selunits.Select( pMO, true, true );
		else if ( bMerge && (nNumSelectedObjects == 1) )
			ResetSelection( pMO );
	}
	selunits.SendAcknowledgement( pAILogic );
	selunits.DoneSelection();
	GetSingleton<IScene>()->AddSound( "int_selectunit", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Select( SMapObject *pMO )
{
	if ( !selunits.IsSelected(pMO) ) 
	{
		CMapObjectsPtrList lst;
		lst.push_back( pMO );
		Select( lst, true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::ResetSelection( SMapObject *pMO )
{
	if ( pMO == 0 )												// reset all selected objects
	{
		if ( !selunits.IsEmpty() ) 
		{
			selunits.Select( 0, false, false );
			selunits.DoneSelection();
			selunits.GetAIGroup();
		}
		else if ( !selbuildings.IsEmpty() ) 
		{
			selbuildings.Select( 0, false, false );
			selbuildings.DoneSelection();
			selunits.GetAIGroup();
		}
		//
		ResetPoints();
	}
	else																	// reset particular object
	{
		if ( selunits.IsSelected(pMO) ) 
		{
			selunits.Select( pMO, false, false );
			selunits.DoneSelection();
		}
		else if ( selbuildings.IsSelected(pMO) ) 
		{
			selbuildings.Select( pMO, false, false );
			selbuildings.DoneSelection();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::ResetSelection( IVisObj *pObj )
{
	SMapObject *pMO = pObj != 0 ? FindByVis( pObj ) : 0;
	ResetSelection( pMO );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::PreSelect( IVisObj **objects, int nNumObjects )
{
	for ( int i=0; i<nNumObjects; ++i )
	{
		if ( SMapObject *pMO = FindByVis(objects[i]) )
		{
			preselectedObjects.push_back( pMO );
			if ( IMOUnit *pUnit = dynamic_cast<IMOUnit*>( pMO ) )
			{
				if ( IMOSquad *pSquad = pUnit->GetSquad() )
				{
						int nUnits = pSquad->GetPassangers( 0 );
						if ( nUnits > 0 )
						{
							std::vector<IMOUnit*> units;
							units.resize( nUnits );
							pSquad->GetPassangers( &(units[0]) );
							for ( std::vector<IMOUnit*>::iterator it = units.begin(); it != units.end(); ++it )
								if ( (*it)->pVisObj.IsValid() && (*it)->CanSelect() && pUnit->fHP > 0 )
									(*it)->pVisObj->SetSpecular( 0x00000020 );
						}
				}
				else if ( pUnit->pVisObj.IsValid() && pUnit->CanSelect() && pUnit->fHP > 0 )
					pUnit->pVisObj->SetSpecular( 0x00000020 );
			}
			if ( pMO->pVisObj.IsValid() && pMO->pVisObj->GetSelectionState() != SGVOSS_SELECTED )
				pMO->pVisObj->Select( SGVOSS_PRESELECTED );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::ResetPreSelection( IVisObj *pObj )
{
	if ( pObj == 0 )
	{
		for ( CMapObjectsList::iterator it = preselectedObjects.begin(); it != preselectedObjects.end(); ++it )
		{
			if ( (*it)->pVisObj.IsValid() && (*it)->pVisObj->GetSelectionState() == SGVOSS_PRESELECTED )
				(*it)->pVisObj->Select( SGVOSS_UNSELECTED );
			if ( IMOUnit *pUnit = dynamic_cast<IMOUnit*>( (SMapObject*)(*it) ) )
			{
				if ( IMOSquad *pSquad = pUnit->GetSquad() )
				{
						int nUnits = pSquad->GetPassangers( 0 );
						if ( nUnits > 0 )
						{
							std::vector<IMOUnit*> units;
							units.resize( nUnits );
							pSquad->GetPassangers( &(units[0]) );
							for ( std::vector<IMOUnit*>::iterator it = units.begin(); it != units.end(); ++it )
								if ( (*it)->pVisObj.IsValid() )
									(*it)->pVisObj->SetSpecular( 0x00000000 );
						}
				}
				else if ( pUnit->pVisObj.IsValid() )
					pUnit->pVisObj->SetSpecular( 0x00000000 );
			}
		}
		preselectedObjects.clear();
	}
	else
	{
		SMapObject *pMO = FindByVis( pObj );
		// reset pre-selected stats
		if ( pMO->pVisObj.IsValid() && pMO->pVisObj->GetSelectionState() == SGVOSS_PRESELECTED )
			pMO->pVisObj->Select( SGVOSS_UNSELECTED );
		if ( IMOUnit *pUnit = dynamic_cast<IMOUnit*>( pMO ) )
		{
				if ( IMOSquad *pSquad = pUnit->GetSquad() )
				{
						int nUnits = pSquad->GetPassangers( 0 );
						if ( nUnits > 0 )
						{
							std::vector<IMOUnit*> units;
							units.resize( nUnits );
							pSquad->GetPassangers( &(units[0]) );
							bool bResetSelection = true;
							for ( std::vector<IMOUnit*>::iterator it = units.begin(); it != units.end(); ++it )
							{
								CMapObjectsList::iterator pos = std::find( preselectedObjects.begin(), preselectedObjects.end(), (SMapObject*)(*it) );
								bResetSelection = bResetSelection && ( pos == preselectedObjects.end() || *it == pUnit );
							}
							if ( bResetSelection )
								for ( std::vector<IMOUnit*>::iterator it = units.begin(); it != units.end(); ++it )
									if ( (*it)->pVisObj.IsValid() )
										(*it)->pVisObj->SetSpecular( 0x00000000 );
						}
				}
				else if ( pUnit->pVisObj.IsValid() )
					pUnit->pVisObj->SetSpecular( 0x00000000 );
		}
		// remove from pre-selected list
		CMapObjectsList::iterator pos = std::find( preselectedObjects.begin(), preselectedObjects.end(), pMO );
		if ( pos != preselectedObjects.end() )
			preselectedObjects.erase( pos );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** pick functions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::PickFoF( const CVec2 &vPos, EObjGameType type, CMapObjectsPtrList &friends, CMapObjectsPtrList &foes, CMapObjectsPtrList &neutrals )
{
	UpdatePick( vPos, pTimer->GetGameTime(), false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (type != SGVOGT_UNKNOWN) && ((*it)->pDesc->eGameType != type) )
			continue;

		SMapObject *pMO = *it;
		if ( IsFriend(pMO) )
			friends.push_back( pMO );
		else if ( IsFoe(pMO) )
			foes.push_back( pMO );
		else
			neutrals.push_back( pMO );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::Pick( const CVec2 &vPos, EObjGameType type, CMapObjectsPtrList &objects )
{
	UpdatePick( vPos, pTimer->GetGameTime(), false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (type != SGVOGT_UNKNOWN) && ((*it)->pDesc->eGameType != type) )
			continue;
		objects.push_back( *it );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 1.level: hp > 0?
// 2.level: unit, building, entrenchment, mine, object
// 3.level: enemy, friend, neutral
// 4.level: can select, can't select
struct SMapObjectLessFunctional
{
	bool operator()( const SMapObject *pMO1, const SMapObject *pMO2 ) const
	{
		if ( (pMO1->fHP > 0) && (pMO2->fHP > 0) ) 
		{
			if ( pMO1->pDesc->eGameType == pMO2->pDesc->eGameType )
			{
				if ( pMO1->diplomacy == pMO2->diplomacy )
				{
					if ( pMO1->CanSelect() == pMO2->CanSelect() ) 
						return pMO1->pDesc->eVisType > pMO2->pDesc->eVisType;
					else
						return pMO1->CanSelect() > pMO2->CanSelect();
				}
				else
					return pMO1->diplomacy > pMO2->diplomacy;
			}
			else
				return pMO1->pDesc->eGameType < pMO2->pDesc->eGameType;
		}
		else
			return pMO1->fHP > pMO2->fHP;
	}
};
void CWorldClient::PickAll( const CVec2 &vPos, CMapObjectsPtrList &objects, EObjGameType upto, bool bVisible, bool bAliveUnits, bool bAliveOther )
{
	std::pair<IVisObj*, CVec2> *pObjects = 0;
	int nNumObjects = 0;
	pScene->Pick( vPos, &pObjects, &nNumObjects, SGVOGT_UNKNOWN );
	for ( int i=0; i<nNumObjects; ++i )
	{
		SMapObject *pMO = FindByVis( pObjects[i].first );
		if ( (pMO == 0) || (pMO->pDesc->eGameType > upto) ) 
			continue;
		// filter out non-interest objects
		if ( ( (pMO->pDesc->eGameType == SGVOGT_UNIT) && ((bAliveUnits && pMO->IsAlive()) || !bAliveUnits) ) ||
			   ( (pMO->pDesc->eGameType != SGVOGT_UNIT) && ((bAliveOther && pMO->IsAlive()) || !bAliveOther) ) )
			objects.push_back( pMO );
	}
	// sort by game type and player
	objects.sort( SMapObjectLessFunctional() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::UpdatePick( const CVec2 &vPos, const NTimer::STime &time, bool bForced, bool bAliveUnits, bool bAliveOther )
{
	if ( (timeLastPick != time) || (vLastPickPos != vPos) || bForced )
	{
		framePick.clear();
		PickAll( vPos, framePick, SGVOGT_FENCE, true, bAliveUnits, bAliveOther );
		timeLastPick = time;
		vLastPickPos = vPos;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SMapObject* CWorldClient::GetFirstPick( const EObjGameType upto, const bool bAliveUnits, bool bAliveOther ) const
{
	for ( CMapObjectsPtrList::const_iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		const SMapObject *pMO = *it;
		const SGDBObjectDesc *pDesc = pMO->GetDesc();
		if ( ( ((pDesc->eGameType == SGVOGT_UNIT) && ((bAliveUnits && pMO->IsAlive()) || !bAliveUnits)) ||
			     ((pDesc->eGameType != SGVOGT_UNIT) && ((bAliveOther && pMO->IsAlive()) || !bAliveOther)) ) &&
				 (pDesc->eGameType <= upto) )
			return pMO;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** selection groups
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::ClearSelectionGroup( int nIndex )
{
	SSelectionGroup &group = selunits.GetSelectionGroup( nIndex );
	group.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::AddUnitToSelectionGroup( IMOUnit *pUnit, const int nSelectionGroupID )
{
	if ( (nSelectionGroupID < 0) || (nSelectionGroupID > 9) ) 
		return;
	// remove from old group
	RemoveFromSelectionGroup( pUnit );
	// add this unit
	SSelectionGroup &group = selunits.GetSelectionGroup( nSelectionGroupID );
	group.Add( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::AssignSelectionGroup( int nIndex )
{
	// clear old croup
	ClearSelectionGroup( nIndex );
	// bind new objects
	SSelectionGroup &group = selunits.GetSelectionGroup( nIndex );
	// assign selection group index for all objects in this group
	CCollectObjectsSelectiorVisitor visitor;
	selunits.Visit( &visitor );
	typedef std::unordered_set<IMOUnit*, SDefaultPtrHash> CUnitsSet;
	CUnitsSet unitset;
	for ( CMapObjectsList::iterator it = visitor.GetObjects().begin(); it != visitor.GetObjects().end(); ++it )
	{
		IMOUnit *pUnit = static_cast_ptr<IMOUnit*>( *it );
		if ( IMOSquad *pSquad = pUnit->GetSquad() )
		{
			const int nNumUnits = pSquad->GetPassangers( 0 );
			IMOUnit **ppUnits = new IMOUnit*[nNumUnits];
			pSquad->GetPassangers( ppUnits );
			for ( int i = 0; i < nNumUnits; ++i )
				unitset.insert( ppUnits[i] );
			delete []ppUnits;
		}
		else
			unitset.insert( pUnit );
	}
	//
	for ( CUnitsSet::iterator it = unitset.begin(); it != unitset.end(); ++it )
	{
		// remove from any selection group
		RemoveFromSelectionGroup( *it );
		// assign to new
		group.Add( *it );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::ActivateSelectionGroup( int nIndex, bool bMerge )
{
	SSelectionGroup &group = selunits.GetSelectionGroup( nIndex );
	if ( group.IsEmpty() )
		return;
	//
	CMapObjectsPtrList mos;
	for ( SSelectionGroup::CUnitsList::iterator it = group.units.begin(); it != group.units.end();  )
	{
		if ( it->IsValid() ) 
		{
			IMOUnit *pMOUnit = *it;
			if ( pMOUnit->CanSelect() )
				mos.push_back( *it );
			++it;
		}
		else
			it = group.units.erase( it );
	}
	//
	Select( mos, bMerge );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::RemoveFromSelectionGroup( SMapObject *pMO )
{
	if ( pMO->nSelectionGroupID == -1 )
		return;
	const int nIndex = pMO->nSelectionGroupID;
	//
	SSelectionGroup &group = selunits.GetSelectionGroup( nIndex );
	group.Remove( pMO );
	pMO->nSelectionGroupID = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::CenterSelectionGroup( int nIndex )
{
	const SSelectionGroup &group = selunits.GetSelectionGroup( nIndex );
	if ( group.units.empty() ) 
		return;

	CVec3 vCenterPos = VNULL3;
	int nCounter = 0;
	for ( SSelectionGroup::CUnitsList::const_iterator it = group.units.begin(); it != group.units.end(); ++it, ++nCounter )
	{
		CVec3 vPos;
		WORD wDir;
		(*it)->GetPlacement( &vPos, &wDir );
		vCenterPos += vPos;
	}
	vCenterPos *= 1.0f / float( nCounter );
	vCenterPos.z = 0;
	pCamera->SetAnchor( vCenterPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** actions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SetAvailablePlanes( int nPlaneType, int nTime, bool bEnable )
{
	if ( bEnable ) 
		availPlanes.SetAction( nPlaneType );
	else
		availPlanes.RemoveAction( nPlaneType );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DisplayObjective( IUIContainer *pDialog, IText *pHeader, IText *pBody )
{
	IUIElement *pTextHeader = checked_cast<IUIElement*>(pDialog->GetChildByID(20001));
	IUIElement *pTextBody = checked_cast<IUIElement*>(pDialog->GetChildByID(3000));

	if ( pHeader ) 
	{
		pTextHeader->SetWindowText( 0, pHeader->GetString() );
	}
	else
		pTextHeader->SetWindowText( 0, L"" );
	
	// main message body
	if ( pBody ) 
	{
		pTextBody->SetWindowText( 0, pBody->GetString() );
	}
	else
		pTextBody->SetWindowText( 0, L"" );
	
	if ( !pHeader && !pBody )
		pDialog->ShowWindow( UI_SW_HIDE );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorldClient::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		// CRAP{ for animations testing (for Max)
		case WCC_TEST_ANIMATIONS:
			{
				if ( (selunits.size() == 1) && selunits.back()->pDesc->IsTechnics() ) 
				{
					IMOUnit *pUnit = static_cast_ptr<IMOUnit*>( selunits.back() );
					const SUnitBaseRPGStats *pRPG = static_cast_gdb<const SUnitBaseRPGStats*>( selunits.back()->pRPG );
					for ( std::vector< std::vector<SUnitBaseRPGStats::SAnimDesc> >::const_iterator it = pRPG->animdescs.begin(); it != pRPG->animdescs.end(); ++it )
					{
						for ( std::vector<SUnitBaseRPGStats::SAnimDesc>::const_iterator anim = it->begin(); anim != it->end(); ++anim )
						{
							pUnit->AddAnimation( &(*anim) );
						}
					}
					AddUpdatableUnit( pUnit );
				}
			}
			break;
		// CRAP}
			
		// CRAP{ for effects testing
		case WCC_PLACE_EFFECT:
			{
				CPtr<IEffectVisObj> pEffect = static_cast<IEffectVisObj*>( pVOB->BuildObject( "effects\\effects\\test", 0, SGVOT_EFFECT ) );
				if ( pEffect )
				{
					CVec3 pos3;
					pScene->GetPos3( &pos3, GetSingleton<ICursor>()->GetPos() );
					pEffect->SetPlacement( pos3, 0 );
					pEffect->SetStartTime( pTimer->GetGameTime() + 500 );
					pScene->AddObject( pEffect, SGVOGT_EFFECT );
				}
			}
			break;
		// CRAP}

		case WCC_SELECT_BY_TYPE:
			SelectType( pCursor->GetPos() );
			break;

		case MC_ADD_ACTION_OFF:
			if ( !aviationPoints.empty() && ActionAviationCall() ) 
			{
				ResetForcedAction();
				PlaySuccessSound();
			}
			else if ( !buildPoints.empty() && DoCommandsList(buildPoints) ) 
			{
				ResetForcedAction();
				PlaySuccessSound();
			}
			pScene->FlashPosMarkers();
			pScene->SetDirectionalArrow( VNULL3, VNULL3, false );
		case MC_ADD_ACTION_ON:
			bActionModifierAdd = ( msg.nEventID == MC_ADD_ACTION_ON );
			//CRAP{ FOR TEST
			{
				IScene * pScene = GetSingleton<IScene>();
				IStatSystem *pStat = pScene->GetStatSystem();
				pStat->UpdateEntry( "bActionModifierAdd = ", NStr::Format("%d", bActionModifierAdd)  );
			}
			//CRAP}
			break;

		case MC_FORCE_ACTION_MOVE_ON:
			bGotGridCommand = bGotMoveCommand;
			bGotMoveCommand = false;
			fRotationStartAngle = FP_PI / 2.0f;
			pScene->SetRotationStartAngle( fRotationStartAngle );
			bActionModifierForcedMove = ( msg.nEventID == MC_FORCE_ACTION_MOVE_ON );
			break;
		case MC_FORCE_ACTION_MOVE_OFF:
			bGotMoveCommand = bGotGridCommand;
			bGotGridCommand = false;
			fRotationStartAngle = 0;
			pScene->SetRotationStartAngle( fRotationStartAngle );
			bActionModifierForcedMove = ( msg.nEventID == MC_FORCE_ACTION_MOVE_ON );
			break;

		case MC_FORCE_ACTION_ATTACK_ON:
		case MC_FORCE_ACTION_ATTACK_OFF:
			bActionModifierForcedAttack = ( msg.nEventID == MC_FORCE_ACTION_ATTACK_ON );
			break;

		case WCC_FORCE_ROTATION:
			ToggleRotation();
			return true;

		case MC_UPDATE_WHO_IN_CONTAINER:
			selunits.UpdateSelection( reinterpret_cast<IMOContainer*>(msg.nParam) );
			selbuildings.UpdateSelection( reinterpret_cast<IMOContainer*>(msg.nParam) );
			break;

		case WCC_SHOW_AI_INFO:
			ToggleAIInfo();
			break;

		case WCC_SHOW_HAZE:
			pScene->ToggleShow( SCENE_SHOW_HAZE );
			break;

		case WCC_SHOW_NOISE:
			pScene->ToggleShow( SCENE_SHOW_NOISE );
			break;

		case WCC_SHOW_HP_INFO:
			bShowHPs = !bShowHPs;
			ShowIcons( -1, bShowHPs );
			break;

		case WCC_SHOW_FIRE_RANGES_ON:
		case WCC_SHOW_FIRE_RANGES_OFF:
			if ( !IsSelectionEmpty() )
				pTransceiver->CommandShowAreas( selunits.GetAIGroup(), ACTION_NOTIFY_SHOOT_AREA, msg.nEventID == WCC_SHOW_FIRE_RANGES_ON );
			break;

		case WCC_SHOW_ZERO_AREAS_ON:
		case WCC_SHOW_ZERO_AREAS_OFF:
			if ( !IsSelectionEmpty() )
				pTransceiver->CommandShowAreas( selunits.GetAIGroup(), ACTION_NOTIFY_RANGE_AREA, msg.nEventID == WCC_SHOW_ZERO_AREAS_ON );
			break;

		case WCC_SHOW_STORAGE_RANGES_ON:
		case WCC_SHOW_STORAGE_RANGES_OFF:
				//pTransceiver->CommandShowAreas( selunits.GetAIGroup(), ACTION_NOTIFY_RU_STORAGE_AREA, msg.nEventID == WCC_SHOW_STORAGE_RANGES_ON );
			break;
			
		case USER_ACTION_ENABLE_PLANE_SCOUT:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_SPY, msg.nParam, true );
			break;

		case USER_ACTION_DISABLE_PLANE_SCOUT:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_SPY, msg.nParam, false );
			break;

		case USER_ACTION_ENABLE_PLANE_BOMBER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_BOMBERS, msg.nParam, true );
			break;

		case USER_ACTION_DISABLE_PLANE_BOMBER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_BOMBERS, msg.nParam, false );
			break;

		case USER_ACTION_ENABLE_PLANE_FIGHTER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_FIGHTERS, msg.nParam, true );
			break;

		case USER_ACTION_DISABLE_PLANE_FIGHTER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_FIGHTERS, msg.nParam, false );
			break;

		case USER_ACTION_ENABLE_PLANE_PARADROPER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_PARADROPERS, msg.nParam, true );
			break;

		case USER_ACTION_DISABLE_PLANE_PARADROPER:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_PARADROPERS, msg.nParam, false );
			break;

		case USER_ACTION_ENABLE_PLANE_GUNPLANE:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_GUNPLANES, msg.nParam, true );
			break;

		case USER_ACTION_DISABLE_PLANE_GUNPLANE:
			SetAvailablePlanes( USER_ACTION_OFFICER_CALL_GUNPLANES, msg.nParam, false );
			break;

		case WCC_UI_SQUAD_SEL:
			{
				CMapObjectsPtrList lst;
				lst.push_back( reinterpret_cast<IMOUnit*>(msg.nParam) );
				Select( lst, false );
			}
			break;

		case WCC_UI_SQUAD_DESEL:
			ResetSelection( reinterpret_cast<IMOUnit*>(msg.nParam) );
			break;

		case WCC_OBJECTIVES_CLOSED:
			if ( !objectives.empty() && (nCurrObjective + 1 < objectives.size()) ) 
			{
				++nCurrObjective;
				CObjectivesList::iterator pos = objectives.begin();
				std::advance( pos, nCurrObjective );
				// call helpscreen with this objective
				if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() ) 
				{
					if ( IUIContainer *pDialog = checked_cast<IUIContainer*>(pUIScreen->GetChildByID(6000)) )
					{
						DisplayObjective( pDialog, pos->first, pos->second );
						pInput->AddMessage( SGameMessage(MC_SHOW_SINGLE_OBJECTIVE, 0) );
						SetGlobalVar( "Mission.Current.ObjectiveShown", 1 );
					}
				}
				bCanShowNextObjective = false;
			}
			else
			{
				nCurrObjective = 1000000;
				pInput->AddMessage( SGameMessage(WCC_HIDE_OBJECTVE_WINDOW, 0) );
				/*
				if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() )
				{
					if ( IUIContainer *pDialog = checked_cast<IUIContainer*>(pUIScreen->GetChildByID(6000)) )
					{
//						DisplayObjective( pDialog, 0, 0 );
						if ( pDialog->IsVisible() )
						{
							// close objective
							pInput->AddMessage( SGameMessage(MC_SHOW_ESCAPE_MENU, 0) );
						}
					}
				}
				*/
				RemoveGlobalVar( "Mission.Current.ObjectiveShown" );
				bCanShowNextObjective = true;
			}
			break;

		case WCC_SHOW_LAST_OBJECTIVE:
			if ( !objectives.empty() ) 
			{
				nCurrObjective = -1;
				pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
			}
			else
			{
				pInput->AddMessage( SGameMessage(WCC_HIDE_OBJECTVE_WINDOW, 0) );
				bCanShowNextObjective = true;
			}
			break;

		case USER_ACTION_CHOOSE_PLANE:
			if ( !bAviationLocked )
			{
				vAviationAppear = pAILogic->LockAvitaionAppearPoint();
				bAviationLocked = true;
				bAADetectedFlag = false;
			}
			AddMessage( msg );
			AddAviationMarker( vAviationAppear );
			return false;

		// load finished - setup everytnig in accordance with settings
		case CMD_LOAD_FINISHED:
			// re-apply all options
			{
				NVar::ReApply( GetSingleton<IOptionSystem>(), GetSingleton<IOptionSystem>()->CreateIterator() );
				UpdateAllUnits();
				GetSingleton<IScene>()->Reposition();
				const CTRect<long> rect = GetSingleton<IGFX>()->GetScreenRect();
				GetSingleton<ICursor>()->SetBounds( rect.x1, rect.y1, rect.x2, rect.y2 );
				AddMessage( msg );
				bShowMultipleMarkers = GetGlobalVar( "Options.NoMultipleMarkers", 0 ) == 0;
			}
			return false;

		default:
			if ( pTimer->GetPauseReason() < PAUSE_TYPE_NO_CONTROL ) 
			{
				if ( msg.nEventID < 200 && CanDoAction(msg.nEventID) )
				{
					if ( USER_ACTION pfnAction = GetAction(msg.nEventID, SActionDesc::INSTANT) )
							(this->*pfnAction)( msg, false );
					else
						SetForcedAction( msg.nEventID );
				}
				else if ( msg.nEventID > 230 && msg.nEventID < (256 + 200) )
					ResetForcedAction( msg.nEventID & 0x000000ff );
				else if ( (msg.nEventID >= WCC_ASSIGN_GROUP_0) && (msg.nEventID <= WCC_ASSIGN_GROUP_9) )
					AssignSelectionGroup( msg.nEventID - WCC_ASSIGN_GROUP_0 );
				else if ( (msg.nEventID >= WCC_SELECT_GROUP_0) && (msg.nEventID <= WCC_SELECT_GROUP_9) )
					ActivateSelectionGroup( msg.nEventID - WCC_SELECT_GROUP_0, bActionModifierAdd );
				else if ( (msg.nEventID >= WCC_CENTER_GROUP_0) && (msg.nEventID <= WCC_CENTER_GROUP_9) )
				{
					ActivateSelectionGroup( msg.nEventID - WCC_CENTER_GROUP_0, bActionModifierAdd );
					CenterSelectionGroup( msg.nEventID - WCC_CENTER_GROUP_0 );
				}
				else if ( (msg.nEventID >= WCC_ASSIGN_TERRAIN_0) && (msg.nEventID <= WCC_ASSIGN_TERRAIN_9) ) 
				{
					const CVec3 vPos = GetSingleton<ICamera>()->GetAnchor();
					if ( STerrainSelectionPoint *pPoint = GetTerrainPoint(msg.nEventID - WCC_ASSIGN_TERRAIN_0) ) 
						pPoint->vPos = vPos;
					else
						terrainPoints.push_back( STerrainSelectionPoint(vPos, msg.nEventID - WCC_ASSIGN_TERRAIN_0) );
				}
				else if ( (msg.nEventID >= WCC_CENTER_TERRAIN_0) && (msg.nEventID <= WCC_CENTER_TERRAIN_9) ) 
				{
					if ( STerrainSelectionPoint *pPoint = GetTerrainPoint(msg.nEventID - WCC_CENTER_TERRAIN_0) ) 
						GetSingleton<ICamera>()->SetAnchor( pPoint->vPos );
				}
				else
					AddMessage( msg );
			}
			else
				AddMessage( msg );
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SetCursorMode( const int nAction, const bool bModifier )
{
	if ( bModifier ) 
		GetSingleton<ICursor>()->SetModifier( nAction );
	else
		GetSingleton<ICursor>()->SetMode( nAction );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::MarkSelectedUnits( const CVec3 &vPos, bool bFadingMark )
{
	const CMapObjectsList &objs = selunits.GetObjects();
	CVec3 vCenter = VNULL3;
	int nNumMarkers = 0;
	std::list<IMOSquad*> squads;
	std::list<CVec3> positions;
	for ( CMapObjectsList::const_iterator it = objs.begin(); it != objs.end(); ++it )
	{
		if ( const IMOUnit *pUnit = dynamic_cast<const IMOUnit*>((SMapObject*)(*it)) )
		{
			if ( IMOSquad *pSquad = const_cast<IMOUnit*>( pUnit )->GetSquad() )
			{
				bool bFound = false;
				for ( std::list<IMOSquad*>::const_iterator it = squads.begin(); it != squads.end(); ++it )
					if ( *it == pSquad ) 
					{
						bFound = true;
						break;
					}
				if ( !bFound )
					squads.push_front( pSquad );
			}
			else if ( pUnit->pVisObj != 0 )
			{
				vCenter += pUnit->pVisObj->GetPosition();
				positions.push_back( pUnit->pVisObj->GetPosition() );
				++nNumMarkers;
			}
		}
	}
	for ( std::list<IMOSquad*>::iterator it = squads.begin(); it != squads.end(); ++it )
	{
		CVec3 vPlacement;
		WORD wPlacement;
		(*it)->GetPlacement( &vPlacement, &wPlacement );
		vCenter += vPlacement;
		positions.push_back( vPlacement );
		++nNumMarkers;
	}
	if ( nNumMarkers != 0 )
	{
		vCenter /= nNumMarkers;
		for ( std::list<CVec3>::iterator it = positions.begin(); it != positions.end(); ++it )
			if ( bFadingMark )
				pScene->SetClickMarker( vPos + ( (*it) - vCenter ) );	
			else
				pScene->SetPosMarker( (*it) - vCenter );	
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::OnMouseMove( const CVec2 &vPos, interface IUIElement *pUIPickElement )
{
	// re-calc available actions of the current selection
	availActions.Clear();
	CalcAvailableActionsSet( &availActions );
	CUserActions allActions;
	CalcAvailableActionsSet( &allActions, 0, IMapObj::ACTIONS_ALL );
	//
	if ( lastActions != availActions )
	{
		pInput->AddMessage( SGameMessage(USER_ACTION_CHANGE_ACTIONS_1, availActions.GetActions(0)) );
		pInput->AddMessage( SGameMessage(USER_ACTION_CHANGE_ACTIONS_2, availActions.GetActions(1)) );

		const DWORD dwDisabledActions0 = allActions.GetActions( 0 ) & ( ~availActions.GetActions( 0 ) );
		const DWORD dwDisabledActions1 = allActions.GetActions( 1 ) & ( ~availActions.GetActions( 1 ) );
		pInput->AddMessage( SGameMessage(USER_ACTION_CHANGE_ACTIONS_3, dwDisabledActions0) );
		pInput->AddMessage( SGameMessage(USER_ACTION_CHANGE_ACTIONS_4, dwDisabledActions1) );
		//
		lastActions = availActions;
	}
	// if we are over interface, just reset cursor and status bar
	if ( pUIPickElement != 0 ) 
	{
		GetSingleton<ICursor>()->SetMode( 0 );
		SetStatusBar( 0 );
		CTRect<float> rcOut; // ��� ������ �������, ����� �� ���������
		if ( bSetPlayerTooltip )
			pScene->SetToolTip( 0, VNULL2, rcOut, 0x00000000 );
		bSetPlayerTooltip = false;
		return;
	}
	// upadte pick under cursor
	UpdatePick( vPos, pTimer->GetGameTime(), true, true, false );
	//
	const SMapObject *pFirstPickMO = GetFirstPick( SGVOGT_FENCE, true, false );
	const int nAutoAction = DetermineBestAutoAction( pFirstPickMO ) & 0x00007fff;
	CheckActions();
	SetAutoAction( nAutoAction );
	// call it each frame because it's parameters can be changed suddenly
	SetStatusBar( GetFirstPick(SGVOGT_FENCE, true, true) );
	if ( nMultiplayer != 1 && nMultiplayer != 0 )
	{
		nMultiplayer = GetGlobalVar("MultiplayerGame", 0);
	}
	if ( nMultiplayer == 1 )
	{
		bSetPlayerTooltip = true;
		CTRect<float> rcOut;
		rcOut.x1 = 0.0f;
		rcOut.y1 = 0.0f;
		rcOut.x2 = 100.0f;
		rcOut.y2 = 100.0f;
		CPtr<IText> pText = 0;
		DWORD dwPlayerColor = 0xff000000;
		if ( const IMOUnit* pMOUnit = dynamic_cast<const IMOUnit*>(GetFirstPick( SGVOGT_BUILDING, true, false )) )
		{
			if ( IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetPlayer(pMOUnit->GetPlayerIndex()) )
			{
				dwPlayerColor = pPlayer->GetColor();
				pText = pPlayer->GetNameObject();
			}
		}
		pScene->SetToolTip( pText, vPos - CVec2( 0, -28 ), rcOut, dwPlayerColor );
	}
	//hiding objects
	if ( !hiddenObjects.empty() )
	{
		for ( std::list< CPtr<IVisObj> >::iterator it = hiddenObjects.begin(); it != hiddenObjects.end(); ++it )
			(*it)->SetOpacity( 0xff );
		hiddenObjects.clear();
	}
	{
		CTRect<float> rect;
		const float fRectSide = 150.0f;
		rect.x1 = vPos.x - fRectSide;
		rect.y1 = vPos.y - fRectSide;
		rect.x2 = vPos.x + fRectSide;
		rect.y2 = vPos.y + fRectSide;
		std::pair<IVisObj*, CVec2> *pObjects = 0;
		int nNumObjects = 0;
		pScene->Pick( rect, &pObjects, &nNumObjects, SGVOGT_UNIT );
		if ( nNumObjects > 0 )
		{
			CVec3 vScreen;
			CVec2 vScreen2;
			CVec3 vScreen3;
			std::pair<IVisObj*, CVec2> *pObjectsToHide;
			int nNumRes;
			for ( int i = 0; i < nNumObjects; ++i )
			{
				pScene->GetScreenCoords( pObjects[i].first->GetPosition(), &vScreen );
				vScreen2.Set( vScreen.x, vScreen.y );
				pObjectsToHide = 0;
				nNumRes = 0;
				pScene->Pick( vScreen2, &pObjectsToHide, &nNumRes, SGVOGT_OBJECT );
				for ( int i = 0; i < nNumRes; ++i )
				{
					pScene->GetScreenCoords( pObjectsToHide[i].first->GetPosition(), &vScreen3 );
					if ( vScreen3.z < vScreen.z )
					{
						if ( ISpriteVisObj *pObj = dynamic_cast<ISpriteVisObj*>(pObjectsToHide[i].first) )
						{
							if ( const IGFXTexture *pTexture = pObj->GetTexture() )
							{
								if ( (pTexture->GetSizeX(0)) * (pTexture->GetSizeY(0)) >= 1024 )
									hiddenObjects.push_back( pObjectsToHide[i].first );								
							}
						}
						else 
							hiddenObjects.push_back( pObjectsToHide[i].first );
					}
				}
				pObjectsToHide = 0;
				nNumRes = 0;
				pScene->Pick( vScreen2, &pObjectsToHide, &nNumRes, SGVOGT_BUILDING );
				for ( int i = 0; i < nNumRes; ++i )
				{
					pScene->GetScreenCoords( pObjectsToHide[i].first->GetPosition(), &vScreen3 );
					if ( vScreen3.z < vScreen.z )
						hiddenObjects.push_back( pObjectsToHide[i].first );
				}
			}
			for ( std::list< CPtr<IVisObj> >::iterator it = hiddenObjects.begin(); it != hiddenObjects.end(); ++it )
				(*it)->SetOpacity( 0x80 );			
		}
	}
	//drawing arrow and markers
	if ( bGotMoveCommand || bGotGridCommand )
	{
		CVec3 vPos3;
		GetPos3( &vPos3, vPos );
		pScene->ResetPosMarkers();
		pScene->SetDirectionalArrow( VNULL3, VNULL3, false );
		if ( fabs2( vPos3 - vRotationalMovePos ) > fMinRotShiftSq && !selunits.IsEmpty() )
		{
			if ( !bCombineRotation )
			{
				bCombineRotation = true;
				if ( !bGotMoveCommand )
					fRotationStartAngle = FP_PI / 2.0f;
				else
					fRotationStartAngle = 0;
				pScene->SetRotationStartAngle( fRotationStartAngle );
			}
			if ( bGotGridCommand )
				pScene->SetDirectionalArrow( vRotationalMovePos, vPos3, true );
			else
				pScene->SetDirectionalArrow( vRotationalMovePos, vPos3, !bShowMultipleMarkers );
			if ( bShowMultipleMarkers )
			{
				if ( bGotMoveCommand )
				{
					MarkSelectedUnits( VNULL3, false );
				}
				else
				{
					CVec2 vGridCenter;
					Vis2AI( &vGridCenter, vRotationalMovePos.x, vRotationalMovePos.y );
					CVec2 *pCoord;
					int nNumPoints;
					pAILogic->GetGridUnitsCoordinates( selunits.GetAIGroup(), vGridCenter, &pCoord, &nNumPoints );
					for ( int i = 0; i < nNumPoints; ++i )
					{
						CVec3 vPointPosition;
						AI2Vis( &vPointPosition, pCoord[i].x, pCoord[i].y, 0 );
						pScene->SetPosMarker( vPointPosition );
					}
				}
			}
			else
				pScene->SetPosMarker( VNULL3 );
		}
		else
			bCombineRotation = false;
	}
	// update line for fence/entrenchment drawing...
	if ( pBoldLine && !fencePoints.empty() )
	{
		if ( fencePoints.back().cmdType == ACTION_COMMAND_BUILD_FENCE_BEGIN )
		{
			CVec3 vPos3;
			GetPos3( &vPos3, vPos );
			Vis2AI( &vPos3 );
			//
			const SAIUnitCmd &prevCmd = fencePoints.back();

			CVec3 vStartPos, vEndPos;
			const float fHalfTile = SAIConsts::TILE_SIZE / 2.0f;
			int ex = int( vPos3.x / SAIConsts::TILE_SIZE );
			int ey = int( vPos3.y / SAIConsts::TILE_SIZE );
			if ( abs(ex - prevCmd.vPos.x) > abs(ey - prevCmd.vPos.y) )
			{
				// horizontal line...
				const int x = prevCmd.vPos.x;
				const int sx = Min( x, ex );
				ex = Max( x, ex ) + 1;
				//
				vStartPos.Set( sx*SAIConsts::TILE_SIZE, prevCmd.vPos.y*SAIConsts::TILE_SIZE + fHalfTile, 0 );
				vEndPos.Set( ex*SAIConsts::TILE_SIZE, prevCmd.vPos.y*SAIConsts::TILE_SIZE + fHalfTile, 0 );
			}
			else
			{
				// vertical line
				const int y = prevCmd.vPos.y;
				const int sy = Min( y, ey );
				ey = Max( y, ey ) + 1;
				//
				vStartPos.Set( prevCmd.vPos.x*SAIConsts::TILE_SIZE + fHalfTile, sy*SAIConsts::TILE_SIZE, 0 );
				vEndPos.Set( prevCmd.vPos.x*SAIConsts::TILE_SIZE + fHalfTile, ey*SAIConsts::TILE_SIZE, 0 );
			}
			//
			AI2Vis( &vStartPos );
			AI2Vis( &vEndPos );

			pBoldLine->Setup( vStartPos, vEndPos, fWorldCellSize / 2, 0xffff0000 );
		}
		else if ( fencePoints.back().cmdType == ACTION_COMMAND_ENTRENCH_BEGIN )
		{
			CVec3 vPos3;
			GetPos3( &vPos3, vPos );
			//
			const SAIUnitCmd &prevCmd = fencePoints.back();
			CVec3 vStartPos( prevCmd.vPos.x, prevCmd.vPos.y, 0 );
			//
			AI2Vis( &vStartPos );

			pBoldLine->Setup( vStartPos, vPos3, fWorldCellSize / 2, 0xffff0000 );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::SetStatusBar( const SMapObject *pMO )
{
	if ( pMO )
	{
		pMO->GetStatus( &statusObject );
		pInput->AddMessage( SGameMessage(MC_STATUS_OBJECT, 1) );
	}
	else if ( !selunits.IsEmpty() )
	{
		selunits.front()->GetStatus( &statusObject );
		pInput->AddMessage( SGameMessage(MC_STATUS_OBJECT, 1) );
	}
	else
		pInput->AddMessage( SGameMessage(MC_STATUS_OBJECT, 0) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::UpdateWhoInContainerInterface()
{
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** sounds
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorldClient::PlaySuccessSound()
{
	pScene->AddSound( "sounds\\buttons\\ok", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
}
void CWorldClient::PlayFailedSound()
{
	//pScene->AddSound( "sounds\\buttons\\ok", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** objectives
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char *pszObjectiveStatus[] = 
{
	"Received",
	"Completed",
	"Failed"
};
static const char *pszObjectiveKeys[] = 
{
	"objective_received",
	"objective_completed",
	"objective_failed"
};
static const char *pszObjectiveSounds[] = 
{
	"sounds\\reports\\objectives\\received",
	"sounds\\reports\\objectives\\completed",
	"sounds\\reports\\objectives\\failed",
};
void CWorldClient::ReportObjectiveStateChanged( int nObjective, int nState )
{

	ITextManager *pTexMan = GetSingleton<ITextManager>();
	NI_ASSERT_T( (nState >= 0) && (nState < 3), NStr::Format("Unknown objective state %d", nState) );
	const char *pszTextName = pszObjectiveKeys[nState];
	const char *pszSound = pszObjectiveSounds[nState];
	const DWORD dwTextColor = GetGlobalVar( (std::string("Scene.Colors.") + GetSeasonName() + ".Text.Objectives." + pszObjectiveStatus[nState] + ".Color").c_str(), int(0xffd8bd3e) );
	// get objective state description (received, completed, failed)
	CPtr<IText> pStateText = pTexMan->GetString( pszTextName );
	// get objective name
	CPtr<IText> pHeaderText;
	CPtr<IText> pDescriptionText;


	bool bSetGlobalVar = true;
	if ( GetGlobalVar( "MultiplayerGame", 0 ) && nObjective == 0xff ) // specific objective for multiplayer game 
	{
		bSetGlobalVar = false;							// don't set global var about this objective
		CMapInfo::GAME_TYPE eGameType = CMapInfo::GAME_TYPE( GetGlobalVar( "Multiplayer.GameType", CMapInfo::TYPE_SINGLE_PLAYER ) );
		if ( eGameType == CMapInfo::TYPE_FLAGCONTROL )
		{
			pHeaderText = pTexMan->GetDialog( "Textes\\MultiplayerObjectives\\Flagcontrol\\caption" );
			pDescriptionText = pTexMan->GetDialog( "Textes\\MultiplayerObjectives\\Flagcontrol\\objective" );
		}
		else if ( eGameType == CMapInfo::TYPE_SABOTAGE )
		{
			pHeaderText = pTexMan->GetDialog( "Textes\\MultiplayerObjectives\\Sabotage\\caption" );
			pDescriptionText = pTexMan->GetDialog( "Textes\\MultiplayerObjectives\\Sabotage\\objective" );
		}
	}
	else
	{
		const std::string szMissionName = GetGlobalVar( "Mission.Current.Name" );
		if ( !szMissionName.empty() )
		{
			const SMissionStats *pMission = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
			if ( pMission )
			{
				const std::string szVarName = NStr::Format( "temp.%s.objective%d", szMissionName.c_str(), nObjective );
				SetGlobalVar( szVarName.c_str(), nState );
				
				NI_ASSERT_T( nObjective < pMission->objectives.size(), NStr::Format("Objective index (%d) larger then available objectives (%d) in mission \"%s\"", nObjective, pMission->objectives.size(), szMissionName.c_str()) );

				// update scenario tracker
				// given objectives are count in FinishMission
				if ( nState == 1 ) 
					GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetMissionStats()->AddValue( STMT_OBJECTIVES_COMPLETED, 1 );

				pHeaderText = pTexMan->GetDialog( pMission->objectives[nObjective].szHeader.c_str() );
				pDescriptionText = pTexMan->GetDialog( pMission->objectives[nObjective].szDescriptionText.c_str() );
				//������ �����
				if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() )
				{
					if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) ) )
					{
						const SMissionStats::SObjective &rObjective = pMission->objectives[nObjective];
						if ( rObjective.vPosOnMap != VNULL2 )
						{
							if ( IGameTimer* pGameTimer = GetSingleton<IGameTimer>() )
							{
								NTimer::STime currentAbsTime = pGameTimer->GetAbsTime();
								
								CVec2 objectivePosition( rObjective.vPosOnMap.x / 512.0f, ( 512 - rObjective.vPosOnMap.y - 1 ) / 512.0f );
								pUIMiniMap->RemoveMarker( nObjective );
								if ( nState == 0 )
								{
									pUIMiniMap->AddMarker( "ObjectiveReceived", objectivePosition, true, nObjective, currentAbsTime, 0, true );
									pUIMiniMap->AddCircle( objectivePosition, 32.0f, MMC_STYLE_LPOLYGON_CONVERGENT, 0xf00f, currentAbsTime, 5000, true, MAKELPARAM( 3, 2 ) );
								}
								else if ( nState == 1 )
								{
									pUIMiniMap->AddMarker( "ObjectiveReceived", objectivePosition, true, nObjective, currentAbsTime, 5000, true );
									pUIMiniMap->AddCircle( objectivePosition, 32.0f, MMC_STYLE_LPOLYGON_CONVERGENT, 0xf0f0, currentAbsTime, 5000, true, MAKELPARAM( 3, 2 ) );
								}
								else if ( nState == 2 )
								{
									pUIMiniMap->AddMarker( "ObjectiveFailed", objectivePosition, true, nObjective, currentAbsTime, 5000, true );
									pUIMiniMap->AddCircle( objectivePosition, 32.0f, MMC_STYLE_LPOLYGON_CONVERGENT, 0xff00, currentAbsTime, 5000, true, MAKELPARAM( 3, 2 ) );
								}
							}
						}
					}
				}
			}
		}
	}
	

	// form complete string with objective report
	if ( (pStateText != 0) && (pStateText->GetString() != 0) ) 
	{
		std::wstring szObjective;
		if ( (pHeaderText != 0) && (pHeaderText->GetString() != 0) ) 
			szObjective = std::wstring(reinterpret_cast<const wchar_t*>(pStateText->GetString())) + L" " + std::wstring(reinterpret_cast<const wchar_t*>(pHeaderText->GetString()));
		else
			szObjective = std::wstring(reinterpret_cast<const wchar_t*>(pStateText->GetString())) + L" " + NStr::ToUnicode( "Unknown Objective" );
		GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szObjective.c_str(), dwTextColor );
		//
		pScene->AddSound( pszSound, VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
	}
	if ( pDescriptionText != 0 ) 
	{
		if ( nState == 0 )									// objective received
		{
			objectives.push_back( SObjectiveText(pHeaderText, pDescriptionText) );
			if ( bCanShowNextObjective ) 
			{
				nCurrObjective = objectives.size() - 2;
				pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
			}
			if ( bSetGlobalVar )
				SetGlobalVar( "Mission.Current.HasObjectivesToShow", 1 );
			SetGlobalVar( "Mission.Current.HasActiveObjective", 1 );
		}
		else																// objective completed/failed
		{
			int nIndex = 0;
			for ( CObjectivesList::iterator it = objectives.begin(); it != objectives.end(); ++it, ++nIndex )
			{
				if ( (it->first == pHeaderText) || (it->second == pDescriptionText) ) 
				{
					objectives.erase( it );
					if ( (nCurrObjective >= nIndex) && (nCurrObjective < 1000000) ) 
						--nCurrObjective;
					break;
				}
			}
			if ( !objectives.empty() ) 
			{
				nCurrObjective = Min( nCurrObjective, int(objectives.size() - 2) );
				pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
			}
			else
			{
				if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() ) 
				{
					if ( IUIContainer *pDialog = checked_cast<IUIContainer*>(pUIScreen->GetChildByID(6000)) )
					{
						if ( pDialog->IsVisible() )
						{
							// close objective
							pInput->AddMessage( SGameMessage(WCC_HIDE_OBJECTVE_WINDOW, 0) );
//							pInput->AddMessage( SGameMessage(MC_SHOW_ESCAPE_MENU, 0) );
							bCanShowNextObjective = true;
						}
						else
							pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
					}
					else
						pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
				}
				//RemoveGlobalVar( "Mission.Current.HasObjectivesToShow" );
				RemoveGlobalVar( "Mission.Current.HasActiveObjective" );
			}
			/*
			if ( (lastObjective.first == pHeaderText) || (lastObjective.second == pDescriptionText) ) 
			{
				lastObjective.first = 0;
				lastObjective.second = 0;
				//
				if ( !objectives.empty() ) 
					pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
				else
				{
					if ( IUIScreen *pUIScreen = pScene->GetMissionScreen() ) 
					{
						if ( IUIContainer *pDialog = checked_cast<IUIContainer*>(pUIScreen->GetChildByID(6000)) )
						{
							if ( pDialog->IsVisible() )
							{
								// close objective
								pInput->AddMessage( SGameMessage(MC_SHOW_ESCAPE_MENU, 0) );
							}
							else
								pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
						}
						else
							pInput->AddMessage( SGameMessage(WCC_OBJECTIVES_CLOSED, 0) );
					}
				}
			}
			*/
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
