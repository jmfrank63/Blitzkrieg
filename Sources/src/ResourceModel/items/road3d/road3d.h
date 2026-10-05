#pragma once
// 3D Road sub-editor - project extension .3rd, project XML root tag
// "Road3D_Composer_Project". MFC source: Sources/src/editor/3dRoadTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class C3DRoadTreeRootItem : public CStatsItem
{
public:
	C3DRoadTreeRootItem() : CStatsItem( ETIT_3DROAD_ROOT_ITEM, "Road3D_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class C3DRoadCommonPropsItem : public CStatsItem
{
public:
	C3DRoadCommonPropsItem() : CStatsItem( ETIT_3DROAD_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class C3DRoadLayerPropsItem : public CStatsItem
{
public:
	C3DRoadLayerPropsItem() : CStatsItem( ETIT_3DROAD_LAYER_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
