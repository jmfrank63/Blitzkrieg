#include "StdAfx.h"

#include "TerrainWater.h"
#include "TerrainInternal.h"
#include "..\GFX\GFXHelper.h"
int STerrainRiver::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bottom );
	saver.Add( 2, &layers );
	return 0;
}
void CTerrainWater::Init( const SVectorStripeObject &_riverInfo, ITerrain *pTerrain )
{
	CTerrainVectorObject::Init( _riverInfo );
	BuildLayers();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	river.bottom.textures.clear();
	river.bottom.textures.push_back( pTM->GetTexture( GetDesc().bottom.szTexture.c_str() ) );
	for ( int i = 0; i != river.layers.size(); ++i )
	{
		river.layers[i].textures.clear();
		if ( GetDesc().layers[i].bAnimated )
		{
			for ( int j = 0; j != 8; ++j )
				river.layers[i].textures.push_back( pTM->GetTexture( NStr::Format("%s%.2d", GetDesc().layers[i].szTexture.c_str(), j + 1) ) );
		}
		else
			river.layers[i].textures.push_back( pTM->GetTexture( GetDesc().layers[i].szTexture.c_str() ) );
	}
} 
void CTerrainWater::BuildLayers()
{
	CreateRiver( GetDesc(), &river );
}
void CTerrainWater::SelectPatches( const std::vector<DWORD> &sels )
{
	river.bottom.SelectPatches( sels, GetDesc().points.size() );
	for ( std::vector<STVOLayer>::iterator layer = river.layers.begin(); layer != river.layers.end(); ++layer )
		layer->SelectPatches( sels, GetDesc().points.size() );
}
bool CTerrainWater::DrawBase( IGFX *pGFX ) const
{
	pGFX->SetTexture( 0, river.bottom.textures[0] );
	::DrawTemp( pGFX, river.bottom.vertices, river.bottom.indices );
	return true;
}
bool CTerrainWater::DrawWater( IGFX *pGFX ) const
{
	SHMatrix matTranslate = MONE;
	const NTimer::STime timeCurr = GetSingleton<IGameTimer>()->GetAbsTime();
	for ( int i = 0; i != river.layers.size(); ++i )
	{
		const float fTimeCoeff = GetDesc().layers[i].fStreamSpeed != 0 ? 1000.0f / GetDesc().layers[i].fStreamSpeed : 1.0f;
		const float fTranslate = GetDesc().layers[i].fStreamSpeed != 0 ? float( fmod( timeCurr, fTimeCoeff ) ) / fTimeCoeff : 0;
		matTranslate._13 = -fTranslate;
		pGFX->SetTextureTransform( 0, matTranslate );
		if ( GetDesc().layers[i].bAnimated )
		{
			const int nFrame = 7 - ( timeCurr / 70 ) % 8;
			pGFX->SetTexture( 0, river.layers[i].textures[nFrame] );
		}
		else
			pGFX->SetTexture( 0, river.layers[i].textures[0] );
		::DrawTemp( pGFX, river.layers[i].vertices, river.layers[i].indices );
	}
	return true;
}
int CTerrainWater::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CTerrainVectorObject*>(this) );
	saver.Add( 2, &river );
	return 0;
}
