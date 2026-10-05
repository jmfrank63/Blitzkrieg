#pragma once
// Object sub-editor - project extension .obt, project XML root tag
// "Object_Composer_Project". MFC source: Sources/src/editor/ObjTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CObjectTreeRootItem : public CStatsItem
{
public:
	CObjectTreeRootItem() : CStatsItem( ETIT_OBJECT_ROOT_ITEM, "Object_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectCommonPropsItem : public CStatsItem
{
public:
	CObjectCommonPropsItem() : CStatsItem( ETIT_OBJECT_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectPassesItem : public CStatsItem
{
public:
	CObjectPassesItem() : CStatsItem( ETIT_OBJECT_PASSES_ITEM ) { bStaticElements = false; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectPassPropsItem : public CStatsItem
{
public:
	CObjectPassPropsItem() : CStatsItem( ETIT_OBJECT_PASS_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectGraphicsItem : public CStatsItem
{
public:
	CObjectGraphicsItem() : CStatsItem( ETIT_OBJECT_GRAPHICS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

// MFC's shared base of the graphic items (the commented-out
// E_OBJECT_GRAPHIC_PROPS_ITEM was never a type); the derived items set theirs.
class CObjectGraphicPropsItem : public CStatsItem
{
protected:
	explicit CObjectGraphicPropsItem( int nType ) : CStatsItem( nType ) { bStaticElements = true; }
};

class CObjectGraphic1PropsItem : public CObjectGraphicPropsItem
{
public:
	CObjectGraphic1PropsItem() : CObjectGraphicPropsItem( ETIT_OBJECT_GRAPHIC1_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectGraphicW1PropsItem : public CObjectGraphicPropsItem
{
public:
	CObjectGraphicW1PropsItem() : CObjectGraphicPropsItem( ETIT_OBJECT_GRAPHICW1_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CObjectGraphicA1PropsItem : public CObjectGraphicPropsItem
{
public:
	CObjectGraphicA1PropsItem() : CObjectGraphicPropsItem( ETIT_OBJECT_GRAPHICA1_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

// MFC's constructor leaves nItemType 0; the port keeps the type the factory
// registers the class under.
class CObjectSpritePropsItem : public CStatsItem
{
public:
	CObjectSpritePropsItem() : CStatsItem( ETIT_OBJECT_SPRITE_PROPS_ITEM ) {}
};

// MFC's constructor leaves nItemType 0; the port keeps the type the factory
// registers the class under.
class CObjectShadowPropsItem : public CStatsItem
{
public:
	CObjectShadowPropsItem() : CStatsItem( ETIT_OBJECT_SHADOW_PROPS_ITEM ) {}
};

// MFC's constructor leaves nItemType 0; the port keeps the type the factory
// registers the class under.
class CObjectParticlesItem : public CStatsItem
{
public:
	CObjectParticlesItem() : CStatsItem( ETIT_OBJECT_PARTICLES_ITEM ) {}
};

class CObjectEffectsItem : public CStatsItem
{
public:
	CObjectEffectsItem() : CStatsItem( ETIT_OBJECT_EFFECTS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
