// The Layers menu (M3, D-32): what the renderer draws, and the fire-range
// layer. Renderer state only - nothing here reads or writes map data, and the
// document is never dirtied. The MFC editor's own handlers are
// TemplateEditorFrame1.cpp:5318-5777 (each a `while ( ToggleShow( X ) !=
// wanted )` over IScene::ToggleShow) and 2115 (ShowFireRange).
//
// ToggleShow flips a flag and answers the new state; the scene has no read of
// it. Driving a layer to a wanted state therefore flips once and, if that was
// the wrong way, once more - two calls at most, never an unbounded loop (a
// toggle that does not answer as expected must not hang the editor). The
// terrain-owned flags (grid, noise) are pushed to the terrain by the LAST
// toggle, so a terrain made after the flag was set gets it with a re-apply
// whichever way the flag stood.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "world.h"
#include "../Scene/Scene.h"
#include "../Scene/Terrain.h"
#include "../GFX/GFX.H"
#include "../AILogic/AILogic.h"
#include "../Main/GameDB.h"
#include <algorithm>
#include <cctype>
#include <Misc/Tools.h>

namespace
{

const unsigned LAYER_ALL_BITS = ( 1u << BK_EDITOR_LAYER_COUNT ) - 1;

// The MFC editor's starting menu (TemplateEditorFrame1.cpp:379 and
// ClearAllDataBeforeNewMap :3667): terrain, noise, black stripes, units,
// objects, shadows and haze shown; grid, wire frame, depth complexity,
// bounding boxes, war fog, passability and fire ranges hidden.
const unsigned LAYER_DEFAULTS =
	( 1u << BK_EDITOR_LAYER_TERRAIN ) | ( 1u << BK_EDITOR_LAYER_TERRAIN_NOISE ) | ( 1u << BK_EDITOR_LAYER_BLACK_STRIPES ) |
	( 1u << BK_EDITOR_LAYER_UNITS ) | ( 1u << BK_EDITOR_LAYER_OBJECTS ) | ( 1u << BK_EDITOR_LAYER_SHADOWS ) | ( 1u << BK_EDITOR_LAYER_HAZE );

// The IScene::ToggleShow flag a layer is, or -1 for a layer that is not one.
int SceneFlagOf( int nLayer )
{
	switch ( nLayer )
	{
		case BK_EDITOR_LAYER_TERRAIN: return SCENE_SHOW_TERRAIN;
		case BK_EDITOR_LAYER_GRID: return SCENE_SHOW_GRID;
		case BK_EDITOR_LAYER_DEPTH_COMPLEXITY: return SCENE_SHOW_DEPTH_COMPLEXITY;
		case BK_EDITOR_LAYER_TERRAIN_NOISE: return SCENE_SHOW_NOISE;
		case BK_EDITOR_LAYER_BLACK_STRIPES: return SCENE_SHOW_BORDER;
		case BK_EDITOR_LAYER_UNITS: return SCENE_SHOW_UNITS;
		case BK_EDITOR_LAYER_OBJECTS: return SCENE_SHOW_OBJECTS;
		case BK_EDITOR_LAYER_BOUNDING_BOXES: return SCENE_SHOW_BBS;
		case BK_EDITOR_LAYER_SHADOWS: return SCENE_SHOW_SHADOWS;
		case BK_EDITOR_LAYER_HAZE: return SCENE_SHOW_HAZE;
		case BK_EDITOR_LAYER_WAR_FOG: return SCENE_SHOW_WARFOG;
		default: return -1;
	}
}

const char *LayerName( int nLayer )
{
	static const char *const names[BK_EDITOR_LAYER_COUNT] = {
		"Terrain", "Grid", "Wire Frame", "Depth Complexity", "Terrain Noise", "Black Stripes", "Units", "Objects",
		"Bounding Boxes", "Shadows", "Haze", "War Fog", "Units Passability", "Unit Fire Ranges" };
	return nLayer >= 0 && nLayer < BK_EDITOR_LAYER_COUNT ? names[nLayer] : "?";
}

// Flips the scene's flag at most twice until it answers `bWanted`.
bool DriveSceneFlag( IScene *pScene, int nFlag, bool bWanted )
{
	for ( int i = 0; i < 2; ++i )
		if ( pScene->ToggleShow( nFlag ) == bWanted )
			return true;
	return false;
}

// Puts one layer on the engine; bUpdateWorld says whether passability marks may
// be asked for now (not in the middle of building a map). The wire frame is a render state that only
// lives inside a frame (ApplyWireframeForFrame says it to the renderer every
// frame), so it has nothing to do here.
bool ApplyLayerToEngine( SEditorSession *pSession, int nLayer, bool bShown, bool bUpdateWorld )
{
	if ( nLayer == BK_EDITOR_LAYER_WIREFRAME || nLayer == BK_EDITOR_LAYER_UNIT_FIRE_RANGES )
		return true;
	if ( nLayer == BK_EDITOR_LAYER_UNITS_PASSABILITY )
	{
		// ToggleAIInfo's off branch clears the terrain's marks, so it needs one.
		IScene *pScene = GetSingleton<IScene>();
		if ( pSession->pWorld == 0 || pScene == 0 || pScene->GetTerrain() == 0 )
		{
			pSession->szMessage = "there is no terrain to show passability on";
			return false;
		}
		bool bAnswered = false;
		for ( int i = 0; i < 2 && !bAnswered; ++i )
			bAnswered = pSession->pWorld->TogglePassability() == bShown;
		if ( !bAnswered )
		{
			pSession->szMessage = "the world would not switch passability";
			return false;
		}
		// The marks are the AI's answer for the screen the camera shows; the
		// toggle only asks for them at the next world update.
		if ( bShown && bUpdateWorld )
			UpdateSessionWorld( pSession );
		return true;
	}
	const int nFlag = SceneFlagOf( nLayer );
	IScene *pScene = GetSingleton<IScene>();
	if ( nFlag < 0 || pScene == 0 )
	{
		pSession->szMessage = "there is no scene";
		return false;
	}
	if ( !DriveSceneFlag( pScene, nFlag, bShown ) )
	{
		pSession->szMessage = std::string( "the scene would not switch " ) + LayerName( nLayer );
		return false;
	}
	return true;
}

std::string Lower( const std::string &rszIn )
{
	std::string szOut = rszIn;
	for ( size_t i = 0; i < szOut.size(); ++i )
		szOut[i] = char( tolower( static_cast<unsigned char>( szOut[i] ) ) );
	return szOut;
}

// The MFC's SSimpleFilter::Check: any condition list whose words are all in
// the lowercased path passes (an empty filter passes nothing).
bool FilterPasses( const std::vector< std::vector<std::string> > &rLists, const std::string &rszPathLower )
{
	for ( size_t nList = 0; nList < rLists.size(); ++nList )
	{
		bool bAll = true;
		for ( size_t nWord = 0; nWord < rLists[nList].size() && bAll; ++nWord )
			bAll = rszPathLower.find( Lower( rLists[nList][nWord] ) ) != std::string::npos;
		if ( bAll )
			return true;
	}
	return false;
}

bool IsFiringUnit( const SMapObject *pObject )
{
	return pObject != 0 && pObject->pAIObj.GetPtr() != 0 && ( pObject->IsHuman() || pObject->IsTechnics() );
}

}

unsigned LayerDefaultBits()
{
	return LAYER_DEFAULTS;
}

// What 05-06's engine-tier probe (TestM3LayerProbe) measured in the GPU
// renderer, layer by layer, in two views of arnheim at 640x480:
//   - every layer's toggle changes the frame and puts it back exactly, except
//     Depth Complexity, which is not a rendering of anything: the MFC's D3D
//     path counts overdraw in the stencil and paints 20 rects by count
//     (SceneDraw.cpp:731-741 over effects 300, 301, 310-329), and the GPU
//     renderer's stencil has one mode (effects.StencilMode.darken_once) and no
//     counter, so the layer paints the whole frame white;
//   - Wire Frame did nothing until the renderer learned the fill mode
//     (GFXGPU_STATE_WIREFRAME was dropped by the state switch); it now
//     changes the frame (Renderer.wireframe, a pipeline-key bit).
// The layer that cannot be drawn is refused rather than offered as a button
// that blanks the picture; the menu greys it with this finding as its tip.
unsigned LayerAvailableMask()
{
	return LAYER_ALL_BITS & ~( 1u << BK_EDITOR_LAYER_DEPTH_COMPLEXITY );
}

BkEditorStatus SetLayerInSession( SEditorSession *pSession, int nLayer, int bShown )
{
	if ( nLayer < 0 || nLayer >= BK_EDITOR_LAYER_COUNT )
	{
		pSession->szMessage = "no such layer";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( nLayer == BK_EDITOR_LAYER_UNIT_FIRE_RANGES )
	{
		pSession->szMessage = "the fire ranges are a mode, not a toggle: BkEditorSetFireRangeMode";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( !pSession->bEngineStarted || !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return BK_EDITOR_REFUSED;
	}
	if ( ( LayerAvailableMask() & ( 1u << nLayer ) ) == 0 )
	{
		pSession->szMessage = std::string( LayerName( nLayer ) ) + " cannot be drawn by this renderer";
		return BK_EDITOR_REFUSED;
	}
	const bool bWanted = bShown != 0;
	if ( !ApplyLayerToEngine( pSession, nLayer, bWanted, true ) )
		return BK_EDITOR_REFUSED;
	if ( bWanted )
		pSession->nLayerBits |= ( 1u << nLayer );
	else
		pSession->nLayerBits &= ~( 1u << nLayer );
	return BK_EDITOR_OK;
}

void ReapplyLayersInSession( SEditorSession *pSession )
{
	// The fire-range bit is not the engine's to carry across: the AI's groups
	// went with the old map and the caller asks for the mode again.
	pSession->nLayerBits &= ~( 1u << BK_EDITOR_LAYER_UNIT_FIRE_RANGES );
	for ( int nLayer = 0; nLayer < BK_EDITOR_LAYER_COUNT; ++nLayer )
	{
		if ( ( LayerAvailableMask() & ( 1u << nLayer ) ) == 0 )
			continue;
		// A layer the engine would not take is left as the engine has it; the
		// session's bit stays what was asked, and the next explicit call says why.
		const std::string szKept = pSession->szMessage;
		ApplyLayerToEngine( pSession, nLayer, ( pSession->nLayerBits & ( 1u << nLayer ) ) != 0, false );
		pSession->szMessage = szKept;
	}
}

void ApplyWireframeForFrame( SEditorSession *pSession )
{
	if ( ( LayerAvailableMask() & ( 1u << BK_EDITOR_LAYER_WIREFRAME ) ) == 0 )
		return;
	if ( IGFX *pGFX = GetSingleton<IGFX>() )
		pGFX->SetWireframe( ( pSession->nLayerBits & ( 1u << BK_EDITOR_LAYER_WIREFRAME ) ) != 0 );
}

bool LayersNeedWorldUpdate( const SEditorSession *pSession )
{
	return ( pSession->nLayerBits & ( ( 1u << BK_EDITOR_LAYER_UNITS_PASSABILITY ) | ( 1u << BK_EDITOR_LAYER_UNIT_FIRE_RANGES ) ) ) != 0;
}

void DropFireRangeInSession( SEditorSession *pSession )
{
	if ( pSession->nFireRangeGroup != -1 )
	{
		if ( IAILogic *pAILogic = GetSingleton<IAILogic>() )
		{
			pAILogic->ShowAreas( pSession->nFireRangeGroup, ACTION_NOTIFY_SHOOT_AREA, false );
			pAILogic->UnregisterGroup( WORD( pSession->nFireRangeGroup ) );
		}
		pSession->nFireRangeGroup = -1;
	}
	pSession->nFireRangeMode = BK_EDITOR_FIRE_OFF;
	pSession->szFireRangeFilter.clear();
	pSession->nLayerBits &= ~( 1u << BK_EDITOR_LAYER_UNIT_FIRE_RANGES );
}

BkEditorStatus SetFireRangeInSession( SEditorSession *pSession, int nMode, const char *pszFilter, const int *pnLinkIDs, int nCount )
{
	if ( nMode < BK_EDITOR_FIRE_OFF || nMode > BK_EDITOR_FIRE_FILTER )
	{
		pSession->szMessage = "no such fire-range mode";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	if ( nCount < 0 || ( nCount > 0 && pnLinkIDs == 0 ) )
	{
		pSession->szMessage = "the selection is a negative count or a null list";
		return BK_EDITOR_BAD_ARGUMENT;
	}
	// The filter is checked before anything changes: an unknown name leaves the
	// ranges that were showing in place.
	std::vector< std::vector<std::string> > filterLists;
	if ( nMode == BK_EDITOR_FIRE_FILTER )
	{
		if ( pszFilter == 0 || *pszFilter == 0 )
		{
			pSession->szMessage = "a filter mode needs a filter name";
			return BK_EDITOR_BAD_ARGUMENT;
		}
		if ( !ReadObjectFilterLists( pszFilter, &filterLists ) )
		{
			pSession->szMessage = std::string( "no object filter is named " ) + pszFilter;
			return BK_EDITOR_BAD_ARGUMENT;
		}
	}
	if ( !pSession->bEngineStarted || !pSession->bMapOpen )
	{
		pSession->szMessage = "no map is open";
		return BK_EDITOR_REFUSED;
	}
	IAILogic *pAILogic = GetSingleton<IAILogic>();
	if ( pAILogic == 0 || pSession->pWorld == 0 )
	{
		pSession->szMessage = "the AI is not there";
		return BK_EDITOR_REFUSED;
	}

	// As the MFC: the old group goes first, then the new one is registered.
	DropFireRangeInSession( pSession );
	if ( nMode == BK_EDITOR_FIRE_OFF )
	{
		UpdateSessionWorld( pSession );
		return BK_EDITOR_OK;
	}

	std::vector<IRefCount*> units;
	std::set<IRefCount*> seen;
	if ( nMode == BK_EDITOR_FIRE_SELECTED )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			const std::unordered_map<int, CPtr<IRefCount> >::const_iterator it = pSession->byLinkID.find( pnLinkIDs[i] );
			if ( it == pSession->byLinkID.end() || it->second.GetPtr() == 0 )
				continue;
			const SMapObject *pObject = pSession->pWorld->FindByAI( it->second.GetPtr() );
			if ( IsFiringUnit( pObject ) && seen.insert( it->second.GetPtr() ).second )
				units.push_back( it->second.GetPtr() );
		}
	}
	else
	{
		std::vector<SMapObject*> objects;
		pSession->pWorld->GetObjects( &objects );
		for ( size_t i = 0; i < objects.size(); ++i )
		{
			if ( !IsFiringUnit( objects[i] ) || !objects[i]->pDesc )
				continue;
			if ( FilterPasses( filterLists, Lower( objects[i]->pDesc->szPath ) ) && seen.insert( objects[i]->pAIObj.GetPtr() ).second )
				units.push_back( objects[i]->pAIObj.GetPtr() );
		}
	}
	if ( !units.empty() )
	{
		pSession->nFireRangeGroup = int( pAILogic->GenerateGroupNumber() );
		pAILogic->RegisterGroup( &units[0], int( units.size() ), WORD( pSession->nFireRangeGroup ) );
		pAILogic->ShowAreas( pSession->nFireRangeGroup, ACTION_NOTIFY_SHOOT_AREA, true );
	}
	pSession->nFireRangeMode = nMode;
	pSession->szFireRangeFilter = nMode == BK_EDITOR_FIRE_FILTER ? pszFilter : "";
	pSession->nLayerBits |= ( 1u << BK_EDITOR_LAYER_UNIT_FIRE_RANGES );
	// The areas reach the scene on the world's update.
	UpdateSessionWorld( pSession );
	return BK_EDITOR_OK;
}
