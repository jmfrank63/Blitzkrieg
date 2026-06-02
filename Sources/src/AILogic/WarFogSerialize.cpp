#include "stdafx.h"

#include "AIWarFog.h"
#include "StaticObject.h"
#include "..\Misc\2DArrayRLEWrapper.h"
int CGlobalWarFog::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 4, &unitsInfo );
	saver.Add( 5, &newUnitsInfo );
	saver.Add( 6, &weights );
	saver.Add( 7, &deletedUnits );
	saver.Add( 8, &newUnits );
	saver.Add( 9, &nMiniMapY );
	saver.Add( 10, &miniMapSums );
	saver.Add( 11, &removedObjects4Units );
	saver.Add( 12, &addedObjects4Units );

/*	
	int nCheck = 1;
	saver.Add( 13, &nCheck );

	if ( nCheck == 1 )
	{
		int nChunk = 14;
		if ( !saver.IsReading() )
		{
			for ( int i = 0; i < fogCnts.size(); ++i )
			{
				CArray2DRLEWrapper<WORD> wrapper( fogCnts[i], 0 );
				saver.Add( nChunk++, &wrapper );
			}
			for ( int i = 0; i < maxVis.size(); ++i )
			{
				CBitArray2DRLEWrapper<CArray2D4Bit> wrapper( maxVis[i], 0 );
				saver.Add( nChunk++, &wrapper );
			}
			for ( int i = 0; i < minCoeff2.size(); ++ i )
			{
				CArray2DRLEWrapper<BYTE> wrapper( minCoeff2[i], floatToByte( 1.0f ) );
				saver.Add( nChunk++, &wrapper );
			}
		}
		else
		{
			std::vector< CArray2D<WORD> > fogCntsRLE( fogCnts.size() );
			std::vector<CArray2D4Bit> maxVisRLE( maxVis.size() );
			std::vector< CArray2D<BYTE> > minCoeff2RLE( minCoeff2.size() );
			
			for ( int i = 0; i < fogCnts.size(); ++i )
			{
				CArray2DRLEWrapper<WORD> wrapper( fogCntsRLE[i], 0 );
				saver.Add( nChunk++, &wrapper );

				NI_ASSERT_T( fogCntsRLE[i].GetSizeX() == fogCnts[i].GetSizeX(), "Wrong size X" );
				NI_ASSERT_T( fogCntsRLE[i].GetSizeY() == fogCnts[i].GetSizeY(), "Wrong size Y" );

				for ( int y = 0; y < fogCnts[i].GetSizeY(); ++y )
				{
					for ( int x = 0; x < fogCnts[i].GetSizeX(); ++x )
						NI_ASSERT_T( fogCntsRLE[i][y][x] == fogCnts[i][y][x], NStr::Format( "Wrong cell (%d,%d)", x, y ) );
				}
			}

			for ( int i = 0; i < maxVis.size(); ++i )
			{
				CBitArray2DRLEWrapper<CArray2D4Bit> wrapper( maxVisRLE[i], 0 );
				saver.Add( nChunk++, &wrapper );

				NI_ASSERT_T( maxVisRLE[i].GetSizeX() == maxVis[i].GetSizeX(), "Wrong size X" );
				NI_ASSERT_T( maxVisRLE[i].GetSizeY() == maxVis[i].GetSizeY(), "Wrong size Y" );

				for ( int y = 0; y < maxVis[i].GetSizeY(); ++y )
				{
					for ( int x = 0; x < maxVis[i].GetSizeX(); ++x )
						NI_ASSERT_T( maxVisRLE[i].GetData( x, y ) == maxVis[i].GetData( x, y ), NStr::Format( "Wrong cell (%d,%d)", x, y ) );
				}
			}
			
			for ( int i = 0; i < minCoeff2.size(); ++i )
			{
				CArray2DRLEWrapper<BYTE> wrapper( minCoeff2RLE[i], floatToByte( 1.0f ) );
				saver.Add( nChunk++, &wrapper );

				NI_ASSERT_T( minCoeff2RLE[i].GetSizeX() == minCoeff2[i].GetSizeX(), "Wrong size X" );
				NI_ASSERT_T( minCoeff2RLE[i].GetSizeY() == minCoeff2[i].GetSizeY(), "Wrong size Y" );

				for ( int y = 0; y < minCoeff2[i].GetSizeY(); ++y )
				{
					for ( int x = 0; x < minCoeff2[i].GetSizeX(); ++x )
					{
						NI_ASSERT_T( minCoeff2RLE[i][y][x] == minCoeff2[i][y][x], NStr::Format( "Wrong cell (%d,%d)", x, y ) );
					}
				}
			}
		}
	}
	*/
	
	fogCnts.resize( 2 );
	maxVis.resize( 2 );
	minCoeff2.resize( 2 );

	int nChunk = 14;
	for ( int i = 0; i < fogCnts.size(); ++i )
	{
		CArray2DRLEWrapper<WORD> wrapper( fogCnts[i], 0 );
		saver.Add( nChunk++, &wrapper );
	}
	for ( int i = 0; i < maxVis.size(); ++i )
	{
		CBitArray2DRLEWrapper<CArray2D4Bit> wrapper( maxVis[i], 0 );
		saver.Add( nChunk++, &wrapper );
	}
	for ( int i = 0; i < minCoeff2.size(); ++ i )
	{
		CArray2DRLEWrapper<BYTE> wrapper( minCoeff2[i], floatToByte( 1.0f ) );
		saver.Add( nChunk++, &wrapper );
	}
	
	{
		CBitArray2DRLEWrapper<CArray2D1Bit> wrapper( areasOpenTiles, 0 );
		saver.Add( nChunk++, &wrapper );
	}
	saver.Add( nChunk++, &areas );

	return 0;
}
int SFogInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &center );
	saver.Add( 2, &wUnitDir );
	saver.Add( 3, &bAngleLimited );
	saver.Add( 4, &pObject );
	saver.Add( 5, &r );
	saver.Add( 6, &wVisionAngle );
	saver.Add( 7, &wMinAngle );
	saver.Add( 8, &wMaxAngle );
	saver.Add( 9, &bPlane );
	saver.Add( 10, &fSightPower );

	return 0;
}
int CGlobalWarFog::SUnitInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &fogInfo );
	saver.Add( 2, &nHeapPos );
	saver.Add( 3, &nParty );
/*
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	saver.Add( 4, &addedTiles );
	saver.Add( 5, &deletedTiles );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
*/
	return 0;
}
