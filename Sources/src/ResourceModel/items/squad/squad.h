#pragma once
// Squad sub-editor - project extension .scp, project XML root tag
// "Squad_Composer_Project". MFC source: Sources/src/editor/SquadTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include <list>

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CSquadTreeRootItem : public CStatsItem
{
public:
	CSquadTreeRootItem() : CStatsItem( ETIT_SQUAD_ROOT_ITEM, "Squad_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSquadCommonPropsItem : public CStatsItem
{
public:
	CSquadCommonPropsItem() : CStatsItem( ETIT_SQUAD_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSquadMembersItem : public CStatsItem
{
public:
	CSquadMembersItem() : CStatsItem( ETIT_SQUAD_MEMBERS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSquadMemberPropsItem : public CStatsItem
{
public:
	CSquadMemberPropsItem() : CStatsItem( ETIT_SQUAD_MEMBER_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSquadFormationsItem : public CStatsItem
{
public:
	CSquadFormationsItem() : CStatsItem( ETIT_SQUAD_FORMATIONS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSquadFormationPropsItem : public CStatsItem
{
public:
	CSquadFormationPropsItem() : CStatsItem( ETIT_SQUAD_FORMATION_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

	// A formation slot. MFC's SUnit also holds the member item and the
	// sprite the view draws; both are set up after loading and not saved.
	struct SUnit
	{
		Vec3 vPos;			// 3d position of the soldier
		float fDir = 0;
	};
	using CUnitsList = std::list<SUnit>;
	CUnitsList units;
	Vec3 vZeroPos{ 16 * fWorldCellSize, 8 * fWorldCellSize, 0 };	// the formation's centre in the view
	float fFormationDir = 0;

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override;

protected:
	void InitDefaultValues() override;
};

}
