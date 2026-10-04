#pragma once
// Chapter sub-editor - project extension .chc, project XML root tag
// "Chapter_Composer_Project". MFC source: Sources/src/editor/ChapterTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:251-256.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CChapterTreeRootItem : public CStatsItem
{
public:
	CChapterTreeRootItem() : CStatsItem( ETIT_CHAPTER_ROOT_ITEM, "Chapter_Composer_Project" ) {}
};
class CChapterCommonPropsItem   : public CStatsItem { public: CChapterCommonPropsItem()   : CStatsItem( ETIT_CHAPTER_COMMON_PROPS_ITEM ) {} };
class CChapterMissionsItem      : public CStatsItem { public: CChapterMissionsItem()      : CStatsItem( ETIT_CHAPTER_MISSIONS_ITEM ) {} };
class CChapterMissionPropsItem  : public CStatsItem { public: CChapterMissionPropsItem()  : CStatsItem( ETIT_CHAPTER_MISSION_PROPS_ITEM ) {} };
class CChapterPlacesItem        : public CStatsItem { public: CChapterPlacesItem()        : CStatsItem( ETIT_CHAPTER_PLACES_ITEM ) {} };
class CChapterPlacePropsItem    : public CStatsItem { public: CChapterPlacePropsItem()    : CStatsItem( ETIT_CHAPTER_PLACE_PROPS_ITEM ) {} };

}
