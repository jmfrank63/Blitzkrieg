#include "stdafx.h"

#include "Soldier.h"
#include "Formation.h"
#include "Commands.h"
#include "Guns.h"
#include "AIStaticMap.h"
#include "Units.h"
#include "FormationStates.h"
#include "GunsInternal.h"
#include "GroupLogic.h"
#include "AILogicInternal.h"
#include "Building.h"
#include "Entrenchment.h"
#include "Technics.h"
#include "Updater.h"
#include "Diplomacy.h"
#include "AckManager.h"
#include "UnitCreation.h"
#include "StandartPath.h"
#include "StandartSmoothSoldierPath.h"
#include "PathFinder.h"
#include "Statistics.h"
#include "General.h"
#include "Graveyard.h"
#include "MPLog.h"
#include "DifficultyLevel.h"
#include "UnitStates.h"

#include "..\Main\ScenarioTracker.h"

// for profiling
#include "TimeCounter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CSupremeBeing theSupremeBeing;
extern CAckManager theAckManager;
extern CPtr<IStaticPathFinder> pThePathFinder;
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
extern CUnits units;
extern CGroupLogic theGroupLogic;
extern CAILogic *pAILogic;
extern CUpdater updater;
extern CDiplomacy theDipl;
extern CUnitCreation theUnitCreation;
extern CStatistics theStatistics;
extern CDifficultyLevel theDifficultyLevel;

extern CTimeCounter timeCounter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CFormationCenter													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::Init( const CVec2 &_center, const int _z, const WORD _dir, const int dbID )
{
	center = _center;
	z = _z;
	dir = GetVectorByDirection( _dir );
	maxDiff = -1.0f;
	fSpeedCoeff = 1.0f;
	lastKnownGoodTile = GetTile();

	pSmoothPath = new CStandartSmoothSoldierPath();

	CCommonUnit::Init( dbID );
	Mem2UniqueIdObjs();
	
	pSmoothPath->Init( this, pThePathFinder->CreatePathByDirection( GetCenter(), CVec2( 1, 1 ), GetCenter(), nBoundTileRadius ), true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SRect CFormationCenter::GetUnitRectForLock() const
{
	const float length = vAABBHalfSize.y * SConsts::COEFF_FOR_LOCK;
	const float width = vAABBHalfSize.x * ( SConsts::COEFF_FOR_LOCK + 0.01f );

	SRect unitRect;
	unitRect.InitRect( GetCenter(), dir, length, width );

	return unitRect;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
bool CFormationCenter::TurnToDir( const WORD &newDir, const bool bCanBackward, const bool bForward )
{
	dir = GetVectorByDirection( newDir );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::UpdateDirection( const CVec2 &newDir )
{
	float fR =  fabs( newDir );
	if ( fR != 0 )
		dir = newDir * ( 1 / fR );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::UpdateDirection( const WORD newDir )
{
	dir = GetVectorByDirection( newDir );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::NotifyDiff( const float fDiff )
{
	if ( fDiff > maxDiff )
		maxDiff = fDiff;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormationCenter::GetPathSegmentsPeriod() const
{
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::Segment()
{
	if ( maxDiff > 0 )
	{
		const float fCoeff = Max( 1.0f, maxDiff / SConsts::TILE_SIZE );

		if ( fCoeff >= 16 )
			fSpeedCoeff = 16;
		else
			fSpeedCoeff = fCoeff;
	}
	
	CVec2 oldDir = dir;
	// ��� update �������� � AI_SEGMENT_DURATION
	NTimer::STime timeDiff = SConsts::AI_SEGMENT_DURATION * GetPathSegmentsPeriod();
	const CVec3 center3D = pSmoothPath->GetPoint( timeDiff );
	theSupremeBeing.UnitChangedPosition( this, CVec2( center3D.x, center3D.y ) );
	center.x = center3D.x;
	center.y = center3D.y;
	z = center3D.z;

	maxDiff = -1.0f;		
	if ( oldDir * dir < 0.9f )
	{
		const int nSegmentsToGo = Min( 20, int(( 3.0f * GetRadius() / GetMaxPossibleSpeed() ) / float(SConsts::AI_SEGMENT_DURATION)) );
		if ( nSegmentsToGo > 0 )
		{
			const CVec3 center3D = pSmoothPath->GetPoint( nSegmentsToGo * SConsts::AI_SEGMENT_DURATION );
			center.x = center3D.x;
			center.y = center3D.y;
			z = center3D.z;
		}
	}

	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_STATIC );
	if ( theStaticMap.CanUnitGo( 0, GetTile(), GetAIClass() ) )
		lastKnownGoodTile = GetTile();
	theStaticMap.RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SVector CFormationCenter::GetLastKnownGoodTile() const
{
	return lastKnownGoodTile;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationCenter::SendAlongPath( IStaticPath *_pStaticPath, const CVec2 &vShift, bool bSmoothTurn )
{
	if ( _pStaticPath )
	{
		pStaticPath = _pStaticPath;
		if ( theStaticMap.IsPointInside( pStaticPath->GetFinishPoint() + vShift ) )
			return pSmoothPath->Init( this, new CStandartPath( GetBoundTileRadius(), GetAIClass(), pThePathFinder, pStaticPath, center, pStaticPath->GetFinishPoint() + vShift, GetLastKnownGoodTile() ), Size() > 1 );
		else
			return pSmoothPath->Init( this, new CStandartPath( GetBoundTileRadius(), GetAIClass(), pThePathFinder, pStaticPath, center, pStaticPath->GetFinishPoint(), GetLastKnownGoodTile() ), Size() > 1 );
	}
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationCenter::SendAlongPath( IPath *pPath )
{
	return pSmoothPath->Init( this, pPath, Size() > 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStaticPathFinder* CFormationCenter::GetPathFinder() const
{
	return pThePathFinder;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationCenter::GetNearFormationPos() const
{
	return GetCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormationCenter::GetFarFormationPos() const
{
	return pSmoothPath->GetShift( SConsts::SPLINE_STEP );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::GetNextTiles( std::list<SVector> *pTiles ) const
{
	pSmoothPath->GetNextTiles( pTiles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::StopFormationCenter()
{
	pSmoothPath->Init( this, pThePathFinder->CreatePathByDirection( GetCenter(), CVec2( 1, 1 ), GetCenter(), nBoundTileRadius ), true );
	speed =	VNULL2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::SetCoordWOUpdate( const CVec3 &newCenter )
{
	theSupremeBeing.UnitChangedPosition( this, CVec2(newCenter.x, newCenter.y) );
	center = CVec2( newCenter.x, newCenter.y );
	z = newCenter.z;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormationCenter::SetNewCoordinates( const CVec3 &newCenter, bool bStopUnit )
{
	SetCoordWOUpdate( newCenter );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CFormationCenter::GetSmoothTurnThreshold() const 
{ 
	return 0.6f; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormationCenter::GetMaxPossibleSpeed() const 
{ 
	return maxSpeed / fSpeedCoeff;// * GetCurSpeedBonus();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormationCenter::GetSpeedForFollowing()
{
	float fOldSpeedCoeff = fSpeedCoeff;
	fSpeedCoeff = 1.0f;
	const float fResult = GetMaxSpeedHere( GetCenter() );
	fSpeedCoeff = fOldSpeedCoeff;

	return fResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStaticPath* CFormationCenter::CreateBigStaticPath( const CVec2 &vStartPoint, const CVec2 &vFinishPoint, interface IPointChecking *pPointChecking )
{
	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_STATIC );

	GetPathFinder()->SetPathParameters( GetBoundTileRadius(), GetAIClass(), pPointChecking, vStartPoint, vFinishPoint, SConsts::INFINITY_PATH_LIMIT, true, lastKnownGoodTile );
	GetPathFinder()->CalculatePathWOCycles();
	GetPathFinder()->SmoothPath();

	theStaticMap.RestoreMode();

	if ( GetPathFinder()->GetPathLength() == -1 )
		return 0;
	else if ( pPointChecking != 0 )
		return new CCommonStaticPath( *GetPathFinder(), CVec2( -1.0f, -1.0f ) );
	else
		return new CCommonStaticPath( *GetPathFinder(), vFinishPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormationCenter::IsInOneTrain( IBasePathUnit *pUnit ) const 
{ 
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CFormation															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CFormation::CCarryedMortar::CreateMortar( const class CFormation *pOwner)
{
	NI_ASSERT_T( bHasMortar, "formation doesn't have mortar");
	const CVec3 vPos( pOwner->GetCenter(), pOwner->GetZ() );
	const int id = theUnitCreation.AddNewUnit( pStats, fHP / pStats->fMaxHP, vPos.x, vPos.y, vPos.z, 
															nDBID, pOwner->GetDir(), pOwner->GetPlayer(),
															SGVOT_MESH, false, true, true );
	bHasMortar = false;
	return id;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::CCarryedMortar::Init( const class CAIUnit *pArt )
{
	NI_ASSERT_T( pArt->GetStats()->type == RPG_TYPE_ART_MORTAR || pArt->GetStats()->type == RPG_TYPE_ART_HEAVY_MG, "foramtion attempted to take not mortar");
	bHasMortar = true;
	fHP = pArt->GetHitPoints();
	nDBID = pArt->GetDBID();
	pStats = pArt->GetStats();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													CFormation															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( CFormation );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::AddUnit( class CSoldier *pUnit, const int nPos )
{
	++nUnits;
	units[nPos].pUnit = pUnit;

	if ( pUnit->GetStats()->type == RPG_TYPE_OFFICER ||
			 pUnit->GetStats()->type == RPG_TYPE_ENGINEER ||
			 pUnit->GetStats()->type == RPG_TYPE_SNIPER )
		bWithMoraleOfficer = true;

	if ( pUnit->GetMemFormation() != this )
		pUnit->SetFormation( this, nPos );

	if ( pUnit->GetStats()->fSpeed < GetMaxPossibleSpeed() )
		SetMaxSpeed( pUnit->GetStats()->fSpeed );
	if ( pUnit->GetBoundTileRadius() > GetBoundTileRadius() )
		SetBoundTileRadius( pUnit->GetBoundTileRadius() );
	if ( pUnit->GetAABBHalfSize().x > GetAABBHalfSize().x )
		SetAABBHalfSize( pUnit->GetAABBHalfSize() );
	if ( pUnit->GetPassability() < fPass )
		fPass = pUnit->GetPassability();
	if ( pUnit->GetTimeToCamouflage() > timeToCamouflage )
		timeToCamouflage = pUnit->GetTimeToCamouflage();
	if ( pUnit->GetMaxFireRange() > fMaxFireRange )
		fMaxFireRange = pUnit->GetMaxFireRange();

	for ( int i = 0; i < availCommands.GetSize(); ++i )
	{
		if ( pUnit->GetStats()->HasCommand( i ) )
			availCommands.SetData( i );
	}

	for ( int i = 0; i < pUnit->GetNGuns(); ++i )
		guns.push_back( SGunInfo( nPos, i ) );

	if ( cPlayer == 255 )
		cPlayer = pUnit->GetPlayer();
	else
	{
		if ( cPlayer != pUnit->GetPlayer() )
			pUnit->ChangePlayer( cPlayer );
	}

	theSupremeBeing.UnitAskedForResupply( this, ERT_HUMAN_RESUPPLY, Size() != pStats->members.size() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::AddNewUnitToSlot( CSoldier *pUnit, const int nSlot, const bool bSendToWorld )
{
	const int nPos = nUnits;
	AddUnit( pUnit, nPos );

	units[nPos].nSlotInStats = nSlot;
	units[nPos].geoms.resize( pStats->formations.size() );

	const CVec2 vStandartDir1( 0, -1 );
	const CVec2 vStandartDir2( 0, 1 );

	for ( int i = 0; i < pStats->formations.size(); ++i )
	{
		for ( int j = 0; j < Size(); ++j )
		{
			const int nSlot = units[j].nSlotInStats;
			const CVec2 vShift = pStats->formations[i].order[nSlot].vPos;
			units[j].geoms[i].vForm2Unit = vShift ^ vStandartDir1;
			units[j].geoms[i].fUnitProj = vShift * vStandartDir2;
			units[j].geoms[i].dir = pStats->formations[i].order[nSlot].nDir;
		}
	}
	MoveGeometries2Center();

	if ( bSendToWorld )
		updater.Update( ACTION_NOTIFY_NEW_FORMATION, pUnit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormation::GetUnitCoord( const BYTE cSlot ) const
{ 
	NI_ASSERT_T( cSlot < Size(), "Wrong unit position" ); 
	return GetCenter() + ( GetDirVector() ^ units[cSlot].geoms[nCurGeometry].vForm2Unit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CFormation::GetUnitSlotInStats( const BYTE cSlot ) const
{
	NI_ASSERT_T( cSlot < Size(), "Wrong unit position" ); 
	NI_ASSERT_T( units[cSlot].nSlotInStats != -1, "Non initialized stats-slot of unit" );
	return units[cSlot].nSlotInStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetUnitToPos( const BYTE cPos, class CSoldier *pUnit )
{
	units[cPos].pUnit = pUnit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormation::GetUnitShift( const BYTE cSlot ) const
{
	return GetDirVector() ^ units[cSlot].geoms[nCurGeometry].vForm2Unit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetUnitLineShift( const BYTE cSlot ) const
{
	NI_ASSERT_T( cSlot >= 0 && cSlot < Size(), "Wrong unit's slot" );
	NI_ASSERT_T( units[cSlot].pUnit && units[cSlot].pUnit->IsAlive(), "Wrong unit in formation" );
	return GetDirVector() * ( units[cSlot].pUnit->GetCenter() - GetCenter() ) - units[cSlot].geoms[nCurGeometry].fUnitProj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::MoveGeometries2Center()
{
	const CVec2 vDir2( 0, -1 );
	const CVec2 vDir( 0, 1 );
	
	for ( int i = 0; i < pStats->formations.size(); ++i )
	{
		CVec2 newCenter( VNULL2 );	
		for ( int j = 0; j < nUnits; ++j )
			newCenter += vDir ^ units[j].geoms[i].vForm2Unit;

		newCenter /= nUnits;

		for ( int j = 0; j < nUnits; ++j )
		{
			const CVec2 vShift = -1.0f * newCenter + (vDir ^ units[j].geoms[i].vForm2Unit);

			units[j].geoms[i].vForm2Unit = vShift ^ vDir2;
			units[j].geoms[i].fUnitProj = vShift * vDir;
		}
	}

	geomInfo.clear();
	geomInfo.resize( pStats->formations.size() );
	for ( int i = 0; i < pStats->formations.size(); ++i )
	{
		for ( int j = 0; j < nUnits; ++j )
		{
			geomInfo[i].fMaxUnitProj = Max( float( fabs( units[j].geoms[i].fUnitProj ) ), float( geomInfo[i].fMaxUnitProj ) );
			geomInfo[i].fRadius = Max( fabs( vDir ^ units[j].geoms[i].vForm2Unit ), geomInfo[i].fRadius );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::InitGeometries()
{
	units.resize( pStats->members.size() );
	
	for ( int i = 0; i < pStats->members.size(); ++i )
	{
		units[i].geoms.resize( pStats->formations.size() );
		units[i].pUnit = 0;
	}

	const CVec2 vStandartDir1( 0, -1 );
	const CVec2 vStandartDir2( 0, 1 );
	for ( int i = 0; i < pStats->formations.size(); ++i )
	{
		for ( int j = 0; j < pStats->formations[i].order.size(); ++j )
		{
			const CVec2 vShift = pStats->formations[i].order[j].vPos;
			units[j].geoms[i].vForm2Unit = vShift ^ vStandartDir1;
			units[j].geoms[i].fUnitProj = vShift * vStandartDir2;
			units[j].geoms[i].dir = pStats->formations[i].order[j].nDir;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Init( const SSquadRPGStats *_pStats, const CVec2 &center, const int z, const WORD dir, const int dbID )
{
	bCanBeResupplied = true;
	pStats = _pStats;
	id = ::units.AddFormation( this );
	cPlayer = 0xff;
	bWaiting = false;
	availCommands.SetSize( 64 );
	timeToCamouflage = 0;
	nCurGeometry = 0;
	fPass = 1;
	nUnits = 0;
	bDisabled = false;
	availCommands.SetData( ACTION_COMMAND_DISBAND_FORMATION );
	availCommands.SetData( ACTION_COMMAND_ROTATE_TO );
	
	geomInfo.resize( pStats->formations.size() );
	eInsideType = EOIO_NONE;
	pObjInside = 0;
	fMaxFireRange = 0.0f;
	nVirtualUnits = 0;
	bBoredInMoveFormationSent = false;
	lastBoredInMoveFormationCheck = 0;
	bWithMoraleOfficer = false;

	CAIUnit::CheckCmdsSize( GetID() );
	CFormationCenter::Init( center, z, dir, dbID );

	InitGeometries();
	
	theGroupLogic.RegisterSegments( this, pAILogic->IsFirstTime(), true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::CheckForMoveFormationBored()
{
	if ( GetStats()->formations[GetCurGeometry()].type == SSquadRPGStats::SFormation::MOVEMENT )
	{
		bool bShouldSendAck = false;
		if ( !IsIdle() )
		{
			int i = 0;
			while ( i < Size() && 
						  fabs2( (*this)[i]->GetCenter() - (*this)[i]->GetUnitPointInFormation() ) <= sqr( float(SConsts::TILE_SIZE) ) )
				++i;

			bShouldSendAck = i >= Size();
		}

		if ( bShouldSendAck )
		{
			if ( !bBoredInMoveFormationSent )
			{
				bBoredInMoveFormationSent = true;
				RegisterAsBored( ACK_BORED_INFANTRY_TRAVEL );
			}
		}
		else if ( bBoredInMoveFormationSent )
		{
			bBoredInMoveFormationSent = false;
			UnRegisterAsBored( ACK_BORED_INFANTRY_TRAVEL );
		}
	} 
	else if ( bBoredInMoveFormationSent )
	{
		bBoredInMoveFormationSent = false;
		UnRegisterAsBored( ACK_BORED_INFANTRY_TRAVEL );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Segment()
{
	if ( !bDisabled )
	{
		CQueueUnit::Segment();

		if ( !IsStopped() )
		{
			CFormationCenter::Segment();

			maxSpeed = 1000000.0f;
			for ( int i = 0; i < Size(); ++i )
			{
				const float fSpeedLen = (*this)[i]->GetMaxSpeedHere( (*this)[i]->GetCenter() );
				if ( fSpeedLen < maxSpeed )
					maxSpeed = fSpeedLen;
			}
		}

		// acknowledgements
		if ( curTime - lastBoredInMoveFormationCheck >= 2000 )
		{
			lastBoredInMoveFormationCheck = curTime;
			CheckForMoveFormationBored();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsStopped() const
{
	return GetCurPath() == 0 || GetCurPath()->IsFinished();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::GetNextPositions( const BYTE cPos, std::list<SVector> *pTiles ) const
{
	GetNextTiles( pTiles );
	const CVec2 unitShift = GetUnitShift( cPos );

	for ( std::list<SVector>::iterator iter = pTiles->begin(); iter != pTiles->end(); ++iter )
		(*iter) = AICellsTiles::GetTile( AICellsTiles::GetPointByTile( *iter ) + unitShift );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 CFormation::GetFarUnitPos( const BYTE cPos )
{
	return GetNearFormationPos() + GetUnitShift( cPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IStatesFactory* CFormation::GetStatesFactory() const
{
	return CFormationStatesFactory::Instance();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsIdle() const
{
	if ( IsStopped() )
	{
		for ( int i = 0; i < Size(); ++i )
		{
			if ( !units[i].pUnit->IsIdle() )
				return false;
		}

		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::BalanceCenter()
{
	if ( Size() > 0 ) 
	{
		CVec2 vNewCenter( VNULL2 );
		for ( int i = 0; i < Size();	++i )
			vNewCenter += units[i].pUnit->GetCenter();

		vNewCenter /= Size();
		SetNewCoordinates( CVec3( vNewCenter, GetZ() ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::StopUnit()
{
	StopFormationCenter();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::StopTurning()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::CanCommandBeExecuted( class CAICommand *pCommand )
{
	const int &nCmd = pCommand->ToUnitCmd().cmdType;
	// ��������� �� ����������� ���������� ������ user �������
	if ( nCmd < 1000 )
	{
		NI_ASSERT_T( nCmd >= 0 && nCmd < availCommands.GetSize(), NStr::Format( "Wrong command ( %d )\n", nCmd ) );
		if ( !availCommands.GetData( nCmd ) )
			return false;
	}

	if ( nCmd == ACTION_COMMAND_FORM_FORMATION )
	{
		// ������ �������, ���� �� single formation
		if ( Size() > 1 )
			return false;

		// ������ �������, ���� ������ �� ���� ���������
		CFormation *pOldFormation = (*this)[0]->GetMemFormation();
		if ( pOldFormation == 0 )
			return false;

		// ������ �������, ���� ���-�� � ����������
		for ( int i = 0; i < pOldFormation->Size(); ++i )
		{
			if ( (*pOldFormation)[i]->IsInTransport() )
				return false;
		}
	}
	
	if ( nCmd == ACTION_COMMAND_DISBAND_FORMATION )
	{
		if ( Size() <= 1 )
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::CanCommandBeExecutedByStats( int nCmd ) const
{
	// ��������� �� ����������� ���������� ������ user �������
	if ( nCmd < 1000 )
	{
		NI_ASSERT_T( nCmd >= 0 && nCmd < availCommands.GetSize(), NStr::Format( "Wrong command ( %d )\n", nCmd ) );
		if ( !availCommands.GetData( nCmd ) )
			return false;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::CanCommandBeExecutedByStats( class CAICommand *pCommand )
{
	return CanCommandBeExecutedByStats( pCommand->ToUnitCmd().cmdType );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsEveryUnitInTransport() const
{
	bool bAllInside = true;
	int nSoldiers = Size();
	for ( int i=0; i< nSoldiers && bAllInside ; ++i )
		bAllInside = operator[](i)->IsInTransport();
	return bAllInside;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsMemberResting( CSoldier *pSoldier ) const
{
	IUnitState *pState = pSoldier->GetState();
	return
		pState && IsRestState( pState->GetName() ) && pSoldier->IsEmptyCmdQueue();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsEveryUnitResting() const
{
	int i = 0;
	while ( i < Size() && IsMemberResting( units[i].pUnit ) )
		++i;

	return i == Size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CFormation::GetGun( const int n ) const
{
	return units[guns[n].nUnit].pUnit->GetGun( guns[n].nUnitGun );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicGun* CFormation::ChooseGunForStatObj( class CStaticObject *pObj, NTimer::STime *pTime )
{
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetSightRadius() const
{
	float fResult = 0;
	for ( int i = 0; i < Size(); ++i )
		fResult = Max( fResult, units[i].pUnit->GetSightRadius() );

	return fResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Disable() 
{ 
	bDisabled = true;
	theSupremeBeing.UnitAskedForResupply( this, ERT_HUMAN_RESUPPLY, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Enable() 
{ 
	bDisabled = false; 
	if ( units.size() != pStats->members.size() )
		theSupremeBeing.UnitAskedForResupply( this, ERT_HUMAN_RESUPPLY, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::DelUnit( const BYTE cPos )
{
	// ������ �� ������
	for ( int i = cPos; i < nUnits - 1; ++i )
		units[i] = units[i+1];

	--nUnits;

	geomInfo.clear();
	geomInfo.resize( pStats->formations.size() );
	guns.clear();
	availCommands.SetZero();
	availCommands.SetData( ACTION_COMMAND_DISBAND_FORMATION );
	AddAvailCmd( ACTION_COMMAND_FOLLOW );
	AddAvailCmd( ACTION_COMMAND_ROTATE_TO );

	const int nCurUnitsNum = nUnits;
	nUnits = 0;
	fMaxFireRange = 0.0f;

	bWithMoraleOfficer = false;
	for ( int i = 0; i < nCurUnitsNum; ++i )
		AddUnit( units[i].pUnit, i );

	if ( nUnits > 0 )
		MoveGeometries2Center();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::WasHitNearUnit()
{
	if ( pStats->formations[nCurGeometry].changesByEvent[SSquadRPGStats::HIT_NEAR] != -1 && 
			 pStats->formations[nCurGeometry].changesByEvent[SSquadRPGStats::HIT_NEAR] != nCurGeometry )
		ChangeGeometry( pStats->formations[nCurGeometry].changesByEvent[SSquadRPGStats::HIT_NEAR] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetSelectable	( bool bSelectable )
{
	CCommonUnit::SetSelectable( bSelectable );
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SetSelectable( bSelectable );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::ChangePlayer( const BYTE _cPlayer )
{
	cPlayer = _cPlayer;
	
	for ( int i = 0; i < Size(); ++i )
	{
		if ( units[i].pUnit->GetPlayer() != cPlayer )
		units[i].pUnit->ChangePlayer( cPlayer );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetPlayerForEditor( const int nPlayer )
{
	cPlayer = nPlayer;

	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SetPlayerForEditor( nPlayer );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD CFormation::GetUnitDir( const BYTE cSlot ) const
{
	NI_ASSERT_T( cSlot < Size(), "Wrong number of cSlot" );

	return GetDir() + units[cSlot].geoms[nCurGeometry].dir;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CFormation::IsVisible( const BYTE party ) const
{
	for ( int i = 0; i < Size(); ++i )
	{
		if ( units[i].pUnit->IsVisible( party ) )
			return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CFormation::CanShootToPlanes() const
{
	for ( int i = 0; i < Size(); ++i )
	{
		if ( (*this)[i]->CanShootToPlanes() )
			return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetAmbush()
{
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SetAmbush();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::RemoveAmbush()
{
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->RemoveAmbush();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetCamoulfage()
{
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SetCamoulfage();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::RemoveCamouflage( ECamouflageRemoveReason eReason )
{
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->RemoveCamouflage( eReason );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const NTimer::STime CFormation::GetTimeToCamouflage() const
{
	return timeToCamouflage;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::UpdateArea( const EActionNotify eAction )
{
	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->UpdateArea( eAction );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::PrepareToDelete()
{
	GetState()->TryInterruptState( 0 );

	theGroupLogic.DelUnitFromGroup( this );
	theGroupLogic.DelUnitFromSpecialGroup( this );
	DelCmdQueue( GetID() );
	
	pAILogic->ToGarbage( this );
	::units.DelFormation( this );
	
	CDeadUnit *pDeadUnit = new CDeadUnit( this, 0, ACTION_NOTIFY_NONE, -1, false );
	updater.Update( ACTION_NOTIFY_DISSAPEAR_UNIT, pDeadUnit );

	theSupremeBeing.UnitDied( this );
	theGroupLogic.UnregisterSegments( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Disappear()
{
	// �.�. ��������� ���� �������� ������� ��� �������, �� ����� �������� �������� ����������� ������ �����
	if ( Size() != 0 )
	{
		while ( Size() != 0 )
			units[0].pUnit->Disappear();
	}
	else
		PrepareToDelete();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::Die( const bool fromExplosion, const float fDamage )
{
	// �.�. ��������� ���� �������� ������� ��� �������, �� ����� �������� �������� ����������� ������ �����	
	if ( Size() != 0 )
	{
		while ( Size() != 0 )
			units[0].pUnit->Disappear();
	}
	else
	{
		PrepareToDelete();
		theStatistics.UnitDead( this );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::SendAlongPath( IStaticPath *pStaticPath, const CVec2 &vShift, bool bSmoothTurn )
{
	for ( int i = 0; i < Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), units[i].pUnit, false );
	CFormationCenter::SendAlongPath( pStaticPath, vShift, bSmoothTurn );

	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SendAlongPath( 0, VNULL2 );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::SendAlongPath( IPath *pPath )
{
	for ( int i = 0; i < Size(); ++i )
		theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_STOP ), units[i].pUnit, false );
	CFormationCenter::SendAlongPath( pPath );

	for ( int i = 0; i < Size(); ++i )
		units[i].pUnit->SendAlongPath( 0, VNULL2 );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetGeometryPropertiesToSoldier( CSoldier *pSoldier, const bool bChangeWarFog )
{
	if ( pStats->formations[nCurGeometry].cLieFlag == 1 )
		pSoldier->StandUp();
	else if ( pStats->formations[nCurGeometry].cLieFlag == 2 )
		pSoldier->LieDown();

	if ( bChangeWarFog )
		pSoldier->ChangeWarFogState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::ChangeGeometry( const int nGeometry )
{
	NI_ASSERT_T( nGeometry < pStats->formations.size(), NStr::Format("Wrong geometry (%d) for squad \"%s\"", nGeometry, pStats->szParentName.c_str()) );
	const float fOldSightMultipier = GetSightMultiplier();
	nCurGeometry = nGeometry;

	const bool bChangeWarfog = ( fOldSightMultipier != GetSightMultiplier() );
	for ( int i = 0; i < Size(); ++i )
		SetGeometryPropertiesToSoldier( (*this)[i], bChangeWarfog );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CFormation::GetNGeometries() const
{
	return pStats->formations.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetCurSpeedBonus() const
{
	return pStats->formations[nCurGeometry].fSpeedBonus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetDispersionBonus() const
{
	return 
		pStats->formations[nCurGeometry].fDispersionBonus * theDifficultyLevel.GetDispersionCoeff( theDipl.GetNParty( GetPlayer() ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetRelaxTimeBonus() const
{
	return pStats->formations[nCurGeometry].fRelaxTimeBonus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetFireRateBonus() const
{
	return pStats->formations[nCurGeometry].fFireRateBonus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetCoverBonus() const
{
	return pStats->formations[nCurGeometry].fCoverBonus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsAllowedLieDown() const
{
	return pStats->formations[nCurGeometry].cLieFlag != 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsAllowedStandUp() const
{
	return pStats->formations[nCurGeometry].cLieFlag != 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	CFormation *pFormation;
	CAICommand *pCmd;
	float fDist;

	SEdge() : pFormation( 0 ), pCmd( 0 ), fDist( 0.0f ) { }

	operator float() const { return fDist; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsLoadCommand( const EActionCommand &cmd )
{
	return cmd == ACTION_COMMAND_LOAD || cmd == ACTION_COMMAND_ENTER;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGetNAvailableSeats
{
	const int operator()( CMilitaryCar *pCar ) { return pCar->GetNAvailableSeats(); }
	const int operator()( CBuilding *pBuilding ) { return pBuilding->GetNFreePlaces(); }
	const int operator()( CEntrenchment *pEntrenchment ) { return 1000000; }
	const int operator()( IRefCount *pObj ) { NI_ASSERT_T( false, NStr::Format( "Unknown object (%s) to get number of seats", typeid( *pObj ).name() ) ); return 0; }
};
struct SGetLoadPoint
{
	template<class T>
	const CVec2 operator()( T *pObj ) { return pObj->GetCenter(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TResult>
const TResult GetLoadInfo( CAICommand *pCommand, T &functor, TResult* )
{
	SAIUnitCmd &cmd = pCommand->ToUnitCmd();
	switch ( cmd.cmdType )
	{
		case ACTION_COMMAND_LOAD: return functor( checked_cast_ptr<CMilitaryCar*>(cmd.pObject) );
		case ACTION_COMMAND_ENTER:
			{
				CStaticObject *pObj = checked_cast_ptr<CStaticObject*>(cmd.pObject);
				switch ( pObj->GetObjectType() )
				{
					case ESOT_ENTRENCHMENT: return functor( checked_cast<CEntrenchment*>( pObj ) );
					case ESOT_ENTR_PART:		return functor( checked_cast<CEntrenchmentPart*>(pObj)->GetOwner() );
					case ESOT_BUILDING:			return functor( checked_cast<CBuilding*>(pObj) );
					default: NI_ASSERT_T( false, NStr::Format( "Can't enter to object of type %d", pObj->GetObjectType() ) );
				}
			}
		default: NI_ASSERT_T( false, NStr::Format( "Unknown load command (%d)", cmd.cmdType ) ); TResult res; return res;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::ProcessLoadCommand( CAICommand *pCommand, bool bPlaceInQueue )
{
	// not distributed
	if ( pCommand->GetFlag() == -1 )
	{
		pCommand->SetFlag( bPlaceInQueue ? 1 : 2 );
		
		if ( !bPlaceInQueue )
		{
			CFormationCenter::UnitCommand( pCommand, bPlaceInQueue, false );
			return;
		}
		
		std::set<int> objects;
		std::unordered_map<int, CAICommand*> object2Command;
		std::unordered_set<int> formations;

		const int nGroup = GetNGroup();
		bool bPushFront = false;

		if ( pCommand->ToUnitCmd().pObject == 0 || dynamic_cast_ptr<CLinkObject*>(pCommand->ToUnitCmd().pObject) == 0 )
			return;
		
		const int nObjNowUniqueID = (checked_cast_ptr<CLinkObject*>(pCommand->ToUnitCmd().pObject))->GetUniqueId();
		object2Command[nObjNowUniqueID] = pCommand;
		objects.insert( nObjNowUniqueID );
		for ( int i = theGroupLogic.BeginGroup( nGroup ); i != theGroupLogic.EndGroup(); i = theGroupLogic.Next( i ) )
		{
			CCommonUnit *pUnit = theGroupLogic.GetGroupUnit( i );
			if ( pUnit->IsFormation() )
			{
				CFormation *pFormation = static_cast<CFormation*>(pUnit);
				const int nUniqueID = pFormation->GetUniqueId();
//				if ( pFormation->IsFree() )
				{
					formations.insert( nUniqueID );					

					if ( pFormation->GetCurCmd() && IsLoadCommand( pFormation->GetCurCmd()->ToUnitCmd().cmdType ) )
					{
						const int nObjectID = checked_cast_ptr<CLinkObject*>(pFormation->GetCurCmd()->ToUnitCmd().pObject)->GetUniqueId();
						objects.insert( nObjectID );
						object2Command[nObjectID] = pFormation->GetCurCmd();
						bPushFront = bPlaceInQueue;
					}
					if ( !bPlaceInQueue || pFormation->GetState() && IsRestState( pFormation->GetState()->GetName() ) )
						bPushFront = bPlaceInQueue;

					for ( int j = pFormation->GetBeginCmdsIter(); j != pFormation->GetEndCmdsIter(); j = pFormation->GetNextCmdsIter( j ) )
					{
						CAICommand *pCommand = pFormation->GetCommand( j );
						if ( pCommand && IsLoadCommand( pCommand->ToUnitCmd().cmdType) )
						{
							const int nObjectID = checked_cast_ptr<CLinkObject*>(pCommand->ToUnitCmd().pObject)->GetUniqueId();
							objects.insert( nObjectID );
							object2Command[nObjectID] = pCommand;
							bPushFront = bPlaceInQueue;
						}
						else
							break;
					}
				}
			}
		}

		std::vector<SEdge> edges( objects.size() * formations.size() );
		std::unordered_map<int, int> availableSeats;
		int cnt = 0;
		for ( std::set<int>::iterator iterObjects = objects.begin(); iterObjects != objects.end(); ++iterObjects )
		{
			SGetNAvailableSeats availSeatsFunctor;
			availableSeats[*iterObjects] = GetLoadInfo( object2Command[*iterObjects], availSeatsFunctor, (int*)0 );
			
			SGetLoadPoint loadPointFunctor;
			for ( std::unordered_set<int>::iterator iterForms = formations.begin(); iterForms != formations.end(); ++iterForms )
			{
				NI_ASSERT_T( cnt < edges.size(), NStr::Format( "Wrong cnt (%d), size (%d)", cnt, edges.size() ) );
				edges[cnt].pFormation = ::GetObjectByUniqueIdSafe<CFormation>( *iterForms );
				edges[cnt].pCmd = object2Command[*iterObjects];
				edges[cnt].fDist = 
					fabs2( edges[cnt].pFormation->GetCenter() - GetLoadInfo( edges[cnt].pCmd, loadPointFunctor, (CVec2*)0 ) );
				cnt++;
			}
		}
		NI_ASSERT_T( cnt == edges.size(), NStr::Format( "Wrong cnt (%d), size (%d)", cnt, edges.size() ) );

		std::sort( edges.begin(), edges.end() );

		bool bCommandGiven = false;
		for ( int i = 0; i < cnt; ++i )
		{
			const int nObjUniqueId = (static_cast_ptr<CLinkObject*>(edges[i].pCmd->ToUnitCmd().pObject))->GetUniqueId();
			if ( formations.find( edges[i].pFormation->GetUniqueId() ) != formations.end() &&
					 availableSeats[nObjUniqueId] >= edges[i].pFormation->Size() )
			{
				formations.erase( edges[i].pFormation->GetUniqueId() );
				availableSeats[nObjUniqueId] -= edges[i].pFormation->Size();

				if ( bPushFront && edges[i].pFormation->IsFree() )
					edges[i].pFormation->PushFrontUnitCommand( new CAICommand( *(edges[i].pCmd) ) );
				else
					edges[i].pFormation->CFormationCenter::UnitCommand( new CAICommand( *(edges[i].pCmd) ), bPlaceInQueue, false );

				bCommandGiven = true;
			}
		}
		if ( !bCommandGiven )
		{
			if ( !bPlaceInQueue ||
					 GetCurCmd() && !IsLoadCommand( GetCurCmd()->ToUnitCmd().cmdType ) ||
					 GetState() && IsRestState( GetState()->GetName() ) )
				SendAcknowledgement( pCommand, ACK_NEGATIVE );
		}
	}
	else
		CFormationCenter::UnitCommand( pCommand, bPlaceInQueue, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand )
{
	if ( !bPlaceInQueue && Size() == 1 && pCommand->ToUnitCmd().cmdType < 1000 )
		(*this)[0]->SetWait2FormFlag( false );
	
	FreezeByState( false );

	if ( pCommand->ToUnitCmd().cmdType == ACTION_COMMAND_PARADE )
		CPtr<IUnitState> pParadeState = CFormationParadeState::Instance( this, pCommand->ToUnitCmd().fNumber );
	else
	{
		if ( !bOnlyThisUnitCommand )
		{
			GetBehaviour().moving = SBehaviour::EMRoaming;

			for ( int i = 0; i < Size(); ++i )
			{
				CSoldier *pSoldier = (*this)[i];
				pSoldier->GetBehaviour().moving = SBehaviour::EMRoaming;
				pSoldier->UnlockTiles();
			}
		}

		if ( !bOnlyThisUnitCommand && IsLoadCommand( pCommand->ToUnitCmd().cmdType ) )
			ProcessLoadCommand( pCommand, bPlaceInQueue );
		else
			CFormationCenter::UnitCommand( pCommand, bPlaceInQueue, bOnlyThisUnitCommand );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::DelUnit( class CSoldier *pUnit )
{
	int i = 0;
	while ( i < Size() && (*this)[i] != pUnit )
		++i;

	NI_ASSERT_T( i < Size(), "Unit not found" );

	DelUnit( i );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetFree() 
{ 
	eInsideType = EOIO_NONE; 
	pObjInside = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetInBuilding( class CBuilding *pBuilding ) 
{ 
	eInsideType = EOIO_BUILDING; 
	pObjInside = pBuilding;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetInTransport(  class CMilitaryCar *pUnit ) 
{ 
	eInsideType = EOIO_TRANSPORT; 
	pObjInside = pUnit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetInEntrenchment( class CEntrenchment *pEntrenchment ) 
{ 
	eInsideType = EOIO_ENTRENCHMENT; 
	pObjInside = pEntrenchment;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding* CFormation::GetBuilding() const
{
	NI_ASSERT_T( IsInBuilding(), "Soldier isn't in a building" );
	return static_cast<CBuilding*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CEntrenchment* CFormation::GetEntrenchment() const
{
	NI_ASSERT_T( IsInEntrenchment(), "Soldier isn't in entrenchment" );
	return static_cast<CEntrenchment*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMilitaryCar* CFormation::GetTransportUnit() const
{
	NI_ASSERT_T( IsInTransport(), "Soldier isn't in a transport" );
	return static_cast<CMilitaryCar*>( pObjInside );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::GetNewUnitInfo( SNewUnitInfo *pNewUnitInfo )
{
	pNewUnitInfo->dir = GetDir();
	pNewUnitInfo->center = GetCenter();
	pNewUnitInfo->z = GetZ();
	
	pNewUnitInfo->dbID = GetDBID();
	pNewUnitInfo->eDipl = theDipl.GetDiplStatus( theDipl.GetMyNumber(), GetPlayer() );
	pNewUnitInfo->nFrameIndex = GetScenarioUnit() ? GetScenarioUnit()->GetScenarioID() : -1;
	pNewUnitInfo->pObj = this;
	pNewUnitInfo->fHitPoints = 1.0f;
	pNewUnitInfo->fMorale = 1.0f;
	pNewUnitInfo->fResize = 1.0f;
	pNewUnitInfo->nPlayer = GetPlayer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SendAcknowledgement( CAICommand *pCommand, EUnitAckType ack, bool bForce )
{
	if ( Size() > 0 && ( bForce || pCommand && !pCommand->IsFromAI() ) )
		(*this)[0]->SendAcknowledgement( pCommand, ack, theDipl.GetMyNumber() == GetPlayer() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SendAcknowledgement( EUnitAckType ack, bool bForce )
{
	SendAcknowledgement( GetCurCmd(), ack, bForce );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetSightMultiplier() const
{
	return pStats->formations[nCurGeometry].fVisibleBonus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitAckType CFormation::GetGunsRejectReason() const
{
	EUnitAckType eBestReason = EUnitAckType( ACK_NONE );

	for ( int i = 0; i < Size(); ++i )
	{
		EUnitAckType eReason = (*this)[i]->GetGunsRejectReason();
		if ( eReason != ACK_NONE && int(eReason) < int(eBestReason) )
			eBestReason = eReason;
	}
	
	return ( eBestReason == ACK_NONE ) ? ACK_NEGATIVE : eBestReason;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CFormation::GetVirtualUnitSlotInStats( const int nVirtualUnit ) const
{
	NI_ASSERT_T( nVirtualUnit < VirtualUnitsSize(), NStr::Format( "Wrong number of virtual unit (%d), size of virtual units (%d)", nVirtualUnit, VirtualUnitsSize() ) );
	return virtualUnits[nVirtualUnit].nSlotInStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////v
void CFormation::AddVirtualUnit( CSoldier *pSoldier, const int nSlotInStats )
{
	if ( virtualUnits.size() <= nVirtualUnits )
		virtualUnits.resize( nVirtualUnits + 2 );

	virtualUnits[nVirtualUnits].pSoldier = pSoldier;
	virtualUnits[nVirtualUnits].nSlotInStats = nSlotInStats;

	++nVirtualUnits;

	pSoldier->SetVirtualFormation( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::DelVirtualUnit( CSoldier *pSoldier )
{
	int i = 0;
	while ( i < VirtualUnitsSize() && virtualUnits[i].pSoldier != pSoldier )
		++i;

	NI_ASSERT_T( i < VirtualUnitsSize(), "Virtual unit not found" );

	for ( int j = i; j < nVirtualUnits - 1; ++j )
		virtualUnits[j] = virtualUnits[j+1];

	virtualUnits[nVirtualUnits-1].pSoldier = 0;
	--nVirtualUnits;

	pSoldier->SetVirtualFormation( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::MakeVirtualUnitReal( CSoldier *pSoldier )
{
	int i = 0;
	while ( i < VirtualUnitsSize() && virtualUnits[i].pSoldier != pSoldier )
		++i;

	NI_ASSERT_T( i < VirtualUnitsSize(), "Virtual unit not found" );

	AddNewUnitToSlot( virtualUnits[i].pSoldier, virtualUnits[i].nSlotInStats, true );
	DelVirtualUnit( pSoldier );

	SetGeometryPropertiesToSoldier( pSoldier, true );

	theSupremeBeing.UnitAskedForResupply( this, ERT_HUMAN_RESUPPLY, Size() != pStats->members.size() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::UnRegisterAsBored( const enum EUnitAckType eBoredType )
{
	for ( int i = 0; i < Size(); ++i )
	{
		(*this)[i]->UnRegisterAsBored( eBoredType );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::RegisterAsBored( const enum EUnitAckType eBoredType )
{
	for ( int i = 0; i < Size(); ++i )
	{
		(*this)[i]->RegisterAsBored( eBoredType );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::SetCarryedMortar( class CAIUnit *pMortar )
{
	mortar.Init( pMortar );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::HasMortar() const
{
	return mortar.HasMortar();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CFormation::InstallCarryedMortar()
{
	return mortar.CreateMortar( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::ResetTargetScan()
{
	for ( int i = 0; i < Size(); ++i )
		(*this)[i]->ResetTargetScan();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CFormation::AnalyzeTargetScan(	CAIUnit *pCurTarget, const bool bDamageUpdated, bool bScanForObstacles, IRefCount *pCheckBuilding )
{
	BYTE cResult = 0;
	for ( int i = 0; i < Size(); ++i )
	{
		if ( IsMemberResting( (*this)[i] ) )
		{
			cResult |= (*this)[i]->AnalyzeTargetScan( pCurTarget, bDamageUpdated, pCheckBuilding );
		}
		else
		{
			cResult |= 1;
		}
	}

	return cResult;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::LookForTarget( CAIUnit *pCurTarget, const bool bDamageUpdated, CAIUnit **pBestTarget, CBasicGun **pGun )
{
	*pBestTarget = 0;
	*pGun = 0;
	for ( int i = 0; i < Size(); ++i )
	{
		(*this)[i]->LookForTarget( pCurTarget, bDamageUpdated, pBestTarget, pGun );
		if ( *pBestTarget != 0 )
			return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CFormation::GetAIClass() const 
{ 
	return AI_CLASS_HUMAN;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CFormation::GetPriceMax() const
{
	float fPrice = 0;
	const int nOrderSize = pStats->members.size();
	for ( int i = 0; i < nOrderSize; ++i )
	{
		fPrice += pStats->members[i]->fPrice;
	}
	return fPrice;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFormation::IsAlive() const 
{ 
	return Size() != 0 || VirtualUnitsSize() != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CFormation::GetTargetScanRadius()
{
	float fRadius = 0.0f;
	for ( int i = 0; i < Size(); ++i )
	{
		const float fSoldierScanRadius = (*this)[i]->GetTargetScanRadius();
		if ( fSoldierScanRadius > fRadius )
			fRadius = fSoldierScanRadius;
	}

	return fRadius;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFormation::FreezeByState( const bool bFreeze )
{
	if ( !bFreeze )
	{
		for ( int i = 0; i < Size(); ++i )
			(*this)[i]->FreezeByState( bFreeze );
	}

	CFormationCenter::FreezeByState( bFreeze );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
