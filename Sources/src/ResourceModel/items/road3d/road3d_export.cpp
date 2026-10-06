// The 3D road exporter: C3DRoadFrame::FillRPGStats / SaveRPGStats
// (Sources/src/editor/3dRoadFrm.cpp:86-190), line for line. An
// SVectorStripeObjectDesc is filled from the tree and written as the
// "VSODescription" chunk through the engine's own operator&, so the file is
// what the game's terrain loader reads. The slots are the indices MFC's
// C3DRoadCommonPropsItem and C3DRoadLayerPropsItem getters use.
#include "StdAfx.h"

#include "road3d_export.h"

#include "../stats_export.h"
#include "../../factory.h"
#include "../../mfc_value.h"
#include "../../xml.h"
#include "../tree_item_types.h"
#include "../../../Formats/fmtVSO.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kRoadAddDir[] = "terrain\\sets\\";

// The frame data of an imported road, as the river keeps its own (river3d_export.cpp).
const char kFrameData[] = "desc";

// C3DRoadCommonPropsItem::GetRoadType: a name that is neither road nor
// railroad asserted in MFC and fell back to a road.
int RoadType( const CTreeItem &common )
{
	const std::string szVal = ValueStr( common, 9 );
	if ( szVal == "railroad" || szVal == "RailRoad" )
		return SVectorStripeObjectDesc::TYPE_RAILROAD;
	return SVectorStripeObjectDesc::TYPE_ROAD;
}

CTreeItem *MutableChild( CTreeItem &parent, int nType )
{
	for ( const auto &pChild : parent.GetChildren() )
		if ( pChild->GetItemType() == nType )
			return pChild.get();
	return nullptr;
}

void SetSlot( CTreeItem *pItem, std::size_t nSlot, const CVariant &value )
{
	if ( pItem != nullptr && nSlot < pItem->MutableValues().size() )
		pItem->MutableValues()[nSlot].value = value;
}

}

bool FillRoad3DDesc( const CTreeItem &root, SVectorStripeObjectDesc &desc, std::string &szError, const NResourceXml::Node *pProjectElement )
{
	const CTreeItem *pCommon = ChildItem( root, ETIT_3DROAD_COMMON_PROPS_ITEM );
	const CTreeItem *pLayer = ChildItem( root, ETIT_3DROAD_LAYER_PROPS_ITEM );
	if ( pCommon == nullptr || pLayer == nullptr )
	{
		szError = std::string( "the road project has no \"" ) + ( pCommon == nullptr ? "Basic info" : "Central layer" ) + "\" item";
		return false;
	}

	desc.bottom.nNumCells = ValueInt( *pCommon, 0 );
	desc.bottom.bAnimated = false;
	desc.bottom.fDisturbance = 0;
	desc.bottom.fRelWidth = 1.0f;
	desc.bottom.fStreamSpeed = 0;
	desc.bottom.fTextureStep = ValueFloat( *pLayer, 2 );
	desc.bottom.opacityBorder = BYTE( ValueInt( *pLayer, 1 ) );
	desc.bottom.opacityCenter = BYTE( ValueInt( *pLayer, 0 ) );
	desc.bottom.szTexture = ValueStr( *pLayer, 3 );

	desc.nPriority = ValueInt( *pCommon, 3 );
	desc.fPassability = ValueFloat( *pCommon, 4 );

	desc.dwAIClasses = 0;
	if ( ValueBool( *pCommon, 5 ) )
		desc.dwAIClasses |= AI_CLASS_HUMAN;
	if ( ValueBool( *pCommon, 6 ) )
		desc.dwAIClasses |= AI_CLASS_WHEEL;
	if ( ValueBool( *pCommon, 7 ) )
		desc.dwAIClasses |= AI_CLASS_HALFTRACK;
	if ( ValueBool( *pCommon, 8 ) )
		desc.dwAIClasses |= AI_CLASS_TRACK;
	desc.dwAIClasses = ~desc.dwAIClasses;
	// The mask a shipped road holds may carry bits the four flags do not (the mod's holds none above them: 0 where the
	// flags give 0xfffffff0); the kept mask stands while its four class bits are what the flags say.
	const DWORD dwClassBits = AI_CLASS_ANY;
	const NResourceXml::Node *pFrame = pProjectElement != nullptr ? NResourceXml::FindChild( *pProjectElement, kFrameData ) : nullptr;
	if ( pFrame != nullptr )
		if ( const std::string *pKept = FindAttr( *pFrame, "AIClasses" ) )
		{
			const DWORD dwKept = DWORD( std::strtoul( pKept->c_str(), nullptr, 10 ) );
			if ( ( dwKept & dwClassBits ) == ( desc.dwAIClasses & dwClassBits ) )
				desc.dwAIClasses = dwKept;
		}

	// MFC set the type to a road first and overwrote it with the item's.
	desc.eType = SVectorStripeObjectDesc::TYPE_ROAD;
	desc.eType = RoadType( *pCommon );

	desc.miniMapCenterColor = ValueInt( *pCommon, 10 );
	desc.miniMapBorderColor = ValueInt( *pCommon, 11 );

	desc.bottomBorders.resize( 0 );
	if ( ValueBool( *pCommon, 1 ) )
	{
		const CTreeItem *pBorder = ChildItem( root, ETIT_3DROAD_LAYER_PROPS_ITEM, 1 );
		if ( pBorder == nullptr )
		{
			szError = "the road has borders but no \"Border layer\" item (a second layer props item)";
			return false;
		}
		desc.bottomBorders.resize( 2 );
		for ( int i = 0; i < 2; i++ )
		{
			SVectorStripeObjectDesc::SLayer &layer = desc.bottomBorders[i];
			layer.bAnimated = false;
			layer.nNumCells = 1;
			layer.fDisturbance = 0;
			layer.fRelWidth = ValueFloat( *pCommon, 2 );
			layer.fStreamSpeed = 0;
			layer.fTextureStep = ValueFloat( *pBorder, 2 );
			layer.opacityBorder = BYTE( ValueInt( *pBorder, 1 ) );
			layer.opacityCenter = BYTE( ValueInt( *pBorder, 0 ) );
			layer.szTexture = ValueStr( *pBorder, 3 );
		}
		desc.bottom.fRelWidth = 1.0 - ValueFloat( *pCommon, 2 );
	}
	BYTE cSoilParams = 0;
	if ( ValueBool( *pCommon, 12 ) )
		cSoilParams |= SVectorStripeObjectDesc::ESP_DUST;
	if ( ValueBool( *pCommon, 13 ) )
		cSoilParams |= SVectorStripeObjectDesc::ESP_TRACE;
	desc.cSoilParams = cSoilParams;
	return true;
}

bool ExportRoad3D( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_3DROAD_ROOT_ITEM, "road", outcome );
	if ( !pProject )
		return false;
	SVectorStripeObjectDesc desc;
	if ( !FillRoad3DDesc( *pProject->root, desc, outcome.szError, &pProject->document.root ) )
		return false;

	const std::string szFile = StatsFileName( project, context, kRoadAddDir, false );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "VSODescription", &desc );
	}, outcome ) )
		return false;
	// The road the preview draws: the file without its extension.
	std::string szObject = szFile;
	const std::string::size_type nDot = szObject.find_last_of( '.' );
	if ( nDot != std::string::npos && nDot > szObject.find_last_of( '\\' ) + 1 )
		szObject.resize( nDot );
	outcome.szObjectName = szObject;
	return true;
}

void WriteRoadFrameData( NResourceXml::Node &root, const SVectorStripeObjectDesc &desc )
{
	NResourceXml::Node frame;
	frame.kind = NResourceXml::Node::Element;
	frame.name = kFrameData;
	SetAttr( frame, "AIClasses", std::to_string( desc.dwAIClasses ) );
	for ( NResourceXml::Node &child : root.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == kFrameData )
		{
			child = std::move( frame );
			return;
		}
	root.children.push_back( std::move( frame ) );
}

void Road3DStatsToTree( const SVectorStripeObjectDesc &desc, CTreeItem &root )
{
	CTreeItem *pCommon = MutableChild( root, ETIT_3DROAD_COMMON_PROPS_ITEM );
	CTreeItem *pLayer = MutableChild( root, ETIT_3DROAD_LAYER_PROPS_ITEM );
	const bool bBorders = !desc.bottomBorders.empty();
	SetSlot( pCommon, 0, CVariant( int( desc.bottom.nNumCells ) ) );
	SetSlot( pCommon, 1, CVariant( bBorders ) );
	SetSlot( pCommon, 2, CVariant( 1.0f - desc.bottom.fRelWidth ) );
	SetSlot( pCommon, 3, CVariant( int( desc.nPriority ) ) );
	SetSlot( pCommon, 4, CVariant( float( desc.fPassability ) ) );
	SetSlot( pCommon, 5, CVariant( !( desc.dwAIClasses & AI_CLASS_HUMAN ) ) );
	SetSlot( pCommon, 6, CVariant( !( desc.dwAIClasses & AI_CLASS_WHEEL ) ) );
	SetSlot( pCommon, 7, CVariant( !( desc.dwAIClasses & AI_CLASS_HALFTRACK ) ) );
	SetSlot( pCommon, 8, CVariant( !( desc.dwAIClasses & AI_CLASS_TRACK ) ) );
	SetSlot( pCommon, 9, CVariant( desc.eType == SVectorStripeObjectDesc::TYPE_RAILROAD ? "railroad" : "road" ) );
	SetSlot( pCommon, 10, CVariant( int( desc.miniMapCenterColor.color ) ) );
	SetSlot( pCommon, 11, CVariant( int( desc.miniMapBorderColor.color ) ) );
	SetSlot( pCommon, 12, CVariant( ( desc.cSoilParams & SVectorStripeObjectDesc::ESP_DUST ) != 0 ) );
	SetSlot( pCommon, 13, CVariant( ( desc.cSoilParams & SVectorStripeObjectDesc::ESP_TRACE ) != 0 ) );

	SetSlot( pLayer, 0, CVariant( int( desc.bottom.opacityCenter ) ) );
	SetSlot( pLayer, 1, CVariant( int( desc.bottom.opacityBorder ) ) );
	SetSlot( pLayer, 2, CVariant( float( desc.bottom.fTextureStep ) ) );
	SetSlot( pLayer, 3, CVariant( desc.bottom.szTexture ) );

	if ( bBorders )
	{
		auto pBorder = CTreeItemFactory::Instance().Create( ETIT_3DROAD_LAYER_PROPS_ITEM );
		// MFC's SetItemName sets only the display name; the default name stays the
		// layer's, which is what lets CreateDefaultChilds keep it on a reload.
		pBorder->SetItemName( "Border layer" );
		pBorder->SetDefaultName( "Central layer" );
		CTreeItem *pRaw = pBorder.get();
		root.AddChild( std::move( pBorder ) );
		const SVectorStripeObjectDesc::SLayer &border = desc.bottomBorders[0];
		SetSlot( pRaw, 0, CVariant( int( border.opacityCenter ) ) );
		SetSlot( pRaw, 1, CVariant( int( border.opacityBorder ) ) );
		SetSlot( pRaw, 2, CVariant( float( border.fTextureStep ) ) );
		SetSlot( pRaw, 3, CVariant( border.szTexture ) );
	}
}

}
