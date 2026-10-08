#include "effect.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CEffectTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_EFFECT_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_EFFECT_ANIMATIONS_ITEM;
	child.szDefaultName = "Animations";
	child.szDisplayName = "Animations";
	defaultChilds.push_back( child );

	/*child.nChildItemType = ETIT_EFFECT_MESHES_ITEM;
	child.szDefaultName = "Meshes";
	child.szDisplayName = "Meshes";
	defaultChilds.push_back( child );*/

	child.nChildItemType = ETIT_EFFECT_FUNC_PARTICLES_ITEM;
	child.szDefaultName = "Function Particles";
	child.szDisplayName = "Function Particles";
	defaultChilds.push_back( child );

	/*child.nChildItemType = ETIT_EFFECT_MAYA_PARTICLES_ITEM;
	child.szDefaultName = "Maya Particles";
	child.szDisplayName = "Maya Particles";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_EFFECT_LIGHTS_ITEM;
	child.szDefaultName = "Lights";
	child.szDisplayName = "Lights";
	defaultChilds.push_back( child );*/
}

void CEffectCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown effect";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CEffectAnimationsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CEffectMeshesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CEffectFuncParticlesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CEffectMayaParticlesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CEffectLightsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CEffectAnimationPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Begin time";
	prop.szDisplayName = "Begin time";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Z position";
	prop.szDisplayName = "Z position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Repeat Count";
	prop.szDisplayName = "Repeat Count";
	prop.value = 1;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CEffectMeshPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Begin time";
	prop.szDisplayName = "Begin time";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Duration";
	prop.szDisplayName = "Duration";
	prop.value = 5000;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Z position";
	prop.szDisplayName = "Z position";
	prop.value = 0;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CEffectMayaPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Begin time";
	prop.szDisplayName = "Begin time";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Duration";
	prop.szDisplayName = "Duration";
	prop.value = 5000;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Z position";
	prop.szDisplayName = "Z position";
	prop.value = 0;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CEffectFuncPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Begin time";
	prop.szDisplayName = "Begin time";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Duration";
	prop.szDisplayName = "Duration";
	prop.value = 15000;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Z position";
	prop.szDisplayName = "Z position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Scale factor";
	prop.szDisplayName = "Scale factor";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
