#pragma once
// Mesh (Unit) sub-editor - project extension .msh, project XML root tag
// "Mesh_Composer_Project". MFC source: Sources/src/editor/MeshTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMeshTreeRootItem : public CStatsItem
{
public:
	CMeshTreeRootItem() : CStatsItem( ETIT_MESH_ROOT_ITEM, "Mesh_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshCommonPropsItem : public CStatsItem
{
public:
	CMeshCommonPropsItem() : CStatsItem( ETIT_MESH_COMMON_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshEffectsItem : public CStatsItem
{
public:
	CMeshEffectsItem() : CStatsItem( ETIT_MESH_EFFECTS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshSoundPropsItem : public CStatsItem
{
public:
	CMeshSoundPropsItem() : CStatsItem( ETIT_MESH_SOUND_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshAviaItem : public CStatsItem
{
public:
	CMeshAviaItem() : CStatsItem( ETIT_MESH_AVIA_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshTrackItem : public CStatsItem
{
public:
	CMeshTrackItem() : CStatsItem( ETIT_MESH_TRACK_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshDefencesItem : public CStatsItem
{
public:
	CMeshDefencesItem() : CStatsItem( ETIT_MESH_DEFENCES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshDefencePropsItem : public CStatsItem
{
public:
	CMeshDefencePropsItem() : CStatsItem( ETIT_MESH_DEFENCE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshJoggingsItem : public CStatsItem
{
public:
	CMeshJoggingsItem() : CStatsItem( ETIT_MESH_JOGGINGS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshJoggingPropsItem : public CStatsItem
{
public:
	CMeshJoggingPropsItem() : CStatsItem( ETIT_MESH_JOGGING_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshPlatformsItem : public CStatsItem
{
public:
	CMeshPlatformsItem() : CStatsItem( ETIT_MESH_PLATFORMS_ITEM ) { bStaticElements = false; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshPlatformPropsItem : public CStatsItem
{
public:
	CMeshPlatformPropsItem() : CStatsItem( ETIT_MESH_PLATFORM_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshGunsItem : public CStatsItem
{
public:
	CMeshGunsItem() : CStatsItem( ETIT_MESH_GUNS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshGunPropsItem : public CStatsItem
{
public:
	CMeshGunPropsItem() : CStatsItem( ETIT_MESH_GUN_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshGraphicsItem : public CStatsItem
{
public:
	CMeshGraphicsItem() : CStatsItem( ETIT_MESH_GRAPHICS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshDeathCratersItem : public CStatsItem
{
public:
	CMeshDeathCratersItem() : CStatsItem( ETIT_MESH_DEATH_CRATERS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshDeathCraterPropsItem : public CStatsItem
{
public:
	CMeshDeathCraterPropsItem() : CStatsItem( ETIT_MESH_DEATH_CRATER_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshLocatorsItem : public CStatsItem
{
public:
	CMeshLocatorsItem() : CStatsItem( ETIT_MESH_LOCATORS_ITEM ) { bStaticElements = false; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMeshLocatorPropsItem : public CStatsItem
{
public:
	CMeshLocatorPropsItem() : CStatsItem( ETIT_MESH_LOCATOR_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

	int nLocatorID = -1;	// MFC's constructor initialiser; the export assigns it, operator& does not store it
	bool bLocator = false;	// the node is in the skeleton's locator list (IMeshAnimationEdit::GetAllLocatorNames); derived like nLocatorID

protected:
	void InitDefaultValues() override;
};

}
