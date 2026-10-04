#pragma once
// Mesh (Unit) sub-editor - project extension .msh, project XML root tag
// "Mesh_Composer_Project". MFC source: Sources/src/editor/MeshTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:86-104.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMeshTreeRootItem : public CStatsItem
{
public:
	CMeshTreeRootItem() : CStatsItem( ETIT_MESH_ROOT_ITEM, "Mesh_Composer_Project" ) {}
};
class CMeshCommonPropsItem     : public CStatsItem { public: CMeshCommonPropsItem()     : CStatsItem( ETIT_MESH_COMMON_PROPS_ITEM ) {} };
class CMeshDefencesItem        : public CStatsItem { public: CMeshDefencesItem()        : CStatsItem( ETIT_MESH_DEFENCES_ITEM ) {} };
class CMeshDefencePropsItem    : public CStatsItem { public: CMeshDefencePropsItem()    : CStatsItem( ETIT_MESH_DEFENCE_PROPS_ITEM ) {} };
class CMeshGraphicsItem        : public CStatsItem { public: CMeshGraphicsItem()        : CStatsItem( ETIT_MESH_GRAPHICS_ITEM ) {} };
class CMeshPlatformsItem       : public CStatsItem { public: CMeshPlatformsItem()       : CStatsItem( ETIT_MESH_PLATFORMS_ITEM ) {} };
class CMeshPlatformPropsItem   : public CStatsItem { public: CMeshPlatformPropsItem()   : CStatsItem( ETIT_MESH_PLATFORM_PROPS_ITEM ) {} };
class CMeshGunsItem            : public CStatsItem { public: CMeshGunsItem()            : CStatsItem( ETIT_MESH_GUNS_ITEM ) {} };
class CMeshGunPropsItem        : public CStatsItem { public: CMeshGunPropsItem()        : CStatsItem( ETIT_MESH_GUN_PROPS_ITEM ) {} };
class CMeshJoggingsItem        : public CStatsItem { public: CMeshJoggingsItem()        : CStatsItem( ETIT_MESH_JOGGINGS_ITEM ) {} };
class CMeshJoggingPropsItem    : public CStatsItem { public: CMeshJoggingPropsItem()    : CStatsItem( ETIT_MESH_JOGGING_PROPS_ITEM ) {} };
class CMeshLocatorsItem        : public CStatsItem { public: CMeshLocatorsItem()        : CStatsItem( ETIT_MESH_LOCATORS_ITEM ) {} };
class CMeshLocatorPropsItem    : public CStatsItem { public: CMeshLocatorPropsItem()    : CStatsItem( ETIT_MESH_LOCATOR_PROPS_ITEM ) {} };
class CMeshAviaItem            : public CStatsItem { public: CMeshAviaItem()            : CStatsItem( ETIT_MESH_AVIA_ITEM ) {} };
class CMeshEffectsItem         : public CStatsItem { public: CMeshEffectsItem()         : CStatsItem( ETIT_MESH_EFFECTS_ITEM ) {} };
class CMeshSoundPropsItem      : public CStatsItem { public: CMeshSoundPropsItem()      : CStatsItem( ETIT_MESH_SOUND_PROPS_ITEM ) {} };
class CMeshDeathCratersItem    : public CStatsItem { public: CMeshDeathCratersItem()    : CStatsItem( ETIT_MESH_DEATH_CRATERS_ITEM ) {} };
class CMeshDeathCraterPropsItem: public CStatsItem { public: CMeshDeathCraterPropsItem(): CStatsItem( ETIT_MESH_DEATH_CRATER_PROPS_ITEM ) {} };
class CMeshTrackItem           : public CStatsItem { public: CMeshTrackItem()           : CStatsItem( ETIT_MESH_TRACK_ITEM ) {} };

}
