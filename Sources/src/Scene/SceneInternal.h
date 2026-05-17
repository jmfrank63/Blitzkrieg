#ifndef __SCENEINTERNAL_H__
#define __SCENEINTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\SFX\SFX.h"
#include "..\UI\UI.h"
#include "..\AILogic\AITypes.h"
#include "FixedObjList.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleInfo
{
	CVec3 vPos;														// position in 3D space
	float fSize;													// half size
	DWORD color, specular;								// color and specular
	CTRect<float> maps;										// texture mapping coords
	float fAngle;													// rotation angle
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVisObjDesc
{
	EObjVisType vistype;									// visual type (sprite/mesh/effect)
	EObjGameType gametype;								// game type (unit/building/object/etc.)
	CGDBPtr<SGDBObjectDesc> pDesc;				// GDB descriptor
	bool bOutbound;												// is this object can be placed out of map bounds ?
	//
	int operator&( IStructureSaver &ss );
};
typedef std::unordered_map<IVisObj*, SVisObjDesc, SDefaultPtrHash> CVisObjDescMap;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SToolTip
{
	CPtr<IGFXText> pText;									// graphics text
	CTRect<long> rcRect;									// rect of this text
	DWORD dwBorderColor;									// border rect color
	bool bHasText;												//
	bool bHasFont;
	//
	SToolTip() : dwBorderColor( 0xffffff00 ), bHasText( false ), bHasFont( false ) {  }
	//
	void Init();
	void Clear();
	//
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &pText );
		saver.Add( 2, &rcRect );
		saver.Add( 3, &bHasText );
		saver.Add( 4, &bHasFont );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STemporalMesh
{
	CPtr<IGFXVertices> pVertices;
	CPtr<IGFXIndices> pIndices;
	CPtr<IGFXTexture> pTexture;
	int nShadingEffect;
	bool bTemporal;
	int nIndex;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMeshPair2
{
	CPtr<IGFXTexture> pTexture;
	std::vector<BYTE> vertices;
	int nNumVertices;
	DWORD dwVertexFormat;
	std::vector<WORD> indices;
	EGFXPrimitiveType ePrimitiveType;
	int nShadingEffect;
	bool bTemporary;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRAP{ before minimap
struct SCircle
{
	CVec2 vCenter;
	float fRadius;
	NTimer::STime timeStart;
	NTimer::STime timeDuration;
};
// CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRainDrop
{
	CVec3 vPos;
	float fLength;
	SRainDrop() 
	{  
		fLength = 100000.0f;
	};
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSnowFlake
{
	CVec3 vPos;
	float fPhase;
	SSnowFlake()
	{
		vPos.z = -100.0f;
	};
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSandParticle
{
	CVec3 vPos;
	CVec3 vPhase;
	bool bCone;
	bool bConeDraw;
	SSandParticle()
	{
		vPos.z = -100.0f;
	};
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SClickMarker
{
	CVec3 vPos;
	NTimer::STime nStartTime;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::list<const SBasicSpriteInfo*> CSpriteVisList;
typedef std::list<IMeshVisObj*> CMeshVisList;
typedef std::list< CPtr<IMeshVisObj> > CMeshObjList;
typedef std::list< CPtr<IEffectVisObj> > CEffectObjList;
typedef std::list< CPtr<ISpriteVisObj> > CSpritesObjList;
typedef std::list<SParticleInfo> CParticlesVisList;
typedef std::list< CPtr<ISceneObject> > CSceneObjectsList;
typedef std::unordered_map<IGFXTexture*, CParticlesVisList, SDefaultPtrHash> CParticlesVisMap;

typedef CAreaMap<IObjVisObj> CObjectsArea;

typedef CAreaMap<ISpriteVisObj> CSpritesArea;
typedef CAreaMap<IObjVisObj> CObjVisObjArea;
typedef CAreaMap<IMeshVisObj> CMeshesArea;
typedef CAreaMap<IEffectVisObj> CEffectsArea;
typedef CAreaMap<IVisObj> CVisObjArea;
typedef CFixedObjAreaMap<4, IObjVisObj> CObjFixedArea;

typedef CStructAreaMap<SMechTrace> CMechTraceArea;
typedef CStructAreaMap<SGunTrace> CGunTraceArea;


static const float AREA_MAP_CELL_SIZE_IN_TILES = 8;
static const float AREA_MAP_CELL_SIZE = AREA_MAP_CELL_SIZE_IN_TILES * fWorldCellSize;

class CSoundScene;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CScene : public IScene
{
	OBJECT_COMPLETE_METHODS( CScene );
	DECLARE_SERIALIZE;
	// managers and LAPIs
	CPtr<IGFX> pGFX;											// main graphics LAPI
	CPtr<ISFX> pSFX;											// main sounds LAPI
	CPtr<IGameTimer> pTimer;							// main game timer
	CPtr<CSoundScene> pSoundScene;
	// scene data
	CVisObjDescMap objdescs;							// object descriptors
	//
	/*
	CObjectsArea areaUnits;								// units - update required
	CObjectsArea areaDynamics;						// dynamic objects (animated buildings and so on) - update required
	CObjectsArea areaStatics;							// static objects (buildings, trees, bushes, etc.)
	CObjectsArea areaTerraobjes;					// objects on the terrain
	CObjectsArea areaShadows;							// shadows
	CEffectsArea areaEffects;							// 
	*/
	// shell holes
	CObjFixedArea areaCraters;						// all craters in fixed area map
	// dynamic objects
	CObjVisObjArea areaUnits;							// sprite and mesh units
	CMeshesArea meshGraveyardArea;				// dead mesh units
	CVisObjArea effectsArea;							// all effects
	// static objects
	CSpritesArea spriteObjectsArea;				// sprite objects - buildings, trees, other stuff...
	// terrain objects
	CObjVisObjArea terraObjectsArea;			// terrain objects
	// shadows
	CSpritesArea shadowObjectsArea;				// shadow objects
	//
	CMeshObjList outboundObjects;					// objects, which are out of map bounds
	CMeshObjList outboundObjects2;				// ��������� ���������� � ��� � ���
	CEffectObjList outboundEffects;				// outbound effect objects
	CSpritesObjList outboundSprites;			// outbound sprite units
	CMechTraceArea mechTracesArea;			  // mech traces
	CGunTraceArea gunTracesArea;			    // gun traces
	CSceneObjectsList alwaysObjects;			// always visible objects
	// UI screens
	std::list< CPtr<IUIScreen> > uiScreens;
	CPtr<IUIScreen> pMissionScreen;
	// lines
	std::list< CPtr<IBoldLineVisObj> > boldLines;
	typedef std::list< CPtr<ISquadVisObj> > CSquadVisObjList;
	CSquadVisObjList squads;							// squad icons on the screen - 'who-in-the-container' interface
	// temporal meshes
	std::list<STemporalMesh> tempmeshes;	//
	std::list<SMeshPair2> meshpairs2;
	// CRAP{ circles
	std::list<SCircle> circles;
	// CRAP}
	//weather
	enum EStormType
	{
		ST_RAIN,
		ST_SNOW,
		ST_SAND
	};
	enum EStormCondition
	{
		SC_NONE,
		SC_STARTING,
		SC_ON,
		SC_FINISHING
	};
	CTRect<float> viewableTerrainRect;
	int nLastWeatherUpdate;
	bool bWeatherInitialized;
	EStormCondition eWeatherCondition;
	bool bWeatherOn;
	EStormType eCurrSetting;
	WORD wAmbientID;
	int nNextRandomSound;
	float fChangeSpeed;
	//rain
	int nRainDensity;
	std::vector<SRainDrop> rainDrops;
	CVec3 vRainDir;
	float fRainHeight;
	DWORD dwRainTopColor;
	DWORD dwRainBottomColor;
	//snow
	int nMinSnowDensity;
	int nMaxSnowDensity;
	std::vector<SSnowFlake> snowFlakes;
	float fSnowHeight;
	DWORD dwSnowColor;
	float fSnowFallingSpeed;
	float fSnowAmplitude;
	float fSnowFrequency;
	//sand
	int nSandDensity;
	std::vector<SSandParticle> sandParticles;
	float fSandHeight;
	CVec2 vSandCone;
	CVec2 vConeSpeed;
	float fConeRadius;
	int nLastConeGenerated;
	float fSandAmplitude;
	float fSandFrequency;
	CVec3 vSandWind;
	float fSandSpeed;
	float fSandConeSpeed;
	CPtr<IGFXTexture> pSandTexture;
	//markers drawing data
	bool bDrawArrow;
	bool bRotateMarkers;
	std::list<SClickMarker> clickMarkers;
	std::list<CVec3> posMarkers;
	float fArrowBegin;
	int nMarkerLifetime;
	CVec3 vArrowStart;
	CVec3 vArrowDir;
	DWORD dwArrowColor;
	DWORD dwMarkerColor;
	// range areas
	typedef std::vector<SShootAreas> CShootAreasList;
	CShootAreasList areas;
	//
	SToolTip tooltip;											// current tooltip
	//
	CPtr<ITerrain> pTerrain;
	CTPoint<int> vMapSize;								// map size (in world units)
	CPtr<ICursor> pCursor;
	// own objects
	CObj<IFrameSelection> pFrameSelection;// frame selection object
	CObj<IStatSystem> pStatSystem;				// statistics system
	// haze params
	bool bEnableHaze;											// depth of field emulation through haze
	DWORD dwHazeColorTop;									// haze color at the top of the area
	DWORD dwHazeColorBottom;							// haze color at the bottom of the area
	DWORD dwGunTraceColor;                // ���� ���������
	float fTraceLen;
	float fHazeHeight;										// haze height
	// enables:
	bool bEnableUnits;										// draw units
	bool bEnableObjects;									// ... objects
	bool bEnableBBs;											// bounding boxes
	bool bEnableEffects;									// effects
	bool bEnableTerrain;									// terrain
	bool bEnableNoise;										// add noise to terrain
	bool bEnableShadows;									// shadows
	bool bEnableGrid;											// grid on the terrain
	bool bEnableWarFog;										// fog'o'war
	bool bEnableDepthComplexity;					// scene depth complexity. VERY SLOW
	bool bEnableShowBorder;               // ��������� ������� �� �����
	bool bShowUI;													// show user interface
	// sprites drawing pipeline
	float fZBias;													// vertical z-bias to keep z-buffer happy
	float fZBias2;												// horizontal z-bias to keep z-buffer happy
	SHMatrix matTransform;								// world => screen transformation matrix
	NTimer::STime tTransformUpdateTime;		// last time of the transformation matrix update
	// CRAP{ // ���� ��������� ������� ��������
	SGFXLightDirectional sunlight;
	SGFXMaterial material;
	// CRAP}
	CPtr<IGFXTexture> pTrackTexture;      // �������� ��� ������ �� ������
	bool AddSpriteObject( ISpriteVisObj *pObj, EObjGameType eGameType );
	bool AddMeshObject( IMeshVisObj *pObj, EObjGameType eGameType );
	bool AddEffectObject( IVisObj *pObj, EObjGameType eGameType );
	bool RemoveSpriteObject( ISpriteVisObj *pObj, EObjGameType eGameType );
	bool RemoveMeshObject( IMeshVisObj *pObj, EObjGameType eGameType );
	bool RemoveEffectObject( IVisObj *pObj, EObjGameType eGameType );
	bool RemoveOutboundObject( IVisObj *pObj, EObjGameType eGameType );
	template <class TYPE>
		bool AddObjectToArea( IVisObj *pObj, CAreaMap<TYPE> &area )
		{
			if ( area.IsInArea(pObj->GetPosition()) ) 
			{
				area.Add( static_cast<TYPE*>(pObj) );
				return true;
			}
			else
			{
				objdescs.erase( pObj );
				pObj->AddRef();
				pObj->Release();
				return false;
			}
		}
	// visibility
	typedef std::list< std::pair<int, int> > CPatchesList;
	void SelectPatches( ICamera *pCamera, float fPatchesX, float fPatchesY, float fPatchSize, CPatchesList *pPatches );
	void SelectPatches2( const CVec3 &vCamera, const CVec2 &vCameraX, const CVec2 &vCameraY,
											 float fPatchesX, float fPatchesY, float fPatchSize, CPatchesList *pPatches );
	void FormVisibilityLists( ICamera *pCamera, ISceneVisitor *pVisitor );
	//
	void DrawSprites( CSpriteVisList &sprites );
	void DrawTerraObjects( CSpriteVisList &terraObjects );
	// particles
	void DrawParticles( const CParticlesVisList &particles );
	void DrawSingleParticlesPack( const CParticlesVisList &particles, int nNumParticles );
	// mech traces
	void DrawMechTraces( const std::list<SMechTrace> &traces );
	// gun traces
	void DrawGunTraces( const std::list<SGunTrace> &traces );
	void DrawMarkers();
	//
	void UpdateTransformMatrix();
	// set area maps size (in area cell units)
	void SetAreaMapSize( int nSizeX, int nSizeY );
	// set map size (in world units)
	void SetMapSize( int nSizeX, int nSizeY );
	//
	//void UpdateAttachedSounds( IVisObj *pObj, const CVec3 &vPos );
	//void RemoveAttachedSounds( IVisObj *pObj );
	void RandomizeRainDrop( SRainDrop &drop );
	void RandomizeSnowFlake( SSnowFlake &flake );
	void RandomizeSand( SSandParticle &particle );
	void UpdateWeather();
	void DrawRain();
	void DrawSnow();
	void DrawSand();
public:
	CScene();
	//
	virtual bool STDCALL Init( ISingleton *pSingleton );
	//
	virtual void STDCALL SetSeason( const int nSeason );
	virtual void STDCALL InitMapSounds( const struct CMapSoundInfo *pSound, int nElements );
	virtual void STDCALL InitMusic(	const std::string &szPartyName );
	virtual void STDCALL InitTerrainSound ( interface ITerrain *pTerrain );
	//
	virtual void STDCALL SetTerrain( ITerrain *pTerrain );
	virtual interface ITerrain * STDCALL GetTerrain() { return pTerrain; }
	// add/remove/change position of the object
	virtual bool STDCALL AddObject( IVisObj *pObject, EObjGameType eGameType, const SGDBObjectDesc *pDesc );
	virtual bool STDCALL AddCraterObject( IVisObj *pObject, EObjGameType eGameType );
	virtual bool STDCALL AddOutboundObject( IVisObj *pObject, EObjGameType eGameType );
	// CRAP{ ��� ����� ��� ����� ��-�� ��������� ����������
	virtual bool STDCALL AddOutboundObject2( IVisObj *pObject, EObjGameType eGameType );
	// CRAP}
	virtual void STDCALL AddMechTrace( const SMechTrace &trace );
	virtual void STDCALL AddGunTrace( const SGunTrace &trace );
	virtual bool STDCALL AddSceneObject( ISceneObject *pObject ) { CPtr<ISceneObject> pObj = pObject; RemoveSceneObject( pObject ); alwaysObjects.push_back( pObject ); return true; }
	virtual bool STDCALL RemoveObject( IVisObj *pObject );
	virtual bool STDCALL RemoveSceneObject( ISceneObject *pObject ) 
	{ 
		if ( pObject ) 
			alwaysObjects.remove( pObject ); 
		else
			alwaysObjects.clear();
		return true; 
	}
	virtual bool STDCALL MoveObject( IVisObj *pObject, const CVec3 &vPos );
	//
	virtual bool STDCALL AddLine( IBoldLineVisObj *pLine ) { boldLines.push_back( pLine ); return true; }
	virtual bool STDCALL RemoveLine( IBoldLineVisObj *pLine ) { boldLines.remove( pLine ); return true; }
	// set areas for fire ranges, zeroing, etc. visualization
	virtual void STDCALL SetAreas( const SShootAreas *areas, int nNumAreas );
	virtual void STDCALL GetAreas( struct SShootAreas **areas, int *pnNumAreas );
	// UI screen
	virtual bool STDCALL AddUIScreen( interface IUIScreen *pUIScreen );
	virtual bool STDCALL RemoveUIScreen( interface IUIScreen *pUIScreen );
	virtual interface IUIScreen* STDCALL GetUIScreen();
	virtual void STDCALL SetMissionScreen( interface IUIScreen *pMissionScreen );
	virtual interface IUIScreen* STDCALL GetMissionScreen();

	// add/remove sound object
	
	virtual void STDCALL SetSoundPos( const WORD wID, const CVec3 &vPos );
	virtual bool STDCALL IsSoundFinished( const WORD wID );
	virtual void STDCALL RemoveSound( const WORD wID ) ;
	virtual WORD STDCALL AddSound( 	const char *pszName,
												const CVec3 &vPos,
												const ESoundMixType eMixType,
												const ESoundAddMode eAddMode,
												const ESoundCombatType eCombatType = ESCT_GENERIC,
												const int nMinRadius = 0,
												const int nMaxRadius = 0,
												const unsigned int nTimeAfterStart = 0 );

	virtual WORD STDCALL AddSoundToMap( const char *pszName, const CVec3 &vPos );
	virtual void STDCALL RemoveSoundFromMap( const WORD	wInstanceID );

	virtual void STDCALL SetSoundSceneMode( const enum ESoundSceneMode eSoundSceneMode );
	virtual void STDCALL UpdateSound( interface ICamera *pCamera );
	virtual void STDCALL CombatNotify();

	// additional objects
	virtual int STDCALL AddMeshPair( IGFXVertices *pVertices, IGFXIndices *pIndices, IGFXTexture *pTexture, int nShadingEffect, bool bTemporary );
	virtual int STDCALL AddMeshPair2( void *vertices, int nNumVertices, int nVertexSize, DWORD dwFormat,
		                                WORD *indices, int nNumIndices, EGFXPrimitiveType ePrimitiveType,
																		IGFXTexture *pTexture, int nShadingEffect, bool bTemporary );
	virtual bool STDCALL RemoveMeshPair( int nID );
	// CRAP{ fake object - circle for artillery reveal - remove, then minimap will be
	virtual void STDCALL AddCircle( const CVec3 &vCenter, const float fRadius, const NTimer::STime &start, const NTimer::STime &duration )
	{
		circles.push_back( SCircle() );
		SCircle &circle = circles.back();
		circle.vCenter.Set( vCenter.x, vCenter.y );
		circle.fRadius = fRadius;
		circle.timeStart = start;
		circle.timeDuration = duration;
	}
	// CRAP}
	// tooltip
	virtual void STDCALL SetToolTip( interface IText *pText, const CVec2 &vPos, const CTRect<float> &rcOut, const DWORD dwColor = 0 );
	// transfer UNIT to graveyard
	virtual bool STDCALL TransferToGraveyard( IVisObj *pObject );
	// set visible objects
	virtual void STDCALL SetVisibleObjects( IVisObj **ppObjects, int nNumObjects );
	virtual void STDCALL SetWarFog( struct SAIVisInfo *pObjects, int nNumObjects );
	// remove all visual objects - clear scene
	virtual void STDCALL Clear();
	// retrieve all objects from scene
	virtual int STDCALL GetNumSceneObjects() const;
	virtual int STDCALL GetAllSceneObjects( std::pair<const SGDBObjectDesc*, CVec3> *pBuffer ) const;
	//
	virtual IFrameSelection* STDCALL GetFrameSelection() { return pFrameSelection; }
	virtual IStatSystem* STDCALL GetStatSystem() { return pStatSystem; }
	//
	virtual void STDCALL Draw( ICamera *pCamera );
	// enables
	virtual bool STDCALL ToggleShow( int nTypeID );
	// picking objects
	virtual void STDCALL Pick( const CVec2 &point, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible );
	virtual void STDCALL Pick( const CTRect<float> &rcRect, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible );
	// 3D <=> 2D position transforms
	virtual void STDCALL GetPos3( CVec3 *pPos, const CVec2 &pos, bool bOnZero = false );
	virtual void STDCALL GetPos2( CVec2 *pPos, const CVec3 &pos );
	virtual void STDCALL GetScreenCoords( const CVec3 &pos, CVec3 *vScreen );
	// check object visibility
	bool IsVisible( IObjVisObj *pObj ) const
	{
		if ( bEnableWarFog )
			return pObj->IsVisible();
		else
			return true;
	}
	const SVisObjDesc* GetDesc( IVisObj *pVisObj ) const
	{
		CVisObjDescMap::const_iterator pos = objdescs.find( pVisObj );
		return pos == objdescs.end() ? 0 : &(pos->second);
	}
	virtual void STDCALL SetDirectionalArrow( const CVec3 &vStart, const CVec3 &vEnd, bool bDraw );
	virtual void STDCALL SetClickMarker( const CVec3 &vPos );
	virtual void STDCALL SetPosMarker( const CVec3 &vPos );
	virtual void STDCALL SetRotationStartAngle( float fAngle, bool bRotate = true );
	virtual void STDCALL FlashPosMarkers();
	virtual void STDCALL ResetPosMarkers();
	virtual void STDCALL SwitchWeather( bool bOn );
	virtual bool STDCALL IsRaining();
	virtual void STDCALL SetWeatherQuality( float fCoeff );
	virtual void STDCALL Reposition();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float Sin( float fAngle );
float Cos( float fAngle );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCENEINTERNAL_H__
