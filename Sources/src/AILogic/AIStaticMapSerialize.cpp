#include "StdAfx.h"

#include "AIStaticMap.h"
int CStaticMap::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	if ( saver.IsReading() )
		CommonInit();

	saver.Add( 19, &unitsBuf );
	saver.Add( 42, &unitsRects );
	saver.Add( 43, &nSizeX );
	saver.Add( 44, &nSizeY );
	saver.Add( 45, &nCellsSizeX );
	saver.Add( 46, &nCellsSizeY );
	saver.Add( 47, &nBigCellsSizeX );
	saver.Add( 48, &nBigCellsSizeY );
	saver.Add( 50, &transparency );
	saver.Add( 51, &passTypes );
	saver.Add( 52, &passabilities );
	saver.Add( 53, &eMode );
	saver.Add( 54, &eMemMode );

	saver.Add( 57, &buf[0] );
	saver.Add( 58, &buf[1] );
	saver.Add( 59, &buf[2] );
	saver.Add( 60, &buf[3] );
	saver.Add( 61, &buf[4] );

	saver.Add( 62, &maxes[0][0] );
	saver.Add( 63, &maxes[0][1] );
	saver.Add( 64, &maxes[0][2] );
	saver.Add( 65, &maxes[0][3] );
	saver.Add( 66, &maxes[0][4] );

	saver.Add( 67, &maxes[1][0] );
	saver.Add( 68, &maxes[1][1] );
	saver.Add( 69, &maxes[1][2] );
	saver.Add( 70, &maxes[1][3] );
	saver.Add( 71, &maxes[1][4] );
	
	saver.Add( 72, &betaSpline3D );
	saver.Add( 73, &heights );
	saver.Add( 74, &tileHeights );
	saver.Add( 75, &terrainTypes );
	
	saver.Add( 76, &oneWayDirs );
	saver.Add( 77, &classToIndex );
	
	saver.Add( 78, &tmpUnlockID );
	saver.Add( 79, &tmpUnlockUnitsBuf );

	saver.Add( 80, &passClasses );
	saver.Add( 81, &terrSubTypes );
	saver.Add( 82, &entrenchPossibility );
	
	saver.Add( 83, &bridgeTiles );
	if ( saver.IsReading() )
	{
		if ( bridgeTiles.GetSizeX() < GetSizeX() || bridgeTiles.GetSizeY() < GetSizeY() )
		{
			bridgeTiles.SetSizes( GetSizeX(), GetSizeY() );
			bridgeTiles.SetZero();
		}
	}
	
	saver.Add( 84, &soil );
	if ( saver.IsReading() )
	{
		if ( soil.GetSizeX() < GetSizeX() || soil.GetSizeY() < GetSizeY() )
		{
			soil.SetSizes( GetSizeX(), GetSizeY() );
			soil.SetZero();
		}
	}

	return 0;
}
