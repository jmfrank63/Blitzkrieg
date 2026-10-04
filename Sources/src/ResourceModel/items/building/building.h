#pragma once
// Building sub-editor - project extension .bld, project XML root tag
// "Building_Composer_Project". MFC source: Sources/src/editor/BuildTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:117-141.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CBuildingTreeRootItem : public CStatsItem
{
public:
	CBuildingTreeRootItem() : CStatsItem( ETIT_BUILDING_ROOT_ITEM, "Building_Composer_Project" ) {}
};
class CBuildingCommonPropsItem      : public CStatsItem { public: CBuildingCommonPropsItem()      : CStatsItem( ETIT_BUILDING_COMMON_PROPS_ITEM ) {} };
class CBuildingEntrancesItem        : public CStatsItem { public: CBuildingEntrancesItem()        : CStatsItem( ETIT_BUILDING_ENTRANCES_ITEM ) {} };
class CBuildingEntrancePropsItem    : public CStatsItem { public: CBuildingEntrancePropsItem()    : CStatsItem( ETIT_BUILDING_ENTRANCE_PROPS_ITEM ) {} };
class CBuildingSlotsItem            : public CStatsItem { public: CBuildingSlotsItem()            : CStatsItem( ETIT_BUILDING_SLOTS_ITEM ) {} };
class CBuildingSlotPropsItem        : public CStatsItem { public: CBuildingSlotPropsItem()        : CStatsItem( ETIT_BUILDING_SLOT_PROPS_ITEM ) {} };
class CBuildingGraphicsItem         : public CStatsItem { public: CBuildingGraphicsItem()         : CStatsItem( ETIT_BUILDING_GRAPHICS_ITEM ) {} };
class CBuildingGraphic1PropsItem    : public CStatsItem { public: CBuildingGraphic1PropsItem()    : CStatsItem( ETIT_BUILDING_GRAPHIC1_PROPS_ITEM ) {} };
class CBuildingGraphic2PropsItem    : public CStatsItem { public: CBuildingGraphic2PropsItem()    : CStatsItem( ETIT_BUILDING_GRAPHIC2_PROPS_ITEM ) {} };
class CBuildingGraphic3PropsItem    : public CStatsItem { public: CBuildingGraphic3PropsItem()    : CStatsItem( ETIT_BUILDING_GRAPHIC3_PROPS_ITEM ) {} };
class CBuildingDefencesItem         : public CStatsItem { public: CBuildingDefencesItem()         : CStatsItem( ETIT_BUILDING_DEFENCES_ITEM ) {} };
class CBuildingDefencePropsItem     : public CStatsItem { public: CBuildingDefencePropsItem()     : CStatsItem( ETIT_BUILDING_DEFENCE_PROPS_ITEM ) {} };
class CBuildingSummerPropsItem      : public CStatsItem { public: CBuildingSummerPropsItem()      : CStatsItem( ETIT_BUILDING_SUMMER_PROPS_ITEM ) {} };
class CBuildingWinterPropsItem      : public CStatsItem { public: CBuildingWinterPropsItem()      : CStatsItem( ETIT_BUILDING_WINTER_PROPS_ITEM ) {} };
class CBuildingGraphicW1PropsItem   : public CStatsItem { public: CBuildingGraphicW1PropsItem()   : CStatsItem( ETIT_BUILDING_GRAPHICW1_PROPS_ITEM ) {} };
class CBuildingGraphicW2PropsItem   : public CStatsItem { public: CBuildingGraphicW2PropsItem()   : CStatsItem( ETIT_BUILDING_GRAPHICW2_PROPS_ITEM ) {} };
class CBuildingGraphicW3PropsItem   : public CStatsItem { public: CBuildingGraphicW3PropsItem()   : CStatsItem( ETIT_BUILDING_GRAPHICW3_PROPS_ITEM ) {} };
class CBuildingPassesItem           : public CStatsItem { public: CBuildingPassesItem()           : CStatsItem( ETIT_BUILDING_PASSES_ITEM ) {} };
class CBuildingPassPropsItem        : public CStatsItem { public: CBuildingPassPropsItem()        : CStatsItem( ETIT_BUILDING_PASS_PROPS_ITEM ) {} };
class CBuildingFirePointsItem       : public CStatsItem { public: CBuildingFirePointsItem()       : CStatsItem( ETIT_BUILDING_FIRE_POINTS_ITEM ) {} };
class CBuildingFirePointPropsItem   : public CStatsItem { public: CBuildingFirePointPropsItem()   : CStatsItem( ETIT_BUILDING_FIRE_POINT_PROPS_ITEM ) {} };
class CBuildingDirExplosionsItem    : public CStatsItem { public: CBuildingDirExplosionsItem()    : CStatsItem( ETIT_BUILDING_DIR_EXPLOSIONS_ITEM ) {} };
class CBuildingDirExplosionPropsItem: public CStatsItem { public: CBuildingDirExplosionPropsItem(): CStatsItem( ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM ) {} };
class CBuildingSmokesItem           : public CStatsItem { public: CBuildingSmokesItem()           : CStatsItem( ETIT_BUILDING_SMOKES_ITEM ) {} };
class CBuildingSmokePropsItem       : public CStatsItem { public: CBuildingSmokePropsItem()       : CStatsItem( ETIT_BUILDING_SMOKE_PROPS_ITEM ) {} };

}
