#pragma once
// Effect sub-editor - project extension .eff, project XML root tag
// "Effect_Composer_Project". MFC source: Sources/src/editor/EffTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:59-69.
// None of the Effect items derive from CKeyFrameTreeItem in MFC; the port
// keeps the same shape with CStatsItem shells.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CEffectTreeRootItem : public CStatsItem
{
public:
	CEffectTreeRootItem() : CStatsItem( ETIT_EFFECT_ROOT_ITEM, "Effect_Composer_Project" ) {}
};
class CEffectCommonPropsItem    : public CStatsItem { public: CEffectCommonPropsItem()    : CStatsItem( ETIT_EFFECT_COMMON_PROPS_ITEM ) {} };
class CEffectAnimationsItem     : public CStatsItem { public: CEffectAnimationsItem()     : CStatsItem( ETIT_EFFECT_ANIMATIONS_ITEM ) {} };
class CEffectMeshesItem         : public CStatsItem { public: CEffectMeshesItem()         : CStatsItem( ETIT_EFFECT_MESHES_ITEM ) {} };
class CEffectFuncParticlesItem  : public CStatsItem { public: CEffectFuncParticlesItem()  : CStatsItem( ETIT_EFFECT_FUNC_PARTICLES_ITEM ) {} };
class CEffectMayaParticlesItem  : public CStatsItem { public: CEffectMayaParticlesItem()  : CStatsItem( ETIT_EFFECT_MAYA_PARTICLES_ITEM ) {} };
class CEffectLightsItem         : public CStatsItem { public: CEffectLightsItem()         : CStatsItem( ETIT_EFFECT_LIGHTS_ITEM ) {} };
class CEffectAnimationPropsItem : public CStatsItem { public: CEffectAnimationPropsItem() : CStatsItem( ETIT_EFFECT_ANIMATION_PROPS_ITEM ) {} };
class CEffectMeshPropsItem      : public CStatsItem { public: CEffectMeshPropsItem()      : CStatsItem( ETIT_EFFECT_MESH_PROPS_ITEM ) {} };
class CEffectFuncPropsItem      : public CStatsItem { public: CEffectFuncPropsItem()      : CStatsItem( ETIT_EFFECT_FUNC_PROPS_ITEM ) {} };
class CEffectMayaPropsItem      : public CStatsItem { public: CEffectMayaPropsItem()      : CStatsItem( ETIT_EFFECT_MAYA_PROPS_ITEM ) {} };

}
