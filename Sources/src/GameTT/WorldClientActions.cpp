#include "StdAfx.h"

#include "WorldClient.h"
#include "..\Common\Actions.h"
#include "..\Formats\fmtTerrain.h"
#include "SelectorVisitors.h"
#include "..\UI\UI.h"
void CWorldClient::RegisterAction( int nAction, DWORD flags, USER_ACTION pfnUserAction )
{
	NI_ASSERT_SLOW_T( (flags != 0) && (pfnUserAction), NStr::Format("Can't register action %d with NULL functions and/or flags", nAction) );
	SActionDesc &action = userActions[nAction];
	action.flags = flags;
	action.pfnAction = pfnUserAction;
}
int CWorldClient::DetermineBestAutoAction( const SMapObject *pMO )
{
	if ( pMO == 0 )
	{
		if ( IsSelectionEmpty() )
			return USER_ACTION_UNKNOWN;
		else
		{
			if ( bActionModifierForcedAttack && availActions.HasAction(USER_ACTION_SUPPRESS) )
				return USER_ACTION_SUPPRESS | 0x80000000;
			else if ( bActionModifierForcedAttack && availActions.HasAction(USER_ACTION_SWARM) )
				return USER_ACTION_SWARM | 0x80000000;
			else if ( bActionModifierForcedMove ) 
				return USER_ACTION_MOVE_TO_GRID | 0x80000000;
			else
				return USER_ACTION_MOVE;
		}
	}
	if ( IsSelectionEmpty() )
	{
		switch ( GetDiplomacyRelation(pMO) )
		{
			case EDI_FRIEND:
				return USER_ACTION_SELECT_FRIEND;
			case EDI_NEUTRAL:
				return USER_ACTION_SELECT_NEUTRAL;
			case EDI_ENEMY:
				return USER_ACTION_SELECT_FOE;
		}
	}
	else
	{
		const std::vector<int> &priority = IsFriend( pMO ) ? userPriorityFriendly : 
		                                                     ( IsFoe(pMO) ? userPriorityEnemy : userPriorityNeutral );
		CUserActions actions, actionsBy, actionsWith;
		if ( selunits.IsSelected(pMO) || selbuildings.IsSelected(pMO) ) 
			CalcAvailableActionsSet( &actionsBy, pMO );
		else
			actionsBy = availActions;
		pMO->GetActions( &actionsWith, IMapObj::ACTIONS_WITH );
		ExcludeActions( &actionsWith, pMO );
		actions = actionsBy;
		actions &= actionsWith;
		availCrossActions = actions;
		if ( bActionModifierForcedMove )
		{
			const EObjGameType eGameType = pMO->GetDesc() != 0 ? pMO->GetDesc()->eGameType : SGVOGT_UNKNOWN;
			if ( eGameType == SGVOGT_UNIT )	// follow by the unit
			{
				if ( actions.HasAction(USER_ACTION_FOLLOW) ) 
					return USER_ACTION_FOLLOW | 0x80000000;
			}
			else if ( (eGameType == SGVOGT_BUILDING) || (eGameType == SGVOGT_ENTRENCHMENT) )	// board building/entrenchment
			{
				if ( actions.HasAction(USER_ACTION_BOARD) ) 
					return USER_ACTION_BOARD | 0x80000000;
				else
					return USER_ACTION_MOVE | 0x80000000;
			}
			else														// move to grid...
				return USER_ACTION_MOVE_TO_GRID | 0x80000000;
		}
		else if ( bActionModifierForcedAttack )
		{
			if ( actionsBy.HasAction(USER_ACTION_SUPPRESS) )
				return USER_ACTION_SUPPRESS | 0x80000000;
			else
				return USER_ACTION_ATTACK | 0x80000000;
		}
		else if ( ((nForcedAction == USER_ACTION_UNKNOWN) || (nForcedAction == -1)) && selunits.IsSelected(pMO) ) 
		{
			CUserActions actions;
			pMO->GetActions( &actions, IMapObj::ACTIONS_BY );
			actions &= selfActions;
			if ( !actions.IsEmpty() ) 
			{
				for ( std::vector<int>::const_iterator action = userPrioritySelf.begin(); action != userPrioritySelf.end(); ++action )
				{
					if ( actions.HasAction(*action) ) 
						return ( *action ) | 0x00008000;
				}
			}
			/*
			CUserActions actions = availActions;
			actions &= selfActions;
			if ( !actions.IsEmpty() ) 
				return USER_ACTION_DO_SELFACTION;
				*/
		}
		for ( std::vector<int>::const_iterator it = priority.begin(); it != priority.end(); ++it )
		{
			if ( actions.HasAction(*it) ) 
				return *it;
		}
		return USER_ACTION_MOVE;
	}
	return USER_ACTION_UNKNOWN;
}
void CWorldClient::BeginAction( const SGameMessage &msg )
{
	const CVec2 vPos = GetPosFromMsg( msg );
	UpdatePick( vPos, pTimer->GetGameTime(), true, true, false );
	const SMapObject *pMO = GetFirstPick( SGVOGT_FENCE, true, false );
	const int nAction = DetermineBestAutoAction( pMO ) & 0x00007fff;
	if ( ( nAction == USER_ACTION_MOVE || nAction == USER_ACTION_SWARM ) && nForcedAction == USER_ACTION_UNKNOWN )
	{
		bGotMoveCommand = true;
		GetPos3( &vRotationalMovePos, vPos );
	}
	else if ( nAction == USER_ACTION_MOVE_TO_GRID && nForcedAction == USER_ACTION_UNKNOWN )
	{
		bGotGridCommand = true;
		GetPos3( &vRotationalMovePos, vPos );
	}
}
void CWorldClient::DoAction( const SGameMessage &msg )
{
	bGotMoveCommand = false;
	bGotGridCommand = false;
	const CVec2 vPos = GetPosFromMsg( msg );
	if ( nForcedAction != USER_ACTION_UNKNOWN )
	{
		CVec3 vCommandPoint;
		GetPos3( &vCommandPoint, vPos );
		if ( DoAction(nForcedAction, SActionDesc::FORCED , msg) == true )
		{
			if ( nForcedAction == USER_ACTION_MOVE || nForcedAction == USER_ACTION_SWARM || nForcedAction == USER_ACTION_MOVE_TO_GRID )
			{
				if ( bShowMultipleMarkers )
				{
					if ( nForcedAction != USER_ACTION_MOVE_TO_GRID )
					{
						MarkSelectedUnits( vCommandPoint );
					}
					else
					{
						CVec2 vGridCenter;
						Vis2AI( &vGridCenter, vCommandPoint.x, vCommandPoint.y );
						CVec2 *pCoord;
						int nNumPoints;
						pAILogic->GetGridUnitsCoordinates( selunits.GetAIGroup(), vGridCenter, &pCoord, &nNumPoints );
						for ( int i = 0; i < nNumPoints; ++i )
						{
							CVec3 vPointPosition;
							AI2Vis( &vPointPosition, pCoord[i].x, pCoord[i].y, 0 );
							pScene->SetClickMarker( vPointPosition );
						}
					}
				}
				else
					pScene->SetClickMarker( vCommandPoint );	
			}
			ResetForcedAction();
			PlaySuccessSound();
		}
		else
			PlayFailedSound();
		bCombineRotation = false;
		if ( !bActionModifierAdd )
		{
			pScene->FlashPosMarkers();
			pScene->SetDirectionalArrow( VNULL3, VNULL3, false );
		}
		return;
	}
	UpdatePick( vPos, pTimer->GetGameTime(), true, true, false );
	const SMapObject *pMO = GetFirstPick( SGVOGT_FENCE, true, false );
	const int _nAction = DetermineBestAutoAction( pMO );
	const int nAction = _nAction & 0x00007fff;
	const int nType = _nAction & 0x80000000 ? SActionDesc::FORCED : SActionDesc::AUTO;
	const bool bSelfAction = ( _nAction & 0x00008000 ) != 0;
	if ( ( _nAction & 0x00008000 ) != 0 ) 
	{
		if ( DoSelfAction(nAction, bActionModifierAdd) )
			PlaySuccessSound();
		else
			PlayFailedSound();
	}
	else if ( bCombineRotation && ( nAction == USER_ACTION_MOVE || nAction == USER_ACTION_SWARM || nAction == USER_ACTION_MOVE_TO_GRID ) )
	{	
		Vis2AI( &vRotationalMovePos );
		bool result;
		CVec3 vCommandPoint;
		GetPos3( &vCommandPoint, vPos );
		Vis2AI( &vCommandPoint );
		CVec2 vDir;
		vDir.Set( vCommandPoint.x - vRotationalMovePos.x, vCommandPoint.y - vRotationalMovePos.y );
		Normalize( &vDir );
		float fFinalAngle = vDir.x < 0 ? -1.0f * acos( vDir.y ) : acos( vDir.y );
		if ( nAction == USER_ACTION_MOVE )
			result = PerformActionPosParam( ACTION_COMMAND_MOVE_TO, vRotationalMovePos, fRotationStartAngle - fFinalAngle, bActionModifierAdd, false );
		else if ( nAction == USER_ACTION_SWARM )
			result = PerformActionPosParam( ACTION_COMMAND_SWARM_TO, vRotationalMovePos, fRotationStartAngle - fFinalAngle, bActionModifierAdd, false );
		else
			result = PerformActionPosParam( ACTION_COMMAND_MOVE_TO_GRID, vRotationalMovePos, fRotationStartAngle - fFinalAngle, bActionModifierAdd, false );
		if ( nAction != USER_ACTION_MOVE_TO_GRID && !bShowMultipleMarkers )
		{
			const CVec2 vPos = GetPosFromMsg( msg );
			result = result && PerformActionPosParam( ACTION_COMMAND_ROTATE_TO_DIR,  vCommandPoint - vRotationalMovePos, 0, true, false );
		}
		if ( result )
			PlaySuccessSound();
		else
			PlayFailedSound();
	}
	else
	{
		CVec3 vCommandPoint;
		GetPos3( &vCommandPoint, vPos );
		if ( DoAction(nAction, nType , msg) == true )
		{
			PlaySuccessSound();
			if ( nAction == USER_ACTION_MOVE || nAction == USER_ACTION_SWARM || nAction == USER_ACTION_MOVE_TO_GRID )
			{
				if ( bShowMultipleMarkers )
				{
					if ( nAction != USER_ACTION_MOVE_TO_GRID )
					{
						MarkSelectedUnits( vCommandPoint );
					}
					else
					{
						CVec2 vGridCenter;
						Vis2AI( &vGridCenter, vCommandPoint.x, vCommandPoint.y );
						CVec2 *pCoord;
						int nNumPoints;
						pAILogic->GetGridUnitsCoordinates( selunits.GetAIGroup(), vGridCenter, &pCoord, &nNumPoints );
						for ( int i = 0; i < nNumPoints; ++i )
						{
							CVec3 vPointPosition;
							AI2Vis( &vPointPosition, pCoord[i].x, pCoord[i].y, 0 );
							pScene->SetClickMarker( vPointPosition );
						}
					}
				}
				else
					pScene->SetClickMarker( vCommandPoint );	
			}
		}
		else
			PlayFailedSound();
	}
	bCombineRotation = false;
	if ( !bActionModifierAdd )
	{
		pScene->FlashPosMarkers();
		pScene->SetDirectionalArrow( VNULL3, VNULL3, false );
	}
}
bool CWorldClient::PerformSelfActionPosParam( SMapObject *pMO, const int nAction, const int nParam, bool bAddAction )
{
	CVec3 vPos;
	WORD wDir;
	pMO->GetPlacement( &vPos, &wDir );
	Vis2AI( &vPos );
	static SAIUnitCmd cmd;
	cmd.cmdType = EActionCommand( nAction );
	cmd.vPos.x = vPos.x;
	cmd.vPos.y = vPos.y;
	cmd.fNumber = nParam;
	IRefCount *pAIObj = pMO->GetAIObj();
	const int nGroupID = pTransceiver->CommandRegisterGroup( &pAIObj, 1 );
	pTransceiver->CommandGroupCommand( &cmd, nGroupID, bAddAction );
	pTransceiver->CommandUnregisterGroup( nGroupID );
	return true;
}
bool CWorldClient::DoSelfAction( SMapObject *pMO, const int nAction )
{
	switch ( nAction ) 
	{
		case USER_ACTION_LEAVE:
			if ( checked_cast<IMOContainer*>(pMO)->GetPassangers(0) != 0 ) 
				return PerformSelfActionPos( pMO, ACTION_COMMAND_UNLOAD, bActionModifierAdd );
			break;
		case USER_ACTION_STAND_GROUND:
			return PerformSelfActionPos( pMO, ACTION_COMMAND_STAND_GROUND, bActionModifierAdd );
		case USER_ACTION_SUPPORT_RESUPPLY:
			return PerformSelfActionPos( pMO, ACTION_COMMAND_RESUPPLY, bActionModifierAdd );
		case USER_ACTION_ENGINEER_REPAIR:
			return PerformSelfActionPos( pMO, ACTION_COMMAND_REPAIR, bActionModifierAdd );
		case USER_ACTION_DEPLOY_ARTILLERY:
			return PerformSelfActionPosParam( pMO, ACTION_COMMAND_DEPLOY_ARTILLERY, 1, bActionModifierAdd );
		case USER_ACTION_ENTRENCH_SELF:
			return PerformSelfActionPos( pMO, ACTION_COMMAND_ENTRENCH_SELF, bActionModifierAdd );
	}
	NI_ASSERT_SLOW_T( false, NStr::Format("Unknown user action %d for self action", nAction) );
	return false;
}
bool CWorldClient::DoSelfAction( const int nAction, const bool bAddAction )
{
	switch ( nAction ) 
	{
		case USER_ACTION_LEAVE:
			return PerformActionPos( ACTION_COMMAND_UNLOAD | 0x00008000, VNULL2, bAddAction );
		case USER_ACTION_STAND_GROUND:
			return PerformActionPos( ACTION_COMMAND_STAND_GROUND | 0x00008000, VNULL2, bAddAction );
		case USER_ACTION_SUPPORT_RESUPPLY:
			return PerformActionPos( ACTION_COMMAND_RESUPPLY | 0x00008000, VNULL2, bAddAction );
		case USER_ACTION_ENGINEER_REPAIR:
			return PerformActionPos( ACTION_COMMAND_REPAIR | 0x00008000, VNULL2, bAddAction );
		case USER_ACTION_DEPLOY_ARTILLERY:
			return PerformActionPosParam( ACTION_COMMAND_DEPLOY_ARTILLERY | 0x00008000, VNULL2, 1, bAddAction );
		case USER_ACTION_ENTRENCH_SELF:
			return PerformActionPos( ACTION_COMMAND_ENTRENCH_SELF | 0x00008000, VNULL2, bAddAction );
	}
	NI_ASSERT_SLOW_T( false, NStr::Format("Unknown user action %d for self action", nAction) );
	return false;
}
bool CWorldClient::ActionDoSelfActionMsg( const SGameMessage &msg, bool bForced )
{	
	bool bRetVal = false;
	selunits.Invalidate();
	const CMapObjectsList &objects = selunits.GetObjects();
	for ( CMapObjectsList::const_iterator it = objects.begin(); it != objects.end(); ++it )
	{
		SMapObject *pMO = *it;
		CUserActions actions;
		pMO->GetActions( &actions, IMapObj::ACTIONS_BY );
		for ( std::vector<int>::const_iterator action = userPrioritySelf.begin(); action != userPrioritySelf.end(); ++action )
		{
			if ( actions.HasAction(*action) ) 
			{
				bRetVal = DoSelfAction( pMO, *action ) || bRetVal;
				break;
			}
		}
	}
	return bRetVal;
}
void CWorldClient::SetAutoAction( int nAction )
{
	if ( !CanDoAction(nAction) )
		nAction = USER_ACTION_UNKNOWN;
	nAutoAction = nAction;
	if ( nForcedAction == USER_ACTION_UNKNOWN )
	{
		SetCursorMode( nAutoAction );
		SetCursorMode( -1, true );
	}
	else
	{
		SetCursorMode( nForcedAction );
		/*
		if ( !availCrossActions.HasAction(nForcedAction) ) 
			SetCursorMode( USER_ACTION_CANCEL, true );
		else
		*/
			SetCursorMode( -1, true );
	}
	/*
	if ( !IsSelectionEmpty() && bActionModifierForcedMove )
		SetCursorMode( USER_ACTION_MOVE );
	else if ( !IsSelectionEmpty() && bActionModifierForcedAttack )
		SetCursorMode( USER_ACTION_ATTACK );
	else if ( nForcedAction == USER_ACTION_UNKNOWN )
		SetCursorMode( nAutoAction );
	else
		SetCursorMode( nForcedAction );
		*/
}
bool CWorldClient::SetForcedAction( int nAction )
{
	if ( nForcedAction == nAction )
	{
		ResetForcedAction();
		return true;
	}
	if ( !CanDoAction(nAction) )
		return false;
	if ( ( IsSelectionEmpty() && 
		     (nAction != USER_ACTION_OFFICER_CALL_BOMBERS) && 
				 (nAction != USER_ACTION_OFFICER_CALL_FIGHTERS) && 
				 (nAction != USER_ACTION_OFFICER_CALL_PARADROPERS) &&
				 (nAction != USER_ACTION_OFFICER_CALL_GUNPLANES) &&
				 (nAction != USER_ACTION_OFFICER_CALL_SPY) && 
				 (nAction != USER_ACTION_PLACE_MARKER) ) && 
		   !( (nAction == USER_ACTION_LEAVE) && IsBuildingsSelected() ) )
		return false;
	if ( nForcedAction != USER_ACTION_UNKNOWN )
		ResetForcedAction();
	nForcedAction = nAction;
	SetCursorMode( nForcedAction );
	return true;
}
void CWorldClient::ResetForcedAction( int nAction )
{
	if ( (nAction == -1) || (nAction == nForcedAction) )
	{
		if ( nAction == -1 )
			pInput->AddMessage( SGameMessage(nForcedAction | 0x00000100) );
		nForcedAction = USER_ACTION_UNKNOWN;
		GetSingleton<ICursor>()->SetMode( USER_ACTION_UNKNOWN );
		ResetPoints();
	}
}
void CWorldClient::ResetPoints()
{
	fencePoints.clear();
	aviationPoints.clear();
	buildPoints.clear();
	pScene->FlashPosMarkers();
	pScene->RemoveLine( pBoldLine );
	pBoldLine = 0;
}
void CWorldClient::CheckActions()
{
	SetAutoAction( nAutoAction );
	if ( !CanDoAction(nForcedAction) )
		ResetForcedAction();
}
bool CWorldClient::CanDoAction( int nAction ) const
{ 
	return nAction < 64 ? availActions.HasAction( nAction ) : true; 
}
void CWorldClient::CalcAvailableActionsSet( CUserActions *pActions, const IMapObj *pException, const IMapObj::EActionsType eActions ) const
{
	pActions->Clear();
	if ( pException == 0 ) 
	{
		CGetActionsSelectiorVisitor visitor( eActions, pActions );
		selunits.Visit( &visitor );
		selbuildings.Visit( &visitor );
	}
	else
	{
		CGetActionsExceptSelectiorVisitor visitor( eActions, pActions, pException );
		selunits.Visit( &visitor );
		selbuildings.Visit( &visitor );
	}
	*pActions |= availPlanes;
	pActions->SetAction( USER_ACTION_UNKNOWN );
	if ( GetGlobalVar( "MultiplayerGame", 0 ) )
		pActions->SetAction( USER_ACTION_PLACE_MARKER );
	pActions->SetAction( USER_ACTION_CHANGE_MOVEMENT_ORDER );
}
void CWorldClient::ExcludeActions( CUserActions *pActions, const SMapObject *pMO ) const
{
	const std::vector<int> &excludes = IsFriend( pMO ) ? excludeFriendly : ( IsFoe(pMO) ? excludeEnemy : excludeNeutral );
	for ( std::vector<int>::const_iterator it = excludes.begin(); it != excludes.end(); ++it )
		pActions->RemoveAction( *it );
}
void CWorldClient::PrepareActionPosParam( SAIUnitCmd *pCmd, int nAction, const CVec2 &vPos, float fParam )
{
	CVec3 vPos3;
	GetPos3( &vPos3, vPos );
	Vis2AI( &vPos3 );
	pCmd->cmdType = EActionCommand( nAction );
	pCmd->vPos.x = vPos3.x;
	pCmd->vPos.y = vPos3.y;
	pCmd->fNumber = fParam;
}
void CWorldClient::PrepareActionObjParam( SAIUnitCmd *pCmd, int nAction, IRefCount *pAIObj, float fParam )
{
	pCmd->cmdType = EActionCommand( nAction );
	pCmd->pObject = pAIObj;
	pCmd->fNumber = fParam;
	NI_ASSERT_SLOW_T( pAIObj != 0, NStr::Format("Trying to perform obj-action %d with NULL obj", nAction) );
}
bool CWorldClient::PerformActionObjParam( int nAction, IRefCount *pAIObj, float fParam, bool bAddAction )
{
	if ( IsSelectionEmpty() )
		return false;
	static SAIUnitCmd cmd;
	PrepareActionObjParam( &cmd, nAction, pAIObj, fParam );
	pTransceiver->CommandGroupCommand( &cmd, selunits.GetAIGroup(), bAddAction );
	cmd.pObject = 0;
	return true;
}
bool CWorldClient::PerformActionPosParam( int nAction, const CVec2 &vPos2, float fParam, bool bAddAction, bool bMarkOnTerrain )
{
	if ( IsSelectionEmpty() )
		return false;
	static SAIUnitCmd cmd;
	CVec3 vPos3;
	GetPos3( &vPos3, vPos2 );
	if ( vPos2 != VNULL2 && bMarkOnTerrain && nAction != ACTION_COMMAND_MOVE_TO && nAction != ACTION_COMMAND_SWARM_TO )
		pScene->SetClickMarker( vPos3 );
	PrepareActionPosParam( &cmd, nAction, vPos2, fParam );
	pTransceiver->CommandGroupCommand( &cmd, selunits.GetAIGroup(), bAddAction );
	return true;
}
bool CWorldClient::PerformActionPosParam( int nAction, const CVec3 &vPos3, float fParam, bool bAddAction, bool bMarkOnTerrain )
{
	if ( IsSelectionEmpty() )
		return false;
	CVec3 vPos;
	AI2Vis( &vPos, vPos3 );
	if ( bMarkOnTerrain && nAction != ACTION_COMMAND_MOVE_TO && nAction != ACTION_COMMAND_SWARM_TO )
		pScene->SetClickMarker( vPos );
	static SAIUnitCmd cmd;
	cmd.cmdType = EActionCommand( nAction );
	cmd.vPos.x = vPos3.x;
	cmd.vPos.y = vPos3.y;
	cmd.fNumber = fParam;
	pTransceiver->CommandGroupCommand( &cmd, selunits.GetAIGroup(), bAddAction );
	return true;
}
int CWorldClient::PerformUnitActionPosParam( int nAction, const CVec2 &vPos2, float fParam, bool bAddAction )
{
	CVec3 vPos3;
	GetPos3( &vPos3, vPos2 );
	Vis2AI( &vPos3 );
	static SAIUnitCmd cmd;
	cmd.cmdType = EActionCommand( nAction );
	cmd.vPos.x = vPos3.x;
	cmd.vPos.y = vPos3.y;
	cmd.fNumber = fParam;
	return pTransceiver->CommandUnitCommand( &cmd );
}
bool CWorldClient::ActionSendTo( const CVec2 &pos, bool bAddAction )
{
	return PerformActionPos( ACTION_COMMAND_MOVE_TO, pos, bAddAction );
}
bool CWorldClient::ActionAttack( const CVec2 &vPos, DWORD flags, bool bAddAction )
{
	if ( IsSelectionEmpty() )
		return false;
	UpdatePick( vPos, pTimer->GetGameTime(), false, true, true );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		const int nDiplomacy = GetDiplomacyRelation( *it );
		if ( flags & nDiplomacy )
		{
			SMapObject *pMO = *it;
			switch ( pMO->pDesc->eGameType )
			{
				case SGVOGT_UNIT:
					return PerformActionObj( ACTION_COMMAND_ATTACK_UNIT, pMO->pAIObj, bAddAction );
				case SGVOGT_ENTRENCHMENT:
					return PerformActionObj( ACTION_COMMAND_ATTACK_OBJECT, GetAIEntrenchment(pMO), bAddAction );
				case SGVOGT_OBJECT:
				case SGVOGT_BRIDGE:
					if ( SBridgeSpanObject *pSpan = FindSpanByAI( pMO->pAIObj ) )
						return PerformActionObj( ACTION_COMMAND_ATTACK_OBJECT, pSpan->pAIObj, bAddAction );
					else
						return PerformActionObj( ACTION_COMMAND_ATTACK_OBJECT, pMO->pAIObj, bAddAction );
				case SGVOGT_MINE:
				case SGVOGT_BUILDING:
				case SGVOGT_FENCE:
					return PerformActionObj( ACTION_COMMAND_ATTACK_OBJECT, pMO->pAIObj, bAddAction );
			}
			return false;
		}
	}
	return false;
}
bool CWorldClient::ActionLeave( const CVec2 &vPos2, bool bAddAction )
{
	static SAIUnitCmd cmd;
	std::vector<IMOUnit*> passangers;
	if ( IsBuildingsSelected() )
	{
		GetPassangers( selbuildings.GetObjects(), passangers, true );
		if ( passangers.empty() )
			return false;
		const int nNumObjects = passangers.size();
		IRefCount **ppAIObjects = GetTempBuffer<IRefCount*>( nNumObjects );
		IRefCount **ppTempObjects = ppAIObjects;
		for ( std::vector<IMOUnit*>::iterator it = passangers.begin(); it != passangers.end(); ++it )
			*ppTempObjects++ = (*it)->GetAIObj();
		const int nLocalSelectionGroup = pTransceiver->CommandRegisterGroup( ppAIObjects, nNumObjects );
		CVec3 vPos;
		GetPos3( &vPos, vPos2 );
		Vis2AI( &vPos );
		cmd.cmdType = ACTION_COMMAND_LEAVE;
		cmd.vPos.x = vPos.x;
		cmd.vPos.y = vPos.y;
		pTransceiver->CommandGroupCommand( &cmd, nLocalSelectionGroup, bAddAction );
		pTransceiver->CommandUnregisterGroup( nLocalSelectionGroup );
		return true;
	}
	else if ( IsUnitsSelected() )
	{
		CCollectObjectsSelectiorVisitor visitor;
		selunits.Visit( &visitor );
		for ( CMapObjectsList::const_iterator it = visitor.GetObjects().begin(); it != visitor.GetObjects().end(); ++it )
		{
			const IMOContainer *pContainer = static_cast_ptr<const IMOContainer *>( *it );
			if ( pContainer->GetPassangers(0) != 0 ) 
				return PerformActionPos( ACTION_COMMAND_UNLOAD, vPos2, bAddAction );
		}
	}
	return false;
}
bool CWorldClient::ActionBoard( SMapObject *pMO, bool bAddAction )
{
	if ( IsSelectionEmpty() )
		return false;
	bool bRes = false;
	if ( pMO->pDesc->eGameType == SGVOGT_BUILDING )
		bRes = PerformActionObjParam( ACTION_COMMAND_ENTER, pMO->pAIObj, 0, bAddAction );
	else if ( pMO->pDesc->eGameType == SGVOGT_ENTRENCHMENT )
		bRes = PerformActionObjParam( ACTION_COMMAND_ENTER, pMO->pAIObj, 2, bAddAction );
	else if ( (pMO->pDesc->eGameType == SGVOGT_UNIT) && IsFriend(pMO) )
		bRes = PerformActionObjParam( ACTION_COMMAND_LOAD, pMO->pAIObj, 1, bAddAction );
	return bRes;
}
bool CWorldClient::ActionBoard( CMapObjectsPtrList &objects, bool bAddAction )
{
	if ( IsSelectionEmpty() )
		return false;
	for ( CMapObjectsPtrList::iterator it = objects.begin(); it != objects.end(); ++it )
	{
		if ( (*it)->IsAlive() && ActionBoard(*it, bAddAction) ) 
			return true;
	}
	return false;
}
bool CWorldClient::ActionMoveMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_MOVE_TO, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionMoveToGridMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_MOVE_TO_GRID, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionSwarmMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_SWARM_TO, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionRotateMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_ROTATE_TO, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionAttackMsg( const SGameMessage &msg, bool bForced )
{
	const DWORD flags = bForced ? ACTION_FLAG_FOE | ACTION_FLAG_NEUTRAL | ACTION_FLAG_FRIEND : ACTION_FLAG_FOE;
	return ActionAttack( GetPosFromMsg(msg), flags, bActionModifierAdd );
}
bool CWorldClient::ActionFollow( SMapObject *pMO, bool bAdd )
{
	return PerformActionObj( ACTION_COMMAND_FOLLOW, pMO->GetAIObj(), bAdd );
}
bool CWorldClient::ActionFollowMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, true );
	if ( framePick.empty() ) 
		return false;
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->IsAlive() && ((*it)->pDesc->eGameType == SGVOGT_UNIT) )
			return ActionFollow( framePick.front(), bActionModifierAdd );
	}
	return false;
}
bool CWorldClient::ActionBoardMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, true );
	return ActionBoard( framePick, bActionModifierAdd );
}
bool CWorldClient::ActionLeaveMsg( const SGameMessage &msg, bool bForced )
{
	return ActionLeave( GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionInstallMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_INSTALL, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionUnInstallMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_UNINSTALL, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionPlaceMineAPMsg( const SGameMessage &msg, bool bForced )
{
	if ( bActionModifierAdd ) 
	{
		static SAIUnitCmd cmd;
		PrepareActionPosParam( &cmd, ACTION_COMMAND_PLACEMINE, GetPosFromMsg(msg), SMineRPGStats::INFANTRY );
		CVec2 vPos = GetPosFromMsg( msg );
		CVec3 vPos3;
		GetPos3( &vPos3, vPos );
		pScene->SetRotationStartAngle( 0, false );
		pScene->SetPosMarker( vPos3 );
		buildPoints.push_back( cmd );
		return false;
	}
	else
		return PerformActionPosParam( ACTION_COMMAND_PLACEMINE, GetPosFromMsg(msg), SMineRPGStats::INFANTRY, bActionModifierAdd );
}
bool CWorldClient::ActionPlaceMineATMsg( const SGameMessage &msg, bool bForced )
{
	if ( bActionModifierAdd ) 
	{
		static SAIUnitCmd cmd;
		PrepareActionPosParam( &cmd, ACTION_COMMAND_PLACEMINE, GetPosFromMsg(msg), SMineRPGStats::TECHNICS );
		CVec2 vPos = GetPosFromMsg( msg );
		CVec3 vPos3;
		GetPos3( &vPos3, vPos );
		pScene->SetRotationStartAngle( 0, false );
		pScene->SetPosMarker( vPos3 );
		buildPoints.push_back( cmd );
		return false;
	}
	else
		return PerformActionPosParam( ACTION_COMMAND_PLACEMINE, GetPosFromMsg(msg), SMineRPGStats::TECHNICS, bActionModifierAdd );
}
bool CWorldClient::ActionClearMineMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_CLEARMINE, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionGuardMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_GUARD, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionAmbush( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_AMBUSH, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionRangingMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_RANGE_AREA, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionSuppressMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_ART_BOMBARDMENT, GetPosFromMsg(msg), bActionModifierAdd );
}
void CWorldClient::AddAviationMarker( const CVec2 &vPos )
{
	CVec3 vCenter;
	AI2Vis( &vCenter, vPos.x, vPos.y, 0 );
	const float fRadius = 500 * fAITileXCoeff;

	CCircle circle( CVec2(vCenter.x, vCenter.y), fRadius );
	aviationCircles.push_back( circle );
}
void CWorldClient::ClearAviationMarkers()
{
	aviationCircles.clear();
}
void CWorldClient::GetAviationCircles( interface IUIMiniMap * pMinimap, const NTimer::STime curTime )
{
	while( !aviationCircles.empty() )
	{
		CCircle & circle = *aviationCircles.begin();
		pMinimap->AddCircle( circle.center, circle.r, MMC_STYLE_MIXED, 0xF0FF, curTime, 5000, false, 0 );
		aviationCircles.pop_front();
	}
}
bool CWorldClient::ActionAviationAddPoint( const EActionCommand cmdType, const CVec2 &vPos2 )
{
	CVec3 vPos;
	GetPos3( &vPos, vPos2 );
	Vis2AI( &vPos );

	if ( bActionModifierAdd ) 
	{
		if ( aviationPoints.empty() ) 
		{
			if ( !bAviationLocked )
			{
				vAviationAppear = pAILogic->LockAvitaionAppearPoint();
				bAviationLocked = true;
			}
			AddAviationMarker( vAviationAppear );
			
			AddAviationMarker( CVec2(vPos.x, vPos.y) );
			aviationPoints.push_back( SAIUnitCmd(cmdType, CVec2(vPos.x, vPos.y)) );
		}
		else
		{
			AddAviationMarker( CVec2( vPos.x, vPos.y ) );
			aviationPoints.push_back( SAIUnitCmd(ACTION_COMMAND_PLANE_ADD_POINT, CVec2(vPos.x, vPos.y)) );
		}
		PlaySuccessSound();
		return false;
	}
	else
	{
		aviationPoints.push_back( SAIUnitCmd(cmdType, CVec2(vPos.x, vPos.y)) );
		const bool bRes = ActionAviationCall();
		ClearAviationMarkers();
		return bRes;
	}
}
bool CWorldClient::ActionCallForBombersMsg( const SGameMessage &msg, bool bForced )
{
	return ActionAviationAddPoint( ACTION_COMMAND_CALL_BOMBERS, GetPosFromMsg(msg) );
}
bool CWorldClient::ActionCallForFightersMsg( const SGameMessage &msg, bool bForced )
{
	return ActionAviationAddPoint( ACTION_COMMAND_CALL_FIGHTERS, GetPosFromMsg(msg) );
}
bool CWorldClient::ActionCallForSpyMsg( const SGameMessage &msg, bool bForced )
{	
	return ActionAviationAddPoint( ACTION_COMMAND_CALL_SCOUT, GetPosFromMsg(msg) );
}
bool CWorldClient::ActionCallForParadropersMsg( const SGameMessage &msg, bool bForced )
{
	return ActionAviationAddPoint( ACTION_COMMAND_PARADROP, GetPosFromMsg(msg) );
}
bool CWorldClient::ActionCallForGunplanesMsg( const SGameMessage &msg, bool bForced )
{
	return ActionAviationAddPoint( ACTION_COMMAND_CALL_SHTURMOVIKS, GetPosFromMsg(msg) );
}
bool CWorldClient::ActionPlaceMarkerMsg( const SGameMessage &msg, bool bForced )
{
	PerformUnitActionPosParam( ACTION_COMMAND_PLACE_MARKER, GetPosFromMsg(msg), 0, false );
	return true;
}
bool CWorldClient::ActionAviationCall()
{
	NI_ASSERT_T( !aviationPoints.empty(), "Wrong call to aviation with 0 points" );
	aviationPoints.push_back( SAIUnitCmd(ACTION_COMMAND_PLANE_TAKEOFF_NOW) );
	const int nAIGroup = pTransceiver->CommandUnitCommand( &(aviationPoints.front()) );
	aviationPoints.pop_front();
	if ( nAIGroup != -1 )
	{
		while ( !aviationPoints.empty() ) 
		{
			pTransceiver->CommandGroupCommand( &(aviationPoints.front()), nAIGroup, true );
			aviationPoints.pop_front();
		}
		pTransceiver->CommandUnregisterGroup( nAIGroup );
	}
	aviationPoints.clear();
	pAILogic->UnlockAviationAppearPoint();
	bAviationLocked = false;

	return true;
}
bool CWorldClient::ActionCallHQMsg( const SGameMessage &msg, bool bForced )
{
	pAILogic->CallScriptFunction( "CallHQ()" );
	return true;
}
bool CWorldClient::ActionStopMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_STOP, VNULL2, false );
}
bool CWorldClient::ActionBuildFenceMsg( const SGameMessage &msg, bool bForced )
{
	static SAIUnitCmd cmd;
	CVec3 vPos;
	GetPos3( &vPos, GetPosFromMsg(msg) );
	Vis2AI( &vPos );
	if ( fencePoints.empty() )
	{
		cmd.cmdType = ACTION_COMMAND_BUILD_FENCE_BEGIN;
		cmd.vPos.x = vPos.x / SAIConsts::TILE_SIZE;
		cmd.vPos.y = vPos.y / SAIConsts::TILE_SIZE;
		fencePoints.push_back( cmd );
		pBoldLine = CreateObject<IBoldLineVisObj>( SCENE_BOLD_LINE );
		pScene->AddLine( pBoldLine );
		return false;
	}
	else
	{
		const SAIUnitCmd &prevCmd = fencePoints.back();
		NI_ASSERT_TF( prevCmd.cmdType == ACTION_COMMAND_BUILD_FENCE_BEGIN, "Can't perform wire fence building - previous command is not a fence begin!", fencePoints.clear(); return true; );
		cmd.cmdType = ACTION_COMMAND_BUILD_FENCE_END;
		cmd.vPos.x = vPos.x / SAIConsts::TILE_SIZE;
		cmd.vPos.y = vPos.y / SAIConsts::TILE_SIZE;
		if ( abs(cmd.vPos.x - prevCmd.vPos.x) > abs(cmd.vPos.y - prevCmd.vPos.y) )
			cmd.vPos.y = prevCmd.vPos.y;
		else
			cmd.vPos.x = prevCmd.vPos.x;
		pTransceiver->CommandGroupCommand( &prevCmd, selunits.GetAIGroup(), bActionModifierAdd );
		pTransceiver->CommandGroupCommand( &cmd, selunits.GetAIGroup(), true );
		ResetPoints();
		return true;
	}
}
bool CWorldClient::ActionBuildEntrenchmentMsg( const SGameMessage &msg, bool bForced )
{
	static SAIUnitCmd cmd;
	CVec3 vPos;
	GetPos3( &vPos, GetPosFromMsg(msg) );
	Vis2AI( &vPos );
	if ( fencePoints.empty() )
	{
		cmd.cmdType = ACTION_COMMAND_ENTRENCH_BEGIN;
		cmd.vPos.x = vPos.x;
		cmd.vPos.y = vPos.y;
		fencePoints.push_back( cmd );
		pBoldLine = CreateObject<IBoldLineVisObj>( SCENE_BOLD_LINE );
		pScene->AddLine( pBoldLine );
		return false;
	}
	else
	{
		const SAIUnitCmd &prevCmd = fencePoints.back();
		NI_ASSERT_TF( prevCmd.cmdType == ACTION_COMMAND_ENTRENCH_BEGIN, "Can't perform entrenchment building - previous command is not an entrenchment begin!", fencePoints.clear(); return true; );
		cmd.cmdType = ACTION_COMMAND_ENTRENCH_END;
		cmd.vPos.x = vPos.x;
		cmd.vPos.y = vPos.y;
		pTransceiver->CommandGroupCommand( &prevCmd, selunits.GetAIGroup(), bActionModifierAdd );
		pTransceiver->CommandGroupCommand( &cmd, selunits.GetAIGroup(), true );
		ResetPoints();
		return true;
	}
}
bool CWorldClient::ActionBuildAntiTank( const SGameMessage &msg, bool bForced )
{
	if ( bActionModifierAdd ) 
	{
		static SAIUnitCmd cmd;
		PrepareActionPosParam( &cmd, ACTION_COMMAND_PLACE_ANTITANK, GetPosFromMsg(msg), 0 );
		CVec2 vPos = GetPosFromMsg( msg );
		CVec3 vPos3;
		GetPos3( &vPos3, vPos );
		pScene->SetRotationStartAngle( 0, false );
		pScene->SetPosMarker( vPos3 );
		buildPoints.push_back( cmd );
		return false;
	}
	else
		return PerformActionPos( ACTION_COMMAND_PLACE_ANTITANK, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionRepairMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->pDesc->eGameType == SGVOGT_UNIT ) 
		{
			CUserActions actions;
			(*it)->GetActions( &actions, IMapObj::ACTIONS_WITH );
			if ( actions.HasAction(USER_ACTION_ENGINEER_REPAIR) ) 
				return PerformActionObj( ACTION_COMMAND_REPAIR, (*it)->pAIObj, bActionModifierAdd );
		}
	}
	return PerformActionPos( ACTION_COMMAND_REPAIR, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionRepairBuildingMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( ((*it)->pDesc->eGameType == SGVOGT_BUILDING) || ((*it)->pDesc->eGameType == SGVOGT_BRIDGE) )
			return PerformActionObj( ACTION_COMMAND_REPEAR_OBJECT, (*it)->pAIObj, bActionModifierAdd );
	}
	return false;
}
bool CWorldClient::ActionResupplyMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->pDesc->eGameType == SGVOGT_UNIT ) 
		{
			CUserActions actions;
			(*it)->GetActions( &actions, IMapObj::ACTIONS_WITH );
			if ( actions.HasAction(USER_ACTION_SUPPORT_RESUPPLY) ) 
				return PerformActionObj( ACTION_COMMAND_RESUPPLY, (*it)->pAIObj, bActionModifierAdd );
		}
	}
	return PerformActionPos( ACTION_COMMAND_RESUPPLY, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionResupplyHRMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_RESUPPLY_HR, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionFillRU( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->IsFriend() && 
         ( (*it)->pDesc->eGameType == SGVOGT_BUILDING ) && 
				 ( (static_cast_gdb<const SBuildingRPGStats*>((*it)->pRPG)->eType == SBuildingRPGStats::TYPE_MAIN_RU_STORAGE) ||
					 (static_cast_gdb<const SBuildingRPGStats*>((*it)->pRPG)->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE) ) )
		{
			return PerformActionObj( ACTION_COMMAND_FILL_RU, (*it)->pAIObj, bActionModifierAdd );
		}
	}
	return false;
}
/*bool CWorldClient::ActionBuildRUStorageMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_CREATE_RU_STORAGE, GetPosFromMsg(msg), bActionModifierAdd );
}*/
/*bool CWorldClient::ActionAssignToGunMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( !IsFoe(*it) && (*it)->pDesc->IsTechnics() && static_cast_gdb<const SUnitBaseRPGStats*>((*it)->pRPG)->IsArtillery() )
			return PerformActionObj( ACTION_COMMAND_CATCH_ARTILLERY, (*it)->pAIObj, bActionModifierAdd );
	}
	return false;
}*/
bool CWorldClient::ActionLookAtBinocularsMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_USE_SPYGLASS, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionCaptureArtilleryMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, true );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->IsAlive() && IsNeutral(*it) && (*it)->pDesc->IsTechnics() && static_cast_gdb<const SUnitBaseRPGStats*>((*it)->pRPG)->IsArtillery() )
		{
			if ( availActions.HasAction(USER_ACTION_HUMAN_RESUPPLY) ) 
				return PerformActionObj( ACTION_COMMAND_RESUPPLY_HR, (*it)->pAIObj, bActionModifierAdd );
			else
				return PerformActionObjParam( ACTION_COMMAND_CATCH_ARTILLERY, (*it)->pAIObj, 1, bActionModifierAdd );
		}
	}
	return false;
}
bool CWorldClient::ActionHookArtilleryMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, true );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( (*it)->IsAlive() && IsFriend(*it) && (*it)->pDesc->IsTechnics() && static_cast_gdb<const SUnitBaseRPGStats*>((*it)->pRPG)->IsArtillery() )
			return PerformActionObj( ACTION_COMMAND_TAKE_ARTILLERY, (*it)->pAIObj, bActionModifierAdd );
	}
	return false;
}
bool CWorldClient::ActionDeployArtilleryMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_DEPLOY_ARTILLERY, GetPosFromMsg(msg), bActionModifierAdd );
}
bool CWorldClient::ActionEntrenchSelfMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_ENTRENCH_SELF, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionDisbandSquadMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_DISBAND_FORMATION, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionFormSquadMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_FORM_FORMATION, VNULL2, bActionModifierAdd );
}
bool CWorldClient::ActionChangeFormationMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPosParam( ACTION_COMMAND_PARADE, VNULL2, msg.nEventID - USER_ACTION_FORMATION_0, bActionModifierAdd );
}
bool CWorldClient::ActionChangeShellTypeMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPosParam( ACTION_COMMAND_CHANGE_SHELLTYPE, VNULL2, msg.nEventID - USER_ACTION_USE_SHELL_DAMAGE, bActionModifierAdd );
}
bool CWorldClient::ActionBuildBridgeMsg( const SGameMessage &msg, bool bForced )
{
	UpdatePick( GetPosFromMsg(msg), pTimer->GetGameTime(), false, true, false );
	for ( CMapObjectsPtrList::iterator it = framePick.begin(); it != framePick.end(); ++it )
	{
		if ( ((*it)->pDesc->eGameType == SGVOGT_BRIDGE) && ((*it)->fHP == -1) )
			return PerformActionObj( ACTION_COMMAND_BUILD_BRIDGE, (*it)->pAIObj, bActionModifierAdd );
	}
	return false;
}
bool CWorldClient::ActionStandGroundMsg( const SGameMessage &msg, bool bForced )
{
	return PerformActionPos( ACTION_COMMAND_STAND_GROUND, VNULL2, bActionModifierAdd );
}
bool CWorldClient::DoCommandsList( CAIUnitCmdList &cmds )
{
	for ( CAIUnitCmdList::const_iterator it = cmds.begin(); it != cmds.end(); ++it )
		pTransceiver->CommandGroupCommand( &(*it), selunits.GetAIGroup(), true );
	cmds.clear();
	pScene->FlashPosMarkers();
	return true;
}
