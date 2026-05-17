#include "stdafx.h"

#include "GroupLogic.h"
#include "Commands.h"
#include "Soldier.h"
#include "UnitsIterators.h"
#include "CollisionInternal.h"
#include "Updater.h"
#include "UnitStates.h"
#include "Formation.h"
#include "Diplomacy.h"
#include "StaticObject.h"
#include "PathUnit.h"
#include "Shell.h"
#include "float.h"
#include "ScanLimiter.h"
#include "Building.h"
#include "Technics.h"
#include "GridCreation.h"
#include "AILogicInternal.h"
#include "Trigonometry.h"

// for profiling
#include "TimeCounter.h"
// for debug
#include "AIStaticMap.h"
#include "MPLog.h"
#include "..\Scene\Scene.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CCollisionsCollector theColCollector;
CGroupLogic theGroupLogic;
extern CUpdater updater;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CShellsStore theShellsStore;
extern CScanLimiter theScanLimiter;
extern CAILogic *pAILogic;

extern CTimeCounter timeCounter;
extern CStaticMap theStaticMap;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*														CGroupLogic														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::Init()
{
	lastSegmTime = curTime - curTime % SConsts::AI_SEGMENT_DURATION;
	segmUnits.resize( 2 );

	registeredGroups.clear();
	lastAmbushCheck = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::AddUnitToGroup( CCommonUnit *pGroupUnit, const int nGroup )
{
	if ( pGroupUnit->nGroup != nGroup )
	{
		// ���� ���� ��� � �����-�� ������, ������ ��� ������
		DelUnitFromGroup( pGroupUnit );

		pGroupUnit->nGroup = nGroup;
		pGroupUnit->nPos = groupUnits.Push( nGroup, pGroupUnit );
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::DelUnitFromGroup( CCommonUnit *pUnit )
{
	if ( pUnit->nGroup != 0 )
	{
		groupUnits.Erase( pUnit->nGroup, pUnit->nPos );
		pUnit->nGroup = 0;
		pUnit->nPos = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::DelUnitFromSpecialGroup( CCommonUnit *pUnit )
{
	if ( pUnit->nSpecialGroup != 0 )
	{
		if ( groupUnits.begin( pUnit->nSpecialGroup ) != groupUnits.end() )
			groupUnits.Erase( pUnit->nSpecialGroup, pUnit->nSpecialPos );

		if ( groupUnits.begin( pUnit->nSpecialGroup ) == groupUnits.end() )
			UnregisterSpecialGroup( pUnit->nSpecialGroup );
		pUnit->nSpecialGroup = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::DelGroup( const int nGroup )
{
	// ��������� ������
	for ( int i = groupUnits.begin( nGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
	{
		groupUnits.GetEl( i )->nGroup = 0;
		groupUnits.GetEl( i )->nPos = 0;
	}
	groupUnits.DelQueue( nGroup );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGroupLogic::GetGroupNumberByID( const WORD wID )
{
	return ( ( wID << 4 ) | BYTE( theDipl.GetMyNumber() ) ) << 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGroupLogic::GetSpecialGroupNumberByID( const WORD wID )
{
	return ( ( ( wID << 4 ) | BYTE( theDipl.GetMyNumber() ) ) << 1 ) | 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGroupLogic::GetIdByGroupNumber( const WORD wGroup )
{
	return wGroup >> 5;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WORD CGroupLogic::GetPlayerByGroupNumber( const WORD wGroup )
{
	return ( wGroup >> 1 ) & 0xf;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::RegisterGroup( IRefCount **pUnitsBuffer, const int nLen, const WORD wGroup )
{
	if ( wGroup >= groupUnits.GetQueuesNum() )
		groupUnits.IncreaseQueuesNum( wGroup * 1.5 );

	if ( groupUnits.GetSize( wGroup ) != 0 )
	{
		DelGroup( wGroup );

		if ( GetPlayerByGroupNumber( wGroup ) == theDipl.GetMyNumber() )
			groupIds.AddToFreeId( GetIdByGroupNumber( wGroup ) );
	}

	registeredGroups.insert( wGroup );

	for ( int i = 0; i < nLen; ++i )
	{
		if ( pUnitsBuffer[i] != 0 && dynamic_cast<CCommonUnit*>(pUnitsBuffer[i]) != 0 )
		{
			CCommonUnit *pUnit = static_cast<CCommonUnit*>( pUnitsBuffer[i] );
			if ( pUnit->IsValid() && pUnit->IsAlive() )
			{
				CCommonUnit *pGroupUnit;
				if ( pUnit->IsInFormation() )
					pGroupUnit = pUnit->GetFormation();
				else
					pGroupUnit = pUnit;

				if ( pGroupUnit->nGroup != wGroup )				
				{
					// ���� ���� ��� � �����-�� ������, ������ ��� ������
					DelUnitFromGroup( pGroupUnit );

					pGroupUnit->nGroup = wGroup;
					pGroupUnit->nPos = groupUnits.Push( wGroup, pGroupUnit );
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD CGroupLogic::GenerateGroupNumber()
{
	return GetGroupNumberByID( groupIds.GetFreeId() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UnregisterSpecialGroup( const WORD wSpecialGroup )
{
	if ( registeredGroups.find( wSpecialGroup ) != registeredGroups.end() )
	{
		for ( int i = groupUnits.begin( wSpecialGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
			groupUnits.GetEl( i )->nSpecialGroup = 0;

		groupUnits.DelQueue( wSpecialGroup );
		groupIds.AddToFreeId( GetIdByGroupNumber( wSpecialGroup ) );

		registeredGroups.erase( wSpecialGroup );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UnregisterGroup( const WORD wGroup ) 
{ 	
	if ( registeredGroups.find( wGroup ) != registeredGroups.end() )
	{
		updater.ResetAreasGroupIfEqual( wGroup );

		DelGroup( wGroup );

		if ( GetPlayerByGroupNumber( wGroup ) == theDipl.GetMyNumber() )
			groupIds.AddToFreeId( GetIdByGroupNumber( wGroup ) );

		registeredGroups.erase( wGroup );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::DivideBySubGroups( const SAIUnitCmd &command, const int nGroup )
{
	float fMinX = 1e10, fMaxX = -1e10, fMinY = 1e10, fMaxY = -1e10;

	for ( int i = groupUnits.begin( nGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
	{
		const CVec2& center = groupUnits.GetEl( i )->GetCenter();

		if ( center.x > fMaxX )
			fMaxX = center.x;
		if ( center.x < fMinX )
			fMinX = center.x;
		if ( center.y > fMaxY )
			fMaxY = center.y;
		if ( center.y < fMinY )
			fMinY = center.y;
	}

	int numRows = ceil( ( fMaxX - fMinX ) / SConsts::GROUP_DISTANCE );
	int numColumns = ceil( ( fMaxY - fMinY ) / SConsts::GROUP_DISTANCE );
	numRows = Max( numRows, 1 );
	numColumns = Max( numColumns, 1 );
	std::vector<CVec2> centers( numRows * numColumns );
	std::vector<int> nums( numRows * numColumns );

	memset( &(centers[0]), 0, centers.size() * sizeof( SVector ) );
	memset( &(nums[0]), 0, nums.size() * sizeof( int ) );

	for ( int i = groupUnits.begin( nGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
	{
		CCommonUnit *pUnit = groupUnits.GetEl( i );

		const int nX = ( pUnit->GetCenter().x - fMinX ) / SConsts::GROUP_DISTANCE;
		const int nY = ( pUnit->GetCenter().y - fMinY ) / SConsts::GROUP_DISTANCE;

		const int nSubGroup = nY * numRows + nX;
		pUnit->nSubGroup = nSubGroup;
		centers[nSubGroup] += pUnit->GetCenter();
		++nums[nSubGroup];
	}

	for ( int i = 0; i != centers.size(); ++i )
	{
		if ( nums[i] != 0 )
			centers[i] /= nums[i];
	}

	for ( int i = groupUnits.begin( nGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
	{
		CCommonUnit *pUnit = groupUnits.GetEl( i );
		pUnit->vShift = pUnit->GetCenter() - centers[pUnit->nSubGroup];
		if ( command.cmdType == ACTION_COMMAND_MOVE_TO || command.cmdType == ACTION_COMMAND_SWARM_TO )
		{
			const CVec2 vAngle( NTrg::Cos( command.fNumber ), NTrg::Sin( command.fNumber ) );
			pUnit->vShift ^= vAngle;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::CreateSpecialGroup( const WORD wGroup )
{
	if ( wGroup != 0 )
	{
		const WORD wSpecialGroup = GetSpecialGroupNumberByID( groupIds.GetFreeId() );
		registeredGroups.insert( wSpecialGroup );

		if ( wSpecialGroup >= groupUnits.GetQueuesNum() )
			groupUnits.IncreaseQueuesNum( wSpecialGroup * 1.5 );

		int nLen = 0;
		for ( int i = groupUnits.begin( wGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
		{
			CCommonUnit *pUnit = groupUnits.GetEl( i );

			if ( pUnit->IsFormation() )
			{
				CFormation *pForm = static_cast<CFormation *>(pUnit);
				int nSold = pForm->Size();
				for ( int i=0; i< nSold; ++i )
				{
					CSoldier *pSold = (*pForm)[i];

					DelUnitFromSpecialGroup( pSold );

					pSold->nSpecialGroup = wSpecialGroup;
					pSold->nSpecialPos = groupUnits.Push( wSpecialGroup, pSold );
					++nLen;
				}
			}
			else
			{
				DelUnitFromSpecialGroup( pUnit );

				pUnit->nSpecialGroup = wSpecialGroup;
				pUnit->nSpecialPos = groupUnits.Push( wSpecialGroup, pUnit );
				++nLen;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::EraseFromAmbushGroups( const SAIUnitCmd &command, const WORD wGroup )
{
	if ( !command.bFromAI || command.cmdType == ACTION_COMMAND_AMBUSH )
	{
		for ( int i = groupUnits.begin( wGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
		{
			CCommonUnit *pUnit = groupUnits.GetEl( i );
			ambushUnits.erase( pUnit->GetUniqueId() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::CreateAmbushGroup( const WORD wGroup )
{
	ambushGroups.push_front();
	for ( int i = groupUnits.begin( wGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
	{
		CCommonUnit *pUnit = groupUnits.GetEl( i );
		const int nUniqueId = pUnit->GetUniqueId();
		ambushGroups.front().push_back( SAmbushInfo( nUniqueId ) );
		ambushUnits.insert( nUniqueId );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::SetToAmbush( CAmbushGroups::iterator &iter )
{
	std::list< std::pair<CCommonUnit*, int> > oldUnitsGroups;
	
	IRefCount **pObjects = GetTempBuffer<IRefCount*>( iter->size() );
	int nLen = 0;
	for ( std::list<SAmbushInfo>::iterator innerIter = iter->begin(); innerIter != iter->end(); ++innerIter )
	{
		CCommonUnit *pUnit = checked_cast<CCommonUnit*>( CLinkObject::GetObjectByUniqueIdSafe( innerIter->nUniqueId ) );
		innerIter->bGivenCommandToRestore = false;
		pObjects[nLen++] = pUnit;

		oldUnitsGroups.push_back( std::pair<CCommonUnit*, int>( pUnit, pUnit->GetNGroup() ) );
	}

	const int nGroup = GetSpecialGroupNumberByID( GetSpecialGroupNumberByID( groupIds.GetFreeId() ) );
	pAILogic->RegisterGroup( pObjects, nLen, nGroup );

	SAIUnitCmd command( ACTION_COMMAND_AMBUSH );
	pAILogic->GroupCommand( &command, nGroup, false );
	pAILogic->UnregisterGroup( nGroup );

	for ( std::list< std::pair<CCommonUnit*, int> >::iterator iter = oldUnitsGroups.begin(); iter != oldUnitsGroups.end(); ++iter )
		theGroupLogic.AddUnitToGroup( iter->first, iter->second );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UnitSetToAmbush( CCommonUnit *pUnit )
{
	for ( CAmbushGroups::iterator iter = ambushGroups.begin(); iter != ambushGroups.end(); ++iter )
	{
		for ( std::list<SAmbushInfo>::iterator innerIter = iter->begin(); innerIter != iter->end(); ++innerIter )
		{
			const int nUniqueID = innerIter->nUniqueId;
			CLinkObject *pObject = CLinkObject::GetObjectByUniqueIdSafe( nUniqueID );

			if ( pObject == pUnit )
			{
				innerIter->vAmbushCenter = pUnit->GetCenter();
				innerIter->wAmbushDir = pUnit->GetDir();
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::ProcessAmbushGroups()
{
	if ( lastAmbushCheck + 5000 < curTime )
	{
		lastAmbushCheck = curTime;
		std::unordered_set<int> checkedUnits;

		CAmbushGroups::iterator iter = ambushGroups.begin();
		while ( iter != ambushGroups.end() )
		{
			bool bCanSetToAmbush = true;
			std::list<SAmbushInfo>::iterator innerIter = iter->begin();
			while ( innerIter != iter->end() )
			{
				const int nUniqueId = innerIter->nUniqueId;
				// ��� � ������ ambush ������, or deleted from ambush groups, ������� �� ����
				if ( checkedUnits.find( nUniqueId ) != checkedUnits.end() || 
						 ambushUnits.find( nUniqueId ) == ambushUnits.end() )
					innerIter = iter->erase( innerIter );
				else
				{
					// ���� ����������
					checkedUnits.insert( nUniqueId );

					CLinkObject *pObject = CLinkObject::GetObjectByUniqueIdSafe( nUniqueId );
					// ���� ����, ������� �� ambush groups
					if ( !IsValidObj( pObject ) )
					{
						ambushUnits.erase( nUniqueId );
						innerIter = iter->erase( innerIter );
					}
					else
					{
						// unit alive, check if can be set to ambush
						CCommonUnit *pUnit = checked_cast<CCommonUnit*>( pObject );

						if ( pUnit->GetState() && innerIter->vAmbushCenter.x != -1.0f )
						{
							const bool bRest = 
								IsRestState( pUnit->GetState()->GetName() ) && pUnit->IsIdle() &&
								( !pUnit->IsFormation() || checked_cast<CFormation*>(pUnit)->IsEveryUnitResting() );

							if ( bRest )
							{
								if ( !innerIter->bGivenCommandToRestore &&
										 ( fabs2( pUnit->GetCenter() - innerIter->vAmbushCenter ) >= sqr( SConsts::TILE_SIZE * 2 ) ||
											 DirsDifference( pUnit->GetFrontDir(), innerIter->wAmbushDir ) >= 3000 ) )
								{
									innerIter->bGivenCommandToRestore = true;
									UnitCommand( SAIUnitCmd( ACTION_COMMAND_SWARM_TO, innerIter->vAmbushCenter , 0 ), pUnit, false );
									UnitCommand( SAIUnitCmd( ACTION_COMMAND_ROTATE_TO_DIR, GetVectorByDirection( innerIter->wAmbushDir ) ), pUnit, true );

									bCanSetToAmbush = false;
								}
								else
								{
									const int nParty = pUnit->GetParty();
									bCanSetToAmbush = 
										bCanSetToAmbush && ( pUnit->GetLastChangeStateTime() + 5000 < curTime ) &&
										nParty < 2 && !pUnit->IsVisible( 1 - nParty );
								}
							}
							else
								bCanSetToAmbush = false;
						}
						else
							bCanSetToAmbush = false;

						++innerIter;
					}
				}
			}

			if ( bCanSetToAmbush )
			{
				// group isn't empty, set to ambush
				if ( !iter->empty() )
					SetToAmbush( iter++ );
				else
					// group is empty, erase from ambush groups
					iter = ambushGroups.erase( iter );
			}
			else
				++iter;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::GroupCommand( const SAIUnitCmd &command, const WORD wGroup, bool bPlaceInQueue )
{
	// �.�. ����� ������ ����� ���������, ��������, ����� ���� �� ������� � multiplayer �����
	if ( registeredGroups.find( wGroup ) != registeredGroups.end() )
	{
		CPtr<CAICommand> pCommand = new CAICommand( command );
		
		// ����� � ����������� ������������� �������
		if ( !bPlaceInQueue )
		{
			if ( command.cmdType == ACTION_COMMAND_MOVE_TO || command.cmdType == ACTION_COMMAND_SWARM_TO || 
					 command.cmdType == ACTION_COMMAND_PLACEMINE || command.cmdType == ACTION_COMMAND_CLEARMINE || 
					 command.cmdType == ACTION_COMMAND_DEPLOY_ARTILLERY || command.cmdType == ACTION_COMMAND_UNLOAD ||
					 command.cmdType == ACTION_COMMAND_RESUPPLY_HR || command.cmdType == ACTION_COMMAND_RESUPPLY ||
					 command.cmdType == ACTION_COMMAND_REPAIR )
				DivideBySubGroups( command, wGroup );
			else
			{
				for ( int i = groupUnits.begin( wGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
				{
					groupUnits.GetEl( i )->nSubGroup = -1;
					groupUnits.GetEl( i )->vShift = VNULL2;
				}
			}
		}

		EraseFromAmbushGroups( command, wGroup );

		if ( command.cmdType == ACTION_COMMAND_AMBUSH )
		{
			CreateSpecialGroup( wGroup );
			CreateAmbushGroup( wGroup );
		}
		
		std::vector<CCommonUnit*> groups( groupUnits.GetSize( wGroup ) );

		int nGroupsIter = 0;
		for ( int i = groupUnits.begin( wGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
			groups[nGroupsIter++] = groupUnits.GetEl( i );

		// ���� ������ �������� ����� ���������� �� command.bFromAI - ��� ���������� ������ 
		// �����������
		if ( groupUnits.GetSize(wGroup) != 0 )
		{
			int nRandom = Random( groupUnits.GetSize(wGroup) );
			if ( !command.bFromAI )
			{
				// ������� �� ������� ����, ������� ����� �������� ��� �� ������������� �������
				groups[nRandom]->SendAcknowledgement( ACK_POSITIVE, true );
			}
		}

		if ( command.cmdType == ACTION_COMMAND_MOVE_TO_GRID )
			ProcessGridCommand( command.vPos, CVec2( NTrg::Cos(command.fNumber), NTrg::Sin(command.fNumber) ), wGroup, bPlaceInQueue );
		else
		{
			std::unordered_set<int> memFormationIDs;
			for ( std::vector<CCommonUnit*>::iterator iter = groups.begin(); iter != groups.end(); ++iter )
			{
				CCommonUnit *pUnit = *iter;

				bool bSendCommand = true;
				if ( command.cmdType == ACTION_COMMAND_FORM_FORMATION && pUnit->IsFormation() )
				{
					CFormation *pFormation = checked_cast<CFormation*>( pUnit );
					if ( pFormation->Size() == 1 && (*pFormation)[0]->GetMemFormation() != 0 )
					{
						const int nMemFormationID = (*pFormation)[0]->GetMemFormation()->GetID();
						if ( memFormationIDs.find( nMemFormationID ) == memFormationIDs.end() )
							memFormationIDs.insert( nMemFormationID );
						else
							bSendCommand = false;
					}
				}
				
				if ( bSendCommand )
					pUnit->UnitCommand( pCommand, bPlaceInQueue, false );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UnitCommand( const SAIUnitCmd &command, CCommonUnit *pGroupUnit, bool bPlaceInQueue  )
{
	if ( pGroupUnit->IsAlive() )
	{
		if ( command.cmdType == ACTION_COMMAND_DIE )
			pGroupUnit->Die( command.fromExplosion, command.fNumber );
		else
		{
			CAICommand *pCommand = new CAICommand( command );
			pGroupUnit->UnitCommand( pCommand, bPlaceInQueue, true );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::InsertUnitCommand( const SAIUnitCmd &command, CCommonUnit *pUnit )
{
	CAICommand *pCmd = new CAICommand( command );
	pUnit->InsertUnitCommand( pCmd );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::PushFrontUnitCommand( const SAIUnitCmd &command, CCommonUnit *pUnit  )
{
	CAICommand *pCmd = new CAICommand( command );
	pUnit->PushFrontUnitCommand( pCmd );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::SegmentFollowingUnits()
{
	while ( !followingUnits.empty() )
	{
		CCommonUnit *pUnit = followingUnits.front();
		CCommonUnit *pHeadUnit = pUnit->GetFollowedUnit();
		followingUnits.pop_front();

		const float fHeadUnitSpeed = pHeadUnit->GetSpeedForFollowing();
		if ( fHeadUnitSpeed != 0.0f )
			pUnit->SetDesirableSpeed( fHeadUnitSpeed );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::StayTimeSegment()
{
	int nCntNotRestUnits = 0;
	
	for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = (*iter);

		if ( pUnit->IsValid() )
		{
			pUnit->AnimationSegment();

			IUnitState *pState = pUnit->GetState();
			if ( pState && IsRestState( pState->GetName() ) && pUnit->IsInFormation() )
				pState = pUnit->GetFormation()->GetState();

			if ( pState )
			{
				const EUnitStateNames eStateName = pState->GetName();
				if ( eStateName == EUSN_SWARM || eStateName == EUSN_ATTACK_STAT_OBJECT ||
						 eStateName == EUSN_ATTACK_UNIT || eStateName == EUSN_ATTACK_UNIT_IN_BUILDING )
				{
					++nCntNotRestUnits;
				}
			}
		}
	}

	GetSingleton<IScene>()->GetStatSystem()->UpdateEntry( "Not rest:", NStr::Format( "%d", nCntNotRestUnits ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::Segment() 
{
	updater.ClearAllUpdates( ACTION_NOTIFY_PLACEMENT );

	const NTimer::STime roundedCurTime = curTime - curTime % SConsts::AI_SEGMENT_DURATION;

	// states ������
	NSegmObjs::Segment( lastSegmTime, roundedCurTime, segmUnits[0], (CStateSegments*)0 );
	NSegmObjs::Segment( lastSegmTime, roundedCurTime, segmUnits[1], (CStateSegments*)0 );
	NSegmObjs::Segment( lastSegmTime, roundedCurTime, freezeUnits, (CFreezeSegments*)0 );

	// ������
	theShellsStore.Segment();

	// �������� ������ � follow
	SegmentFollowingUnits();

	// ����� ��������
	NSegmObjs::Segment( lastSegmTime, roundedCurTime, firstPathUnits, (CFirstPathSegments*)0 );
	theScanLimiter.SegmentsFinished();

	// ��������� ���������� ��-�� �������� ������
	NSegmObjs::SegmentWOMove( lastSegmTime, roundedCurTime, secondPathUnits, (CStayTimeSegments*)0 );
	StayTimeSegment();

	// ��������� ��������
	theColCollector.HandOutCollisions();

	// �������� ������ ����� ����
	NSegmObjs::Segment( lastSegmTime, roundedCurTime, secondPathUnits, (CSecondPathSegments*)0 );

	lastSegmTime = curTime - curTime % SConsts::AI_SEGMENT_DURATION + SConsts::AI_SEGMENT_DURATION;

	ProcessAmbushGroups();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UpdateAllAreas( const int nGroup, const EActionNotify eAction )
{
	for ( int i = groupUnits.begin( nGroup ); i != groupUnits.end(); i = groupUnits.GetNext( i ) )
		groupUnits.GetEl( i )->UpdateArea( eAction );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::RegisterSegments( CCommonUnit *pUnit, const bool bInitialization, bool bAllInfo )
{
	segmUnits[pUnit->IsInfantry()].RegisterSegments( pUnit, bInitialization );
	if ( bAllInfo )
		freezeUnits.RegisterSegments( pUnit, bInitialization );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::RegisterPathSegments( CAIUnit *pUnit, const bool bInitialization )
{
	const int nUniqueId = pUnit->GetUniqueId();
	firstPathUnits.RegisterSegments( pUnit, bInitialization );
	secondPathUnits.RegisterSegments( pUnit, bInitialization );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::UnregisterSegments( CCommonUnit *pUnit )
{
	segmUnits[pUnit->IsInfantry()].UnregisterSegments( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 GetGoPointByCommand( const SAIUnitCmd &cmd )
{
	// -1 - ��� �����, 0 - cmd.vPos, 1 - ����� ����� cmd.pObject, 2 - ����� ������. ������� cmd.pObject
	int nType = -1;

	switch ( cmd.cmdType )
	{
		case ACTION_COMMAND_MOVE_TO:						nType =  0; break;
		case ACTION_COMMAND_ATTACK_UNIT:				nType =  1; break;
		case ACTION_COMMAND_ATTACK_OBJECT:			nType =  2; break;
		case ACTION_COMMAND_SWARM_TO:						nType =  0; break;
		case ACTION_COMMAND_LOAD:								nType =  1; break;
		case ACTION_COMMAND_UNLOAD:							nType = -1; break;
		case ACTION_COMMAND_ENTER:							nType =  2; break;
		case ACTION_COMMAND_LEAVE:							nType =  0; break;
		case ACTION_COMMAND_ROTATE_TO:					nType = -1; break;
		case ACTION_COMMAND_ROTATE_TO_DIR:			nType = -1; break;
		case ACTION_COMMAND_STOP:								nType = -1; break;
		case ACTION_COMMAND_PARADE:							nType = -1; break;
		case ACTION_COMMAND_PLACEMINE:					nType =  0; break;
		case ACTION_COMMAND_CLEARMINE:					nType =  0; break;
		case ACTION_COMMAND_GUARD:							nType =  0; break;
		case ACTION_COMMAND_AMBUSH:							nType = -1; break;
		case ACTION_COMMAND_RANGE_AREA:					nType = -1; break;
		case ACTION_COMMAND_ART_BOMBARDMENT:		nType = -1; break;
		case ACTION_COMMAND_INSTALL:						nType = -1; break;
		case ACTION_COMMAND_UNINSTALL:					nType = -1; break;
		case ACTION_COMMAND_CALL_BOMBERS:				nType = -1; break;
		case ACTION_COMMAND_CALL_FIGHTERS:			nType = -1; break;
		case ACTION_COMMAND_CALL_SCOUT:					nType = -1; break;
		case ACTION_COMMAND_PARADROP:						nType = -1; break;
		case ACTION_COMMAND_RESUPPLY:						nType = -1; break;
		case ACTION_COMMAND_REPAIR:							nType = -1; break;
		case ACTION_COMMAND_BUILD_FENCE_BEGIN:	nType =  0; break;
		case ACTION_COMMAND_ENTRENCH_BEGIN:			nType =  0; break;
		case ACTION_COMMAND_CATCH_ARTILLERY:		nType =  1; break;
		case ACTION_COMMAND_USE_SPYGLASS:				nType = -1; break;
		case ACTION_COMMAND_TAKE_ARTILLERY:			nType = -1; break;
		case ACTION_COMMAND_DEPLOY_ARTILLERY:		nType = -1; break;
		case ACTION_COMMAND_PLACE_ANTITANK:			nType =  0; break;
		case ACTION_COMMAND_DISBAND_FORMATION:	nType = -1; break;
		case ACTION_COMMAND_FORM_FORMATION:			nType = -1; break;
	}

	CVec2 vResult( -1.0f, -1.0f );
	switch ( nType )
	{
		case -1:
			break;
		case 0:
			vResult = cmd.vPos;
			break;
		case 1:
			NI_ASSERT_T( dynamic_cast_ptr<CCommonUnit*>(cmd.pObject) != 0, NStr::Format( "Non-compliance for command %d", cmd.cmdType ) );
			vResult = static_cast_ptr<CCommonUnit*>(cmd.pObject)->GetCenter();
			break;
		case 2:
			{
				NI_ASSERT_T( dynamic_cast_ptr<CStaticObject*>(cmd.pObject) != 0, NStr::Format( "Non-compliance for command %d", cmd.cmdType ) );
				CStaticObject* pObj = dynamic_cast_ptr<CStaticObject*>(cmd.pObject);
				if ( pObj->GetObjectType() == ESOT_BUILDING )
				{
					CBuilding *pBuilding = checked_cast<CBuilding*>(pObj);
					if ( pBuilding->GetNEntrancePoints() > 0 )
						vResult = pBuilding->GetEntrancePoint( 0 );
					else
						vResult = pObj->GetCenter();
				}
				else
					vResult = pObj->GetCenter();
			}

			break;
	}

	return vResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::AddFollowingUnit( CCommonUnit *pUnit )
{
	followingUnits.push_back( pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGroupLogic::ProcessGridCommand( const CVec2 &vGridCenter, const CVec2 &vGridDir, const int nGroup, bool bPlaceInQueue )
{
	CGrid grid( vGridCenter, nGroup, vGridDir );

	SAIUnitCmd moveToCmd( ACTION_COMMAND_MOVE_TO_GRID, VNULL2, GetDirectionByVector( vGridDir ) );
	for ( int i = 0; i < grid.GetNUnitsInGrid(); ++i )
	{
		CCommonUnit *pUnit = grid.GetUnit( i );

		if ( pUnit->GetBehaviour().moving == SBehaviour::EMHoldPos )
			pUnit->GetBehaviour().moving = SBehaviour::EMRoaming;

		moveToCmd.vPos = grid.GetUnitCenter( i );
		UnitCommand( moveToCmd, pUnit, bPlaceInQueue );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
