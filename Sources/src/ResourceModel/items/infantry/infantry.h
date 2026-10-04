#pragma once
// Infantry (Animation/Unit) sub-editor - project extension .unt, project XML
// root tag "Unit_Composer_Project". MFC source:
// Sources/src/editor/AnimTreeItem.{h,cpp}. Despite the ".unt" extension this
// is the Infantry sub-editor; CParentFrame subclass AnimFrm.cpp, see MEM005.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:36-53
// (sans E_LOCALIZATION_ITEM, which the port parks with the Localization owner
// in references.cpp - it is shared with mission/chapter sub-editors).

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CAnimationTreeRootItem : public CStatsItem
{
public:
	CAnimationTreeRootItem() : CStatsItem( ETIT_ANIMATION_ROOT_ITEM, "Unit_Composer_Project" ) {}
};
class CUnitCommonPropsItem    : public CStatsItem { public: CUnitCommonPropsItem()    : CStatsItem( ETIT_UNIT_COMMON_PROPS_ITEM ) {} };
class CUnitAIPropsItem        : public CStatsItem { public: CUnitAIPropsItem()        : CStatsItem( ETIT_UNIT_AI_PROPS_ITEM ) {} };
class CUnitWeaponPropsItem    : public CStatsItem { public: CUnitWeaponPropsItem()    : CStatsItem( ETIT_UNIT_WEAPON_PROPS_ITEM ) {} };
class CUnitGrenadePropsItem   : public CStatsItem { public: CUnitGrenadePropsItem()   : CStatsItem( ETIT_UNIT_GRENADE_PROPS_ITEM ) {} };
class CDirectoryPropsItem     : public CStatsItem { public: CDirectoryPropsItem()     : CStatsItem( ETIT_UNIT_DIRECTORY_PROPS_ITEM ) {} };
class CUnitSeasonPropsItem    : public CStatsItem { public: CUnitSeasonPropsItem()    : CStatsItem( ETIT_UNIT_SEASON_PROPS_ITEM ) {} };
class CDirectoriesItem        : public CStatsItem { public: CDirectoriesItem()        : CStatsItem( ETIT_UNIT_DIRECTORIES_ITEM ) {} };
class CUnitAnimationsItem     : public CStatsItem { public: CUnitAnimationsItem()     : CStatsItem( ETIT_UNIT_ANIMATIONS_ITEM ) {} };
class CUnitAnimationPropsItem : public CStatsItem { public: CUnitAnimationPropsItem() : CStatsItem( ETIT_UNIT_ANIMATION_PROPS_ITEM ) {} };
class CUnitFramePropsItem     : public CStatsItem { public: CUnitFramePropsItem()     : CStatsItem( ETIT_UNIT_FRAME_PROPS_ITEM ) {} };
class CUnitActionsItem        : public CStatsItem { public: CUnitActionsItem()        : CStatsItem( ETIT_UNIT_ACTIONS_ITEM ) {} };
class CUnitActionPropsItem    : public CStatsItem { public: CUnitActionPropsItem()    : CStatsItem( ETIT_UNIT_ACTION_PROPS_ITEM ) {} };
class CUnitExposuresItem      : public CStatsItem { public: CUnitExposuresItem()      : CStatsItem( ETIT_UNIT_EXPOSURES_ITEM ) {} };
class CUnitAcksItem           : public CStatsItem { public: CUnitAcksItem()           : CStatsItem( ETIT_UNIT_ACKS_ITEM ) {} };
class CUnitAckTypesItem       : public CStatsItem { public: CUnitAckTypesItem()       : CStatsItem( ETIT_UNIT_ACK_TYPES_ITEM ) {} };
class CUnitAckTypePropsItem   : public CStatsItem { public: CUnitAckTypePropsItem()   : CStatsItem( ETIT_UNIT_ACK_TYPE_PROPS_ITEM ) {} };

}
