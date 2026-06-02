#include "StdAfx.h"

#include "common.h"

/**
void FillTileMaps( int nSizeX, int nSizeY, vector<STileMapsDesc> &tileMaps, bool bGenerateInverse )
{
	int nNumColumn = 0;
	for ( int i=0; i<nSizeY-32; i+=32 )
	{
		CVec2 maps[4];
		STileMapsDesc tileMap;
		
		for ( int k=0; k<4; k++ )
		{
			GetPrimaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetPrimaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}
		
		for ( int k=0; k<3; k++ )
		{
			GetSecondaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetSecondaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}

		nNumColumn++;
	}
	
	{
		CVec2 maps[4];
		STileMapsDesc tileMap;
		
		for ( int k=0; k<4; k++ )
		{
			GetPrimaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetPrimaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}
	}
}
/**/