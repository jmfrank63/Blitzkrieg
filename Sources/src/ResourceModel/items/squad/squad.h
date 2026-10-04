#pragma once
// Squad sub-editor - project extension .scp, project XML root tag
// "Squad_Composer_Project". MFC source: Sources/src/editor/SquadTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:197-202.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CSquadTreeRootItem : public CStatsItem
{
public:
	CSquadTreeRootItem() : CStatsItem( ETIT_SQUAD_ROOT_ITEM, "Squad_Composer_Project" ) {}
};
class CSquadCommonPropsItem   : public CStatsItem { public: CSquadCommonPropsItem()   : CStatsItem( ETIT_SQUAD_COMMON_PROPS_ITEM ) {} };
class CSquadMembersItem       : public CStatsItem { public: CSquadMembersItem()       : CStatsItem( ETIT_SQUAD_MEMBERS_ITEM ) {} };
class CSquadMemberPropsItem   : public CStatsItem { public: CSquadMemberPropsItem()   : CStatsItem( ETIT_SQUAD_MEMBER_PROPS_ITEM ) {} };
class CSquadFormationsItem    : public CStatsItem { public: CSquadFormationsItem()    : CStatsItem( ETIT_SQUAD_FORMATIONS_ITEM ) {} };
class CSquadFormationPropsItem: public CStatsItem { public: CSquadFormationPropsItem(): CStatsItem( ETIT_SQUAD_FORMATION_PROPS_ITEM ) {} };

}
