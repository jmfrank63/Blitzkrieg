#pragma once
// 3dRiver sub-editor - project extension .3rv, project XML root tag
// "River3D_Composer_Project". MFC source: Sources/src/editor/3dRiverTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:269-272.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class C3DRiverTreeRootItem : public CStatsItem
{
public:
	C3DRiverTreeRootItem() : CStatsItem( ETIT_3DRIVER_ROOT_ITEM, "River3D_Composer_Project" ) {}
};
class C3DRiverBottomLayerPropsItem : public CStatsItem { public: C3DRiverBottomLayerPropsItem() : CStatsItem( ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM ) {} };
class C3DRiverLayerPropsItem       : public CStatsItem { public: C3DRiverLayerPropsItem()       : CStatsItem( ETIT_3DRIVER_LAYER_PROPS_ITEM ) {} };
class C3DRiverLayersItem           : public CStatsItem { public: C3DRiverLayersItem()           : CStatsItem( ETIT_3DRIVER_LAYERS_ITEM ) {} };

}
