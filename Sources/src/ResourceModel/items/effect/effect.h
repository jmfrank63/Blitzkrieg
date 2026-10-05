#pragma once
// Effect sub-editor - project extension .eff, project XML root tag
// "Effect_Composer_Project". MFC source: Sources/src/editor/EffTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CEffectTreeRootItem : public CStatsItem
{
public:
	CEffectTreeRootItem() : CStatsItem( ETIT_EFFECT_ROOT_ITEM, "Effect_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectCommonPropsItem : public CStatsItem
{
public:
	CEffectCommonPropsItem() : CStatsItem( ETIT_EFFECT_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectAnimationsItem : public CStatsItem
{
public:
	CEffectAnimationsItem() : CStatsItem( ETIT_EFFECT_ANIMATIONS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectMeshesItem : public CStatsItem
{
public:
	CEffectMeshesItem() : CStatsItem( ETIT_EFFECT_MESHES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectFuncParticlesItem : public CStatsItem
{
public:
	CEffectFuncParticlesItem() : CStatsItem( ETIT_EFFECT_FUNC_PARTICLES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectMayaParticlesItem : public CStatsItem
{
public:
	CEffectMayaParticlesItem() : CStatsItem( ETIT_EFFECT_MAYA_PARTICLES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectLightsItem : public CStatsItem
{
public:
	CEffectLightsItem() : CStatsItem( ETIT_EFFECT_LIGHTS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectAnimationPropsItem : public CStatsItem
{
public:
	CEffectAnimationPropsItem() : CStatsItem( ETIT_EFFECT_ANIMATION_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectMeshPropsItem : public CStatsItem
{
public:
	CEffectMeshPropsItem() : CStatsItem( ETIT_EFFECT_MESH_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectMayaPropsItem : public CStatsItem
{
public:
	CEffectMayaPropsItem() : CStatsItem( ETIT_EFFECT_MAYA_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CEffectFuncPropsItem : public CStatsItem
{
public:
	CEffectFuncPropsItem() : CStatsItem( ETIT_EFFECT_FUNC_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
