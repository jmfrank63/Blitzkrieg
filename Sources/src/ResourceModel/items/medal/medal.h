#pragma once
// Medal sub-editor - project extension .mdc, project XML root tag
// "Medal_Composer_Project". MFC source: Sources/src/editor/MedalTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:274-277.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMedalTreeRootItem : public CStatsItem
{
public:
	CMedalTreeRootItem() : CStatsItem( ETIT_MEDAL_ROOT_ITEM, "Medal_Composer_Project" ) {}
};
class CMedalCommonPropsItem   : public CStatsItem { public: CMedalCommonPropsItem()   : CStatsItem( ETIT_MEDAL_COMMON_PROPS_ITEM ) {} };
class CMedalPicturePropsItem  : public CStatsItem { public: CMedalPicturePropsItem()  : CStatsItem( ETIT_MEDAL_PICTURE_PROPS_ITEM ) {} };
class CMedalTextPropsItem     : public CStatsItem { public: CMedalTextPropsItem()     : CStatsItem( ETIT_MEDAL_TEXT_PROPS_ITEM ) {} };

}
