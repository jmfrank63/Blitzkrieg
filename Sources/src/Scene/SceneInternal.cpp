#include "StdAfx.h"

#include "SceneInternal.h"
#include "SceneScreenScale.h"

#include "FrameSelection.h"
#include "StatSystem.h"
#include "..\GFX\GFXHelper.h"
#include "SoundScene.h"
#include "..\AILogic\AILogic.h"
#include "..\Misc\Win32Random.h"
#include "FastSinCos.h"
HINSTANCE hDLLInstance = 0;
BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved   // reserved
)
{
	if ( fdwReason == DLL_PROCESS_ATTACH ) 
		hDLLInstance = hinstDLL;
	return true;
}
void SToolTip::Init()
{
	pText = CreateObject<IGFXText>( GFX_TEXT );
	pText->SetColor( 0xffffffff );
	pText->SetWidth( 200 );
	pText->EnableRedLine( false );
}
void SToolTip::Clear()
{
	pText->SetFont( 0 );
	bHasFont = false;
	bHasText = false;
}
CScene::CScene() 
: areaCraters( fWorldCellSize ), areaUnits( AREA_MAP_CELL_SIZE ), meshGraveyardArea( AREA_MAP_CELL_SIZE ),
	effectsArea( AREA_MAP_CELL_SIZE ), spriteObjectsArea( AREA_MAP_CELL_SIZE ), terraObjectsArea( AREA_MAP_CELL_SIZE ), 
	shadowObjectsArea( AREA_MAP_CELL_SIZE ), mechTracesArea( AREA_MAP_CELL_SIZE ), gunTracesArea( AREA_MAP_CELL_SIZE )
{  
	pSoundScene = new CSoundScene;
	bEnableUnits = true;
	bEnableObjects = true;
	bEnableBBs = false;
	bEnableEffects = true;
	bEnableTerrain = true;
	bEnableNoise = true;
	bEnableShadows = true;
	bEnableGrid = false;
	bEnableWarFog = true;
	bEnableDepthComplexity = false;
	bEnableShowBorder = true;
	bShowUI = true;
	bEnableHaze = false;
	pSandTexture = 0;
	pTerrain = 0;
	bWeatherOn = false;
	bDrawArrow = false;
	wAmbientID = 0;
	nNextRandomSound = 0;
}
int SVisObjDesc::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vistype );
	saver.Add( 2, &gametype );
	if ( saver.IsReading() || (!saver.IsReading() && pDesc != 0) )
		saver.Add( 3, &pDesc );
	saver.Add( 4, &bOutbound );
	return 0;
}
int CScene::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 3, &spriteObjectsArea );
	saver.Add( 4, &terraObjectsArea );
	saver.Add( 5, &shadowObjectsArea );
	saver.Add( 6, &meshGraveyardArea );
	saver.Add( 7, &effectsArea );
	saver.Add( 8, &outboundObjects );
	saver.Add( 9, &boldLines );
	
	saver.Add( 10, &bEnableUnits );
	saver.Add( 11, &bEnableObjects );
	saver.Add( 12, &bEnableBBs );
	saver.Add( 13, &bEnableEffects );
	saver.Add( 14, &bEnableTerrain );
	saver.Add( 15, &bEnableShadows );
	saver.Add( 16, &bEnableGrid );
	saver.Add( 17, &bEnableWarFog );

	saver.Add( 30, &pTerrain );
	saver.Add( 31, &vMapSize );
	saver.Add( 32, pFrameSelection.GetPtr() );
	saver.Add( 33, pStatSystem.GetPtr() );
	if ( saver.IsReading() )
	{
		std::vector< CPtr<IVisObj> > objects;
		std::vector< SVisObjDesc > descs;
		saver.Add( 34, &objects );
		saver.Add( 35, &descs );
		NI_ASSERT_T( objects.size() == descs.size(), NStr::Format("Wrong data readed - number of objects (%d) are not equal to the number of descs (%d)", objects.size(), descs.size()) );
		objdescs.clear();
		const int nSize = objects.size();
		for ( int i=0; i<nSize; ++i )
			objdescs[ objects[i] ] = descs[i];
	}
	else
	{
		std::vector< CPtr<IVisObj> > objects;
		std::vector< SVisObjDesc > descs;

		const int nSize = objdescs.size();
		objects.reserve( nSize );
		descs.reserve( nSize );
		for ( CVisObjDescMap::const_iterator it = objdescs.begin(); it != objdescs.end(); ++it )
		{
			const SVisObjDesc &desc = it->second;
			IVisObj *pVisObj = it->first;
			objects.push_back( it->first );
			descs.push_back( it->second );
		}
		saver.Add( 34, &objects );
		saver.Add( 35, &descs );
	}
	saver.Add( 40, &uiScreens );
	saver.Add( 41, &areas );

	saver.Add( 42, &bEnableHaze );
	saver.Add( 43, &dwHazeColorTop );
	saver.Add( 44, &dwHazeColorBottom );
	saver.Add( 45, &fHazeHeight );
	saver.Add( 46, &tooltip );
	saver.Add( 47, &pSoundScene );
	saver.Add( 48, &sunlight );
	saver.Add( 49, &alwaysObjects );
	saver.Add( 50, &mechTracesArea );
	saver.Add( 51, &outboundEffects );
	saver.Add( 52, &outboundSprites );
	saver.Add( 53, &outboundObjects2 );
	saver.Add( 55, &wAmbientID );
	saver.Add( 56, &bWeatherOn );
	saver.Add( 57, &eWeatherCondition );
	saver.Add( 58, &eCurrSetting );
	int nSandSize = sandParticles.size();
	int nRainSize = rainDrops.size();
	int nSnowSize = snowFlakes.size();
	saver.Add( 59, &nSandSize );
	saver.Add( 60, &nRainSize );
	saver.Add( 61, &nSnowSize );
	saver.Add( 63, &areaUnits );
	saver.Add( 64, &areaCraters );
	saver.Add( 65, &dwArrowColor );
	saver.Add( 66, &dwMarkerColor );
	saver.Add( 67, &pMissionScreen );
	
	if ( saver.IsReading() && !pMissionScreen )
	{
		pMissionScreen = uiScreens.front();
	}

	if ( saver.IsReading() )
	{
		if ( dwArrowColor == 0x00000000 )
		{
			dwMarkerColor = GetGlobalVar( "Scene.Colors.Summer.Markup.Circle.Color", int(0x0000ff00) );
			dwArrowColor = GetGlobalVar( "Scene.Colors.Summer.Markup.Arrow.Color", int(0x60000080) );
		}
		pSandTexture = 0;
		bWeatherInitialized = false;
		sandParticles.resize( nSandSize );
		rainDrops.resize( nRainSize );
		snowFlakes.resize( nSnowSize );
		if ( (mechTracesArea.GetSizeX() != spriteObjectsArea.GetSizeX()) || (mechTracesArea.GetSizeY() != spriteObjectsArea.GetSizeY()) ) 
			mechTracesArea.SetSizes( spriteObjectsArea.GetSizeX(), spriteObjectsArea.GetSizeY() );
		if ( (gunTracesArea.GetSizeX() != spriteObjectsArea.GetSizeX()) || (gunTracesArea.GetSizeY() != spriteObjectsArea.GetSizeY()) ) 
			gunTracesArea.SetSizes( spriteObjectsArea.GetSizeX(), spriteObjectsArea.GetSizeY() );
		if ( (areaUnits.GetSizeX() != spriteObjectsArea.GetSizeX()) || (areaUnits.GetSizeY() != spriteObjectsArea.GetSizeY()) ) 
			areaUnits.SetSizes( spriteObjectsArea.GetSizeX(), spriteObjectsArea.GetSizeY() );
		CSpritesArea spriteUnitsArea( AREA_MAP_CELL_SIZE );
		saver.Add( 1, &spriteUnitsArea );
		if ( (spriteUnitsArea.GetSizeX() == areaUnits.GetSizeX()) && (spriteUnitsArea.GetSizeY() == areaUnits.GetSizeY()) ) 
		{
			for ( int i = 0; i < areaUnits.GetSizeY(); ++i )
			{
				for ( int j = 0; j < areaUnits.GetSizeX(); ++j )
				{
					for ( CSpritesArea::CDataList::iterator it = spriteUnitsArea[i][j].begin(); it != spriteUnitsArea[i][j].end(); ++it )
						areaUnits[i][j].push_back( it->GetPtr() );
				}
			}
		}
		CMeshesArea meshUnitsArea( AREA_MAP_CELL_SIZE );
		saver.Add( 2, &meshUnitsArea );
		if ( (meshUnitsArea.GetSizeX() == areaUnits.GetSizeX()) && (meshUnitsArea.GetSizeY() == areaUnits.GetSizeY()) ) 
		{
			for ( int i = 0; i < areaUnits.GetSizeY(); ++i )
			{
				for ( int j = 0; j < areaUnits.GetSizeX(); ++j )
				{
					for ( CMeshesArea::CDataList::iterator it = meshUnitsArea[i][j].begin(); it != meshUnitsArea[i][j].end(); ++it )
						areaUnits[i][j].push_back( it->GetPtr() );
				}
			}
		}
		if ( ( areaCraters.GetSizeX() != areaUnits.GetSizeX() * AREA_MAP_CELL_SIZE_IN_TILES ) ||
			   ( areaCraters.GetSizeY() != areaUnits.GetSizeY() * AREA_MAP_CELL_SIZE_IN_TILES ) ) 
			areaCraters.SetSizes( areaUnits.GetSizeX() * AREA_MAP_CELL_SIZE_IN_TILES, 
			                      areaUnits.GetSizeY() * AREA_MAP_CELL_SIZE_IN_TILES );
	}
	return 0;
}
bool CScene::ToggleShow( int nTypeID )
{
	switch ( nTypeID )
	{
		case SCENE_SHOW_HAZE:
			bEnableHaze = !bEnableHaze;
			return bEnableHaze;
		case SCENE_SHOW_UNITS:
			bEnableUnits = !bEnableUnits;
			return bEnableUnits;
		case SCENE_SHOW_OBJECTS:
			bEnableObjects = !bEnableObjects;
			return bEnableObjects;
		case SCENE_SHOW_BBS:
			bEnableBBs = !bEnableBBs;
			return bEnableBBs;
		case SCENE_SHOW_SHADOWS:
			bEnableShadows = !bEnableShadows;
			return bEnableShadows;
		case SCENE_SHOW_EFFECTS:
			bEnableEffects = !bEnableEffects;
			return bEnableEffects;
		case SCENE_SHOW_TERRAIN:
			bEnableTerrain = !bEnableTerrain;
			return bEnableTerrain;
		case SCENE_SHOW_GRID:
			bEnableGrid = !bEnableGrid;
			if ( pTerrain != 0 ) 
				pTerrain->EnableGrid( bEnableGrid );
			return bEnableGrid;
		case SCENE_SHOW_NOISE:
			bEnableNoise = !bEnableNoise;
			if ( pTerrain != 0 ) 
				pTerrain->EnableNoise( bEnableNoise );
			return bEnableNoise;
		case SCENE_SHOW_WARFOG:
			bEnableWarFog = !bEnableWarFog;
			return bEnableWarFog;
		case SCENE_SHOW_DEPTH_COMPLEXITY:
			bEnableDepthComplexity = !bEnableDepthComplexity;
			return bEnableDepthComplexity;
		case SCENE_SHOW_BORDER:
			bEnableShowBorder = !bEnableShowBorder;
			return bEnableShowBorder;
		case SCENE_SHOW_UI:
			bShowUI = !bShowUI;
			return bShowUI;
	}
	NI_ASSERT_TF( false, NStr::Format("Can't toggle parameter %d - unknown parameter", nTypeID), return false );
	return false;
}
void CScene::InitTerrainSound ( interface ITerrain *pTerrain )
{
	pSoundScene->InitTerrain( pTerrain );
}
void CScene::InitMapSounds( const struct CMapSoundInfo *pSound, int nElements )
{
	pSoundScene->InitMap( pSound, nElements );
}
void CScene::InitMusic(	const std::string &szPartyName )
{
	pSoundScene->InitMusic( szPartyName );
}
bool CScene::Init( ISingleton *pSingleton ) 
{ 
	pGFX = GetSingleton<IGFX>( pSingleton );
	pSFX = GetSingleton<ISFX>( pSingleton );
	pTimer = GetSingleton<IGameTimer>( pSingleton );
	pCursor = GetSingleton<ICursor>( pSingleton );
	pFrameSelection = new CFrameSelection();
	pStatSystem = new CStatSystem();
	pSoundScene->Init( 0, 0 );
	tooltip.Init();
	sunlight.vDiffuse.Set( 0.5f, 0.5f, 0.5f, 0.5f );
	sunlight.vAmbient.Set( 0.5f, 0.5f, 0.5f, 0.5f );
	sunlight.vDir.Set( 1, 1, -2 );
	Normalize( &sunlight.vDir );
	sunlight.vSpecular.Set( 1, 1, 1, 1 );
	Zero( material );
	material.vAmbient = CVec4( 1, 1, 1, 1 );
	material.vDiffuse = CVec4( 1, 1, 1, 1 );
	tTransformUpdateTime = 0;
	SetAreaMapSize( 8, 8 );
	SetMapSize( 4*16*fWorldCellSize, 4*16*fWorldCellSize );
	bEnableHaze = GetGlobalVar( "Scene.Haze.Enable", 1 ) == 1;
	dwHazeColorTop = MAKE_ARGB( GetGlobalVar( "Scene.Haze.TopColor.A", 36  ),
			                        GetGlobalVar( "Scene.Haze.TopColor.R", 0   ),
															GetGlobalVar( "Scene.Haze.TopColor.G", 174 ),
															GetGlobalVar( "Scene.Haze.TopColor.B", 242 ) );
	dwHazeColorBottom = MAKE_ARGB( GetGlobalVar( "Scene.Haze.BottomColor.A", 0   ),
																 GetGlobalVar( "Scene.Haze.BottomColor.R", 0   ),
																 GetGlobalVar( "Scene.Haze.BottomColor.G", 174 ),
																 GetGlobalVar( "Scene.Haze.BottomColor.B", 242 ) );
	dwGunTraceColor = MAKE_ARGB( GetGlobalVar( "Scene.GunTrace.Color.A", 255   ),
															 GetGlobalVar( "Scene.GunTrace.Color.R", 255   ),
															 GetGlobalVar( "Scene.GunTrace.Color.G", 64    ),
															 GetGlobalVar( "Scene.GunTrace.Color.B", 0     ) );
  fHazeHeight = GetGlobalVar( "Scene.Haze.Height", 1.0f/3.0f );
	pTrackTexture = 0;

	nLastWeatherUpdate = 0;
	eWeatherCondition = SC_NONE;
	bWeatherInitialized = false;
	fChangeSpeed = GetGlobalVar( "AI.Weather.TimeToFadeOff", 5.0f ) * 1000.0f;
	SetWeatherQuality( GetGlobalVar( "Options.GFX.DensityCoeff", 1.0f ) );
	rainDrops.resize( 0 );
	vRainDir.x = GetGlobalVar( "Scene.Weather.Rain.Direction.x", 0.01f );
	vRainDir.y = GetGlobalVar( "Scene.Weather.Rain.Direction.y", 0.01f );
	vRainDir.z = GetGlobalVar( "Scene.Weather.Rain.Direction.z", -0.7f );
	fRainHeight = GetGlobalVar( "Scene.Weather.Rain.Height", 300.0f);
	dwRainTopColor = GetGlobalVar( "Scene.Weather.Rain.TopColor", int(0x20404060) );
	dwRainBottomColor = GetGlobalVar( "Scene.Weather.Rain.BottomColor", int(0x40404060) );

	snowFlakes.resize( 0 );
	fSnowHeight = GetGlobalVar( "Scene.Weather.Snow.Height", 500.0f );
	dwSnowColor = GetGlobalVar( "Scene.Weather.Snow.Color", int(0xffffffff) );
	fSnowFallingSpeed = GetGlobalVar( "Scene.Weather.Snow.FallingSpeed", 0.05f );
	fSnowAmplitude = GetGlobalVar( "Scene.Weather.Snow.Amplitude", 0.05f );
	fSnowFrequency = GetGlobalVar( "Scene.Weather.Snow.Frequency", 0.003f );
	
	sandParticles.resize( 0 );
	fSandHeight = GetGlobalVar( "Scene.Weather.Sand.Height", 300.0f );
	vSandCone = CVec2( -100.0f, -100.0f );
	fConeRadius = GetGlobalVar( "Scene.Weather.Sand.ConeRadius", 70.0f );
	fSandAmplitude = GetGlobalVar( "Scene.Weather.Sand.Amplitude", 0.01f );
	fSandFrequency = GetGlobalVar( "Scene.Weather.Sand.Frequency", 0.001f );
	vSandWind.x = GetGlobalVar( "Scene.Weather.Sand.Wind.x", -0.01f );
	vSandWind.y = GetGlobalVar( "Scene.Weather.Sand.Wind.y", -0.01f );
	vSandWind.z = GetGlobalVar( "Scene.Weather.Sand.Wind.z", 0.0f );
	fSandSpeed = GetGlobalVar( "Scene.Weather.Sand.Speed", 10.0f );
	fSandConeSpeed = GetGlobalVar( "Scene.Weather.Sand.ConeSpeed", 0.1f );

	fTraceLen = GetGlobalVar( "Scene.GunTrace.Length", 0.33f );
	clickMarkers.clear();
	nMarkerLifetime = 1500;
	dwMarkerColor = GetGlobalVar( "Scene.Colors.Summer.Markup.Circle.Color", int(0x0000ff00) );
	dwArrowColor = GetGlobalVar( "Scene.Colors.Summer.Markup.Arrow.Color", int(0x60000080) );
	return true; 
}
void CScene::SetSeason( const int nSeason )
{
	const std::string szSeason = nSeason == 0 ? "Summer" : (nSeason == 1 ? "Winter" : "Africa");
	sunlight.vDiffuse.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.A").c_str(), 1.0f ),
		                     GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.R").c_str(), 1.0f ),
												 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.G").c_str(), 1.0f ),
												 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.B").c_str(), 1.0f ) );
	sunlight.vAmbient.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.A").c_str(), 1.0f ),
		                     GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.R").c_str(), 1.0f ),
												 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.G").c_str(), 1.0f ),
												 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.B").c_str(), 1.0f ) );
	sunlight.vDir.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.X").c_str(),  1.0f ),
		                 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.Y").c_str(),  1.0f ),
										 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.Z").c_str(), -2.0f ) );
	Normalize( &sunlight.vDir );
	sunlight.vSpecular.Set( 1, 1, 1, 1 );
	dwMarkerColor = GetGlobalVar( ("Scene.Colors." + szSeason + ".Markup.Circle.Color").c_str(), int(0x0000ff00) );
	dwArrowColor = GetGlobalVar( ("Scene.Colors." + szSeason + ".Markup.Arrow.Color").c_str(), int(0x60000080) );
}
void CScene::UpdateTransformMatrix()
{
	NTimer::STime time = pTimer->GetGameTime();
	{
		static CMatrixStack<4> mstack;
		const CTRect<float> rcScreen = pGFX->GetScreenRect();
		SHMatrix matProjection;
		if ( !NSceneScreenScale::CreateGameplayProjectionMatrix( &matProjection, rcScreen ) )
			matProjection = pGFX->GetProjectionMatrix();
		mstack.Push( pGFX->GetViewportMatrix() );
		mstack.Push( matProjection );
		mstack.Push( pGFX->GetViewMatrix() );
		matTransform = mstack();
		mstack.Pop( 3 );
		tTransformUpdateTime = time;
	}
}
void CScene::SetAreaMapSize( int nSizeX, int nSizeY )
{
	objdescs.clear();

	areaCraters.Clear();
	areaUnits.Clear();
	effectsArea.Clear();
	spriteObjectsArea.Clear();
	terraObjectsArea.Clear();
	shadowObjectsArea.Clear();
	meshGraveyardArea.Clear();
	mechTracesArea.Clear();
	gunTracesArea.Clear();

	areaUnits.SetSizes( nSizeX, nSizeY );
	areaCraters.SetSizes( nSizeX * AREA_MAP_CELL_SIZE_IN_TILES, nSizeY * AREA_MAP_CELL_SIZE_IN_TILES );
	effectsArea.SetSizes( nSizeX, nSizeY );
	meshGraveyardArea.SetSizes( nSizeX, nSizeY );
	spriteObjectsArea.SetSizes( nSizeX, nSizeY );
	terraObjectsArea.SetSizes( nSizeX, nSizeY );
	shadowObjectsArea.SetSizes( nSizeX, nSizeY );
	mechTracesArea.SetSizes( nSizeX, nSizeY );
	gunTracesArea.SetSizes( nSizeX, nSizeY );
}
void CScene::SetMapSize( int nSizeX, int nSizeY )
{
	vMapSize.x = nSizeX;
	vMapSize.y = nSizeY;
}
void CScene::SetTerrain( ITerrain *_pTerrain )
{
	pTerrain = _pTerrain;
	if ( pTerrain )
	{
		float fSizeX = pTerrain->GetSizeX();
		float fSizeY = pTerrain->GetSizeY();
		int nSizeX = int( fSizeX / AREA_MAP_CELL_SIZE_IN_TILES );
		if ( nSizeX * AREA_MAP_CELL_SIZE_IN_TILES < fSizeX )
			++nSizeX;

		int nSizeY = int( fSizeY / AREA_MAP_CELL_SIZE_IN_TILES );
		if ( nSizeY * AREA_MAP_CELL_SIZE_IN_TILES < fSizeY )
			++nSizeY;
		SetAreaMapSize( nSizeX, nSizeY );	
		pSoundScene->Init( fSizeX, fSizeY );
		SetMapSize( pTerrain->GetSizeX()*fWorldCellSize, pTerrain->GetSizeY()*fWorldCellSize );
		bWeatherOn = true;
	}
	else
		bWeatherOn = false;
	bWeatherInitialized = false;
	eWeatherCondition = SC_NONE;
	pSoundScene->SetTerrain( pTerrain );
}
bool CScene::MoveObject( IVisObj *pObject, const CVec3 &vPos )
{
	CVisObjDescMap::const_iterator it = objdescs.find( pObject );
	if ( it == objdescs.end() )
		return false;
	const SVisObjDesc &desc = it->second;

	if ( !desc.bOutbound && ((vPos.x < 0) || (vPos.y < 0) || (vPos.x >= vMapSize.x) || (vPos.y >= vMapSize.y)) )
		return false;
	if ( desc.bOutbound )
		pObject->SetPosition( vPos );
	else if ( desc.vistype == SGVOT_SPRITE ) 
	{
		switch ( desc.gametype )
		{
			case SGVOGT_UNIT:
				areaUnits.MoveTo( static_cast<IObjVisObj*>(pObject), vPos );
				break;
			case SGVOGT_EFFECT:
				if ( effectsArea.IsInArea(vPos) ) 
					effectsArea.MoveTo( pObject, vPos );
				else
					return false;
				break;
			case SGVOGT_TERRAOBJ:
			case SGVOGT_BRIDGE:
			case SGVOGT_FORTIFICATION:
				if ( terraObjectsArea.IsInArea(vPos) ) 
					terraObjectsArea.MoveTo( static_cast<IObjVisObj*>(pObject), vPos );
				else
					return false;
				break;
			case SGVOGT_SHADOW:
				shadowObjectsArea.MoveTo( static_cast<ISpriteVisObj*>(pObject), vPos );
				break;
			default:
				spriteObjectsArea.MoveTo( static_cast<ISpriteVisObj*>(pObject), vPos );
		}
	}
	else if ( desc.vistype == SGVOT_MESH )
	{
		switch ( desc.gametype ) 
		{
			case SGVOGT_UNIT:
				areaUnits.MoveTo( static_cast<IObjVisObj*>(pObject), vPos );
				break;
			case SGVOGT_TERRAOBJ:
				if ( terraObjectsArea.IsInArea(vPos) ) 
					terraObjectsArea.MoveTo( static_cast<IObjVisObj*>(pObject), vPos );
				else
					return false;
				break;
			default:
				meshGraveyardArea.MoveTo( static_cast<IMeshVisObj*>(pObject), vPos );
		}
	}
	else if ( desc.vistype == SGVOT_EFFECT )
		effectsArea.MoveTo( pObject, vPos );
	else
		NI_ASSERT_T( false, "unknown object" );
	return true;
}
int CScene::AddMeshPair( IGFXVertices *pVertices, IGFXIndices *pIndices, IGFXTexture *pTexture, int nShadingEffect, bool bTemporary )
{
	const int nIndex = tempmeshes.empty() ? 0 : tempmeshes.back().nIndex + 1;
	tempmeshes.push_back( STemporalMesh() );
	STemporalMesh &mesh = tempmeshes.back();

	mesh.pVertices = pVertices;
	mesh.pIndices = pIndices;
	mesh.pTexture = pTexture;
	mesh.nShadingEffect = nShadingEffect;
	mesh.bTemporal = bTemporary;
	mesh.nIndex = nIndex;

	return nIndex;
}
int CScene::AddMeshPair2( void *vertices, int nNumVertices, int nVertexSize, DWORD dwFormat,
		                      WORD *indices, int nNumIndices, EGFXPrimitiveType ePrimitiveType,
													IGFXTexture *pTexture, int nShadingEffect, bool bTemporary )
{
	meshpairs2.push_back( SMeshPair2() );
	SMeshPair2 &mesh = meshpairs2.back();
	mesh.vertices.resize( nVertexSize*nNumVertices );
	memcpy( &(mesh.vertices[0]), vertices, nVertexSize*nNumVertices );
	mesh.nNumVertices = nNumVertices;
	mesh.dwVertexFormat = dwFormat;
	if ( nNumIndices > 0 ) 
	{
		mesh.indices.resize( nNumIndices );
		memcpy( &(mesh.indices[0]), indices, nNumIndices*sizeof(WORD) );
	}
	mesh.ePrimitiveType = ePrimitiveType;
	mesh.pTexture = pTexture;
	mesh.nShadingEffect = nShadingEffect;
	mesh.bTemporary = bTemporary;
	return 0;
}
bool CScene::RemoveMeshPair( int nID )
{
	for ( std::list<STemporalMesh>::iterator it = tempmeshes.begin(); it != tempmeshes.end(); ++it )
	{
		if ( it->nIndex == nID )
		{
			tempmeshes.erase( it );
			return true;
		}
	}
	return false;
}
void CScene::SetAreas( const SShootAreas *_areas, int nNumAreas )
{
	areas.clear();
	areas.reserve( nNumAreas );
	for ( const SShootAreas *pAreas = _areas; pAreas != _areas + nNumAreas; ++pAreas )
	{
		areas.push_back( *pAreas );		
		for ( std::list<SShootArea>::iterator iter = areas.back().areas.begin(); iter != areas.back().areas.end(); ++iter )
		{
			if ( iter->fMaxR > 0 )
			{
				AI2Vis( &(iter->vCenter3D) );
				iter->fMaxR *= fAITileXCoeff;
				iter->fMinR *= fAITileXCoeff;
			}
		}
	}
}
void CScene::GetAreas( SShootAreas **_areas, int *pnNumAreas )
{
	if ( areas.empty() )
	{
		*pnNumAreas = 0;
		*_areas = 0;
		return;
	}
	else
	{
		*pnNumAreas = areas.size();
		*_areas = &( areas[0] );
	}
}
bool CScene::AddOutboundObject( IVisObj *pObj, EObjGameType eGameType )
{
	NI_ASSERT_SLOW_TF( (dynamic_cast<IMeshVisObj*>(pObj) != 0) || (dynamic_cast<IEffectVisObj*>(pObj) != 0) || (dynamic_cast<ISpriteVisObj*>(pObj) != 0), "Outbound object must be a mesh, effect or sprite vis obj", return false );
	SVisObjDesc &desc = objdescs[pObj];
	desc.gametype = eGameType;
	desc.pDesc = 0;
	desc.bOutbound = true;
	switch ( eGameType ) 
	{
		case SGVOGT_EFFECT:
			outboundEffects.push_back( checked_cast<IEffectVisObj*>(pObj) );
			break;
		case SGVOGT_UNIT:
			if ( ISpriteVisObj *pSprite = dynamic_cast<ISpriteVisObj*>(pObj) ) 
				outboundSprites.push_back( pSprite );
			else
				outboundObjects.push_back( checked_cast<IMeshVisObj*>(pObj) );
			break;
		default:
			outboundObjects.push_back( checked_cast<IMeshVisObj*>(pObj) );
	}
	return true;
}
bool CScene::AddOutboundObject2( IVisObj *pObj, EObjGameType eGameType )
{
	NI_ASSERT_SLOW_TF( dynamic_cast<IMeshVisObj*>(pObj) != 0, "Outbound object must be a mesh, effect or sprite vis obj", return false );
	SVisObjDesc &desc = objdescs[pObj];
	desc.gametype = eGameType;
	desc.pDesc = 0;
	desc.bOutbound = true;
	outboundObjects2.push_back( checked_cast<IMeshVisObj*>(pObj) );
	return true;
}
void CScene::AddMechTrace( const SMechTrace &trace )
{
	mechTracesArea.Add( trace );
}
void CScene::AddGunTrace( const SGunTrace &trace )
{
	if ( gunTracesArea.IsInArea( trace.GetPosition() ) )
		gunTracesArea.Add( trace );
}
bool CScene::AddObject( IVisObj *pObj, EObjGameType eGameType, const SGDBObjectDesc *pDesc )
{
	if ( pObj == 0 )
		return false;
	SVisObjDesc &desc = objdescs[pObj];
	desc.gametype = eGameType;
	desc.pDesc = pDesc;
	desc.bOutbound = false;
	if ( ISpriteVisObj *pSprite = dynamic_cast<ISpriteVisObj*>( pObj ) )
	{
		desc.vistype = SGVOT_SPRITE;
		checked_cast<IObjVisObj*>(pObj)->SetGameType( desc.gametype );
		if ( eGameType != SGVOGT_UNIT )
			pSprite->SetAnimation( 0 );
		return AddSpriteObject( pSprite, eGameType );
	}
	else if ( IMeshVisObj *pMesh = dynamic_cast<IMeshVisObj*>( pObj ) )
	{
		desc.vistype = SGVOT_MESH;
		checked_cast<IObjVisObj*>(pObj)->SetGameType( desc.gametype );
		return AddMeshObject( pMesh, eGameType );
	}
	else if ( IEffectVisObj *pEffect = dynamic_cast<IEffectVisObj*>( pObj ) )
	{
		desc.vistype = SGVOT_EFFECT;
		return AddEffectObject( pEffect, eGameType );
	}
	else if ( IFlashVisObj *pFlash = dynamic_cast<IFlashVisObj*>(pObj) ) 
	{
		desc.vistype = SGVOT_FLASH;
		return AddEffectObject( pFlash, eGameType );
	}
	else
		return false;
}
bool CScene::AddCraterObject( IVisObj *_pObj, EObjGameType eGameType )
{
	IObjVisObj *pObj = checked_cast<IObjVisObj*>( _pObj );
	pObj->SetAnimation( 0 );
	pObj->Update( pTimer->GetGameTime() );	
	return areaCraters.Add( pObj );
}
bool CScene::AddSpriteObject( ISpriteVisObj *pObj, EObjGameType eGameType )
{
	switch ( eGameType )
	{
		case SGVOGT_UNIT:
			AddObjectToArea( pObj, areaUnits );
			break;
		case SGVOGT_SHADOW:
			pObj->Update( pTimer->GetGameTime() );
			AddObjectToArea( pObj, shadowObjectsArea );
			break;
		case SGVOGT_TERRAOBJ:
		case SGVOGT_BRIDGE:
		case SGVOGT_FORTIFICATION:
			pObj->Update( pTimer->GetGameTime() );
			AddObjectToArea( pObj, terraObjectsArea );
			break;
		default:
			pObj->Update( pTimer->GetGameTime() );
			AddObjectToArea( pObj, spriteObjectsArea );
	}
	return true;
}
bool CScene::AddMeshObject( IMeshVisObj *pObj, EObjGameType eGameType )
{
	switch ( eGameType ) 
	{
		case SGVOGT_UNIT:
			AddObjectToArea( pObj, areaUnits );
			break;
		case SGVOGT_TERRAOBJ:
			{
				const SVisObjDesc *pDesc = GetDesc( pObj );
				if ( pDesc && pDesc->pDesc ) 
				{
					NI_ASSERT_T( false, NStr::Format("Trying to add mesh object \"%s\" as a terraobj", pDesc->pDesc->szPath.c_str()) );
				}
				else
				{
					NI_ASSERT_T( false, "Trying to add mesh object as a terraobj" );
				}
			}
			pObj->SetAnimation( 0 );
			pObj->Update( pTimer->GetGameTime() );
			AddObjectToArea( pObj, terraObjectsArea );
			break;
		default:
			pObj->SetAnimation( 0 );
			AddObjectToArea( pObj, meshGraveyardArea );
	}
	return true;
}
bool CScene::AddEffectObject( IVisObj *pObj, EObjGameType eGameType )
{
	if ( bEnableEffects )
		return AddObjectToArea( pObj, effectsArea );
	else
	{
		objdescs.erase( pObj );
		return false;
	}
}
void CScene::Clear()
{
	const int nSizeX = areaUnits.GetSizeX();
	const int nSizeY = areaUnits.GetSizeY();
	tooltip.Clear();
	objdescs.clear();
	areaUnits.Clear();
	areaCraters.Clear();
	effectsArea.Clear();
	spriteObjectsArea.Clear();
	terraObjectsArea.Clear();
	shadowObjectsArea.Clear();
	meshGraveyardArea.Clear();
	outboundObjects.clear();
	outboundObjects2.clear();
	outboundEffects.clear();
	outboundSprites.clear();
	alwaysObjects.clear();
	mechTracesArea.Clear();
	gunTracesArea.Clear();
	uiScreens.clear();
	SetAreaMapSize( nSizeX, nSizeY );
	pSoundScene->Clear();
	pTrackTexture = 0;
	rainDrops.resize( 0 );
	snowFlakes.resize( 0 );
	sandParticles.resize( 0 );
	nLastWeatherUpdate = 0;
	pSandTexture = 0;
	eWeatherCondition = SC_NONE;
	bWeatherOn = false;
	bWeatherInitialized = false;
	bDrawArrow = false;
	clickMarkers.clear();
	wAmbientID = 0;
	nNextRandomSound = 0;
}
bool CScene::RemoveObject( IVisObj *pObj )
{
	CVisObjDescMap::iterator it = objdescs.find( pObj );
	if ( it == objdescs.end() )
		return false;
	SVisObjDesc desc = it->second;
	objdescs.erase( it );
	if ( desc.bOutbound )
		return RemoveOutboundObject( pObj, desc.gametype );
	else if ( desc.vistype == SGVOT_SPRITE )
		return RemoveSpriteObject( static_cast<ISpriteVisObj*>( pObj ), desc.gametype );
	else if ( desc.vistype == SGVOT_MESH ) 
		return RemoveMeshObject( static_cast<IMeshVisObj*>( pObj ), desc.gametype );
	else if ( (desc.vistype == SGVOT_EFFECT) || (desc.vistype == SGVOT_FLASH) )
		return RemoveEffectObject( pObj, desc.gametype );
	else
		return false;
}
bool CScene::RemoveOutboundObject( IVisObj *pObj, EObjGameType eGameType )
{
	switch ( eGameType ) 
	{
		case SGVOGT_EFFECT:
			outboundEffects.remove( checked_cast<IEffectVisObj*>(pObj) );
			break;
		case SGVOGT_UNIT:
			if ( ISpriteVisObj *pSprite = dynamic_cast<ISpriteVisObj*>(pObj) ) 
				outboundSprites.remove( pSprite );
			else
			{
				outboundObjects.remove( checked_cast<IMeshVisObj*>(pObj) );
				outboundObjects2.remove( checked_cast<IMeshVisObj*>(pObj) );
			}
			break;
		default:
			outboundObjects.remove( checked_cast<IMeshVisObj*>(pObj) );
			outboundObjects2.remove( checked_cast<IMeshVisObj*>(pObj) );
	}
	return true;
}
bool CScene::RemoveSpriteObject( ISpriteVisObj *pObj, EObjGameType eGameType )
{
	switch ( eGameType )
	{
		case SGVOGT_UNIT:
			areaUnits.Remove( pObj );
			break;
		case SGVOGT_FORTIFICATION:
		case SGVOGT_BRIDGE:
		case SGVOGT_TERRAOBJ:
			terraObjectsArea.Remove( pObj );
			break;
		case SGVOGT_SHADOW:
			shadowObjectsArea.Remove( pObj );
			break;
		default:
			spriteObjectsArea.Remove( pObj );
	}
	return true;
}
bool CScene::RemoveMeshObject( IMeshVisObj *pObj, EObjGameType eGameType )
{
	if ( eGameType == SGVOGT_UNIT ) 
		areaUnits.Remove( pObj );
	else
	{
		CPtr<IMeshVisObj> pMesh = pObj;
		meshGraveyardArea.Remove( pObj );
		terraObjectsArea.Remove( pObj );
	}
	return true;
}
bool CScene::RemoveEffectObject( IVisObj *pObj, EObjGameType eGameType )
{
	effectsArea.Remove( pObj );
	return true;
}
bool CScene::TransferToGraveyard( IVisObj *pObj )
{
	SVisObjDesc &desc = objdescs[pObj];
	if ( (desc.gametype != SGVOGT_UNIT) || desc.bOutbound ) 
		return false;
	if ( desc.vistype == SGVOT_SPRITE )
	{
		AddSpriteObject( static_cast<ISpriteVisObj*>( pObj ), SGVOGT_TERRAOBJ );
		RemoveSpriteObject( static_cast<ISpriteVisObj*>( pObj ), SGVOGT_UNIT );
		desc.gametype = SGVOGT_TERRAOBJ;
	}
	/*
	else if ( desc.vistype == SGVOT_MESH )
	{
		meshGraveyardArea.Add( static_cast<IMeshVisObj*>(pObj) );
		areaUnits.Remove( static_cast<IObjVisObj*>(pObj) );
		desc.gametype = SGVOGT_TERRAOBJ;
	}
	*/
	return true;
}
void CScene::SetMissionScreen( interface IUIScreen *_pMissionScreen )
{
	pMissionScreen = _pMissionScreen;
}
IUIScreen* CScene::GetMissionScreen()
{
	return pMissionScreen;
}
bool CScene::AddUIScreen( IUIScreen *pUIScreen )
{
	RemoveUIScreen( pUIScreen );
	uiScreens.push_back( pUIScreen );
	tooltip.bHasText = false;
	return true;
}
bool CScene::RemoveUIScreen( IUIScreen *pUIScreen )
{
	uiScreens.remove( pUIScreen );
	tooltip.bHasText = false;
	return true;
}
IUIScreen* CScene::GetUIScreen()
{
	return uiScreens.empty() ? 0 : uiScreens.back();
}
void CScene::GetPos2( CVec2 *pPos, const CVec3 &pos )
{
	UpdateTransformMatrix();
	CVec3 pos3;
	matTransform.RotateHVector( &pos3, pos );
	pPos->Set( pos3.x, pos3.y );
}
void CScene::GetScreenCoords( const CVec3 &pos, CVec3 *vScreen )
{
	UpdateTransformMatrix();
	matTransform.RotateHVector( vScreen, pos );
}
void CScene::GetPos3( CVec3 *pPos, const CVec2 &pos, bool bOnZero )
{
	CVec3 vNear, vFar;
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	const SHMatrix matScreenProjection = pGFX->GetProjectionMatrix();
	SHMatrix matGameplayProjection;
	const bool bScaleGameplayProjection = NSceneScreenScale::CreateGameplayProjectionMatrix( &matGameplayProjection, rcScreen );
	if ( bScaleGameplayProjection )
		pGFX->SetProjectionTransform( matGameplayProjection );
	pGFX->GetViewVolumeCrosses( pos, &vNear, &vFar );
	if ( bScaleGameplayProjection )
		pGFX->SetProjectionTransform( matScreenProjection );
	Vis2AI( &vNear );
	Vis2AI( &vFar );
	if ( !bOnZero && GetSingleton<IAILogic>()->GetIntersectionWithTerrain(pPos, vNear, vFar) )
		AI2Vis( pPos );
	else
	{
		UpdateTransformMatrix();
		float x =  ( matTransform._12*matTransform._24 - matTransform._12*pos.y - matTransform._14*matTransform._22 + pos.x*matTransform._22 ) / 
							 ( matTransform._11*matTransform._22 - matTransform._12*matTransform._21 );
		float y = -( matTransform._11*matTransform._24 - matTransform._11*pos.y - matTransform._14*matTransform._21 + pos.x*matTransform._21 ) / 
							 ( matTransform._11*matTransform._22 - matTransform._12*matTransform._21 );
		pPos->Set( x, y, 0 );
	}
}
int CScene::GetNumSceneObjects() const
{
	return objdescs.size();
}
int CScene::GetAllSceneObjects( std::pair<const SGDBObjectDesc*, CVec3> *pBuffer ) const
{
	for ( CVisObjDescMap::const_iterator it = objdescs.begin(); it != objdescs.end(); ++it )
	{
		pBuffer->first = it->second.pDesc;
		pBuffer->second = it->first->GetPosition();
		++pBuffer;
	}
	return objdescs.size();
}
void CScene::SetVisibleObjects( IVisObj **ppObjects, int nNumObjects )
{
}
void CScene::SetWarFog( struct SAIVisInfo *pObjects, int nNumObjects )
{
	if ( pTerrain )
		pTerrain->SetWarFog( pObjects, nNumObjects );
}
void CScene::SetToolTip( interface IText *pText, const CVec2 &vPos, const CTRect<float> &rcOut, const DWORD dwColor )
{
	if ( pText == 0 )
		tooltip.bHasText = false;
	else
	{
		tooltip.pText->SetFont( GetSingleton<IFontManager>()->GetFont("fonts\\medium") );
		const CTRect<float> rcScreenRect = pGFX->GetScreenRect();
		tooltip.bHasText = true;
		tooltip.pText->SetText( pText );
		tooltip.pText->SetColor( dwColor != 0 ? dwColor : 0xffcdcd00 );
		const int nWidth = Min( tooltip.pText->GetWidth() + 5, 300 );
		tooltip.pText->SetWidth( nWidth );
		const int nHeight = tooltip.pText->GetLineSpace() * tooltip.pText->GetNumLines();
		tooltip.rcRect.Set( vPos.x, vPos.y, vPos.x + nWidth, vPos.y + nHeight );
		if ( rcOut.IsEmpty() ) 
		{
			tooltip.pText->SetWidth( rcScreenRect.Width() );
			const int nWidth = tooltip.pText->GetWidth() + 5;
			tooltip.rcRect.Set( vPos.x, vPos.y, vPos.x + nWidth, vPos.y + nHeight );
		}
		if ( tooltip.rcRect.x1 - 5 < rcScreenRect.x1 ) 
			tooltip.rcRect.Move( rcScreenRect.x1 - tooltip.rcRect.x1 + 5, 0 );
		if ( tooltip.rcRect.y1 - 5 < rcScreenRect.y1 ) 
			tooltip.rcRect.Move( 0, rcScreenRect.y1 - tooltip.rcRect.y1 + 5 );
		if ( tooltip.rcRect.x2 + 5 >= rcScreenRect.x2 ) 
			tooltip.rcRect.Move( rcScreenRect.x2 - tooltip.rcRect.x2 - 5, 0 );
		if ( tooltip.rcRect.y2 + 5 >= rcScreenRect.y2 ) 
			tooltip.rcRect.Move( 0, rcScreenRect.y2 - tooltip.rcRect.y2 - 5 );
		tooltip.dwBorderColor = dwColor != 0 ? dwColor : 0xffcdcd00;
	}
}
void CScene::RandomizeRainDrop( SRainDrop &drop )
{
	drop.vPos.x = NWin32Random::Random( viewableTerrainRect.x1, viewableTerrainRect.x2 );
	drop.vPos.y = NWin32Random::Random( viewableTerrainRect.y1, viewableTerrainRect.y2 );
	drop.vPos.z = fRainHeight;
	drop.fLength = NWin32Random::Random( 0.0f, -drop.vPos.z / vRainDir.z );
}
void CScene::RandomizeSnowFlake( SSnowFlake &flake )
{
	flake.vPos.x = NWin32Random::Random( viewableTerrainRect.x1, viewableTerrainRect.x2 );
	flake.vPos.y = NWin32Random::Random( viewableTerrainRect.y1, viewableTerrainRect.y2 );
	flake.vPos.z = NWin32Random::Random( 0.0f, fSnowHeight );
	flake.fPhase = NWin32Random::Random( 0.0f, float(PI) );
}
float GetCondensedRandom( float fMin, float fMax, float fMedium, bool bDoubleDensity = true )
{
	float result = NWin32Random::Random( 0.0f, 1.0f );
	result *= result;
	if ( bDoubleDensity )
	{
		result *= result;
		result *= result;
		result *= result;
		result *= result;
	}
	if ( NWin32Random::Random( fMin, fMax ) > fMedium )
		return result * ( fMax - fMedium ) + fMedium;
	else
		return (1.0f - result) * ( fMedium - fMin ) + fMin;
}
void CScene::RandomizeSand( SSandParticle &particle )
{
	particle.bCone = (NWin32Random::Random( 0.0f, 1.0f ) < 0.1f);
	particle.bConeDraw = particle.bCone;
	if ( particle.bCone )
	{
		particle.vPos.x = GetCondensedRandom( viewableTerrainRect.x1, viewableTerrainRect.x2, vSandCone.x );
		particle.vPos.y = GetCondensedRandom( viewableTerrainRect.y1, viewableTerrainRect.y2, vSandCone.y );
		particle.vPos.z = GetCondensedRandom( 0.0f, fSandHeight, 0.0f, false );
	}
	else
	{
		particle.vPos.x = NWin32Random::Random( viewableTerrainRect.x1, viewableTerrainRect.x2 );
		particle.vPos.y = NWin32Random::Random( viewableTerrainRect.y1, viewableTerrainRect.y2 );
		particle.vPos.z = NWin32Random::Random( 0.0f, fSandHeight );
	}
	particle.vPhase.x = NWin32Random::Random( 0.0f, 2.0f * float(PI) );
	particle.vPhase.y = NWin32Random::Random( 0.0f, 2.0f * float(PI) );
	particle.vPhase.z = NWin32Random::Random( 0.0f, 2.0f * float(PI) );
}
void CScene::SetDirectionalArrow( const CVec3 &vStart, const CVec3 &vEnd, bool bDraw )
{
	bDrawArrow = bDraw;
	vArrowStart = vStart;
	vArrowDir = vEnd - vStart;
	float fArrowLenTemp = fabs( vArrowDir );
	fArrowLenTemp *= 0.2f;
	if ( fArrowLenTemp != 0 )
		vArrowDir /= fArrowLenTemp;
}
void CScene::SetClickMarker( const CVec3 &vPos )
{
	SClickMarker mark;
	mark.vPos = vPos;
	mark.nStartTime = GetSingleton<IGameTimer>()->GetAbsTime();
	clickMarkers.push_back( mark );
}
void CScene::SetPosMarker( const CVec3 &vPos )
{
	posMarkers.push_back( vPos );
}
void CScene::SetRotationStartAngle( float fAngle, bool bRotate )
{
	bRotateMarkers = bRotate;
	fArrowBegin = fAngle;
}
void CScene::FlashPosMarkers()
{
	if ( !posMarkers.empty() )
	{
		float fShiftAngle = 0;
		if ( bRotateMarkers )
		{
			CVec2 vDir;
			vDir.Set( vArrowDir.x, vArrowDir.y );
			Normalize( &vDir );
			fShiftAngle = vDir.x < 0 ? -1.0f * acos( vDir.y ) : acos( vDir.y );
			fShiftAngle -= fArrowBegin;
		}
		for ( std::list<CVec3>::iterator it = posMarkers.begin(); it != posMarkers.end(); ++it )
		{
			CVec3 vPos = (*it);
			if ( bRotateMarkers )
			{
				vPos.Set( vPos.x * FCos( fShiftAngle ) + vPos.y * FSin( fShiftAngle ), - vPos.x * FSin( fShiftAngle ) + vPos.y * FCos( fShiftAngle ), vPos.z );
				vPos += vArrowStart;
			}
			SetClickMarker( vPos );
		}
		posMarkers.clear();
	}
}
void CScene::ResetPosMarkers()
{
	posMarkers.clear(); 
}
void CScene::Reposition()
{
	for ( std::list< CPtr<IUIScreen> >::iterator it = uiScreens.begin(); it != uiScreens.end(); ++it )
		(*it)->Reposition( pGFX->GetScreenRect() );
	pSoundScene->InitScreenResolutionConsts();
	if ( pTerrain ) 
		pTerrain->ResetPosition();
}
void CScene::SwitchWeather( bool bOn )
{
	if ( bWeatherOn )
	{
		if ( ((eWeatherCondition == SC_NONE || eWeatherCondition == SC_FINISHING) && !bOn) || ((eWeatherCondition == SC_STARTING || eWeatherCondition== SC_ON) && bOn) )
			return;
		if ( bOn )
		{
			eWeatherCondition = SC_STARTING;
			if ( wAmbientID != 0 )
				RemoveSound( wAmbientID );
			if ( eCurrSetting == ST_SAND )
			{
				wAmbientID = AddSound( "Amb_tornado_circle", VNULL3, SFX_MIX_IF_TIME_EQUALS, SAM_LOOPED_NEED_ID );
			}
			else if ( eCurrSetting == ST_RAIN )
			{
				wAmbientID = AddSound( "Amb_Rain_start", VNULL3, SFX_INTERFACE, SAM_NEED_ID );
			}
		}
		else
		{
			eWeatherCondition = SC_FINISHING;
			RemoveSound( wAmbientID );
			if ( eCurrSetting == ST_RAIN )
				wAmbientID = AddSound( "Amb_Rain_end", VNULL3, SFX_INTERFACE, SAM_NEED_ID );
			else
				wAmbientID = 0;
		}
		pSoundScene->MuteTerrain( bOn );
	}
}
bool CScene::IsRaining()
{
	return eCurrSetting == ST_RAIN && eWeatherCondition != SC_NONE;
}
void CScene::SetWeatherQuality( float fCoeff )
{
	nRainDensity = GetGlobalVar( "Scene.Weather.Rain.Density", 1000 ) * fCoeff;
	nMinSnowDensity = GetGlobalVar( "Scene.Weather.Snow.MinDensity", 300 ) * fCoeff;
	nMaxSnowDensity = GetGlobalVar( "Scene.Weather.Snow.MaxDensity", 3000 ) * fCoeff;
	nSandDensity = GetGlobalVar( "Scene.Weather.Sand.Density", 2000 ) * fCoeff;
}
class CSinCosTable
{
	float sintable[32768];
	float costable[32768];
public:
	CSinCosTable()
	{
		for ( int i=0; i<32768; ++i )
		{
			double fAngle = Index2Angle( i );
			sintable[i] = sin( fAngle );
			costable[i] = cos( fAngle );
		}
	}
	int Angle2Index( float fAngle ) const { return MINT( fAngle / FP_2PI * 32768.0f ); }
	double Index2Angle( int nIndex ) const { return ( double( nIndex ) * 2.0 * PI / 32768.0 ); }
	float Sin( float fAngle ) const { return sintable[ Angle2Index( fAngle ) ]; }
	float Cos( float fAngle ) const { return costable[ Angle2Index( fAngle ) ]; }
};
static CSinCosTable sincos;
float Sin( float fAngle ) { return sincos.Sin( fAngle ); }
float Cos( float fAngle ) { return sincos.Cos( fAngle ); }
