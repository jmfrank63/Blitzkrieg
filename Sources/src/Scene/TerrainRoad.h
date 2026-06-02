#ifndef __TERRAINROAD_H__
#define __TERRAINROAD_H__
#pragma ONCE
#include "VectorObject.h"
struct STerrainRoad
{
	STVOLayer center;
	std::vector<STVOLayer> borders;
	int operator&( IStructureSaver &ss );
};
class CTerrainRoad : public CTerrainVectorObject
{
	DECLARE_SERIALIZE;
	STerrainRoad road;
public:
	void Init( const SVectorStripeObject &_roadInfo, interface ITerrain *pTerrain );
	void BuildLayers();
	void SelectPatches( const std::vector<DWORD> &sels );
	bool DrawBorder( interface IGFX *pGFX ) const;
	bool DrawCenter( interface IGFX *pGFX ) const;
	int GetPriority() const { return GetDesc().nPriority; }
};
inline bool operator<( const CTerrainRoad &road1, const CTerrainRoad &road2 )
{
	return road1.GetPriority() < road2.GetPriority();
}
#endif // __TERRAINROAD_H__
