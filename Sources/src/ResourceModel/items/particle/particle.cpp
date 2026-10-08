#include "particle.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CParticleTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_PARTICLE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_SOURCE_PROP_ITEMS;
	child.szDefaultName = "Particle source props";
	child.szDisplayName = "Particle source props";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_COMPLEX_SOURCE_ITEM;
	child.szDefaultName = "Particle complex source props";
	child.szDisplayName = "Particle complex source props";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_PROP_ITEMS;
	child.szDefaultName = "Particle props";
	child.szDisplayName = "Particle props";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_COMPLEX_ITEM;
	child.szDefaultName = "Particle complex props";
	child.szDisplayName = "Particle complex props";
	defaultChilds.push_back( child );
}

void CParticleCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Particle";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Life time";
	prop.szDisplayName = "Life time";
	prop.value = 15000;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Scale factor";
	prop.szDisplayName = "Scale factor";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

/*
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Generate angle";
	prop.szDisplayName = "Generate angle";
	prop.value = 15000;
	defaultValues.push_back( prop );
*/

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Gravity";
	prop.szDisplayName = "Gravity";
	prop.value = 0.0001f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Z position";
	prop.szDisplayName = "Z position";
	prop.value = 0.0f;
	defaultValues.push_back( prop );


	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "X direction";
	prop.szDisplayName = "X direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Y direction";
	prop.szDisplayName = "Y direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Z direction";
	prop.szDisplayName = "Z direction";
	prop.value = 1.0f;
	defaultValues.push_back( prop );


	prop.nId = 11;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "X wind";
	prop.szDisplayName = "X wind";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Y wind";
	prop.szDisplayName = "Y wind";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Z wind";
	prop.szDisplayName = "Z wind";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 14;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Radial wind power";
	prop.szDisplayName = "Radial wind power";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 15;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Generate area type";
	prop.szDisplayName = "Generate area type";
	prop.value = "square";
	prop.szStrings.push_back( "square" );
	prop.szStrings.push_back( "disk" );
	prop.szStrings.push_back( "circle" );
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CParticleSourcePropItems::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_PARTICLE_TEXTURE_REF;
	prop.szDefaultName = "Texture file";
	prop.szDisplayName = "Texture reference";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Texture X size";
	prop.szDisplayName = "Texture X size";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Texture Y size";
	prop.szDisplayName = "Texture Y size";
	prop.value = 1;
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_PARTICLE_GENERATE_LIFE_ITEM;
	child.szDefaultName = "Life";
	child.szDisplayName = "Life";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_RAND_LIFE_ITEM;
	child.szDefaultName = "Random life";
	child.szDisplayName = "Random life";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_SPEED_ITEM;
	child.szDefaultName = "Speed";
	child.szDisplayName = "Speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_RAND_SPEED_ITEM;
	child.szDefaultName = "Random speed";
	child.szDisplayName = "Random speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_SPIN_ITEM;
	child.szDefaultName = "Spin";
	child.szDisplayName = "Spin";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM;
	child.szDefaultName = "Random spin";
	child.szDisplayName = "Random spin";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_AREA_ITEM;
	child.szDefaultName = "Area";
	child.szDisplayName = "Area";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_ANGLE_ITEM;
	child.szDefaultName = "Angle";
	child.szDisplayName = "Angle";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_DENSITY_ITEM;
	child.szDefaultName = "Density";
	child.szDisplayName = "Density";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_OPACITY_ITEM;
	child.szDefaultName = "Opacity";
	child.szDisplayName = "Opacity";
	defaultChilds.push_back( child );
}

void CParticleGenerateSpeedItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 2.0f;
	fStepY = 0.1f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.20f );
	framesList.push_back( para );
}

void CParticleRandSpeedItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.1f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.0f );
	framesList.push_back( para );
}

void CParticleGenerateLifeItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 100.0f;
	fMaxValY = 5000.0f;
	fStepY = 100.0f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 500.0f );
	framesList.push_back( para );
}

void CParticleRandLifeItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.1f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.0f );
	framesList.push_back( para );
}

void CParticleGenerateSpinItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 0.1f;
	fStepY = 0.005f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.01f );
	framesList.push_back( para );
}

void CParticleGenerateRandomSpinItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.1f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.0f );
	framesList.push_back( para );
}

void CParticleGenerateAreaItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 100.0f;
	fStepY = 5.0f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 10.0f );
	framesList.push_back( para );
}

void CParticleGenerateAngleItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 360.0f;
	fStepY = 20.0f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 60.0f );
	framesList.push_back( para );
}

void CParticleGenerateDensityItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 0.5f;
	fStepY = 0.005f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.01f );
	framesList.push_back( para );
}

void CParticleGenerateOpacityItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 255.0f;
	fStepY = 10.0f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 200.0f );
	framesList.push_back( para );
}

void CParticleComplexSourceItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FUNC_PARTICLE_REF;
	prop.szDefaultName = "Particle reference";
	prop.szDisplayName = "Particle reference";
	prop.value = "";
	defaultValues.push_back( prop );
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_PARTICLE_GENERATE_SPEED_ITEM;
	child.szDefaultName = "Speed";
	child.szDisplayName = "Speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_RAND_SPEED_ITEM;
	child.szDefaultName = "Random speed";
	child.szDisplayName = "Random speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_AREA_ITEM;
	child.szDefaultName = "Area";
	child.szDisplayName = "Area";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_ANGLE_ITEM;
	child.szDefaultName = "Angle";
	child.szDisplayName = "Angle";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_GENERATE_DENSITY_ITEM;
	child.szDefaultName = "Density";
	child.szDisplayName = "Density";
	defaultChilds.push_back( child );
}

void CParticlePropItems::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_PARTICLE_SPIN_ITEM;
	child.szDefaultName = "Spin";
	child.szDisplayName = "Spin";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_WEIGHT_ITEM;
	child.szDefaultName = "Weight";
	child.szDisplayName = "Weight";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_SPEED_ITEM;
	child.szDefaultName = "Speed";
	child.szDisplayName = "Speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_C_RANDOM_SPEED_ITEM;
	child.szDefaultName = "Random speed";
	child.szDisplayName = "Random speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_SIZE_ITEM;
	child.szDefaultName = "Size";
	child.szDisplayName = "Size";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_OPACITY_ITEM;
	child.szDefaultName = "Opacity";
	child.szDisplayName = "Opacity";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_TEXTURE_FRAME_ITEM;
	child.szDefaultName = "Texture frame";
	child.szDisplayName = "Texture frame";
	defaultChilds.push_back( child );
}

void CParticleComplexItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_PARTICLE_WEIGHT_ITEM;
	child.szDefaultName = "Weight";
	child.szDisplayName = "Weight";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_SPEED_ITEM;
	child.szDefaultName = "Speed";
	child.szDisplayName = "Speed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_PARTICLE_C_RANDOM_SPEED_ITEM;
	child.szDefaultName = "Random speed";
	child.szDisplayName = "Random speed";
	defaultChilds.push_back( child );
}

void CParticleSpinItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.05f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 1.0f );
	framesList.push_back( para );
}

void CParticleWeightItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = -5.0f;
	fMaxValY = 5.0f;
	fStepY = 0.5f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 1.0f );
	framesList.push_back( para );
}

void CParticleSpeedItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.05f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 1.0f );
	framesList.push_back( para );
}

void CParticleCRandomSpeedItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.05f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 1.0f );
	framesList.push_back( para );
}

void CParticleSizeItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 200.0f;
	fStepY = 10.0f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 50.0f );
	framesList.push_back( para );
}

void CParticleOpacityItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.05f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 1.0f );
	framesList.push_back( para );
}

void CParticleTextureFrameItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	fMinValX = 0.0f;
	fMaxValX = 1.0f;
	fStepX = 0.05f;
	fMinValY = 0.0f;
	fMaxValY = 1.0f;
	fStepY = 0.05f;
	bResizeMode = true;
	std::pair<float, float> para( 0.0f, 0.0f );
	framesList.push_back( para );
}

}
