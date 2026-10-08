#pragma once
// GUI sub-editor - project extension .gui, project XML root tag
// "GUI_Composer_Project". MFC source: Sources/src/editor/GUITreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

// The window types of Sources/src/UI/UI.h the template folders hold, copied so
// the model does not include the engine's UI headers.
enum EUIWindowType
{
	UI_BASE_VALUE = 0x10000000,
	UI_UI         = UI_BASE_VALUE + 0x00001100 + 1,
	UI_BUTTON     = UI_BASE_VALUE + 0x00001100 + 3,
	UI_STATIC     = UI_BASE_VALUE + 0x00001100 + 5,
	UI_STATUS_BAR = UI_BASE_VALUE + 0x00001100 + 6,
	UI_DIALOG     = UI_BASE_VALUE + 0x00001100 + 7,
	UI_SLIDER     = UI_BASE_VALUE + 0x00001100 + 8,
	UI_SCROLLBAR  = UI_BASE_VALUE + 0x00001100 + 9,
	UI_LIST       = UI_BASE_VALUE + 0x00001100 + 10
};

class CGUITreeRootItem : public CStatsItem
{
public:
	CGUITreeRootItem() : CStatsItem( ETIT_GUI_ROOT_ITEM, "GUI_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CGUIMouseSelectItem : public CStatsItem
{
public:
	CGUIMouseSelectItem() : CStatsItem( ETIT_GUI_MOUSE_SELECT_ITEM ) {}
};

// A folder of UI templates under the editor data dir (editor\\UI\\<szDirectory>).
// MFC's operator& is the CTreeItem one and does not write childs: the editor
// lists the folder's XML files as children (InsertChildItems) each time.
class CTemplatesTreeItem : public CStatsItem
{
public:
	int GetWindowType() const { return nWindowType; }
	const std::string &GetDirectory() const { return szDirectory; }

protected:
	explicit CTemplatesTreeItem( int nType ) : CStatsItem( nType ) { bSerializeChilds = false; }

	std::string szDirectory;
	int nWindowType = -1;
};

class CStaticsTreeItem : public CTemplatesTreeItem
{
public:
	CStaticsTreeItem() : CTemplatesTreeItem( ETIT_STATICS_TREE_ITEM ) { szDirectory = "Statics\\"; nWindowType = UI_STATIC; }
};

class CButtonsTreeItem : public CTemplatesTreeItem
{
public:
	CButtonsTreeItem() : CTemplatesTreeItem( ETIT_BUTTONS_TREE_ITEM ) { szDirectory = "Buttons\\"; nWindowType = UI_BUTTON; }
};

class CSlidersTreeItem : public CTemplatesTreeItem
{
public:
	CSlidersTreeItem() : CTemplatesTreeItem( ETIT_SLIDERS_TREE_ITEM ) { szDirectory = "Sliders\\"; nWindowType = UI_SLIDER; }
};

class CScrollBarsTreeItem : public CTemplatesTreeItem
{
public:
	CScrollBarsTreeItem() : CTemplatesTreeItem( ETIT_SCROLLBARS_TREE_ITEM ) { szDirectory = "Scrollbars\\"; nWindowType = UI_SCROLLBAR; }
};

class CStatusBarsTreeItem : public CTemplatesTreeItem
{
public:
	CStatusBarsTreeItem() : CTemplatesTreeItem( ETIT_STATUSBARS_TREE_ITEM ) { szDirectory = "StatusBars\\"; nWindowType = UI_STATUS_BAR; }
};

class CListsTreeItem : public CTemplatesTreeItem
{
public:
	CListsTreeItem() : CTemplatesTreeItem( ETIT_LISTS_TREE_ITEM ) { szDirectory = "Lists\\"; nWindowType = UI_LIST; }
};

class CDialogsTreeItem : public CTemplatesTreeItem
{
public:
	CDialogsTreeItem() : CTemplatesTreeItem( ETIT_DIALOGS_TREE_ITEM ) { szDirectory = "Dialogs\\"; nWindowType = UI_DIALOG; }
};

class CUnknownsTreeItem : public CTemplatesTreeItem
{
public:
	CUnknownsTreeItem() : CTemplatesTreeItem( ETIT_UNKNOWNS_UI_TREE_ITEM ) { szDirectory = "Unknowns\\"; nWindowType = UI_UI; }
};

// One UI template, an XML file in its folder. Neither the window type nor the
// file is serialised: the folder sets both when it lists its files.
class CTemplatePropsTreeItem : public CStatsItem
{
public:
	int GetWindowType() const { return nWindowType; }
	const std::string &GetXMLFileName() const { return szXMLFile; }
	void SetWindowType( int nType ) { nWindowType = nType; }
	void SetXMLFile( const std::string &szFileName ) { szXMLFile = szFileName; }

protected:
	explicit CTemplatePropsTreeItem( int nType ) : CStatsItem( nType ) { bSerializeChilds = false; }

private:
	int nWindowType = -1;
	std::string szXMLFile;
};

class CStaticPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CStaticPropsTreeItem() : CTemplatePropsTreeItem( ETIT_STATIC_PROPS_TREE_ITEM ) {}
};

class CButtonPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CButtonPropsTreeItem() : CTemplatePropsTreeItem( ETIT_BUTTON_PROPS_TREE_ITEM ) {}
};

class CSliderPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CSliderPropsTreeItem() : CTemplatePropsTreeItem( ETIT_SLIDER_PROPS_TREE_ITEM ) {}
};

class CScrollBarPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CScrollBarPropsTreeItem() : CTemplatePropsTreeItem( ETIT_SCROLLBAR_PROPS_TREE_ITEM ) {}
};

class CStatusBarPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CStatusBarPropsTreeItem() : CTemplatePropsTreeItem( ETIT_STATUSBAR_PROPS_TREE_ITEM ) {}
};

class CListPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CListPropsTreeItem() : CTemplatePropsTreeItem( ETIT_LIST_PROPS_TREE_ITEM ) {}
};

class CDialogPropsTreeItem : public CTemplatePropsTreeItem
{
public:
	CDialogPropsTreeItem() : CTemplatePropsTreeItem( ETIT_DIALOG_PROPS_TREE_ITEM ) {}
};

}
