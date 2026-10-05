#include "medal.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CMedalTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MEDAL_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

/*
	child.nChildItemType = ETIT_MEDAL_PICTURE_PROPS_ITEM;
	child.szDefaultName = "Picture";
	child.szDisplayName = "Picture";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MEDAL_TEXT_PROPS_ITEM;
	child.szDefaultName = "Description";
	child.szDisplayName = "Description";
	defaultChilds.push_back( child );
*/
}

void CMedalCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "name";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.szDefaultName = "Description";
	prop.szDisplayName = "Description";
	prop.value = "desc";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Medal image";
	prop.szDisplayName = "Medal image";
	prop.value = "medal";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CMedalPicturePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Medal image";
	prop.szDisplayName = "Medal image";
	prop.value = "1";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Image position X";
	prop.szDisplayName = "Image position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Image position Y";
	prop.szDisplayName = "Image position Y";
	prop.value = 0;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMedalTextPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Description text";
	prop.szDisplayName = "Description text";
	prop.value = "desc";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

/*
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Text center position X";
	prop.szDisplayName = "Text center position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Text center position Y";
	prop.szDisplayName = "Text center position Y";
	prop.value = 0;
	defaultValues.push_back( prop );
*/

	values = defaultValues;
}

}
