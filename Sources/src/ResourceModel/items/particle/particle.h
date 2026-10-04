#pragma once
// Particle sub-editor - project extension .pcp, project XML root tag
// "Particle_Composer_Project". MFC source:
// Sources/src/editor/ParticleTreeItem.{h,cpp} and the keyframe base in
// Sources/src/editor/TreeItem.h:378. Mirrors REGISTER_CLASS entries in
// Sources/src/editor/TreeItemFactory.cpp:164-188. Seventeen of the twenty-five
// per-kind classes derive from CKeyFrameTreeItem; the eight non-keyframe ones
// are CStatsItem shells matching the T02 pattern.

#include "../../key_frame_tree_item.h"
#include "../../tree_item.h"
#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CParticleTreeRootItem : public CStatsItem
{
public:
	CParticleTreeRootItem() : CStatsItem( ETIT_PARTICLE_ROOT_ITEM, "Particle_Composer_Project" ) {}
};
class CParticleCommonPropsItem         : public CStatsItem { public: CParticleCommonPropsItem()         : CStatsItem( ETIT_PARTICLE_COMMON_PROPS_ITEM ) {} };
class CParticleSourcePropItems         : public CStatsItem { public: CParticleSourcePropItems()         : CStatsItem( ETIT_PARTICLE_SOURCE_PROP_ITEMS ) {} };
class CParticleGenerateSpinItem        : public CKeyFrameTreeItem { public: CParticleGenerateSpinItem()        : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_SPIN_ITEM ) {} };
class CParticleGenerateAreaItem        : public CKeyFrameTreeItem { public: CParticleGenerateAreaItem()        : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_AREA_ITEM ) {} };
class CParticleGenerateAngleItem       : public CKeyFrameTreeItem { public: CParticleGenerateAngleItem()       : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_ANGLE_ITEM ) {} };
class CParticleGenerateOpacityItem     : public CKeyFrameTreeItem { public: CParticleGenerateOpacityItem()     : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_OPACITY_ITEM ) {} };
class CParticleGenerateSpeedItem       : public CKeyFrameTreeItem { public: CParticleGenerateSpeedItem()       : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_SPEED_ITEM ) {} };
class CParticleGenerateLifeItem        : public CKeyFrameTreeItem { public: CParticleGenerateLifeItem()        : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_LIFE_ITEM ) {} };
class CParticleGenerateDensityItem     : public CKeyFrameTreeItem { public: CParticleGenerateDensityItem()     : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_DENSITY_ITEM ) {} };
class CParticleGenerateRandomSpinItem  : public CKeyFrameTreeItem { public: CParticleGenerateRandomSpinItem()  : CKeyFrameTreeItem( ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM ) {} };
class CParticlePropItems               : public CStatsItem { public: CParticlePropItems()               : CStatsItem( ETIT_PARTICLE_PROP_ITEMS ) {} };
class CParticleSpinItem                : public CKeyFrameTreeItem { public: CParticleSpinItem()                : CKeyFrameTreeItem( ETIT_PARTICLE_SPIN_ITEM ) {} };
class CParticleWeightItem              : public CKeyFrameTreeItem { public: CParticleWeightItem()              : CKeyFrameTreeItem( ETIT_PARTICLE_WEIGHT_ITEM ) {} };
class CParticleSpeedItem               : public CKeyFrameTreeItem { public: CParticleSpeedItem()               : CKeyFrameTreeItem( ETIT_PARTICLE_SPEED_ITEM ) {} };
class CParticleSizeItem                : public CKeyFrameTreeItem { public: CParticleSizeItem()                : CKeyFrameTreeItem( ETIT_PARTICLE_SIZE_ITEM ) {} };
class CParticleOpacityItem             : public CKeyFrameTreeItem { public: CParticleOpacityItem()             : CKeyFrameTreeItem( ETIT_PARTICLE_OPACITY_ITEM ) {} };
class CParticleTextureFrameItem        : public CKeyFrameTreeItem { public: CParticleTextureFrameItem()        : CKeyFrameTreeItem( ETIT_PARTICLE_TEXTURE_FRAME_ITEM ) {} };
class CParticleComplexSourceItem       : public CStatsItem { public: CParticleComplexSourceItem()       : CStatsItem( ETIT_PARTICLE_COMPLEX_SOURCE_ITEM ) {} };
class CParticleRandLifeItem            : public CKeyFrameTreeItem { public: CParticleRandLifeItem()            : CKeyFrameTreeItem( ETIT_PARTICLE_RAND_LIFE_ITEM ) {} };
class CParticleRandSpeedItem           : public CKeyFrameTreeItem { public: CParticleRandSpeedItem()           : CKeyFrameTreeItem( ETIT_PARTICLE_RAND_SPEED_ITEM ) {} };
class CParticleComplexItem             : public CStatsItem { public: CParticleComplexItem()             : CStatsItem( ETIT_PARTICLE_COMPLEX_ITEM ) {} };
class CParticleCRandomSpeedItem        : public CKeyFrameTreeItem { public: CParticleCRandomSpeedItem()        : CKeyFrameTreeItem( ETIT_PARTICLE_C_RANDOM_SPEED_ITEM ) {} };

}
