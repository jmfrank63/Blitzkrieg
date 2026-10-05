#include "gui.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CGUITreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_GUI_MOUSE_SELECT_ITEM;
	child.szDefaultName = "Mouse select";
	child.szDisplayName = "Mouse select";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_STATICS_TREE_ITEM;
	child.szDefaultName = "Statics";
	child.szDisplayName = "Statics";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUTTONS_TREE_ITEM;
	child.szDefaultName = "Buttons";
	child.szDisplayName = "Buttons";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_SLIDERS_TREE_ITEM;
	child.szDefaultName = "Sliders";
	child.szDisplayName = "Sliders";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_SCROLLBARS_TREE_ITEM;
	child.szDefaultName = "Scrollbars";
	child.szDisplayName = "Scrollbars";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_STATUSBARS_TREE_ITEM;
	child.szDefaultName = "Statusbars";
	child.szDisplayName = "Statusbars";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_LISTS_TREE_ITEM;
	child.szDefaultName = "Lists";
	child.szDisplayName = "Lists";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_DIALOGS_TREE_ITEM;
	child.szDefaultName = "Dialogs";
	child.szDisplayName = "Dialogs";
	defaultChilds.push_back( child );
}

}
