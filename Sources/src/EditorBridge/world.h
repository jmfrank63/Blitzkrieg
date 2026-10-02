#ifndef __EDITOR_BRIDGE_WORLD_H__
#define __EDITOR_BRIDGE_WORLD_H__
#include "../Common/WorldBase.h"

// The engine's own object layer, as the MFC editor uses it
// (TemplateEditorFrame1.h:75): CWorldBase::Update turns the AI's notifications
// into map objects with visuals in the scene, which is what draws a unit and
// what IScene::Pick finds. The editor has no selection in the game's sense, so
// the one pure virtual does nothing.
class CEditorWorld : public CWorldBase
{
public:
	virtual void ResetSelection( SMapObject *pMO ) {  }
	// One world update at the current time, as the MFC editor runs one after
	// every edit (TemplateEditorFrame1.cpp:697-698): the game timer is advanced
	// the way the game's main loop advances it (Main/iMainInternal.cpp:889-891),
	// then the world reads what the AI has to tell it.
	void UpdateNow();
	// Hide checked (04-09, D-16), the MFC editor's own way (TemplateEditorFrame1
	// .cpp, the group check boxes: RemoveFromScene and AddToScene): a hidden
	// object leaves the scene - its visual, its shadow and its icons with it -
	// and comes back whole. Both are no-ops for an object already out or in.
	void HideMapObject( SMapObject *pMO ) { if ( pMO != 0 && pMO->pVisObj.GetPtr() != 0 && IsInScene( pMO ) ) RemoveFromScene( pMO ); }
	void ShowMapObject( SMapObject *pMO ) { if ( pMO != 0 && pMO->pVisObj.GetPtr() != 0 && !IsInScene( pMO ) ) AddToScene( pMO ); }
	// The Layers menu's Units Passability (M3, D-32): CWorldBase::ToggleAIInfo is
	// protected - the game's own caller is a subclass - so the bridge reaches it
	// through the world it owns. Flips and answers the new state.
	bool TogglePassability() { return ToggleAIInfo(); }
	// Every map object the world holds, for the engine tier's check that the
	// picture and the session agree (WorldMatchesSession).
	void GetObjects( std::vector<SMapObject*> *pObjects );
};

#endif // __EDITOR_BRIDGE_WORLD_H__
