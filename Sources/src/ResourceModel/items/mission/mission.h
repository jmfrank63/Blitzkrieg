#pragma once
// Mission sub-editor - project extension .mip, project XML root tag
// "Mission_Composer_Project". MFC source: Sources/src/editor/MissionTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:244-249.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMissionTreeRootItem : public CStatsItem
{
public:
	CMissionTreeRootItem() : CStatsItem( ETIT_MISSION_ROOT_ITEM, "Mission_Composer_Project" ) {}
};
class CMissionCommonPropsItem    : public CStatsItem { public: CMissionCommonPropsItem()    : CStatsItem( ETIT_MISSION_COMMON_PROPS_ITEM ) {} };
class CMissionObjectivesItem     : public CStatsItem { public: CMissionObjectivesItem()     : CStatsItem( ETIT_MISSION_OBJECTIVES_ITEM ) {} };
class CMissionObjectivePropsItem : public CStatsItem { public: CMissionObjectivePropsItem() : CStatsItem( ETIT_MISSION_OBJECTIVE_PROPS_ITEM ) {} };
class CMissionMusicsItem         : public CStatsItem { public: CMissionMusicsItem()         : CStatsItem( ETIT_MISSION_MUSICS_ITEM ) {} };
class CMissionMusicPropsItem     : public CStatsItem { public: CMissionMusicPropsItem()     : CStatsItem( ETIT_MISSION_MUSIC_PROPS_ITEM ) {} };

}
