#pragma once
// Trench sub-editor - project extension .trc, project XML root tag
// "Trench_Composer_Project". MFC source: Sources/src/editor/TrenchTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CTrenchTreeRootItem : public CStatsItem
{
public:
	CTrenchTreeRootItem() : CStatsItem( ETIT_TRENCH_ROOT_ITEM, "Trench_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTrenchCommonPropsItem : public CStatsItem
{
public:
	CTrenchCommonPropsItem() : CStatsItem( ETIT_TRENCH_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTrenchSourcesItem : public CStatsItem
{
public:
	CTrenchSourcesItem() : CStatsItem( ETIT_TRENCH_SOURCES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTrenchSourcePropsItem : public CStatsItem
{
public:
	CTrenchSourcePropsItem() : CStatsItem( ETIT_TRENCH_SOURCE_PROPS_ITEM ) { InitDefaultValues(); }

	int nTrenchIndex = -1;	// MFC's constructor initialiser; the export assigns it

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override;

protected:
	void InitDefaultValues() override;
};

class CTrenchDefencesItem : public CStatsItem
{
public:
	CTrenchDefencesItem() : CStatsItem( ETIT_TRENCH_DEFENCES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTrenchDefencePropsItem : public CStatsItem
{
public:
	CTrenchDefencePropsItem() : CStatsItem( ETIT_TRENCH_DEFENCE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
