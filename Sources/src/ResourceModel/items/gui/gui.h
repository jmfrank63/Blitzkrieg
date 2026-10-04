#pragma once
// GUI sub-editor - project extension .gui, project XML root tag
// "GUI_Composer_Project". MFC source: Sources/src/editor/GUITreeItem.{h,cpp}
// (switched-off UI tree registered in Sources/src/editor/TreeItemFactory.cpp:204-221).
// E_TEMPLATE_* entries from the MFC enum (TreeItem.h:173, 179) are not
// REGISTER_CLASSed in the MFC factory so the port keeps the same shape and
// does not emit a factory entry for them either.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CGUITreeRootItem : public CStatsItem
{
public:
	CGUITreeRootItem() : CStatsItem( ETIT_GUI_ROOT_ITEM, "GUI_Composer_Project" ) {}
};
class CGUIMouseSelectItem      : public CStatsItem { public: CGUIMouseSelectItem()      : CStatsItem( ETIT_GUI_MOUSE_SELECT_ITEM ) {} };
class CStaticsTreeItem         : public CStatsItem { public: CStaticsTreeItem()         : CStatsItem( ETIT_STATICS_TREE_ITEM ) {} };
class CButtonsTreeItem         : public CStatsItem { public: CButtonsTreeItem()         : CStatsItem( ETIT_BUTTONS_TREE_ITEM ) {} };
class CSlidersTreeItem         : public CStatsItem { public: CSlidersTreeItem()         : CStatsItem( ETIT_SLIDERS_TREE_ITEM ) {} };
class CScrollBarsTreeItem      : public CStatsItem { public: CScrollBarsTreeItem()      : CStatsItem( ETIT_SCROLLBARS_TREE_ITEM ) {} };
class CStatusBarsTreeItem      : public CStatsItem { public: CStatusBarsTreeItem()      : CStatsItem( ETIT_STATUSBARS_TREE_ITEM ) {} };
class CListsTreeItem           : public CStatsItem { public: CListsTreeItem()           : CStatsItem( ETIT_LISTS_TREE_ITEM ) {} };
class CDialogsTreeItem         : public CStatsItem { public: CDialogsTreeItem()         : CStatsItem( ETIT_DIALOGS_TREE_ITEM ) {} };
class CUnknownsTreeItem        : public CStatsItem { public: CUnknownsTreeItem()        : CStatsItem( ETIT_UNKNOWNS_UI_TREE_ITEM ) {} };
class CStaticPropsTreeItem     : public CStatsItem { public: CStaticPropsTreeItem()     : CStatsItem( ETIT_STATIC_PROPS_TREE_ITEM ) {} };
class CButtonPropsTreeItem     : public CStatsItem { public: CButtonPropsTreeItem()     : CStatsItem( ETIT_BUTTON_PROPS_TREE_ITEM ) {} };
class CSliderPropsTreeItem     : public CStatsItem { public: CSliderPropsTreeItem()     : CStatsItem( ETIT_SLIDER_PROPS_TREE_ITEM ) {} };
class CScrollBarPropsTreeItem  : public CStatsItem { public: CScrollBarPropsTreeItem()  : CStatsItem( ETIT_SCROLLBAR_PROPS_TREE_ITEM ) {} };
class CStatusBarPropsTreeItem  : public CStatsItem { public: CStatusBarPropsTreeItem()  : CStatsItem( ETIT_STATUSBAR_PROPS_TREE_ITEM ) {} };
class CListPropsTreeItem       : public CStatsItem { public: CListPropsTreeItem()       : CStatsItem( ETIT_LIST_PROPS_TREE_ITEM ) {} };
class CDialogPropsTreeItem     : public CStatsItem { public: CDialogPropsTreeItem()     : CStatsItem( ETIT_DIALOG_PROPS_TREE_ITEM ) {} };

}
