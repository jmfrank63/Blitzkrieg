#include "sprite.h"

namespace NResourceModel
{

void CSpriteTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_SPRITES_ITEM;
	child.szDefaultName = "Sprites";
	child.szDisplayName = "Sprites";
	defaultChilds.push_back( child );
}

void CSpritePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSpritesItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Directory";
	prop.szDisplayName = "Directory";
	prop.value = "_.";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Frame time";
	prop.szDisplayName = "Frame time";
	prop.value = 125;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 32;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 32;
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
