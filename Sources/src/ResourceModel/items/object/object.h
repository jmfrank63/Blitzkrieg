#pragma once
// Object sub-editor - project extension .obt, project XML root tag
// "Object_Composer_Project". MFC source: Sources/src/editor/ObjTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:71-84.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CObjectTreeRootItem : public CStatsItem
{
public:
	CObjectTreeRootItem() : CStatsItem( ETIT_OBJECT_ROOT_ITEM, "Object_Composer_Project" ) {}
};
class CObjectCommonPropsItem    : public CStatsItem { public: CObjectCommonPropsItem()    : CStatsItem( ETIT_OBJECT_COMMON_PROPS_ITEM ) {} };
class CObjectGraphicsItem       : public CStatsItem { public: CObjectGraphicsItem()       : CStatsItem( ETIT_OBJECT_GRAPHICS_ITEM ) {} };
class CObjectSpritePropsItem    : public CStatsItem { public: CObjectSpritePropsItem()    : CStatsItem( ETIT_OBJECT_SPRITE_PROPS_ITEM ) {} };
class CObjectShadowPropsItem    : public CStatsItem { public: CObjectShadowPropsItem()    : CStatsItem( ETIT_OBJECT_SHADOW_PROPS_ITEM ) {} };
class CObjectParticlesItem      : public CStatsItem { public: CObjectParticlesItem()      : CStatsItem( ETIT_OBJECT_PARTICLES_ITEM ) {} };
class CObjectPassesItem         : public CStatsItem { public: CObjectPassesItem()         : CStatsItem( ETIT_OBJECT_PASSES_ITEM ) {} };
class CObjectPassPropsItem      : public CStatsItem { public: CObjectPassPropsItem()      : CStatsItem( ETIT_OBJECT_PASS_PROPS_ITEM ) {} };
class CObjectGraphic1PropsItem  : public CStatsItem { public: CObjectGraphic1PropsItem()  : CStatsItem( ETIT_OBJECT_GRAPHIC1_PROPS_ITEM ) {} };
class CObjectGraphicW1PropsItem : public CStatsItem { public: CObjectGraphicW1PropsItem() : CStatsItem( ETIT_OBJECT_GRAPHICW1_PROPS_ITEM ) {} };
class CObjectEffectsItem        : public CStatsItem { public: CObjectEffectsItem()        : CStatsItem( ETIT_OBJECT_EFFECTS_ITEM ) {} };
class CObjectGraphicA1PropsItem : public CStatsItem { public: CObjectGraphicA1PropsItem() : CStatsItem( ETIT_OBJECT_GRAPHICA1_PROPS_ITEM ) {} };

}
