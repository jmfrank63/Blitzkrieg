#ifndef __TERRAIN_H__
#define __TERRAIN_H__
#pragma ONCE
enum
{
	TERRAIN_BASE_VALUE = 0x10080000,
	TERRAIN_TERRAIN = TERRAIN_BASE_VALUE + 1
};
struct SSoundTerrainInfo
{
	class PrSoundsMassSort
	{
		public: bool operator()( const SSoundTerrainInfo &s1, const SSoundTerrainInfo &s2 )
					{ return s1.fWeight > s2.fWeight; }
	};

	class PrTerrainTypeSort
	{
		public: bool operator()( const SSoundTerrainInfo &s1, const SSoundTerrainInfo &s2 )
					{ return s1.nTerrainType < s2.nTerrainType; }
	};
	class PrZeroMass
	{
		public: bool operator()( const SSoundTerrainInfo &s1 )
					{ return s1.fWeight == 0.0f ; }
	};
	
	CVec2 vPos;														// position of mass center of terrain
	float fWeight;												// weight of this terrain on screen
	int nTerrainType;											// type of this terrain
	SSoundTerrainInfo() : fWeight( 0.0f ), vPos( VNULL2 ), nTerrainType( -1 ) {}
};
interface ITerrain : public IRefCount
{
	virtual void STDCALL Init( interface ISingleton *pSingleton ) = 0;
	virtual void STDCALL ResetPosition() = 0;
	virtual int STDCALL GetSizeX() const = 0;
	virtual int STDCALL GetSizeY() const = 0;
	virtual int STDCALL GetPatchesX() const = 0;
	virtual int STDCALL GetPatchesY() const = 0;
	virtual float STDCALL GetHeight( const CVec2 &vPos ) = 0;
	virtual bool STDCALL Draw( interface ICamera *pCamera ) = 0;
	virtual bool STDCALL DrawVectorObjects() = 0;
	virtual bool STDCALL DrawMarkers() = 0;
	virtual void STDCALL DrawBorder( DWORD dwColor, int nTiles, bool bUseFog ) = 0;
	virtual bool STDCALL DrawWarFog() = 0;
	virtual void STDCALL SetWarFog( struct SAIVisInfo *vises, int nNumVises ) = 0;
	virtual bool STDCALL EnableGrid( bool bGridOn ) = 0;
	virtual bool STDCALL EnableNoise( bool bEnable ) = 0;
	virtual bool STDCALL Load( const char *pszName, const struct STerrainInfo &terrainInfo ) = 0;
	virtual void STDCALL SetAIMarker( struct SAIPassabilityInfo *infos, int nNumInfos ) = 0;


	virtual const char* STDCALL GetTerrainSound( int nTerrainType ) = 0;

	virtual const char* STDCALL GetTerrainCycleSound( int nTerrainType ) = 0;
	virtual void STDCALL GetTerrainMassData( SSoundTerrainInfo **ppData, int *pnSize ) = 0;
	virtual float STDCALL GetSoundVolume( int nTerrainType ) const = 0 ;
};
interface ITerrainEditor
{
	virtual bool STDCALL Import( interface IImage *pImage ) = 0;
	virtual interface IImage* STDCALL Export() = 0;
	virtual bool STDCALL GetTileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact = false ) = 0;
	virtual bool STDCALL GetAITileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact = false ) = 0;
	virtual void STDCALL SetTile( int x, int y, BYTE tile ) = 0;
	virtual BYTE STDCALL GetTile( int x, int y ) = 0;
	virtual void STDCALL SetShade( int x, int y, BYTE shade ) = 0;
	virtual BYTE STDCALL GetShade( int x, int y ) = 0;
	virtual void STDCALL Update( const CTRect<int> &rcPatches ) = 0;
	virtual void STDCALL SetMarker( const CTPoint<int> *pPoints, int nNumPoints ) = 0;
	virtual void STDCALL SampleCurve( const CVec3 *plots, int nNumPlots, float fStep, 
		                                struct SVectorStripeObjectPoint **ppSamples, int *pnNumSamples ) = 0;
	virtual void STDCALL SmoothCurveWidth( SVectorStripeObjectPoint *points, const int nNumPoints ) = 0;
	virtual int STDCALL AddRiver( const struct SVectorStripeObject &river ) = 0;
	virtual bool STDCALL UpdateRiver( const int nID ) = 0;
	virtual bool STDCALL RemoveRiver( const int nID ) = 0;
	virtual int STDCALL AddRoad( const struct SVectorStripeObject &road ) = 0;
	virtual bool STDCALL UpdateRoad( const int nID ) = 0;
	virtual bool STDCALL RemoveRoad( const int nID ) = 0;
	virtual const struct STerrainInfo& STDCALL GetTerrainInfo() const = 0;
	virtual const struct STilesetDesc& STDCALL GetTilesetDesc() const = 0;
	virtual const struct SCrossetDesc& STDCALL GetCrossetDesc() const = 0;
};
inline ITerrain* CreateTerrain()
{
	ITerrain *pTerrain = static_cast<ITerrain*>( GetCommonFactory()->CreateObject( TERRAIN_TERRAIN ) );
	pTerrain->Init( GetSingletonGlobal() );
	return pTerrain;
}
#endif // __TERRAIN_H__
