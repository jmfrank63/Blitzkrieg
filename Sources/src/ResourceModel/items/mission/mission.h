#pragma once
// Mission sub-editor - project extension .mip, project XML root tag
// "Mission_Composer_Project". MFC source: Sources/src/editor/MissionTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMissionTreeRootItem : public CStatsItem
{
public:
	CMissionTreeRootItem() : CStatsItem( ETIT_MISSION_ROOT_ITEM, "Mission_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMissionCommonPropsItem : public CStatsItem
{
public:
	CMissionCommonPropsItem() : CStatsItem( ETIT_MISSION_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMissionMusicsItem : public CStatsItem
{
public:
	CMissionMusicsItem() : CStatsItem( ETIT_MISSION_MUSICS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMissionMusicPropsItem : public CStatsItem
{
public:
	CMissionMusicPropsItem() : CStatsItem( ETIT_MISSION_MUSIC_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMissionObjectivesItem : public CStatsItem
{
public:
	CMissionObjectivesItem() : CStatsItem( ETIT_MISSION_OBJECTIVES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMissionObjectivePropsItem : public CStatsItem
{
public:
	CMissionObjectivePropsItem() : CStatsItem( ETIT_MISSION_OBJECTIVE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
	// CMissionFrame::FillRPGStats (MissionFrm.cpp:127) reads the secret flag on every save.
	bool BoolsReadAsInt() const override { return true; }
};

}
