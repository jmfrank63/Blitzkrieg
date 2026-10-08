#include "trench.h"

#include <cstdlib>

#include "../../editor_env.h"
#include "../../mfc_value.h"

namespace NResourceModel
{

static const std::string szModFilter = "Model files (*.mod)|*.mod||";

void CTrenchTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();

	SChildItem child;

	child.nChildItemType = ETIT_TRENCH_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCES_ITEM;
	child.szDefaultName = "Defences";
	child.szDisplayName = "Defences";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trenches with embrasure";
	child.szDisplayName = "Trenches with embrasure";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trenches line";
	child.szDisplayName = "Trenches line";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trench ends";
	child.szDisplayName = "Trench ends";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trench arcs";
	child.szDisplayName = "Trench arcs";
	defaultChilds.push_back( child );
}

void CTrenchCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Trench";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Rest slots";
	prop.szDisplayName = "Rest slots";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Medical slots";
	prop.szDisplayName = "Medical slots";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Cover";
	prop.szDisplayName = "Silhouette";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTrenchSourcesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTrenchSourcePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Source file";
	prop.szDisplayName = "Source file";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szModFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Coverage";
	prop.szDisplayName = "Coverage";
	prop.value = 0.2f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTrenchDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );
}

void CTrenchDefencePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Min armor";
	prop.szDisplayName = "Min armor";
	prop.value = 300;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Max armor";
	prop.szDisplayName = "Max armor";
	prop.value = 300;
	defaultValues.push_back( prop );

	values = defaultValues;
}

// CTrenchSourcePropsItem::operator&: the base fields, then TrenchIndex.
void CTrenchSourcePropsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	if ( const std::string *v = FindAttr( node, "TrenchIndex" ) )
		nTrenchIndex = (int)std::strtol( v->c_str(), nullptr, 0 );
}

void CTrenchSourcePropsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	SetAttr( node, "TrenchIndex", MfcInt( nTrenchIndex ) );
}

bool CTrenchSourcePropsItem::OwnsField( const std::string &name ) const
{
	return name == "TrenchIndex" || CStatsItem::OwnsField( name );
}

}
