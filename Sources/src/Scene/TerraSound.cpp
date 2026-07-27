#include "StdAfx.h"

#include "TerrainInternal.h"
const char* CTerrain::GetTerrainSound( int nTerrainType )
{
	return tilesetDesc.terrtypes[nTerrainType].szSound.c_str();
}
float CTerrain::GetSoundVolume( int nTerrainType ) const 
{
	return tilesetDesc.terrtypes[nTerrainType].fSoundVolume;
}
const char* CTerrain::GetTerrainCycleSound( int nTerrainType )
{
	return tilesetDesc.terrtypes[nTerrainType].szLoopedSound.c_str();
}
void CTerrain::GetTerrainMassData( SSoundTerrainInfo **ppData, int *pnSize )
{
	if ( *pnSize == 0 )
		return;

	collectedInfo.clear();
	collectedInfo.resize( tilesetDesc.terrtypes.size() );
	
	for ( std::list<STerrainPatch>::const_iterator it = patches.begin(); it != patches.end(); ++it )
	{
		const int nPatchX = (*it).nX * STerrainPatchInfo::nSizeX;
		const int nPatchY = (*it).nY * STerrainPatchInfo::nSizeY;

		for ( int nTileY = 0 ; nTileY< STerrainPatchInfo::nSizeY; ++nTileY )
			for ( int nTileX = 0; nTileX < STerrainPatchInfo::nSizeX; ++nTileX )
				FillSoundInfo( &collectedInfo, nPatchX+nTileX, nPatchY+nTileY );
	}
	SSoundTerrainInfo::PrSoundsMassSort prMassSort;
	std::sort( collectedInfo.begin(), collectedInfo.end(), prMassSort );
	SSoundTerrainInfo::PrZeroMass prZeroMass;
	std::vector<SSoundTerrainInfo>::iterator firstZeromass = std::find_if( collectedInfo.begin(), collectedInfo.end(), prZeroMass );
	*pnSize = Min( *pnSize, static_cast<int>( firstZeromass-collectedInfo.begin() ) );
	collectedInfo.resize( *pnSize );
	SSoundTerrainInfo::PrTerrainTypeSort prTerrainType;
	std::sort( collectedInfo.begin(), collectedInfo.end(), prTerrainType );

	if ( *pnSize )
	{
		*ppData = GetTempBuffer<SSoundTerrainInfo>( *pnSize );
		for ( int i =0; i != collectedInfo.size(); ++i )
		{
			const SSoundTerrainInfo & info = collectedInfo[i];
			(*ppData)[i].fWeight = info.fWeight;
			(*ppData)[i].vPos = info.vPos/info.fWeight;
			(*ppData)[i].nTerrainType = info.nTerrainType ;
		}
	}
}
void CTerrain::FillSoundInfo( std::vector<SSoundTerrainInfo>  *collectedInfo, const int nX, const int nY )
{
	BYTE type = terrabuild.GetTerrainType( terrainInfo.tiles[nY][nX].tile );
	SSoundTerrainInfo &info = (*collectedInfo)[type];
	++info.fWeight;
	info.nTerrainType = type;
	info.vPos += CVec2( nX*fWorldCellSize, 
		fWorldCellSize * (STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY() - nY) );
}