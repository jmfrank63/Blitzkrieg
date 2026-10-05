#include "object.h"

#include "../../combos.h"
#include "../../editor_env.h"

namespace NResourceModel
{

void CObjectTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_OBJECT_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_PASSES_ITEM;
	child.szDefaultName = "AI classes to pass";
	child.szDisplayName = "AI classes to pass";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_GRAPHICS_ITEM;
	child.szDefaultName = "Graphics Info";
	child.szDisplayName = "Graphics Info";
	defaultChilds.push_back( child );
}

void CObjectCommonPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Armor";
	prop.szDisplayName = "Armor";
	prop.value = 2;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Silhouette";
	prop.szDisplayName = "Silhouette";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Cycled sound";
	prop.szDisplayName = "Cycled sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CObjectPassesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CObjectPassPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "AI class to pass";
	prop.szDisplayName = "AI class to pass";
	prop.value = "";
	LoadAIClassCombo( &prop );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CObjectGraphicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_OBJECT_GRAPHIC1_PROPS_ITEM;
	child.szDefaultName = "Summer picture";
	child.szDisplayName = "Summer picture";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_GRAPHICW1_PROPS_ITEM;
	child.szDefaultName = "Winter picture";
	child.szDisplayName = "Winter picture";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_GRAPHICA1_PROPS_ITEM;
	child.szDefaultName = "Africa picture";
	child.szDisplayName = "Africa picture";
	defaultChilds.push_back( child );
}

void CObjectGraphic1PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1s.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CObjectGraphicW1PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1w.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1ws.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CObjectGraphicA1PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1a.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1as.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CObjectEffectsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Death with explosion";
	prop.szDisplayName = "Death with explosion";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Silent death";
	prop.szDisplayName = "Silent death";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
