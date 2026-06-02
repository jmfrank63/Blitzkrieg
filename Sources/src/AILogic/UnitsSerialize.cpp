#include "stdafx.h"

#include "Units.h"
#include "UnitsIterators.h"
int CUnits::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &units );
	saver.Add( 2, &nUnitsCell );
	saver.Add( 3, &nCell );
	saver.Add( 6, &cellsIds );
	saver.Add( 7, &cellIdToCoord );
	saver.Add( 8, &planes );
	saver.Add( 9, &sizes );
	saver.Add( 10, &formations );

	int nCheck = -2;
	saver.Add( 11, &nCheck );

	int cnt = 12;
	if ( saver.IsReading() && nCheck != -2 )
	{
		const int nBigCellsSizeX = nUnitsCell.GetSizeX();
		const int nBigCellsSizeY = nUnitsCell.GetSizeY();
		for ( int k = 0; k < 2; ++k )
		{
			for ( int nCellLevel = 0; nCellLevel < N_CELLS_LEVELS; ++nCellLevel )
			{
				for ( int nDipl = 0; nDipl < 3; ++nDipl )
				{
					for ( int nType = 0; nType < 2; ++nType )
					{
						numUnits[k][nCellLevel][nDipl][nType].SetSizes
						(
							nBigCellsSizeX / ( 1 << (nCellLevel+1) ) + 1,
							nBigCellsSizeY / ( 1 << (nCellLevel+1) ) + 1
						);

						numUnits[k][nCellLevel][nDipl][nType].SetZero();
					}
				}
			}
		}
	}
	else
	{
		for ( int nVis = 0; nVis < 2; ++nVis )
		{
			for ( int i = 0; i < N_CELLS_LEVELS; ++i )
			{
				for ( int j = 0; j < 3; ++j )
				{
					for ( int k = 0; k < 2; ++k )
						saver.Add( cnt++, &(numUnits[nVis][i][j][k]) );
				}
			}
		}
	}
	
	saver.Add( 100, &unitsInCellsSet );
	saver.Add( 101, &nUnitsOfType );
	
	if ( nUnitsOfType.empty() )
		nUnitsOfType.resize( 3 );

	saver.Add( 102, &posUnitInCell );
	if ( posUnitInCell.empty() )
		posUnitInCell.resize( 10000 );

	saver.Add( 103, &unitsInCells );
	if ( unitsInCells.empty() )
		unitsInCells.resize( 2 );

	return 0;
}
int CGlobalIter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &iter );
	saver.Add( 2, &nCurParty );
	saver.Add( 3, &nParties );
	saver.Add( 4, &parties );

	return 0;
}
int CPlanesIter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &iter );

	return 0;
}
