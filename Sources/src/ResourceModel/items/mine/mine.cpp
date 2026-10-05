#include "mine.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CMineTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MINE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );
}

void CMineCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Weapon";
	prop.szDisplayName = "Weapon";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Weight";
	prop.szDisplayName = "Weight";
	prop.value = 10;
	defaultValues.push_back( prop );

/*
	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Flag picture";
	prop.szDisplayName = "Flag picture";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
*/

	values = defaultValues;
}

}
