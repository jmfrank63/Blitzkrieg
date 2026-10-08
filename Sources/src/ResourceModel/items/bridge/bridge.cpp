#include "bridge.h"

#include "../../mfc_value.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CBridgeTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BRIDGE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCES_ITEM;
	child.szDefaultName = "Defence";
	child.szDisplayName = "Defence";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Destroyed";
	child.szDisplayName = "Destroyed";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_FIRE_POINTS_ITEM;
	child.szDefaultName = "Fire points";
	child.szDisplayName = "Fire points";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_SMOKES_ITEM;
	child.szDefaultName = "Smoke points";
	child.szDisplayName = "Smoke points";
	defaultChilds.push_back( child );
}

void CBridgeCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Bridge";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Bridge type";
	prop.szDisplayName = "Bridge type";
	prop.value = "horizontal";
	prop.szStrings.push_back( "horizontal" );
	prop.szStrings.push_back( "vertical" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Repair cost";
	prop.szDisplayName = "Repair cost";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = true;
	defaultValues.push_back( prop );
	values = defaultValues;
}

void CBridgeDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );
}

void CBridgeDefencePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Min armor";
	prop.szDisplayName = "Min armor";
	prop.value = 40;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Max armor";
	prop.szDisplayName = "Max armor";
	prop.value = 90;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Silhouette";
	prop.szDisplayName = "Silhouette";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBridgeStagePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BRIDGE_BEGIN_SPANS_ITEM;
	child.szDefaultName = "Begin spans";
	child.szDisplayName = "Begin spans";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_CENTER_SPANS_ITEM;
	child.szDefaultName = "Center spans";
	child.szDisplayName = "Center spans";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_END_SPANS_ITEM;
	child.szDefaultName = "End spans";
	child.szDisplayName = "End spans";
	defaultChilds.push_back( child );
}

void CBridgeBeginSpansItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBridgeCenterSpansItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBridgeEndSpansItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBridgePartsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

/*
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Length";
	prop.szDisplayName = "Length";
	prop.value = 2;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Width";
	prop.szDisplayName = "Width";
	prop.value = 4;
	defaultValues.push_back( prop );
*/

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Back girder";
	child.szDisplayName = "Back girder";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Front girder";
	child.szDisplayName = "Front girder";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Slab";
	child.szDisplayName = "Slab";
	defaultChilds.push_back( child );
}

void CBridgePartPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Picture";
	prop.szDisplayName = "Picture";
	prop.value = "";
	prop.szStrings.push_back( "" );

/*
	CParentFrame *pFrame = g_frameManager.GetActiveFrame();
	if ( pFrame == 0 )
		prop.szStrings.push_back( "" );
	else
		prop.szStrings.push_back( GetDirectory( pFrame->GetProjectFileName().c_str() ) );
*/
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	values = defaultValues;
}

void CBridgeFirePointsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBridgeFirePointPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Fire effect";
	prop.szDisplayName = "Fire effect";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBridgeDirExplosionsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect explosion";
	prop.szDisplayName = "Effect explosion";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front left";
	child.szDisplayName = "Front left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front right";
	child.szDisplayName = "Front right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back right";
	child.szDisplayName = "Back right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back left";
	child.szDisplayName = "Back left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Top center";
	child.szDisplayName = "Top center";
	defaultChilds.push_back( child );
}

void CBridgeDirExplosionPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBridgeSmokesItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect explosion";
	prop.szDisplayName = "Effect explosion";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBridgeSmokePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

// CBridgePartsItem::operator&: the base fields and SpanIndex, then what
// CBridgeFrame::SaveMyData / LoadMyData add: LockedTiles, Transparences and
// UnLockedTiles.
void CBridgePartsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	ReadInt( node, "SpanIndex", nSpanIndex );
	if ( const NResourceXml::Node *list = FindElement( node, "LockedTiles" ) )
		ReadTiles( *list, lockedTiles );
	if ( const NResourceXml::Node *list = FindElement( node, "Transparences" ) )
		ReadTiles( *list, transeparences );
	if ( const NResourceXml::Node *list = FindElement( node, "UnLockedTiles" ) )
		ReadTiles( *list, unLockedTiles );
}

void CBridgePartsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	SetAttr( node, "SpanIndex", MfcInt( nSpanIndex ) );
	node.children.push_back( TilesElement( "LockedTiles", lockedTiles ) );
	node.children.push_back( TilesElement( "Transparences", transeparences ) );
	node.children.push_back( TilesElement( "UnLockedTiles", unLockedTiles ) );
}

bool CBridgePartsItem::OwnsField( const std::string &name ) const
{
	return name == "SpanIndex" || name == "LockedTiles" || name == "Transparences" || name == "UnLockedTiles" || CStatsItem::OwnsField( name );
}

}
