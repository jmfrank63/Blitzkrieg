#ifndef __TERRAINBUILDER_H__
#define __TERRAINBUILDER_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Formats\fmtTerrain.h"
#include "..\Image\Image.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainBuilder
{
public:
	typedef std::list<SCrossTileInfo> CCrossesList;
	struct SComplexCrosses
	{
		CCrossesList base;									// base crosses
		std::vector<CCrossesList> layers;		// crosses with noise by layers 
		CCrossesList noise;									// noise only
		//
		void Clear()
		{
			base.clear();
			layers.clear();
			noise.clear();
		}
	};
private:
	const STilesetDesc &tileset;
	//const SRoadsetDesc &roadset;
	const SCrossetDesc &crosset;
	typedef std::unordered_map<char, int> CTypesMap;
	mutable CTypesMap terratypes;					// terrain type
	mutable CTypesMap crossettypes;				// cross set type
	mutable CTypesMap crosstypes;					// cross tile type
	// terrain builder
	void ConvertImageSegment( CImageAccessor &image, const CTRect<int> &rect ) const;
	void GenerateTiles( const CImageAccessor &image, STerrainInfo *pTerrainInfo ) const;
	int ComparePriority( BYTE r1, BYTE r2 ) const;
	BYTE SelectMinPriority( BYTE r1, BYTE r2 ) const { return ComparePriority( GetTerrainType(r1), GetTerrainType(r2) ) == -1 ? r1 : r2; }
	BYTE SelectMaxPriority( BYTE r1, BYTE r2 ) const { return ComparePriority( GetTerrainType(r1), GetTerrainType(r2) ) == -1 ? r2 : r1; }
	int GetNeighboursMask( const CArray2D<SMainTileInfo> &tiles, const int nX, const int nY, 
		                     const CTRect<int> &rect, const CTRect<int> &rcSuper, SComplexCrosses *pCrosses ) const;
	BYTE GetNeighboursMask( const CArray2D<SMainTileInfo> &tiles, const int nX, const int nY, 
		                      BYTE cSuper, const CTRect<int> &rect, const CTRect<int> &rcSuper ) const;
	void SetNoise( CArray2D<SMainTileInfo> &tiles, const CTRect<int> &rect ) const;
public:
	CTerrainBuilder( const STilesetDesc &_tileset, const SCrossetDesc &_crosset/*, const SRoadsetDesc &_roadset*/ ) 
		: tileset( _tileset ), crosset( _crosset )/* roadset( _roadset )*/{  }
	//
	const bool HasNoise( BYTE tile ) const;
	int GetTerrainType( BYTE tile ) const;
	int GetCrossType( BYTE tile ) const;
	int GetCrossetType( BYTE tile ) const;
	void GetPatchRect( int nX, int nY, CTRect<int> *pRect ) const;
	int PreprocessMapSegment( CArray2D<SMainTileInfo> &tiles, const CTRect<int> &rect ) const;
	int MapSegmentGenerateCrosses( CArray2D<SMainTileInfo> &tiles, 
		                             const CTRect<int> &rect, const CTRect<int> &rcSuper, 
																 SComplexCrosses *pCrosses ) const;
	void PreprocessMap( IImage *pImage, STerrainInfo *pTerrainInfo ) const;
	//
	void CopyCrosses( STerrainPatchInfo *pPatch, const SComplexCrosses &crosses ) const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TERRAINBUILDER_H__