#include "stdafx.h"

#include "Units.h"
#include "AIUnit.h"
#include "AIStaticMap.h"
#include "Diplomacy.h"
#include "Technics.h"
#include "Aviation.h"
#include "Formation.h"
#include "General.h"
#include "UnitsIterators.h"

#include "UnitsIterators2.h"
extern CSupremeBeing theSupremeBeing;
extern CStaticMap theStaticMap;
CUnits units;
extern CDiplomacy theDipl;
const SVector GetLeveledCell( const SVector &bigCell, const int nCellLevel )
{
	return SVector( bigCell.x / (1 << (nCellLevel+1)), bigCell.y / (1 << (nCellLevel+1) ) );
}
const bool CUnits::IsUnitInCell( const int nUnitID ) const
{
	return posUnitInCell[nUnitID].nUnitPos != 0 || posUnitInCell[nUnitID].nCellID != 0;
}
void CUnits::Init()
{
  nUnitsOfType.resize( 3 );
	
	units.IncreaseListsNum( 3 + 1 );
	posUnitInCell.resize( SConsts::AI_START_VECTOR_SIZE );
	
	nUnitsCell.SetSizes( theStaticMap.GetBigCellsSizeX(), theStaticMap.GetBigCellsSizeY() );
	nUnitsCell.SetZero();

	nCell.SetSizes( theStaticMap.GetBigCellsSizeX(), theStaticMap.GetBigCellsSizeY() );
	nCell.SetZero();

	sizes.resize( 3 );
	
	for ( int nVis = 0; nVis < 2; ++nVis )
	{
		for ( int nCellLevel = 0; nCellLevel < N_CELLS_LEVELS; ++nCellLevel )
		{
			for ( int nDipl = 0; nDipl < 3; ++nDipl )
			{
				for ( int nType = 0; nType < 2; ++nType )
				{
					numUnits[nVis][nCellLevel][nDipl][nType].SetSizes
					(
						theStaticMap.GetBigCellsSizeX() / ( 1 << (nCellLevel+1) ) + 1,
						theStaticMap.GetBigCellsSizeY() / ( 1 << (nCellLevel+1) ) + 1
					);

					numUnits[nVis][nCellLevel][nDipl][nType].SetZero();
				}
			}
		}
	}
	
	unitsInCells.resize( 2 );
}
void CUnits::CheckCorrectness( const SVector &tile )
{
	const SVector cell = AICellsTiles::GetBigCell( AICellsTiles::GetPointByTile( tile ) );

	int cnt = 0;
	for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( !pUnit->IsInSolidPlace() )
		{
			const SVector unitCell = AICellsTiles::GetBigCell( pUnit->GetCenter() );
			if ( unitCell == cell )
				++cnt;
		}
	}

	const int nUnitsInCell = nUnitsCell[cell.y][cell.x];
	NI_ASSERT_T( cnt == nUnitsInCell, "Wrong number of units in cell" );
}
void CUnits::AddUnitToLeveledCells( CAIUnit *pUnit, const SVector &bigCell, const int nVis )
{
	NI_ASSERT_T( nVis < 2, NStr::Format( "Wrong nVis (%d)", nVis ) );

	const int nUnitParty = pUnit->GetParty();
	const bool bUnitMech = pUnit->IsMech();
	for ( int i = 0; i < N_CELLS_LEVELS; ++i )
	{
		const SVector leveledCell( GetLeveledCell( bigCell, i ) );
		++numUnits[nVis][i][nUnitParty][!bUnitMech][leveledCell.y][leveledCell.x];
	}
}
void CUnits::DelUnitFromLeveledCells( CAIUnit *pUnit, const SVector &bigCell, const int nVis )
{
	NI_ASSERT_T( nVis < 2, NStr::Format( "Wrong nVis (%d)", nVis ) );

	const int nUnitParty = pUnit->GetParty();
	const bool bUnitMech = pUnit->IsMech();
	for ( int i = 0; i < N_CELLS_LEVELS; ++i )
	{
		const SVector leveledCell( GetLeveledCell( bigCell, i ) );
		--numUnits[nVis][i][nUnitParty][!bUnitMech][leveledCell.y][leveledCell.x];

		NI_ASSERT_T( numUnits[nVis][i][nUnitParty][!bUnitMech][leveledCell.y][leveledCell.x] >= 0, "Wrong number of units in leveled cell" );
	}
}
const int CUnits::GetVisIndex( CAIUnit *pUnit )
{
	const int nUnitParty = pUnit->GetParty();
	const int nOppositeParty = nUnitParty < 2 ? 1 - nUnitParty : nUnitParty;

	return pUnit->IsVisible( nOppositeParty ) || pUnit->IsRevealed();
}
void CUnits::AddUnitToConcreteCell( CAIUnit *pUnit, const SVector &cell, bool bWithLeveledCelles )
{
	if ( ++nUnitsCell[cell.y][cell.x] == 1 )
		nCell[cell.y][cell.x] = cellsIds.GetFreeId();
	
	const int newId = nCell[cell.y][cell.x] * 2 * 3 + ( 2 * pUnit->GetParty() + BYTE( pUnit->GetStats()->IsInfantry() ) ) + 1;

	if ( newId >= unitsInCells[0].GetListsNum() || newId >= unitsInCells[1].GetListsNum() )
	{
		unitsInCells[0].IncreaseListsNum( newId * 1.5 );
		unitsInCells[1].IncreaseListsNum( newId * 1.5 );
	}

	const int nUnitID = pUnit->GetID();
	const int nVisIndex = pUnit->GetNVisIndexInUnits();


	posUnitInCell[nUnitID].nCellID = newId;
	posUnitInCell[nUnitID].nUnitPos = unitsInCells[nVisIndex].Add( newId, nUnitID );
	posUnitInCell[nUnitID].cell = cell;

	if ( bWithLeveledCelles )
		AddUnitToLeveledCells( pUnit, cell, nVisIndex );

	unitsInCellsSet.insert( nUnitID );
}
void CUnits::AddUnitToCell( CAIUnit *pUnit, const CVec2 &newPos, bool bWithLeveledCelles )
{
	const SVector bigCell( AICellsTiles::GetBigCell( newPos ) );
	if ( theStaticMap.IsBigCellInside( bigCell ) )
		AddUnitToConcreteCell( pUnit, bigCell, bWithLeveledCelles );
}
void CUnits::AddUnitToCell( CAIUnit *pUnit, bool bWithLeveledCelles )
{
	const CVec2 vCenter( pUnit->GetCenter() );
	const SVector bigCell = AICellsTiles::GetBigCell( vCenter );

	if ( theStaticMap.IsBigCellInside( bigCell ) )
		AddUnitToConcreteCell( pUnit, bigCell, bWithLeveledCelles );
}
void CUnits::DelUnitFromCell( CAIUnit *pUnit, bool bWithLeveledCelles )
{
	const int nUnitID = pUnit->GetID();
	if ( IsUnitInCell( nUnitID ) )
	{
		const SVector &cell = posUnitInCell[nUnitID].cell;
		
		const int nVisIndex = pUnit->GetNVisIndexInUnits();
		unitsInCells[nVisIndex].Erase( posUnitInCell[nUnitID].nCellID, posUnitInCell[nUnitID].nUnitPos );
		posUnitInCell[nUnitID].nCellID = posUnitInCell[nUnitID].nUnitPos = 
		posUnitInCell[nUnitID].cell.x = posUnitInCell[nUnitID].cell.y = 0;

		if ( --nUnitsCell[cell.y][cell.x] == 0 )
			cellsIds.AddToFreeId( nCell[cell.y][cell.x] );

		if ( bWithLeveledCelles )
			DelUnitFromLeveledCells( pUnit, cell, nVisIndex );

		NI_ASSERT_T( unitsInCellsSet.find( nUnitID ) != unitsInCellsSet.end(), "Unit is not in cell" );
		unitsInCellsSet.erase( nUnitID );
	}
}
int CUnits::AddUnitToUnits( CAIUnit *pUnit, const int nPlayer, const int nUnitType )
{
	const int nParty = theDipl.GetNParty( nPlayer );
	NI_ASSERT_T( nParty >= 0 && nParty < 3, NStr::Format( "Wrong number of player (%d)", nPlayer ) );

	return units.Add( nParty, pUnit );
}
void CUnits::AddUnitToMap( CAIUnit *pUnit )
{
	if ( pUnit->GetStats()->IsAviation() )
		planes.push_back( static_cast<CAviation*>(pUnit) );

	const int nUnitID = pUnit->GetID();
	if ( nUnitID >= posUnitInCell.size() )
		posUnitInCell.resize( nUnitID * 1.5 );

	if ( !pUnit->IsInSolidPlace() )
		AddUnitToCell( pUnit, true );

	const int nParty = pUnit->GetParty();

	++sizes[nParty];

	const int nUnitType = pUnit->GetStats()->type;
	if ( nUnitsOfType[nParty].find( nUnitType ) == nUnitsOfType[nParty].end() )
		nUnitsOfType[nParty][nUnitType] = 1;
	else
		++nUnitsOfType[nParty][nUnitType];
}
void CUnits::DeleteUnitFromMap( CAIUnit *pUnit )
{
	if ( pUnit->GetStats()->IsAviation() )
	{
		NI_ASSERT_T( std::find( planes.begin(), planes.end(), pUnit ) != planes.end(), "Can't find deleted plane in units" );
		planes.erase( std::find( planes.begin(), planes.end(), pUnit ) );
	}

	const int nUnitID = pUnit->GetID();
	if ( units.GetEl( nUnitID ) != 0 )
	{
		DelUnitFromCell( pUnit, true );
		units.GetEl( nUnitID ) = 0;
	}

	const int nType = pUnit->GetStats()->type;
	--nUnitsOfType[pUnit->GetParty()][nType];
	--sizes[pUnit->GetParty()];
}
void CUnits::FullUnitDelete( CAIUnit *pUnit )
{
	const int nParty = pUnit->GetParty();
	NI_ASSERT_T( nParty >= 0 && nParty < 3, NStr::Format( "Wrong number of player (%d)", nParty ) );

	units.Erase( nParty, pUnit->GetID() );
}
void CUnits::UnitChangedPosition( CAIUnit *pUnit, const CVec2 &newPos )
{
	const int nUnitID = pUnit->GetID();

	const SVector oldBigCell = posUnitInCell[nUnitID].cell;
	const SVector newBigCell = AICellsTiles::GetBigCell( newPos );
	const bool bInOldCell = IsUnitInCell( nUnitID );

	NI_ASSERT_T( !bInOldCell || theStaticMap.IsBigCellInside( oldBigCell ), 
								NStr::Format( "Wrong old big cell (%d,%d)", oldBigCell.x, oldBigCell.y ) );
	if ( oldBigCell != newBigCell || !bInOldCell )
	{
		if ( bInOldCell ) 
			DelUnitFromCell( pUnit, false );

		AddUnitToCell( pUnit, newPos, false );
		const bool bInNewCell = IsUnitInCell( nUnitID );

		const int nVisIndex = pUnit->GetNVisIndexInUnits();
		NI_ASSERT_T( nVisIndex < 2, NStr::Format( "Wrong nVis (%d)", nVisIndex ) );

		const int nUnitParty = pUnit->GetParty();
		const bool bUnitMech = pUnit->IsMech();
		for ( int i = 0; i < N_CELLS_LEVELS; ++i )
		{
			const SVector oldLeveledCell = GetLeveledCell( oldBigCell, i );
			const SVector newLeveledCell = GetLeveledCell( newBigCell, i );

			if ( oldLeveledCell != newLeveledCell || !bInOldCell || !bInNewCell )
			{
				if ( bInOldCell )
				{
					--numUnits[nVisIndex][i][nUnitParty][!bUnitMech][oldLeveledCell.y][oldLeveledCell.x];
					NI_ASSERT_T( numUnits[nVisIndex][i][nUnitParty][!bUnitMech][oldLeveledCell.y][oldLeveledCell.x] >= 0, "Wrong number of units in leveled cell" );
				}

				if ( bInNewCell )
					++numUnits[nVisIndex][i][nUnitParty][!bUnitMech][newLeveledCell.y][newLeveledCell.x];
			}
		}
	}

	if ( AICellsTiles::GetGeneralCell( pUnit->GetCenter() ) != AICellsTiles::GetGeneralCell( newPos ) )
		theSupremeBeing.UnitChangedPosition( pUnit, newPos );
}
CAIUnit* CUnits::operator[]( const int id )
{ 
	NI_ASSERT_T( units.GetEl( id ) == 0 || !units.GetEl( id )->IsValid() || units.GetEl( id )->GetID() == id, "Wrong units' id" );
	return units.GetEl( id ); 
}
void CUnits::ChangePlayer( CAIUnit *pUnit, const BYTE cNewPlayer )
{
	if ( pUnit->GetPlayer() != cNewPlayer )
	{
		CObj<CAIUnit> pSaveUnit = pUnit;
		DeleteUnitFromMap( pUnit );
		FullUnitDelete( pUnit );

		pUnit->SetPlayer( cNewPlayer );

		const int newID = AddUnitToUnits( pUnit, cNewPlayer, pUnit->GetStats()->type );
		NI_ASSERT_T( newID == pUnit->GetID(), "Wrong id after changing of player" );

		AddUnitToMap( pUnit );
	}
}
int CUnits::AddFormation( CFormation *pFormation )
{
	formations.insert( pFormation );
	return units.Add( 3, 0 );
}
void CUnits::DelFormation( CFormation *pFormation )
{
	units.Erase( 3, pFormation->GetID() );	
	formations.erase( pFormation );
}
const int CUnits::Size( const int nParty ) const
{
	NI_ASSERT_T( nParty < 3, "Wrong number of party" );
	return sizes[nParty];
}
const int CUnits::GetNSoldiers( const CVec2 &vCenter, const float fRadius, const int nParty )
{
	int cnt = 0;
	const float fRadius2 = sqr( fRadius );
	for ( CUnitsIter<0,0> iter( nParty, EDI_FRIEND, vCenter, fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( IsValidObj( pUnit ) && pUnit->GetStats()->IsInfantry() )
		{
			const float fDist2 = fabs2( pUnit->GetCenter() - vCenter );
			if ( fDist2 < fRadius2 )
				++cnt;
		}
	}

	return cnt;
}
const int CUnits::GetNUnits( const CVec2 &vCenter, const float fRadius, const int nParty )
{
	int cnt = 0;
	const float fRadius2 = sqr( fRadius );
	for ( CUnitsIter<0,0> iter( nParty, EDI_FRIEND, vCenter, fRadius ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		const float fDist2 = fabs2( pUnit->GetCenter() - vCenter );
		if ( fDist2 < fRadius2 )
			++cnt;
	}

	return cnt;
}
void CUnits::CheckUnitCell()
{
	for ( int k = 0; k < 2; ++k )
	{
		for ( int i = 0; i < theStaticMap.GetSizeY() / SConsts::BIG_CELL_COEFF; ++i )
		{
			for ( int j = 0; j < theStaticMap.GetSizeX() / SConsts::BIG_CELL_COEFF; ++j )
			{
				int nCnt = 0;
				if ( nUnitsCell[i][j] != 0 )
				{
					for ( int cCurDipl = 0; cCurDipl < 3; ++cCurDipl )
					{
						for ( int cCurMech = 0; cCurMech < 2; ++cCurMech )
						{
							const int id = nCell[i][j] * 2 * 3 + 2 * cCurDipl + cCurMech + 1;
							int nIter = unitsInCells[k].begin( id );

							while ( nIter != unitsInCells[k].end() )
							{
								NI_ASSERT_T( unitsInCells[k].GetEl( nIter ) != 0, "Wrong cell" );
								const int nOldIter = nIter;
								nIter = unitsInCells[k].GetNext( nIter );
								++nCnt;

								NI_ASSERT_T( nOldIter != nIter, "Wrong iter" );
							}
						}
					}
				}
				NI_ASSERT_T( nCnt == nUnitsCell[i][j], "Wrong number of units in cell" );
			}
		}
	}

	/*
	bool bOk = false;
	if ( !pUnit->IsInSolidPlace() && theStaticMap.IsPointInside( pUnit->GetCenter() ) )
	{
		const SVector cell = AICellsTiles::GetBigCell( pUnit->GetCenter() );
		const int id = nCell[cell.y][cell.x] * 2 * 3 + ( 2 * pUnit->GetParty() + BYTE( pUnit->GetStats()->IsInfantry() ) ) + 1;

		for ( int i = unitsInCells.begin( id ); i != unitsInCells.end(); i = unitsInCells.GetNext( i ) )
		{
			const int unitID = unitsInCells.GetEl( i );
			if ( unitID == pUnit->GetID() )
				bOk = true;

			CAIUnit *pCellUnit = units.GetEl( unitID );
			SVector cell1 = AICellsTiles::GetBigCell( pCellUnit->GetCenter() );
			if ( cell != cell1 )
				NPlatform::BreakIntoDebugger();
		}
	}

	if ( !bOk )
		NPlatform::BreakIntoDebugger();
	*/
}
void CUnits::UpdateUnitVis4Enemy( CAIUnit *pUnit )
{
	const int nVisIndex = GetVisIndex( pUnit );
	const int nOldVisIndex = pUnit->GetNVisIndexInUnits();
	if ( nVisIndex != nOldVisIndex )
	{
		const int nUnitID = pUnit->GetID();

		if ( IsUnitInCell( nUnitID ) )
		{
			unitsInCells[nOldVisIndex].Erase( posUnitInCell[nUnitID].nCellID, posUnitInCell[nUnitID].nUnitPos );
			posUnitInCell[nUnitID].nUnitPos = unitsInCells[nVisIndex].Add( posUnitInCell[nUnitID].nCellID, nUnitID );

			DelUnitFromLeveledCells( pUnit, posUnitInCell[nUnitID].cell, nOldVisIndex );
			AddUnitToLeveledCells( pUnit, posUnitInCell[nUnitID].cell, nVisIndex );

			pUnit->SetNVisIndexInUnits( nVisIndex );
		}
	}
}
