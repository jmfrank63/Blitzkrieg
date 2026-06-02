#ifndef __TERRAINWATER_H__
#define __TERRAINWATER_H__
#pragma ONCE
#include "VectorObject.h"
struct STerrainRiver
{
	STVOLayer bottom;
	std::vector<STVOLayer> layers;
	int operator&( IStructureSaver &ss );
};
class CTerrainWater : public CTerrainVectorObject
{
	DECLARE_SERIALIZE;
	STerrainRiver river;
public:
	void Init( const SVectorStripeObject &_riverInfo, interface ITerrain *pTerrain );
	void BuildLayers();
	void SelectPatches( const std::vector<DWORD> &sels );
	bool DrawBase( interface IGFX *pGFX ) const;
	bool DrawWater( interface IGFX *pGFX ) const;
};
#endif // __TERRAINWATER_H__
