#include "StdAfx.h"

#include "TerrainRoad.h"
#include "TerrainInternal.h"
#include "..\GFX\GFXHelper.h"
int STerrainRoad::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &center );
	saver.Add( 2, &borders );
	return 0;
}
void CTerrainRoad::Init( const SVectorStripeObject &_roadInfo, ITerrain *pTerrain )
{
	CTerrainVectorObject::Init( _roadInfo );
	NI_ASSERT_T( GetDesc().bottomBorders.empty() || (GetDesc().bottomBorders.size() == 2), "Wrong number of borders" );
	BuildLayers();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	road.center.textures.clear();
	road.center.textures.push_back( pTM->GetTexture(GetDesc().bottom.szTexture.c_str()) );
	if ( GetDesc().bottomBorders.size() == 2 ) 
	{
		road.borders.resize( 2 );
		road.borders[0].textures.clear();
		road.borders[1].textures.clear();
		road.borders[0].textures.push_back( pTM->GetTexture(GetDesc().bottomBorders[0].szTexture.c_str()) );
		road.borders[1].textures.push_back( pTM->GetTexture(GetDesc().bottomBorders[1].szTexture.c_str()) );
	}
	else
		road.borders.clear();
} 
void CTerrainRoad::BuildLayers()
{
	CreateRoad( GetDesc(), &road );
}
void CTerrainRoad::SelectPatches( const std::vector<DWORD> &sels )
{
	road.center.SelectPatches( sels, GetDesc().points.size() );
	for ( std::vector<STVOLayer>::iterator layer = road.borders.begin(); layer != road.borders.end(); ++layer )
		layer->SelectPatches( sels, GetDesc().points.size() );
}
bool CTerrainRoad::DrawBorder( IGFX *pGFX ) const
{
	for ( std::vector<STVOLayer>::const_iterator layer = road.borders.begin(); layer != road.borders.end(); ++layer )
	{
		pGFX->SetTexture( 0, layer->textures[0] );
		::DrawTemp( pGFX, layer->vertices, layer->indices );
	}
	return true;
}
bool CTerrainRoad::DrawCenter( IGFX *pGFX ) const
{
	pGFX->SetTexture( 0, road.center.textures[0] );
	::DrawTemp( pGFX, road.center.vertices, road.center.indices );
	return true;
}
int CTerrainRoad::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTerrainVectorObject*>(this) );
	saver.Add( 2, &road );
	return 0;
}
