#include "StdAfx.h"

#include "GeneralIntendant.h"

#include "STaticObjects.h"
#include "Building.h"
#include "CommonUnit.h"
#include "AIUnit.h"
#include "GroupLogic.h"
#include "Diplomacy.h"
#include "GeneralConsts.h"
#include "GeneralHelper.h"
#include "General.h"
#include "UnitStates.h"
#include "AIStaticMap.h"
#include "TimeCounter.h"
extern CTimeCounter timeCounter;
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern CGroupLogic theGroupLogic;
extern CStaticObjects theStatObjs;
extern CSupremeBeing theSupremeBeing;
extern NTimer::STime curTime;
BASIC_REGISTER_CLASS( CGeneralIntendant );
static const unsigned int INTENDANT_TASKS_PER_SEGMENT = 5;
CGeneralTaskToDefendStorage::CWaitForChangePlayer::CWaitForChangePlayer( CBuildingStorage * pStorage, CGeneralTaskToDefendStorage * pMainTask, const int nParty )
	: pStorage( pStorage ), pMainTask( pMainTask ), nParty( nParty )
{
}
bool CGeneralTaskToDefendStorage::CWaitForChangePlayer::IsTimeToRun() const
{
	return theDipl.GetNParty( pStorage->GetPlayer() ) == nParty ;
}
void CGeneralTaskToDefendStorage::CWaitForChangePlayer::Run()
{
	pMainTask->Recaptured();
}
CGeneralTaskToDefendStorage::CGeneralTaskToDefendStorage ( CBuildingStorage * pStorage, const int nParty ) 
: pStorage( pStorage ),
  fSeverity( 0 ),
	nParty( nParty ),
	wRequestID( 0 )
{  
	eState = EDI_FRIEND == theDipl.GetDiplStatusForParties( theDipl.GetNParty(pStorage->GetPlayer()), nParty ) ?
					 TS_OPERATE : TS_RECAPTURE ;
}
void CGeneralTaskToDefendStorage::AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit )
{
	if ( !bInit && fSeverity < fMaxSeverity )
	{
		switch( eState )
		{
		case TS_RECAPTURE:
			if ( !wRequestID )
				wRequestID = pManager->RequestForSupport( pStorage->GetCenter(), FT_RECAPTURE_STORAGE );
			break;
		case TS_REPAIR:
			if ( !IsValidObj(pRepairTransport) )
				pManager->EnumWorkers( FT_TRUCK_REPAIR_BUILDING, this );
			break;
		}
	}
}
bool CGeneralTaskToDefendStorage::EnumWorker( class CCommonUnit *pUnit, const enum EForceType eType )
{
	switch( eType )
	{
	case FT_TRUCK_REPAIR_BUILDING:
		pRepairTransport = pUnit;
		theGroupLogic.InsertUnitCommand( SAIUnitCmd(ACTION_COMMAND_REPEAR_OBJECT, pStorage), pRepairTransport );
		fSeverity = 0;

		return false;
	default:
		NI_ASSERT_T(false, NStr::Format( "didn't asked worker of type %i", eType ) );

	}
	return false;
}
int CGeneralTaskToDefendStorage::NeedNBest( const enum EForceType eType ) const 
{ 
	return (eType == FT_TRUCK_REPAIR_BUILDING ? 1 : 0);
}
float CGeneralTaskToDefendStorage::EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType eType ) const 
{ 
	switch( eType )
	{
	case FT_TRUCK_REPAIR_BUILDING:
		{
			const float fDist = fabs2( pStorage->GetCenter() - pUnit->GetCenter() );
			if ( fDist == 0.0f )
				return 1;
			return 1.0f / fDist; 
		}

		break;
	}
	return 0;
}
bool CGeneralTaskToDefendStorage::EvaluateWorker( CCommonUnit * pUnit, const enum EForceType eType ) const
{
	switch( eType )
	{
	case FT_TRUCK_REPAIR_BUILDING:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_REPEAR_OBJECT );
		break;
	}
	return false;
}
void CGeneralTaskToDefendStorage::ReleaseWorker( ICommander *pManager, const float fMinSeverity )
{
	if ( wRequestID && eState == TS_FINISH_RECAPTURE )
	{
		pManager->CancelRequest( wRequestID, FT_RECAPTURE_STORAGE );
		wRequestID = 0;
	}

	if ( IsValidObj( pRepairTransport ) && 
			( eState == TS_FINISHED || pStorage->GetHitPoints() != 0 ) )
	{
		pManager->Give( pRepairTransport );
		pRepairTransport = 0;
	}
}
void CGeneralTaskToDefendStorage::CancelTask( ICommander *pManager )
{
	if ( pRepairTransport )
		pManager->Give( pRepairTransport );
	pRepairTransport = 0;
}
void CGeneralTaskToDefendStorage::Recaptured()
{
	eState = TS_FINISH_RECAPTURE;
}
void CGeneralTaskToDefendStorage::Segment()
{
	switch( eState )
	{
	case TS_FINISHED:

		break;
	case TS_OPERATE:
		if ( theDipl.GetNParty(pStorage->GetPlayer()) != nParty )
			eState = TS_START_RECAPTURE;
		else if ( pStorage->GetHitPoints()	== 0 ) 
			eState = TS_START_REPAIR;

		break;
	case TS_START_RECAPTURE:
		if ( Random( 0.0f, 1.0f ) < SGeneralConsts::RECAPTURE_STORAGE_PROBALITY )
		{
			eState = TS_RECAPTURE;
			fSeverity = -1;
			theSupremeBeing.RegisterDelayedTask( new CWaitForChangePlayer( pStorage, this, nParty ) );
		}

		break;
	case TS_RECAPTURE:
		if ( theDipl.GetNParty(pStorage->GetPlayer()) == nParty )
		{
			eState = TS_FINISH_RECAPTURE;
		}

		break;
	case TS_FINISH_RECAPTURE:
		if ( !wRequestID )
		{
			fSeverity = 0;
			eState = TS_OPERATE;
		}
		break;
	case TS_START_REPAIR:
		if ( Random( 0.0f, 1.0f ) < SGeneralConsts::REPAIR_STORAGE_PROBABILITY ) 
		{
			eState = TS_REPAIR;
			fSeverity = -1;
		}

		break;
	case TS_REPAIR:
		if ( pStorage->GetHitPoints() != 0 )
		{
			fSeverity = pStorage->GetHitPoints() / pStorage->GetStats()->fMaxHP;
			eState = TS_OPERATE;
		}
		
		if ( pRepairTransport && !IsValidObj( pRepairTransport ) )
			pRepairTransport = 0;
		
		break;
	}
}

struct SEnumStorages : public CStaticObjects::IEnumStoragesPredicate 
{
	Storages storages;
	virtual bool OnlyConnected() const { return false ; }
	virtual bool AddStorage( class CBuildingStorage * pStorage, const float fPathLenght )
	{
		const SBuildingRPGStats * pStats = static_cast<const SBuildingRPGStats*>( pStorage->GetStats() );
		if ( SBuildingRPGStats::TYPE_TEMP_RU_STORAGE == pStats->eType )
		{
			storages.push_back( pStorage );
		}
		return true;
	}
};
CGeneralTaskToResupplyCell::CGeneralTaskToResupplyCell( CResupplyCellInfo * pCell, const int nParty, const enum EResupplyType _eResupplyType, class CGeneralIntendant *_pCells ) 
: pCell( pCell ), fSeverity( 1.0f ), eResupplyType( _eResupplyType ), bFinished( false ), nParty( nParty ),
	timeNextCheck ( curTime ), pCells( _pCells )
{ 
	pCell->MarkUnderSupply( eResupplyType );
	vResupplyCenter = pCell->CalcResupplyPos( eResupplyType );
	theSupremeBeing.RegisterDelayedTask( new CGeneralTaskCheckCellDanger( this, pCell, eResupplyType, _pCells ) );
}
void CGeneralTaskToResupplyCell::AskForWorker( ICommander *pManager, const float fMaxSeverity, const bool bInit ) 
{ 
	if ( !bInit &&
			 pCell->GetNNeeded( eResupplyType ) != 0 &&
			 fSeverity <= fMaxSeverity && 
			 !IsValidObj( pResupplyTransport) )
		pManager->EnumWorkers( FT_TRUCK_RESUPPLY, this );
}
void CGeneralTaskToResupplyCell::ReleaseWorker( ICommander *pManager, const float fMinSeverity ) 
{ 
	if ( bFinished )
	{
		pCell->MarkUnderSupply( eResupplyType, false );
		if ( IsValidObj( pResupplyTransport ) )
		{
			pManager->Give( pResupplyTransport );
			pResupplyTransport = 0;
		}
	}
}
void CGeneralTaskToResupplyCell::CancelTask( ICommander *pManager ) 
{ 
	bFinished = true;
	ReleaseWorker( pManager, 0 );
}
void CGeneralTaskToResupplyCell::Segment() 
{ 
	if ( pResupplyTransport && !IsValidObj( pResupplyTransport ) )
	{
		pCells->MarkCellsDangerous( pCell->GetCenter() );		
	}
	if ( pCell->GetNNeeded( eResupplyType ) == 0 || pCell->IsDangerous() )
		bFinished = true;

	if ( IsValidObj( pResupplyTransport ) && 
				EUSN_REST == pResupplyTransport->GetState()->GetName() )
	{
		bFinished = true;
	}

	if ( fSeverity == 1 && curTime >= timeNextCheck )
		fSeverity = -1;
}
bool CGeneralTaskToResupplyCell::EnumWorker( class CCommonUnit *pUnit, const enum EForceType _eType ) 
{ 
	NI_ASSERT_T( !IsValidObj( pResupplyTransport ), "2 transport at 1 task" );
	pResupplyTransport = pUnit;

	pCell->IssueCommand( pUnit, eResupplyType, vResupplyCenter );
	fSeverity = 0;
	return false;
}
bool CGeneralTaskToResupplyCell::EvaluateWorker( CCommonUnit * pUnit, const enum EForceType _eType ) const
{ 
	return pCell->IsUnitSuitable( pUnit, eResupplyType );
}
int CGeneralTaskToResupplyCell::NeedNBest( const enum EForceType _eType ) const
{ 
	return 1;
}
float CGeneralTaskToResupplyCell::EvaluateWorkerRating( CCommonUnit * pUnit, const enum EForceType _eType ) const
{ 
	return 1000.0f / fabs2( pUnit->GetCenter() - vResupplyCenter );
}
CResupplyCellInfo::CResupplyCellInfo()
: cMarkedUnderSupply( 0 ), fCount( 0.0f ), timeLastDanger( 0 )
{
	resupplyCount.resize( _ERT_COUNT );
	std::fill( resupplyCount.begin(), resupplyCount.end(), 0.0f );
}
void CResupplyCellInfo::AddUnitResupply( CCommonUnit *pUnit, const enum EResupplyType eType )
{
	if ( pUnit->IsValid() && pUnit->IsAlive() )
	{
		const int nID = pUnit->GetUniqueId();

		if ( resupplyInfo.find( nID ) == resupplyInfo.end() )
			resupplyInfo.insert( std::pair<int,BYTE>( nID,0) );

		if ( !(resupplyInfo[nID] & (1<<eType)) )
		{
			resupplyInfo[nID] |= (1<<eType);
			const float fPrice = pUnit->GetPriceMax();
			resupplyCount[eType] += fPrice;
			fCount += fPrice;
		}
	}
}
void CResupplyCellInfo::RemoveUnitResupply( class CCommonUnit *pUnit, const enum EResupplyType eType )
{
	const int nID = pUnit->GetUniqueId();
	CResupplyInfo::iterator curResupply = resupplyInfo.find( nID );
	if ( resupplyInfo.end() != curResupply )
	{
		const byte cRes = curResupply->second;
		if ( cRes & (1<<eType) )
			RemoveUnitResupplyInternal( pUnit, eType );
	}
}
void CResupplyCellInfo::RemoveUnitResupplyInternal( class CCommonUnit *pUnit, const enum EResupplyType eType )
{
	const float fPrice = pUnit->GetPriceMax();
	resupplyCount[eType] -= fPrice;
	fCount -= fPrice;

	const int nID = pUnit->GetUniqueId();
	NI_ASSERT_T( resupplyInfo.end() != resupplyInfo.find( nID ), "unit unregistered" );
	CResupplyInfo::iterator curInfo = resupplyInfo.find( nID );
	curInfo->second &= ~(1<<eType);
	
	if ( 0 == curInfo->second )
		resupplyInfo.erase( nID );
}
void CResupplyCellInfo::SetDanger( const NTimer::STime timeDanger ) 
{ 
	timeLastDanger = timeDanger; 
}
const bool CResupplyCellInfo::IsDangerous() const 
{ 
	return timeLastDanger && timeLastDanger > curTime; 
}
void CResupplyCellInfo::AddUnit( class CCommonUnit *pUnit, const byte cRes )
{
	NI_ASSERT_T( 0 !=  cRes, "unit doesn't need resupply" );
	NI_ASSERT_T( cRes < (1<<_ERT_COUNT), NStr::Format( "wrong resupply mask %d", cRes ) ) ;
	
	const int nID = pUnit->GetUniqueId();
	const float fPrice = pUnit->GetPriceMax();
	for ( int i = 0; i < _ERT_COUNT; ++i )
	{
		if ( cRes & (1<<i) )
			AddUnitResupply( pUnit, static_cast<EResupplyType>(i) );
	}
}
byte CResupplyCellInfo::RemoveUnit( class CCommonUnit *pUnit )
{
	const float fPrice = pUnit->GetPriceMax();
	const int nID = pUnit->GetUniqueId();

	CResupplyInfo::iterator curResupply = resupplyInfo.find( nID );
	
	if ( resupplyInfo.end() != curResupply )
	{
		const byte cRes = curResupply->second;
		for ( int i = 0; i < _ERT_COUNT && resupplyInfo.find( nID ) != resupplyInfo.end(); ++i )
		{
			if ( cRes & (1<<i) )
				RemoveUnitResupplyInternal( pUnit, static_cast<EResupplyType>(i) );
		}
		return cRes;
	}
	return 0;
}
float CResupplyCellInfo::GetNNeeded( const byte cTypeMask ) const
{
	float fLocalCount = 0;
	for ( int i = 0; i < _ERT_COUNT; ++i )
	{
		if ( (cTypeMask & (1<<i)) && !(cMarkedUnderSupply & (1<<i)) )
			fLocalCount += resupplyCount[i];
	}
	return fLocalCount;
}
bool CResupplyCellInfo::IsEmpty() const
{
	return fCount == 0.0f;
}
const bool CResupplyCellInfo::IsUnitRegistered( CCommonUnit * pUnit ) const
{
	CResupplyInfo::const_iterator it = resupplyInfo.find( pUnit->GetUniqueId() );
	return it != resupplyInfo.end();
}
void CResupplyCellInfo::MarkUnderSupply( const enum EResupplyType eType, const bool bSupply )
{
	if ( bSupply )
		cMarkedUnderSupply |= (1<<eType); 
	else
		cMarkedUnderSupply &= ~(1<<eType);
}
bool CResupplyCellInfo::IsUnitSuitable( const class CCommonUnit * pUnit, const enum EResupplyType eType )
{
	switch( eType )
	{
	case ERT_REPAIR:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_REPAIR );
	case ERT_RESUPPLY:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_RESUPPLY );
	case ERT_HUMAN_RESUPPLY:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_RESUPPLY_HR );
	case ERT_MORALE:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_RESUPPLY_MORALE );
	case ERT_MEDICINE:
		return pUnit->CanCommandBeExecutedByStats( ACTION_COMMAND_HEAL_INFANTRY );
	default:
		NI_ASSERT_T( false, NStr::Format( "unknown resupply type asked %d", eType ) );
		return false;
	}
}
void CResupplyCellInfo::IssueCommand( class CCommonUnit * pUnit, const enum EResupplyType eType, const CVec2 &vResupplyCenter )
{
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_MOVE_TO_NOT_PRESIZE, vResupplyCenter, SConsts::GENERAL_CELL_SIZE * 2 ), pUnit, false );
	switch( eType )
	{
	case ERT_REPAIR:
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_REPAIR, vResupplyCenter), pUnit, true );
		break;
	case ERT_RESUPPLY:
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_RESUPPLY, vResupplyCenter), pUnit, true );
		break;
	case ERT_HUMAN_RESUPPLY:
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_RESUPPLY_HR, vResupplyCenter), pUnit, true );
		break;
	case ERT_MEDICINE:
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, vResupplyCenter), pUnit, true );
		break;
	case ERT_MORALE:
		theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, vResupplyCenter), pUnit, true );
		break;
	default:
		NI_ASSERT_T( false, "unknown resupply type asked" );
	}
	
}
CVec2 CResupplyCellInfo::CalcResupplyPos( const enum EResupplyType eType ) const
{
	return AICellsTiles::GetCenterOfGeneralCell( vCell );
}
CGeneralIntendant::CGeneralIntendant( const int nParty, CCommander *pGeneral)
: nParty( nParty ), pGeneral( pGeneral ), bInitedByParcel( false )
{
	cells.SetSizes( theStaticMap.GetSizeX() * SConsts::TILE_SIZE / SConsts::GENERAL_CELL_SIZE + 1,
									theStaticMap.GetSizeY() * SConsts::TILE_SIZE / SConsts::GENERAL_CELL_SIZE + 1 );
}
void CGeneralIntendant::Give( CCommonUnit *pWorker )
{
	NI_ASSERT_T( dynamic_cast<CAIUnit*>( pWorker ) != 0, "not mech unit" );
	resupplyTrucks.push_back( static_cast<CAIUnit*>( pWorker ) );

	int nBest = 0;
	float fBestDist = 0;
	const CVec2 vWorkerPos ( pWorker->GetCenter() );

	for ( int i = 0; i < vPositions.size(); ++i )
	{
		const float fCurDist = fabs2( vWorkerPos - vPositions[i].first );
		if ( nBest == i || fCurDist < fBestDist )
		{
			fBestDist = fCurDist;
			nBest = i;
		}
	}
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_MOVE_TO, vPositions[nBest].first), pWorker, false );
	const CVec2 vLookPoint( vPositions[nBest].first + GetVectorByDirection( vPositions[nBest].second ) );
	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_ROTATE_TO, vLookPoint), pWorker, true );
}
void CGeneralIntendant::AddReiforcePositions( const struct SAIGeneralParcelInfo &patchInfo )
{
	bInitedByParcel = true;
	const int nFormer = vPositions.size();
	
	vPositions.resize( vPositions.size() + patchInfo.reinforcePoints.size() );
	
	for ( int i = 0; i < patchInfo.reinforcePoints.size(); ++i )
	{
		const CVec2 vTransReinforcePoint ( patchInfo.reinforcePoints[i].vCenter.y, -patchInfo.reinforcePoints[i].vCenter.x );
		const CVec2 vReinforcePoint( patchInfo.vCenter + ( vTransReinforcePoint ^ GetVectorByDirection(patchInfo.wDefenceDirection) ) );

		vPositions[nFormer+i].first = vReinforcePoint;
		vPositions[nFormer+i].second = patchInfo.wDefenceDirection + patchInfo.reinforcePoints[i].wDir;
	}
}
void CGeneralIntendant::AddReiforcePosition( const CVec2 & vPos, const WORD wDirection )
{
	if ( !bInitedByParcel )
		vPositions.push_back( CPosition( vPos, wDirection ) );
}
void CGeneralIntendant::Init()
{
	SEnumStorages en;
	theStatObjs.EnumStoragesForParty( nParty, &en );
	for ( Storages::iterator storIter = en.storages.begin(); storIter != en.storages.end(); ++storIter )
	{
		CGeneralTaskToDefendStorage * pTask = new CGeneralTaskToDefendStorage( *storIter, nParty );
		tasks.push_back( pTask );
	}
	
	theStatObjs.EnumStoragesForParty( 1 - nParty, &en );
	for ( Storages::iterator storIter = en.storages.begin(); storIter != en.storages.end(); ++storIter )
	{
		CGeneralTaskToDefendStorage * pTask = new CGeneralTaskToDefendStorage( *storIter, nParty );
		tasks.push_back( pTask );
	}

	for ( int x = 0; x < cells.GetSizeX(); ++x )
	{
		for ( int y = 0; y < cells.GetSizeY(); ++y )
		{
			cells( x, y ) = new CResupplyCellInfo;
			cells( x, y )->Init( SVector( x, y ) );
		}
	}
}
void CGeneralIntendant::EnumWorkers( const EForceType eType, IWorkerEnumerator *pEnumerator )
{
	EnumWorkersInternal( eType, pEnumerator, &resupplyTrucks );
}
int CGeneralIntendant::RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType )
{
	return pGeneral->RequestForSupport( vSupportCenter, eType );
}
void CGeneralIntendant::CancelRequest( int nRequestID, enum EForceType eType )
{
	pGeneral->CancelRequest( nRequestID, eType );
}
void CGeneralIntendant::UnitDead( class CCommonUnit * pUnit )
{
	if ( !IsUnitRegistered( pUnit ) )	return;

	ResupplyCells::iterator pFormerCell = GetCell( pUnit->GetCenter() );
	pFormerCell->second->RemoveUnit( pUnit );
	if ( pFormerCell->second->IsEmpty() )
		cellsWithRequests.erase( pFormerCell );
}
void CGeneralIntendant::UnitChangedPosition( class CCommonUnit *pUnit, const CVec2 &vNewPos )
{
	if ( !IsUnitRegistered( pUnit ) ) return;

	ResupplyCells::iterator pFormerCell = GetCell( pUnit->GetCenter() );
	ResupplyCells::iterator pNewCell = GetCell( vNewPos );
	NI_ASSERT_T( pFormerCell != pNewCell, "cannot move to the same cell" );
	
	pNewCell->second->AddUnit( pUnit, pFormerCell->second->RemoveUnit( pUnit ) );
	if ( pFormerCell->second->IsEmpty() )
		cellsWithRequests.erase( pFormerCell );
}
void CGeneralIntendant::SetArtilleryVisible( const CAIUnit *pArtillery, const bool bVisible )
{
	const int nID = pArtillery->GetUniqueId();
	CFreeArtillery::iterator it = freeArtillery.find( nID );
	if ( it != freeArtillery.end() )
		it->second->SetVisible( pArtillery, bVisible );
}
void CGeneralIntendant::UnitAskedForResupply( class CCommonUnit * pUnit, const enum EResupplyType eType, const bool bSet )
{
	if ( bSet )
	{
		if ( eType == ERT_HUMAN_RESUPPLY && !pUnit->IsFormation() )
		{
			CAIUnit * pAIUnit = static_cast<CAIUnit*>( pUnit );
			NI_ASSERT_T( pAIUnit->GetStats()->IsArtillery(), "NOT SQUIAD AND NOT ARTILLERY ASKED FOR HUMAH RESUPPLY" );
			if ( pAIUnit->IsVisible( nParty ) )
			{
				ResupplyCells::iterator pCell = GetCell( pAIUnit->GetCenter() );

				pCell->second->AddUnitResupply( pUnit, eType );	
				const int nID = pAIUnit->GetUniqueId();
				if ( !freeArtillery[nID].IsValid() )
					freeArtillery[nID] = new CEnemyRememberer( SGeneralConsts::TIME_DONT_SEE_EMPTY_ARTILLERY_BEFORE_FORGET );
				freeArtillery[nID]->SetVisible( pUnit, true );
			}
		}
		else
		{
			ResupplyCells::iterator pCell = GetCell( pUnit->GetCenter() );
			pCell->second->AddUnitResupply( pUnit, eType );
		}
	}
	else if ( IsUnitRegistered( pUnit ) )
	{
		ResupplyCells::iterator pFormerCell = GetCell( pUnit->GetCenter() );
		pFormerCell->second->RemoveUnitResupply( pUnit, eType );
		if ( pFormerCell->second->IsEmpty() )
			cellsWithRequests.erase( pFormerCell );
		freeArtillery.erase( pUnit->GetUniqueId() );
	}
}
const bool CGeneralIntendant::IsUnitRegistered( CCommonUnit * pUnit ) const
{
	const SVector vCell( AICellsTiles::GetGeneralCell( pUnit->GetCenter() ) );
	ResupplyCells::const_iterator it = cellsWithRequests.find( vCell );
	return cellsWithRequests.end() != it && it->second->IsUnitRegistered( pUnit );
}
CGeneralIntendant::ResupplyCells::iterator CGeneralIntendant::GetCell( const CVec2 &vPos )
{

	SVector vCell( AICellsTiles::GetGeneralCell( vPos ) );
	vCell.x = Clamp( vCell.x, 0, cells.GetBoundX() );
	vCell.y = Clamp( vCell.y, 0, cells.GetBoundY() );
	if ( !cells( vCell.x, vCell.y ) )
	{
		cells( vCell.x, vCell.y ) = new CResupplyCellInfo;
		cells( vCell.x, vCell.y )->Init( vCell );
	}
	cellsWithRequests[vCell] = cells( vCell.x, vCell.y );
	return cellsWithRequests.find( vCell );
}
void CGeneralIntendant::Segment()
{
	CCommander::Segment();
	SGeneralHelper::RemoveDead( &resupplyTrucks );

	const BYTE cResupplyAll = ((1<<_ERT_COUNT)-1);
	BYTE cResupplyPossible = 0;

	if ( !cellsWithRequests.empty() )
	{
		for ( CommonUnits::const_iterator it = resupplyTrucks.begin();
						resupplyTrucks.end() != it && cResupplyAll != cResupplyPossible; ++it )
		{
			CCommonUnit * pUnit = *it;
			for ( int i = 0; i < _ERT_COUNT; ++i )
			{
				if ( CResupplyCellInfo::IsUnitSuitable( pUnit, static_cast<EResupplyType>(i) ) )
					cResupplyPossible |= (1<<i);
			}
		}
	}

	if ( !cellsWithRequests.empty() && cResupplyPossible ) 
	{
		std::vector< CPtr<CResupplyCellInfo> > cellsToSort;
		cellsToSort.resize( cellsWithRequests.size() );
		int i = 0;
		for ( ResupplyCells::iterator it = cellsWithRequests.begin(); it != cellsWithRequests.end(); ++it )
			cellsToSort[i++] = it->second;
		CResupplyCellInfo::SSortByResupplyMaskPredicate pr( cResupplyPossible );
		std::sort( cellsToSort.begin(), cellsToSort.end(), pr );

		int nTasksCreated = 0; //INTENDANT_TASKS_PER_SEGMENT
		for ( int nCell = 0; nCell < cellsToSort.size() && nTasksCreated <INTENDANT_TASKS_PER_SEGMENT; ++nCell )
		{
			for ( int i = 0; i < _ERT_COUNT && nTasksCreated <INTENDANT_TASKS_PER_SEGMENT; ++i )
			{
				CResupplyCellInfo * pCell = cellsToSort[nCell];
				if ( !pCell->IsDangerous() &&
							(cResupplyPossible&(1<<i)) && 
						 !pCell->IsMarkedUnderSupply( static_cast<EResupplyType>(i) ) &&
						 pCell->GetNNeeded( static_cast<EResupplyType>(i) ) > 0.0f )
				{
					++nTasksCreated;
					IGeneralTask * pTask = new CGeneralTaskToResupplyCell( pCell, nParty, static_cast<EResupplyType>(i), this );
					tasks.push_back( pTask );
				}
			}
		}
	}
	DeleteForgottenArtillery();
	
}
void CGeneralIntendant::DeleteForgottenArtillery()
{
	std::list<int> deleted;
	for ( CFreeArtillery::iterator it = freeArtillery.begin(); freeArtillery.end() != it; ++it )
		if ( it->second->IsTimeToForget() )
			deleted.push_back( it->first );

	while( !deleted.empty() )
	{
		CCommonUnit *pUnit = static_cast<CCommonUnit*>( CLinkObject::GetObjectByUniqueId( deleted.front() ) );
		if ( pUnit ) 
			UnitAskedForResupply( pUnit, ERT_HUMAN_RESUPPLY, false );
		freeArtillery.erase( deleted.front() );
		deleted.pop_front();
	}
}
void CGeneralIntendant::MarkCellsDangerous( const SVector &vCell )
{
	const int nRadius = SGeneralConsts::INTENDANT_DANGEROUS_CELL_RADIUS / ( SConsts::GENERAL_CELL_SIZE ) + 1;
	const NTimer::STime timeDanger = curTime + 
								1000 * (SGeneralConsts::RESUPPLY_CELL_AFTER_TRANSPORT_DEATH +
												Random( SGeneralConsts::RESUPPLY_CELL_AFTER_TRANSPORT_DEATH_RAND ) );


	const int nStartX = Clamp( vCell.x - nRadius, 0, cells.GetSizeX() );
	const int nStartY = Clamp( vCell.y - nRadius, 0, cells.GetSizeY() );
	const int nFinishX = Clamp( vCell.x + nRadius, 0, cells.GetSizeX() );
	const int nFinishY = Clamp( vCell.y + nRadius, 0, cells.GetSizeY() );

	for ( int x = nStartX; x < nFinishX; ++x )
	{
		for ( int y = nStartY; y < nFinishY; ++y )
		{
			cells( x, y )->SetDanger( timeDanger );
		}
	}
}
