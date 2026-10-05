#pragma once
// Medal sub-editor - project extension .mdc, project XML root tag
// "Medal_Composer_Project". MFC source: Sources/src/editor/MedalTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMedalTreeRootItem : public CStatsItem
{
public:
	CMedalTreeRootItem() : CStatsItem( ETIT_MEDAL_ROOT_ITEM, "Medal_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMedalCommonPropsItem : public CStatsItem
{
public:
	CMedalCommonPropsItem() : CStatsItem( ETIT_MEDAL_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMedalPicturePropsItem : public CStatsItem
{
public:
	CMedalPicturePropsItem() : CStatsItem( ETIT_MEDAL_PICTURE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMedalTextPropsItem : public CStatsItem
{
public:
	CMedalTextPropsItem() : CStatsItem( ETIT_MEDAL_TEXT_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
