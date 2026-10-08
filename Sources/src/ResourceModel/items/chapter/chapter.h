#pragma once
// Chapter sub-editor - project extension .chc, project XML root tag
// "Chapter_Composer_Project". MFC source: Sources/src/editor/ChapterTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CChapterTreeRootItem : public CStatsItem
{
public:
	CChapterTreeRootItem() : CStatsItem( ETIT_CHAPTER_ROOT_ITEM, "Chapter_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CChapterCommonPropsItem : public CStatsItem
{
public:
	CChapterCommonPropsItem() : CStatsItem( ETIT_CHAPTER_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CChapterMissionsItem : public CStatsItem
{
public:
	CChapterMissionsItem() : CStatsItem( ETIT_CHAPTER_MISSIONS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CChapterMissionPropsItem : public CStatsItem
{
public:
	CChapterMissionPropsItem() : CStatsItem( ETIT_CHAPTER_MISSION_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CChapterPlacesItem : public CStatsItem
{
public:
	CChapterPlacesItem() : CStatsItem( ETIT_CHAPTER_PLACES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CChapterPlacePropsItem : public CStatsItem
{
public:
	CChapterPlacePropsItem() : CStatsItem( ETIT_CHAPTER_PLACE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
