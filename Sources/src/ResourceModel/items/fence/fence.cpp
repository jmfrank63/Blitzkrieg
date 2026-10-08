#include "fence.h"

#include "../../mfc_value.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CFenceTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();

	SChildItem child;

	child.nChildItemType = ETIT_FENCE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_OBJECT_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "North-east";
	child.szDisplayName = "North-east";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "North-west";
	child.szDisplayName = "North-west";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "South-west";
	child.szDisplayName = "South-west";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "South-east";
	child.szDisplayName = "South-east";
	defaultChilds.push_back( child );
}

void CFenceCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Fence";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Fences directory";
	prop.szDisplayName = "Fences directory";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Absorbtion";
	prop.szDisplayName = "Armor";
	prop.value = 20;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = false;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CFenceDirectionItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();

	SChildItem child;

	child.nChildItemType = ETIT_FENCE_INSERT_ITEM;
	child.szDefaultName = "Safe";
	child.szDisplayName = "Safe";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_INSERT_ITEM;
	child.szDefaultName = "Destroyed left";
	child.szDisplayName = "Destroyed left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_INSERT_ITEM;
	child.szDefaultName = "Destroyed right";
	child.szDisplayName = "Destroyed right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_FENCE_INSERT_ITEM;
	child.szDefaultName = "Full destroyed";
	child.szDisplayName = "Full destroyed";
	defaultChilds.push_back( child );
}

void CFenceInsertItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CFencePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

// CFencePropsItem::operator&: the base fields, SpritePos and SegmentIndex, then
// what CFenceFrame::SaveMyData / LoadMyData add: LockedTiles and Transparences.
// MFC's bLoaded, set on a read, only tells the view the tiles are drawn.
void CFencePropsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	if ( const NResourceXml::Node *pos = FindElement( node, "SpritePos" ) )
		ReadVec3( *pos, vSpritePos );
	ReadInt( node, "SegmentIndex", nSegmentIndex );
	if ( const NResourceXml::Node *list = FindElement( node, "LockedTiles" ) )
		ReadTiles( *list, lockedTiles );
	if ( const NResourceXml::Node *list = FindElement( node, "Transparences" ) )
		ReadTiles( *list, transeparences );
}

void CFencePropsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	node.children.push_back( Vec3Element( "SpritePos", vSpritePos ) );
	SetAttr( node, "SegmentIndex", MfcInt( nSegmentIndex ) );
	node.children.push_back( TilesElement( "LockedTiles", lockedTiles ) );
	node.children.push_back( TilesElement( "Transparences", transeparences ) );
}

bool CFencePropsItem::OwnsField( const std::string &name ) const
{
	return name == "SpritePos" || name == "SegmentIndex" || name == "LockedTiles" || name == "Transparences" || CStatsItem::OwnsField( name );
}

}
