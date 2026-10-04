#pragma once
// 3dRoad sub-editor - project extension .3rd, project XML root tag
// "Road3D_Composer_Project". MFC source: Sources/src/editor/3dRoadTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:265-267.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class C3DRoadTreeRootItem : public CStatsItem
{
public:
	C3DRoadTreeRootItem() : CStatsItem( ETIT_3DROAD_ROOT_ITEM, "Road3D_Composer_Project" ) {}
};
class C3DRoadCommonPropsItem : public CStatsItem { public: C3DRoadCommonPropsItem() : CStatsItem( ETIT_3DROAD_COMMON_PROPS_ITEM ) {} };
class C3DRoadLayerPropsItem  : public CStatsItem { public: C3DRoadLayerPropsItem()  : CStatsItem( ETIT_3DROAD_LAYER_PROPS_ITEM ) {} };

}
