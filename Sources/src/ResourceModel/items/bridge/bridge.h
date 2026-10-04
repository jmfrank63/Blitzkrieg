#pragma once
// Bridge sub-editor - project extension .bdg, project XML root tag
// "Bridge_Composer_Project". MFC source: Sources/src/editor/BridgeTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:226-242.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CBridgeTreeRootItem : public CStatsItem
{
public:
	CBridgeTreeRootItem() : CStatsItem( ETIT_BRIDGE_ROOT_ITEM, "Bridge_Composer_Project" ) {}
};
class CBridgeDefencesItem         : public CStatsItem { public: CBridgeDefencesItem()         : CStatsItem( ETIT_BRIDGE_DEFENCES_ITEM ) {} };
class CBridgeDefencePropsItem     : public CStatsItem { public: CBridgeDefencePropsItem()     : CStatsItem( ETIT_BRIDGE_DEFENCE_PROPS_ITEM ) {} };
class CBridgeCommonPropsItem      : public CStatsItem { public: CBridgeCommonPropsItem()      : CStatsItem( ETIT_BRIDGE_COMMON_PROPS_ITEM ) {} };
class CBridgeBeginSpansItem       : public CStatsItem { public: CBridgeBeginSpansItem()       : CStatsItem( ETIT_BRIDGE_BEGIN_SPANS_ITEM ) {} };
class CBridgeCenterSpansItem      : public CStatsItem { public: CBridgeCenterSpansItem()      : CStatsItem( ETIT_BRIDGE_CENTER_SPANS_ITEM ) {} };
class CBridgeEndSpansItem         : public CStatsItem { public: CBridgeEndSpansItem()         : CStatsItem( ETIT_BRIDGE_END_SPANS_ITEM ) {} };
class CBridgePartsItem            : public CStatsItem { public: CBridgePartsItem()            : CStatsItem( ETIT_BRIDGE_PARTS_ITEM ) {} };
class CBridgePartPropsItem        : public CStatsItem { public: CBridgePartPropsItem()        : CStatsItem( ETIT_BRIDGE_PART_PROPS_ITEM ) {} };
class CBridgeStagePropsItem       : public CStatsItem { public: CBridgeStagePropsItem()       : CStatsItem( ETIT_BRIDGE_STAGE_PROPS_ITEM ) {} };
class CBridgeFirePointsItem       : public CStatsItem { public: CBridgeFirePointsItem()       : CStatsItem( ETIT_BRIDGE_FIRE_POINTS_ITEM ) {} };
class CBridgeFirePointPropsItem   : public CStatsItem { public: CBridgeFirePointPropsItem()   : CStatsItem( ETIT_BRIDGE_FIRE_POINT_PROPS_ITEM ) {} };
class CBridgeDirExplosionsItem    : public CStatsItem { public: CBridgeDirExplosionsItem()    : CStatsItem( ETIT_BRIDGE_DIR_EXPLOSIONS_ITEM ) {} };
class CBridgeDirExplosionPropsItem: public CStatsItem { public: CBridgeDirExplosionPropsItem(): CStatsItem( ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM ) {} };
class CBridgeSmokesItem           : public CStatsItem { public: CBridgeSmokesItem()           : CStatsItem( ETIT_BRIDGE_SMOKES_ITEM ) {} };
class CBridgeSmokePropsItem       : public CStatsItem { public: CBridgeSmokePropsItem()       : CStatsItem( ETIT_BRIDGE_SMOKE_PROPS_ITEM ) {} };

}
