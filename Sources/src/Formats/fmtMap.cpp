#include "StdAfx.h"

#include "fmtMap.h"

const int STerrainPatchInfo::nSizeX = 16;
const int STerrainPatchInfo::nSizeY = 16;
int SMapObjectInfo::SLinkInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "LinkID", &nLinkID );
	saver.Add( "Intention", &bIntention );
	saver.Add( "LinkWith", &nLinkWith );

	return 0;
}

SMapObjectInfo::SMapObjectInfo() 
: nDir( 0 ), nPlayer( -1 ), nScriptID( -1 ), fHP( 1 ), nFrameIndex( -1 ) 
{  
}

int SMapObjectInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Name", &szName );
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &nDir );
	saver.Add( "Player", &nPlayer );
	saver.Add( "ScriptID", &nScriptID );
	saver.Add( "HP", &fHP );
	saver.Add( "FrameIndex", &nFrameIndex );
	saver.Add( "Link", &link );

	return 0;
}

int SMapObjectInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szName );
	saver.Add( 2, &vPos );
	saver.Add( 3, &nDir );
	saver.Add( 4, &nPlayer );
	saver.Add( 5, &nScriptID );
	saver.Add( 6, &fHP );
	saver.Add( 7, &nFrameIndex );
	saver.Add( 8, &link );

	return 0;
}

int SEntrenchmentInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Sections", &sections );
	return 0;
}

int SEntrenchmentInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &sections );
	return 0;
}

int SCrossTileInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "x", &x );
	saver.Add( "y", &y );
	saver.Add( "tile", &tile );
	saver.Add( "cross", &cross );
	saver.Add( "flags", &flags );
	return 0;
}

int SRoadTileInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "x", &x );
	saver.Add( "y", &y );
	saver.Add( "tile", &tile );
	return 0;
}

int SRoadItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "rect", &rect );
	saver.Add( "type", &nType );
	saver.Add( "dir", &nDir );
	return 0;
}

int STerrainPatchInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "start_x", &nStartX );
	saver.Add( "start_y", &nStartY );
	saver.Add( "BaseCrosses", &basecrosses );
	saver.Add( "LayerCrosses", &layercrosses );
	saver.Add( "NoiseCrosses", &noisecrosses );
	return 0;
}

int STerrainInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "tileset", &szTilesetDesc );
	saver.Add( "crosset", &szCrossetDesc );
	saver.Add( "noise", &szNoise );
	saver.Add( "patches", &patches );
	saver.Add( "tiles", &tiles );	
	saver.Add( "Rivers", &rivers );
	saver.Add( "Roads", &roads3 );
	saver.Add( "Altitudes", &altitudes );
	
	return 0;
}

int STerrainPatchInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &nStartX );
	saver.Add( 2, &nStartY );
	saver.Add( 3, &basecrosses );
	saver.Add( 4, &layercrosses );
	saver.Add( 5, &noisecrosses );
	
	return 0;
}

int STerrainInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &szTilesetDesc );
	saver.Add( 2, &szCrossetDesc );
	saver.Add( 4, &patches );
	saver.Add( 5, &tiles );	
	saver.Add( 7, &rivers );
	saver.Add( 8, &roads3 );
	saver.Add( 9, &altitudes );
	saver.Add( 10, &szNoise );
	
	return 0;
}
void STerrainInfo::FillMinMaxHeights()
{
	for ( int i = 0; i < patches.GetSizeY(); ++i )
	{
		for ( int j = 0; j < patches.GetSizeX(); ++j )
		{
			STerrainPatchInfo &patch = patches[i][j];
			patch.fSubMinHeight[0] = patch.fSubMinHeight[1] = patch.fSubMinHeight[2] = patch.fSubMinHeight[3] = 1e6f;
			patch.fSubMaxHeight[0] = patch.fSubMaxHeight[1] = patch.fSubMaxHeight[2] = patch.fSubMaxHeight[3] = -1e6f;
			for ( int m = 0; m <= 9; ++m )
			{
				for ( int n = 0; n <= 9; ++n )
				{
					const float fAltitude = altitudes[patch.nStartY + m][patch.nStartX + n].fHeight;
					patch.fSubMinHeight[0] = Min( patch.fSubMinHeight[0], fAltitude );
					patch.fSubMaxHeight[0] = Max( patch.fSubMaxHeight[0], fAltitude );
				}
				for ( int n = 7; n <= 16; ++n )
				{
					const float fAltitude = altitudes[patch.nStartY + m][patch.nStartX + n].fHeight;
					patch.fSubMinHeight[1] = Min( patch.fSubMinHeight[1], fAltitude );
					patch.fSubMaxHeight[1] = Max( patch.fSubMaxHeight[1], fAltitude );
				}
			}
			for ( int m = 7; m <= 16; ++m )
			{
				for ( int n = 0; n <= 9; ++n )
				{
					const float fAltitude = altitudes[patch.nStartY + m][patch.nStartX + n].fHeight;
					patch.fSubMinHeight[2] = Min( patch.fSubMinHeight[2], fAltitude );
					patch.fSubMaxHeight[2] = Max( patch.fSubMaxHeight[2], fAltitude );
				}
				for ( int n = 7; n <= 16; ++n )
				{
					const float fAltitude = altitudes[patch.nStartY + m][patch.nStartX + n].fHeight;
					patch.fSubMinHeight[3] = Min( patch.fSubMinHeight[3], fAltitude );
					patch.fSubMaxHeight[3] = Max( patch.fSubMaxHeight[3], fAltitude );
				}
			}
			patch.fMinHeight = Min( Min( patch.fSubMinHeight[0], patch.fSubMinHeight[1] ),
				                      Min( patch.fSubMinHeight[2], patch.fSubMinHeight[3] ) );
			patch.fMaxHeight = Max( Max( patch.fSubMaxHeight[0], patch.fSubMaxHeight[1] ),
				                      Max( patch.fSubMaxHeight[2], patch.fSubMaxHeight[3] ) );
		}
	}
}
const std::string SScriptArea::names[2] = { "Rectangle", "Circle" };
int SScriptArea::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	std::string szType;
	if ( saver.IsReading() )
	{
		saver.Add( "AreaType", &szType );
		if ( szType == names[0] )
			eType = SScriptArea::EAT_RECTANGLE;
		else if ( szType == names[1] )
			eType = SScriptArea::EAT_CIRCLE;
		else
			NI_ASSERT_TF( false, NStr::Format( "Wrong area type (%s)", szType.c_str() ), return 0 );
	}
	else
	{
		szType = names[eType];
		saver.Add( "AreaType", &szType );
	}

	saver.Add( "Center", &center );
	saver.Add( "AABBHalfSize", &vAABBHalfSize );
	saver.Add( "Radius", &fR );
	saver.Add( "Name", &szName );
	
	return 0;
}

int SScriptArea::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &eType );
	saver.Add( 2, &center );
	saver.Add( 3, &vAABBHalfSize );
	saver.Add( 4, &fR );
	saver.Add( 5, &szName );
	
	return 0;
}

int SReinforcementGroupInfo::SGroupsVector::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Groups", &ids );
	return 0;
}

int SReinforcementGroupInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Reinforcements", &groups );
	return 0;
}

int SReinforcementGroupInfo::SGroupsVector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &ids );
	return 0;
}

int SReinforcementGroupInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &groups );
	return 0;
}
