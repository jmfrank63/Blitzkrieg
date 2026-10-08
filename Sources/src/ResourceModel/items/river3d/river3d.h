#pragma once
// 3D River sub-editor - project extension .3rv, project XML root tag
// "River3D_Composer_Project". MFC source: Sources/src/editor/3dRiverTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class C3DRiverTreeRootItem : public CStatsItem
{
public:
	C3DRiverTreeRootItem() : CStatsItem( ETIT_3DRIVER_ROOT_ITEM, "River3D_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class C3DRiverBottomLayerPropsItem : public CStatsItem
{
public:
	C3DRiverBottomLayerPropsItem() : CStatsItem( ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class C3DRiverLayersItem : public CStatsItem
{
public:
	C3DRiverLayersItem() : CStatsItem( ETIT_3DRIVER_LAYERS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class C3DRiverLayerPropsItem : public CStatsItem
{
public:
	C3DRiverLayerPropsItem() : CStatsItem( ETIT_3DRIVER_LAYER_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
	// C3DRiverFrame::FillRPGStats (3dRiverFrm.cpp:145) reads the Animated flag of each layer on every save.
	bool BoolsReadAsInt() const override { return true; }
};

}
