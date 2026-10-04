#pragma once
// Trench sub-editor - project extension .trc, project XML root tag
// "Trench_Composer_Project". MFC source: Sources/src/editor/TrenchTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:190-195.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CTrenchTreeRootItem : public CStatsItem
{
public:
	CTrenchTreeRootItem() : CStatsItem( ETIT_TRENCH_ROOT_ITEM, "Trench_Composer_Project" ) {}
};
class CTrenchCommonPropsItem : public CStatsItem { public: CTrenchCommonPropsItem() : CStatsItem( ETIT_TRENCH_COMMON_PROPS_ITEM ) {} };
class CTrenchSourcesItem     : public CStatsItem { public: CTrenchSourcesItem()     : CStatsItem( ETIT_TRENCH_SOURCES_ITEM ) {} };
class CTrenchSourcePropsItem : public CStatsItem { public: CTrenchSourcePropsItem() : CStatsItem( ETIT_TRENCH_SOURCE_PROPS_ITEM ) {} };
class CTrenchDefencesItem    : public CStatsItem { public: CTrenchDefencesItem()    : CStatsItem( ETIT_TRENCH_DEFENCES_ITEM ) {} };
class CTrenchDefencePropsItem: public CStatsItem { public: CTrenchDefencePropsItem(): CStatsItem( ETIT_TRENCH_DEFENCE_PROPS_ITEM ) {} };

}
