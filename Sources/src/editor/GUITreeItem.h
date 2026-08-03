#ifndef __GUI_TREE_ITEM_H__
#define __GUI_TREE_ITEM_H__

#include "..\UI\ui.h"
#include "TreeItem.h"

class CGUITreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CGUITreeRootItem );
public:
	CGUITreeRootItem() { bStaticElements = true; nItemType = E_GUI_ROOT_ITEM; InitDefaultValues(); }
	~CGUITreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CGUIMouseSelectItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CGUIMouseSelectItem );
public:
	CGUIMouseSelectItem() { nItemType = E_GUI_MOUSE_SELECT_ITEM; nImageIndex = 0; }
	~CGUIMouseSelectItem() {}

	virtual void MyLButtonClick();
};

class CTemplatesTreeItem : public CTreeItem
{
protected:
	string szDirectory;
	int nWindowType;
	
public:
	CTemplatesTreeItem() : nWindowType( -1 ) { nItemType = E_TEMPLATE_TREE_ITEM; InitDefaultValues(); bSerializeChilds = false; }
	~CTemplatesTreeItem() {}

	int GetWindowType() { return nWindowType; }

	virtual void InsertChildItems();					//Вызывается после создания всех компонентов для занесения их в дерево
	virtual int operator&( IDataTree &ss );
};

class CStaticsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CStaticsTreeItem );
public:
	CStaticsTreeItem() { nItemType = E_STATICS_TREE_ITEM; InitDefaultValues(); nImageIndex = 1; szDirectory = "Statics\\"; nWindowType = UI_STATIC; }
	~CStaticsTreeItem() {}
};

class CButtonsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CButtonsTreeItem );
public:
	CButtonsTreeItem() { nItemType = E_BUTTONS_TREE_ITEM; InitDefaultValues(); nImageIndex = 2; szDirectory = "Buttons\\"; nWindowType = UI_BUTTON; }
	~CButtonsTreeItem() {}
};

class CSlidersTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CSlidersTreeItem );
public:
	CSlidersTreeItem() { nItemType = E_SLIDERS_TREE_ITEM; InitDefaultValues(); nImageIndex = 3; szDirectory = "Sliders\\"; nWindowType = UI_SLIDER; }
	~CSlidersTreeItem() {}
};

class CScrollBarsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CScrollBarsTreeItem );
public:
	CScrollBarsTreeItem() { nItemType = E_SCROLLBARS_TREE_ITEM; InitDefaultValues(); nImageIndex = 4; szDirectory = "Scrollbars\\"; nWindowType = UI_SCROLLBAR; }
	~CScrollBarsTreeItem() {}
};

class CStatusBarsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CStatusBarsTreeItem );
public:
	CStatusBarsTreeItem() { nItemType = E_STATUSBARS_TREE_ITEM; InitDefaultValues(); nImageIndex = 5; szDirectory = "StatusBars\\"; nWindowType = UI_STATUS_BAR; }
	~CStatusBarsTreeItem() {}
};

class CListsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CListsTreeItem );
public:
	CListsTreeItem() { nItemType = E_LISTS_TREE_ITEM; InitDefaultValues(); nImageIndex = 6; szDirectory = "Lists\\"; nWindowType = UI_LIST; }
	~CListsTreeItem() {}
};

class CDialogsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CDialogsTreeItem );
public:
	CDialogsTreeItem() { nItemType = E_DIALOGS_TREE_ITEM; InitDefaultValues(); nImageIndex = 7; szDirectory = "Dialogs\\"; nWindowType = UI_DIALOG; }
	~CDialogsTreeItem() {}
};

class CUnknownsTreeItem : public CTemplatesTreeItem
{
	OBJECT_NORMAL_METHODS( CUnknownsTreeItem );
public:
	CUnknownsTreeItem() { nItemType = E_UNKNOWNS_UI_TREE_ITEM; InitDefaultValues(); nImageIndex = 8; szDirectory = "Unknowns\\"; nWindowType = UI_UI; }
	~CUnknownsTreeItem() {}
};


class CTemplatePropsTreeItem : public CTreeItem
{
private:
	int nWindowType;			//инициализируется из XML файла при загрузке
	string szXMLFile;
	
public:
	CTemplatePropsTreeItem() { nItemType = E_TEMPLATE_PROPS_TREE_ITEM; InitDefaultValues(); bSerializeChilds = false; }
	~CTemplatePropsTreeItem() {}
	
	int GetWindowType() { return nWindowType; }
	const char *GetXMLFileName() { return szXMLFile.c_str(); }

	void SetWindowType( int nType ) { nWindowType = nType; }
	void SetXMLFile( const char *pszFileName ) { szXMLFile = pszFileName; }

	virtual void MyLButtonClick();

/*
	virtual void InsertChildItems();					//Вызывается после создания всех компонентов для занесения их в дерево
*/
	virtual int operator&( IDataTree &ss );
	virtual void MyKeyDown( int nChar );
};

class CStaticPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CStaticPropsTreeItem );
public:
	CStaticPropsTreeItem() { nItemType = E_STATIC_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 1; }
	~CStaticPropsTreeItem() {}
};

class CButtonPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CButtonPropsTreeItem );
public:
	CButtonPropsTreeItem() { nItemType = E_BUTTON_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CButtonPropsTreeItem() {}
};

class CSliderPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CSliderPropsTreeItem );
public:
	CSliderPropsTreeItem() { nItemType = E_SLIDER_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CSliderPropsTreeItem() {}
};

class CScrollBarPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CScrollBarPropsTreeItem );
public:
	CScrollBarPropsTreeItem() { nItemType = E_SCROLLBAR_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 4; }
	~CScrollBarPropsTreeItem() {}
};

class CStatusBarPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CStatusBarPropsTreeItem );
public:
	CStatusBarPropsTreeItem() { nItemType = E_STATUSBAR_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 5; }
	~CStatusBarPropsTreeItem() {}
};

class CListPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CListPropsTreeItem );
public:
	CListPropsTreeItem() { nItemType = E_LIST_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 6; }
	~CListPropsTreeItem() {}
};

class CDialogPropsTreeItem : public CTemplatePropsTreeItem
{
	OBJECT_NORMAL_METHODS( CDialogPropsTreeItem );
public:
	CDialogPropsTreeItem() { nItemType = E_DIALOG_PROPS_TREE_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CDialogPropsTreeItem() {}
};

int GetTreeItemTypeByWindowType( int nWindowType );


#endif		//__GUI_TREE_ITEM_H__
