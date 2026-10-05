#include "river3d.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void C3DRiverTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_3DRIVER_LAYERS_ITEM;
	child.szDefaultName = "Layers";
	child.szDisplayName = "Layers";
	defaultChilds.push_back( child );
}

void C3DRiverBottomLayerPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Bottom width";
	prop.szDisplayName = "Bottom width";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Center opacity";
	prop.szDisplayName = "Center opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Border opacity";
	prop.szDisplayName = "Border opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Texture step";
	prop.szDisplayName = "Texture step";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_WATER_TEXTURE_REF;
	prop.szDefaultName = "Texture";
	prop.szDisplayName = "Texture";
	prop.value = "water\\bottom";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void C3DRiverLayersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void C3DRiverLayerPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Center opacity";
	prop.szDisplayName = "Center opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Border opacity";
	prop.szDisplayName = "Border opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Stream speed";
	prop.szDisplayName = "Stream speed";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Texture step";
	prop.szDisplayName = "Texture step";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Animated flag";
	prop.szDisplayName = "Animated flag";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_WATER_TEXTURE_REF;
	prop.szDefaultName = "Texture";
	prop.szDisplayName = "Texture";
	prop.value = "water\\water";
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Disturbance";
	prop.szDisplayName = "Disturbance";
	prop.value = 0.3f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
