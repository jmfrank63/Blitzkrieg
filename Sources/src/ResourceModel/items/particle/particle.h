#pragma once
// Particle sub-editor - project extension .pcp, project XML root tag
// "Particle_Composer_Project". MFC source: Sources/src/editor/ParticleTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"
#include "../../key_frame_tree_item.h"

namespace NResourceModel
{

class CParticleTreeRootItem : public CStatsItem
{
public:
	CParticleTreeRootItem() : CStatsItem( ETIT_PARTICLE_ROOT_ITEM, "Particle_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleCommonPropsItem : public CStatsItem
{
public:
	CParticleCommonPropsItem() : CStatsItem( ETIT_PARTICLE_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleSourcePropItems : public CStatsItem
{
public:
	CParticleSourcePropItems() : CStatsItem( ETIT_PARTICLE_SOURCE_PROP_ITEMS ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateLifeItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateLifeItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_LIFE_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleRandLifeItem : public CKeyFrameTreeItem
{
public:
	CParticleRandLifeItem() : CKeyFrameTreeItem( ETIT_PARTICLE_RAND_LIFE_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateSpeedItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateSpeedItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_SPEED_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleRandSpeedItem : public CKeyFrameTreeItem
{
public:
	CParticleRandSpeedItem() : CKeyFrameTreeItem( ETIT_PARTICLE_RAND_SPEED_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateSpinItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateSpinItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_SPIN_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateRandomSpinItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateRandomSpinItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateAreaItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateAreaItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_AREA_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateAngleItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateAngleItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_ANGLE_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateDensityItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateDensityItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_DENSITY_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleGenerateOpacityItem : public CKeyFrameTreeItem
{
public:
	CParticleGenerateOpacityItem() : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_OPACITY_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleComplexSourceItem : public CStatsItem
{
public:
	CParticleComplexSourceItem() : CStatsItem( ETIT_PARTICLE_COMPLEX_SOURCE_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticlePropItems : public CStatsItem
{
public:
	CParticlePropItems() : CStatsItem( ETIT_PARTICLE_PROP_ITEMS ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleComplexItem : public CStatsItem
{
public:
	CParticleComplexItem() : CStatsItem( ETIT_PARTICLE_COMPLEX_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleSpinItem : public CKeyFrameTreeItem
{
public:
	CParticleSpinItem() : CKeyFrameTreeItem( ETIT_PARTICLE_SPIN_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleWeightItem : public CKeyFrameTreeItem
{
public:
	CParticleWeightItem() : CKeyFrameTreeItem( ETIT_PARTICLE_WEIGHT_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleSpeedItem : public CKeyFrameTreeItem
{
public:
	CParticleSpeedItem() : CKeyFrameTreeItem( ETIT_PARTICLE_SPEED_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleCRandomSpeedItem : public CKeyFrameTreeItem
{
public:
	CParticleCRandomSpeedItem() : CKeyFrameTreeItem( ETIT_PARTICLE_C_RANDOM_SPEED_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleSizeItem : public CKeyFrameTreeItem
{
public:
	CParticleSizeItem() : CKeyFrameTreeItem( ETIT_PARTICLE_SIZE_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleOpacityItem : public CKeyFrameTreeItem
{
public:
	CParticleOpacityItem() : CKeyFrameTreeItem( ETIT_PARTICLE_OPACITY_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CParticleTextureFrameItem : public CKeyFrameTreeItem
{
public:
	CParticleTextureFrameItem() : CKeyFrameTreeItem( ETIT_PARTICLE_TEXTURE_FRAME_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
