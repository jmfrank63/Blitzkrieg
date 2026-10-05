#include "squad.h"

#include <cstdlib>

#include "../../editor_env.h"
#include "../../mfc_value.h"

namespace NResourceModel
{

void CSquadTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_SQUAD_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_SQUAD_MEMBERS_ITEM;
	child.szDefaultName = "Members";
	child.szDisplayName = "Members";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_SQUAD_FORMATIONS_ITEM;
	child.szDefaultName = "Formations";
	child.szDisplayName = "Formations";
	defaultChilds.push_back( child );
}

void CSquadCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Squad";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Squad picture";
	prop.szDisplayName = "Squad picture";
	prop.value = "icon.tga";
	prop.szStrings.push_back( "" );			// the picture is to be copied when the project is exported
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Squad type";
	prop.szDisplayName = "Squad type";
	prop.value = "riflemans";
	prop.szStrings.push_back( "riflemans" );
	prop.szStrings.push_back( "infantry" );
	prop.szStrings.push_back( "submachine gunners" );
	prop.szStrings.push_back( "machine gunners" );
	prop.szStrings.push_back( "AT team" );
	prop.szStrings.push_back( "mortar team" );
	prop.szStrings.push_back( "snipers" );
	prop.szStrings.push_back( "gunners" );
	prop.szStrings.push_back( "engineers" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CSquadMembersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSquadMemberPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_SOLDIER_REF;
	prop.szDefaultName = "Soldier";
	prop.szDisplayName = "Soldier";
	prop.value = "USSR\\Mosin";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CSquadFormationsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSquadFormationPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Formation type";
	prop.szDisplayName = "Formation type";
	prop.value = "default";
	prop.szStrings.push_back( "default" );
	prop.szStrings.push_back( "movement" );
	prop.szStrings.push_back( "defensive" );
	prop.szStrings.push_back( "offensive" );
	prop.szStrings.push_back( "sneak" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Hit switch formation";
	prop.szDisplayName = "Hit switch formation";
	prop.value = -1;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Lie state";
	prop.szDisplayName = "Lie state";
	prop.szStrings.push_back( "standart" );
	prop.szStrings.push_back( "always stand" );
	prop.szStrings.push_back( "always lie" );
	prop.value = "standart";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed bonus";
	prop.szDisplayName = "Speed bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Dispersion bonus";
	prop.szDisplayName = "Dispersion bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Fire rate bonus";
	prop.szDisplayName = "Fire rate bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relax time bonus";
	prop.szDisplayName = "Relax time bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Cover bonus";
	prop.szDisplayName = "Cover bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Visible bonus";
	prop.szDisplayName = "Sight bonus";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

namespace
{

// DTHelper.h writes a CVec3 as an element with x, y and z attributes, each a
// double through "%lg".
NResourceXml::Node Vec3Element( const std::string &name, const Vec3 &v )
{
	NResourceXml::Node n;
	n.kind = NResourceXml::Node::Element;
	n.name = name;
	SetAttr( n, "x", MfcFloat( v.x ) );
	SetAttr( n, "y", MfcFloat( v.y ) );
	SetAttr( n, "z", MfcFloat( v.z ) );
	return n;
}

void ReadFloat( const NResourceXml::Node &node, const char *name, float &f )
{
	if ( const std::string *v = FindAttr( node, name ) )
		f = (float)std::strtod( v->c_str(), nullptr );
}

void ReadVec3( const NResourceXml::Node &node, Vec3 &v )
{
	ReadFloat( node, "x", v.x );
	ReadFloat( node, "y", v.y );
	ReadFloat( node, "z", v.z );
}

}

// CSquadFormationPropsItem::operator&: the base fields, then the units list
// (SUnit::operator&: Pos, Dir), ZeroPos and FormationDir.
void CSquadFormationPropsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	if ( const NResourceXml::Node *list = FindElement( node, "units" ) )
	{
		units.clear();
		for ( const auto &entry : list->children )
		{
			if ( entry.kind != NResourceXml::Node::Element || entry.name != "item" )
				continue;
			SUnit unit;
			if ( const NResourceXml::Node *pos = FindElement( entry, "Pos" ) )
				ReadVec3( *pos, unit.vPos );
			ReadFloat( entry, "Dir", unit.fDir );
			units.push_back( unit );
		}
	}
	if ( const NResourceXml::Node *pos = FindElement( node, "ZeroPos" ) )
		ReadVec3( *pos, vZeroPos );
	ReadFloat( node, "FormationDir", fFormationDir );
}

void CSquadFormationPropsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	NResourceXml::Node list;
	list.kind = NResourceXml::Node::Element;
	list.name = "units";
	for ( const SUnit &unit : units )
	{
		NResourceXml::Node entry;
		entry.kind = NResourceXml::Node::Element;
		entry.name = "item";
		entry.children.push_back( Vec3Element( "Pos", unit.vPos ) );
		SetAttr( entry, "Dir", MfcFloat( unit.fDir ) );
		list.children.push_back( std::move( entry ) );
	}
	node.children.push_back( std::move( list ) );
	node.children.push_back( Vec3Element( "ZeroPos", vZeroPos ) );
	SetAttr( node, "FormationDir", MfcFloat( fFormationDir ) );
}

bool CSquadFormationPropsItem::OwnsField( const std::string &name ) const
{
	return name == "units" || name == "ZeroPos" || name == "FormationDir" || CStatsItem::OwnsField( name );
}

}
