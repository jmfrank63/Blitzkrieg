#include "StdAfx.h"

#include "fmtTerrain.h"
int STileMapsDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "maps0", &( maps[0] ) );
	saver.Add( "maps1", &( maps[1] ) );
	saver.Add( "maps2", &( maps[2] ) );
	saver.Add( "maps3", &( maps[3] ) );
	return 0;
}
int SMainTileDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "index", &nIndex );
	if ( saver.IsReading() )
		saver.Add( "probability", &fProbFrom );
	else
	{
		float fProbability = fProbTo - fProbFrom;
		saver.Add( "probability", &fProbability );
	}
	return 0;
}
int STileTypeDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "tiles", &tiles );
	if ( saver.IsReading() )
	{
		float fTotal = 0;
		for ( std::vector<SMainTileDesc>::const_iterator it = tiles.begin(); it != tiles.end(); ++it )
			fTotal += it->fProbFrom;
		const float fCoeff = 100.0f / fTotal;
		fTotal = 0;
		for ( std::vector<SMainTileDesc>::iterator it = tiles.begin(); it != tiles.end(); ++it )
		{
			const float fProbability = it->fProbFrom * fCoeff;
			it->fProbFrom = fTotal;
			it->fProbTo = it->fProbFrom + fProbability;
			fTotal = it->fProbTo;
		}
	}
	return 0;
}
/*int STerrTypeDesc::STerrainLoopedSound::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Name", &szName );
	saver.Add( "Peaceful", &bPeaceful );
	return 0;
}
int STerrTypeDesc::STerrainSound::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Name", &szName );
	saver.Add( "Probability", &fProbability );
	if ( saver.IsReading() )
		fProbability /= 100.0f;
	saver.Add( "Peaceful", &bPeaceful );
	return 0;
}*/
int STerrTypeDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "name", &szName );
	saver.AddTypedSuper( static_cast<STileTypeDesc*>(this) );
	saver.Add( "crosset", &nCrosset );
	saver.Add( "priority", &nPriority );
	saver.Add( "passability", &fPassability );
	saver.Add( "AIClasses", &dwAIClasses );
	saver.Add( "MicroTexture", &bMicroTexture );
	saver.Add( "SoundVolume", &fSoundVolume );
	saver.Add( "CanEntrench", &bCanEntrench );
	saver.Add( "LoopedAmbientSound", &szLoopedSound );
	saver.Add( "AmbientSound", &szSound );
	saver.Add( "SoilParams", &cSoilParams );

	return 0;
}
int STilesetDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "name", &szName );
	saver.Add( "terrtypes", &terrtypes );
	saver.Add( "tilemaps", &tilemaps );
	return 0;
}
/**
int SRoadTileTypeDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<STileTypeDesc*>(this) );
	return 0;
}
int SRoadDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "name", &szName );
	saver.Add( "roadtiles", &tiles );
	saver.Add( "priority", &nPriority );
	return 0;
}
int SRoadsetDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "roads", &roads );
	saver.Add( "tilemaps", &tilemaps );
	return 0;
}
/**/
int SCrossTileTypeDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<STileTypeDesc*>(this) );
	saver.Add( "name", &szName );
	return 0;
}
int SCrossDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "name", &szName );
	saver.Add( "crosstiles", &tiles );
	return 0;
}
int SCrossetDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "crosses", &crosses );
	saver.Add( "tilemaps", &tilemaps );
	return 0;
}
