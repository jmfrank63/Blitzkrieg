#ifndef __TERRAININTERNAL_H__
#define __TERRAININTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Terrain.h"
#include "Builders.h"
#include "..\AILogic\AITypes.h"
#include "TerrainWater.h"
#include "TerrainRoad.h"
#include "TerrainBuilder.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainPatch
{
#ifdef _USE_HWTL	
	typedef STerrainLVertex SVertex1;
	typedef SGFXLVertex SVertex2;
#else
	typedef STerrainTLVertex SVertex1;
	typedef SGFXTLVertex SVertex2;
#endif // _USE_HWTL
	typedef std::vector<SVertex1> CVertex1List;
	typedef std::vector<SVertex2> CVertex2List;
	typedef std::vector<WORD> CIndexList;
	// main vertices
	CVertex1List mainverts1;							// main verts with noise
	CVertex2List mainverts2;							// main verts w/o noise
	CVertex1List basecrossverts;					// base cross vertices
	std::vector<CVertex1List> layercrossverts;	// layered crosses
	std::vector<CVertex1List> layernoiseverts;	// layered noises
	CVertex2List noiseverts;							// noise w/o crosses
	CVertex2List warfogverts;							// fog of war (line of sight)
	CIndexList warfoginds;								// indices for fog of war
	int nX, nY;
	bool bSubPatches[4];									//
	//
	void Clear()
	{
		mainverts1.clear();
		mainverts2.clear();
		basecrossverts.clear();
		layercrossverts.clear();
		layernoiseverts.clear();
		noiseverts.clear();
		warfogverts.clear();
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef SGFXTLVertex SMarkerVertex;
struct SVisMarker
{
	std::vector< CTPoint<int> > info;
	std::vector<SMarkerVertex> vertices;
	std::vector<WORD> indices;
	//
	void Clear()
	{
		info.clear();
		vertices.clear();
		indices.clear();
	}
};
struct SAIMarker
{
	std::vector<SAIPassabilityInfo> info;
	std::vector<SMarkerVertex> vertices;
	std::vector<WORD> indices;
	//
	void Clear()
	{
		info.clear();
		vertices.clear();
		indices.clear();
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVertex>
struct STerraMesh
{
	std::vector<TVertex> vertices;
	std::vector<WORD> indices;
	//
	void Clear() { vertices.clear(); indices.clear(); }
	void Reserve( const int nNumVertices, const int nNumIndices )
	{
		vertices.clear();
		vertices.reserve( nNumVertices );
		indices.clear();
		indices.reserve( nNumIndices );
	}
	const bool IsEmpty() const { return vertices.empty() || indices.empty(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainCurrMeshData
{
	struct SCrossesLayer
	{
		STerraMesh<STerrainPatch::SVertex1> mshCrosses;
		STerraMesh<STerrainPatch::SVertex1> mshNoises;
		//
		const bool IsEmpty() const { return mshCrosses.IsEmpty() || mshNoises.IsEmpty(); }
	};
	//
	STerraMesh<STerrainPatch::SVertex1> mshNoiseTiles;		// main tiles with noise
	STerraMesh<STerrainPatch::SVertex2> mshNoNoiseTiles;	// main tiles w/o noise
	STerraMesh<STerrainPatch::SVertex1> mshBaseCrosses;		// base crosses
	std::vector<SCrossesLayer> mshCrossLayers;						// layered crosses
	STerraMesh<STerrainPatch::SVertex2> mshNoises;				// noise over crosses
	STerraMesh<STerrainPatch::SVertex2> mshWarFog;				// warfog
	//
	bool Draw( IGFX *pGFX, IGFXTexture *pTileset, IGFXTexture *pCrosset, IGFXTexture *pNoise, bool bEnableNoise );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
__forceinline DWORD GetVisibilityColor( const DWORD dwKey, const std::unordered_map<DWORD, DWORD> &visibilities )
{
	std::unordered_map<DWORD, DWORD>::const_iterator pos = visibilities.find( dwKey );
	return pos != visibilities.end() ? DWORD( ( 112UL - (DWORD(pos->second) << 4) ) << 24 ) : 112UL << 24;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
__forceinline DWORD GetVisibilityColor( const int nX, const int nY, const std::unordered_map<DWORD, DWORD> &visibilities )
{
	return GetVisibilityColor( ( DWORD( nY ) << 16 ) | DWORD( nX ), visibilities );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// terrain objects by layers
//
// * terrain configuration (hills, rocks, etc.)
// * roads
// * explosion craters & corpses (in order of appearing) <= dynamic layer
// * shadows
//
class CTerrain : public ITerrain, public ITerrainEditor
{
	OBJECT_NORMAL_METHODS( CTerrain );
	DECLARE_SERIALIZE;
	// shortcuts
	CPtr<IGFX> pGFX;
	CPtr<ITextureManager> pTM;
	CPtr<IDataStorage> pStorage;
	// 
	std::string szMapName;								// terrain map name
	//
	STilesetDesc tilesetDesc;							// tileset descriptor
	SCrossetDesc crossetDesc;							// crosset descriptor
	//SRoadsetDesc roadsetDesc;							// roadset descriptor
	CPtr<IGFXTexture> pTileset, pCrosset;	// tileset and crosset textures
	CPtr<IGFXTexture> pNoise;							// noise texture
	int nTilesetSizeX, nTilesetSizeY;			// tileset texture size
	int nCrossetSizeX, nCrossetSizeY;			// crosset texture size
	int nNoiseSizeX, nNoiseSizeY;					// noise texture size
	//CPtr<IGFXTexture> roadsets[nNumRoadTypes];	// roadset textures
	//
	STerrainInfo terrainInfo;							// current terrain packed info
	std::vector<CTerrainWater> rivers;		// mesh rivers
	std::vector<CTerrainRoad> roads;			// mesh roads
	SVisMarker vismarker;									// terrain vis marker
	SAIMarker aimarker;										// terrain AI marker
	typedef std::list<STerrainPatch> CPatchesList;	
	CPatchesList patches;									// current active patches (mesh)
	std::vector<DWORD> roadPatches;				// patches for vector objects
	bool bGridOn;													// turn grid on
	bool bEnableNoise;										// enable noise drawing
	CVec3 vOldAnchor;											// old camera's anchor for re-positioning
	//
	STerrainCurrMeshData mshCurrent;			// current mesh data
	// visibilities
	std::unordered_map<DWORD, DWORD> visibilities;
	// terrain editor fields
	CTerrainBuilder terrabuild;
	// for terrain sound info
	std::vector<SSoundTerrainInfo> collectedInfo;
	//
	void CreatePatch( const STerrainPatchInfo &patch, STerrainPatch *pPatch );
	void MovePatch( int nX, int nY, const STerrainPatchInfo &patch, STerrainPatch *pPatch );
	void MoveWarFog();
	void MoveWarFogPatch( int nX, int nY, STerrainPatch *pPatch );
	bool ExtractVisiblePatches( ICamera *pCamera );
	void MovePatches();
	void ReBuildMeshes();
	void Clear();
	void ReservePatchesData();
	//
	void LoadLocal( const std::string &szName, const STerrainInfo &terrainInfo, bool bMinimizeRoadsets, bool bLoadTextures );
	//
	void DrawMarker();
	bool DrawGrid( const STerrainPatch &patch );
	bool DrawAISurface( const STerrainPatch &patch );
	void FillSoundInfo( std::vector<SSoundTerrainInfo>  *collectedInfo, const int nX, const int nY );
	void DrawPatchBorder( CVec3 &vStartPos, const CVec3 &vFarOffset, const CVec3 &vStepOffset, bool bRightOrder, bool bXIncrement, int nXStart, int nYStart, DWORD dwColor, bool bUseFog );
public:
	CTerrain() : bGridOn( false ), bEnableNoise( true ), terrabuild( tilesetDesc, crossetDesc/*, roadsetDesc*/ ) {  }
	// initialization
	virtual void STDCALL Init( ISingleton *pSingleton );
	virtual void STDCALL ResetPosition() { vOldAnchor.Set( -1000000, -1000000, -1000000 ); }
	// sizes
	virtual int STDCALL GetSizeX() const { return terrainInfo.tiles.GetSizeX(); }
	virtual int STDCALL GetSizeY() const { return terrainInfo.tiles.GetSizeY(); }
	virtual int STDCALL GetPatchesX() const { return terrainInfo.patches.GetSizeX(); }
	virtual int STDCALL GetPatchesY() const { return terrainInfo.patches.GetSizeY(); }
	// height in the point
	virtual float STDCALL GetHeight( const CVec2 &vPos );
	// drawing
	virtual bool STDCALL Draw( ICamera *pCamera );
	virtual bool STDCALL DrawWarFog();
	virtual bool STDCALL DrawVectorObjects();
	virtual bool STDCALL DrawMarkers();
	virtual void STDCALL DrawBorder( DWORD dwColor, int nTiles, bool bUseFog );
	virtual void STDCALL SetWarFog( struct SAIVisInfo *vises, int nNumVises );
	// enables
	virtual bool STDCALL EnableGrid( bool _bGridOn ) { bool bOld = bGridOn; bGridOn = _bGridOn; return bOld; }
	virtual bool STDCALL EnableNoise( bool bEnable ) { bool bOld = bEnableNoise; bEnableNoise = bEnable; return bOld; }
	//
	virtual bool STDCALL Load( const char *pszName, const struct STerrainInfo &terrainInfo );
	// markers
	virtual void STDCALL SetAIMarker( SAIPassabilityInfo *infos, int nNumInfos );
	// import/export � ��������
	virtual bool STDCALL Import( interface IImage *pImage );
	virtual interface IImage* STDCALL Export();
	// editor part
	virtual bool STDCALL GetTileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact = false );
	virtual bool STDCALL GetAITileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact = false );
	virtual void STDCALL SetTile( int x, int y, BYTE tile );
	virtual BYTE STDCALL GetTile( int x, int y );

	virtual void STDCALL SetShade( int x, int y, BYTE shade );
	virtual BYTE STDCALL GetShade( int x, int y );

	virtual void STDCALL Update( const CTRect<int> &rcPatches );
	virtual void STDCALL SetMarker( const CTPoint<int> *pPoints, int nNumPoints );
	//virtual void STDCALL SetRoads( const SRoadItem *pItems, int nNumItems );
	// rivers & roads
	virtual void STDCALL SampleCurve( const CVec3 *plots, int nNumPlots, float fStep, 
		                                SVectorStripeObjectPoint **ppSamples, int *pnNumSamples );
	virtual void STDCALL SmoothCurveWidth( SVectorStripeObjectPoint *points, const int nNumPoints );
	virtual int STDCALL AddRiver( const SVectorStripeObject &river );
	virtual bool STDCALL UpdateRiver( const int nID );
	virtual bool STDCALL RemoveRiver( const int nID );
	virtual int STDCALL AddRoad( const struct SVectorStripeObject &road );
	virtual bool STDCALL UpdateRoad( const int nID );
	virtual bool STDCALL RemoveRoad( const int nID );
	// �������� ���������� ��������� �������� ��� ���������
	virtual const struct STerrainInfo& STDCALL GetTerrainInfo() const { return terrainInfo; }
	// ��������� �����
	virtual const struct STilesetDesc& STDCALL GetTilesetDesc() const { return tilesetDesc; }
	virtual const struct SCrossetDesc& STDCALL GetCrossetDesc() const { return crossetDesc; }
	//virtual const struct SRoadsetDesc& STDCALL GetRoadsetDesc() const { return roadsetDesc; }
	
	virtual const char* STDCALL GetTerrainSound( int nTerrainType );
	virtual const char* STDCALL GetTerrainCycleSound( int nTerrainType );
	virtual void STDCALL GetTerrainMassData( SSoundTerrainInfo **ppData, int *pnSize );
	virtual float STDCALL GetSoundVolume( int nTerrainType ) const ;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TERRAININTERNAL_H__