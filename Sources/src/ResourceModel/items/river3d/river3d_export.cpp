// The 3D river exporter: C3DRiverFrame::FillRPGStats / SaveRPGStats
// (Sources/src/editor/3dRiverFrm.cpp:107-160), line for line. An
// SVectorStripeObjectDesc is filled from the tree and written as the
// "VSODescription" chunk through the engine's own operator&, so the file is
// what the game's terrain loader reads. The slots are the indices MFC's
// C3DRiverBottomLayerPropsItem and C3DRiverLayerPropsItem getters use.
#include "StdAfx.h"

#include "river3d_export.h"

#include "../stats_export.h"
#include "../../factory.h"
#include "../../mfc_value.h"
#include "../../xml.h"
#include "../tree_item_types.h"
#include "../../../Formats/fmtVSO.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kRiverAddDir[] = "terrain\\sets\\";

// The frame data of an imported river: what the engine's reader reads of a descriptor and the river frame has no item
// for. MFC's frame wrote these as constants, so a river authored elsewhere lost them on its first export; the import
// keeps them in the project's desc element, as the building and bridge frames keep theirs, and the export writes them back.
const char kFrameData[] = "desc";

DWORD AttrDword( const NResourceXml::Node &node, const char *pszName, DWORD dwDefault )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? DWORD( std::strtoul( pValue->c_str(), nullptr, 10 ) ) : dwDefault;
}

float AttrFloat( const NResourceXml::Node &node, const char *pszName, float fDefault )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? float( std::strtod( pValue->c_str(), nullptr ) ) : fDefault;
}

void WriteLayerAttrs( NResourceXml::Node &node, const SVectorStripeObjectDesc::SLayer &layer )
{
	SetAttr( node, "OpacityCenter", MfcInt( layer.opacityCenter ) );
	SetAttr( node, "OpacityBorder", MfcInt( layer.opacityBorder ) );
	SetAttr( node, "StreamSpeed", MfcFloat( layer.fStreamSpeed ) );
	SetAttr( node, "TextureStep", MfcFloat( layer.fTextureStep ) );
	SetAttr( node, "NumCells", MfcInt( layer.nNumCells ) );
	SetAttr( node, "Animated", layer.bAnimated ? "1" : "0" );
	SetAttr( node, "Texture", layer.szTexture );
	SetAttr( node, "Disturbance", MfcFloat( layer.fDisturbance ) );
	SetAttr( node, "RelWidth", MfcFloat( layer.fRelWidth ) );
}

void ReadLayerAttrs( const NResourceXml::Node &node, SVectorStripeObjectDesc::SLayer &layer )
{
	layer.opacityCenter = BYTE( AttrDword( node, "OpacityCenter", layer.opacityCenter ) );
	layer.opacityBorder = BYTE( AttrDword( node, "OpacityBorder", layer.opacityBorder ) );
	layer.fStreamSpeed = AttrFloat( node, "StreamSpeed", layer.fStreamSpeed );
	layer.fTextureStep = AttrFloat( node, "TextureStep", layer.fTextureStep );
	layer.nNumCells = int( AttrDword( node, "NumCells", DWORD( layer.nNumCells ) ) );
	layer.bAnimated = AttrDword( node, "Animated", layer.bAnimated ? 1 : 0 ) != 0;
	if ( const std::string *pTexture = FindAttr( node, "Texture" ) )
		layer.szTexture = *pTexture;
	layer.fDisturbance = AttrFloat( node, "Disturbance", layer.fDisturbance );
	layer.fRelWidth = AttrFloat( node, "RelWidth", layer.fRelWidth );
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

bool FillRiver3DDesc( const CTreeItem &root, SVectorStripeObjectDesc &desc, std::string &szError, const NResourceXml::Node *pProjectElement )
{
	const CTreeItem *pBottom = ChildItem( root, ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM );
	const CTreeItem *pLayers = ChildItem( root, ETIT_3DRIVER_LAYERS_ITEM );
	if ( pBottom == nullptr || pLayers == nullptr )
	{
		szError = std::string( "the river project has no \"" ) + ( pBottom == nullptr ? "Bottom" : "Layers" ) + "\" item";
		return false;
	}

	desc.szAmbientSound = ValueStr( *pBottom, 5 );
	desc.bottom.nNumCells = ValueInt( *pBottom, 0 );
	desc.bottom.bAnimated = false;
	desc.bottom.fDisturbance = 0;
	desc.bottom.fRelWidth = 1.0f;
	desc.bottom.fStreamSpeed = 0;
	desc.bottom.fTextureStep = ValueFloat( *pBottom, 3 );
	desc.bottom.opacityBorder = BYTE( ValueInt( *pBottom, 2 ) );
	desc.bottom.opacityCenter = BYTE( ValueInt( *pBottom, 1 ) );
	desc.bottom.szTexture = ValueStr( *pBottom, 4 );

	desc.layers.clear();
	for ( const auto &pChild : pLayers->GetChildren() )
	{
		SVectorStripeObjectDesc::SLayer layer;
		layer.nNumCells = ValueInt( *pBottom, 0 );
		layer.bAnimated = ValueBool( *pChild, 4 );
		layer.fDisturbance = ValueFloat( *pChild, 6 );
		layer.fRelWidth = 1.0f;
		layer.fStreamSpeed = ValueFloat( *pChild, 2 );
		layer.fTextureStep = ValueFloat( *pChild, 3 );
		layer.opacityBorder = BYTE( ValueInt( *pChild, 1 ) );
		layer.opacityCenter = BYTE( ValueInt( *pChild, 0 ) );
		layer.szTexture = ValueStr( *pChild, 5 );
		desc.layers.push_back( layer );
	}

	const NResourceXml::Node *pFrame = pProjectElement != nullptr ? NResourceXml::FindChild( *pProjectElement, kFrameData ) : nullptr;
	if ( pFrame == nullptr )
		return true;
	// What the frame holds no item for comes from the import's frame data; the tree's own values are the frame's.
	desc.eType = int( AttrDword( *pFrame, "Type", DWORD( desc.eType ) ) );
	desc.nPriority = int( AttrDword( *pFrame, "Priority", DWORD( desc.nPriority ) ) );
	desc.fPassability = AttrFloat( *pFrame, "Passability", desc.fPassability );
	desc.dwAIClasses = AttrDword( *pFrame, "AIClasses", desc.dwAIClasses );
	desc.cSoilParams = BYTE( AttrDword( *pFrame, "SoilParams", desc.cSoilParams ) );
	desc.miniMapCenterColor = SColor( AttrDword( *pFrame, "MiniMapCenterColor", desc.miniMapCenterColor.color ) );
	desc.miniMapBorderColor = SColor( AttrDword( *pFrame, "MiniMapBorderColor", desc.miniMapBorderColor.color ) );
	desc.bottom.fStreamSpeed = AttrFloat( *pFrame, "BottomStreamSpeed", desc.bottom.fStreamSpeed );
	desc.bottom.fDisturbance = AttrFloat( *pFrame, "BottomDisturbance", desc.bottom.fDisturbance );
	desc.bottom.fRelWidth = AttrFloat( *pFrame, "BottomRelWidth", desc.bottom.fRelWidth );
	if ( const NResourceXml::Node *pBorders = NResourceXml::FindChild( *pFrame, "BottomBorders" ) )
		for ( const NResourceXml::Node &item : pBorders->children )
			if ( item.kind == NResourceXml::Node::Element )
			{
				SVectorStripeObjectDesc::SLayer layer;
				ReadLayerAttrs( item, layer );
				desc.bottomBorders.push_back( layer );
			}
	if ( const NResourceXml::Node *pLayers = NResourceXml::FindChild( *pFrame, "Layers" ) )
	{
		std::size_t nLayer = 0;
		for ( const NResourceXml::Node &item : pLayers->children )
			if ( item.kind == NResourceXml::Node::Element && nLayer < desc.layers.size() )
			{
				desc.layers[nLayer].fRelWidth = AttrFloat( item, "RelWidth", desc.layers[nLayer].fRelWidth );
				desc.layers[nLayer].nNumCells = int( AttrDword( item, "NumCells", DWORD( desc.layers[nLayer].nNumCells ) ) );
				++nLayer;
			}
	}
	return true;
}

void WriteRiverFrameData( NResourceXml::Node &root, const SVectorStripeObjectDesc &desc )
{
	NResourceXml::Node frame;
	frame.kind = NResourceXml::Node::Element;
	frame.name = kFrameData;
	SetAttr( frame, "Type", MfcInt( desc.eType ) );
	SetAttr( frame, "Priority", MfcInt( desc.nPriority ) );
	SetAttr( frame, "Passability", MfcFloat( desc.fPassability ) );
	SetAttr( frame, "AIClasses", std::to_string( desc.dwAIClasses ) );
	SetAttr( frame, "SoilParams", MfcInt( desc.cSoilParams ) );
	SetAttr( frame, "MiniMapCenterColor", std::to_string( desc.miniMapCenterColor.color ) );
	SetAttr( frame, "MiniMapBorderColor", std::to_string( desc.miniMapBorderColor.color ) );
	SetAttr( frame, "BottomStreamSpeed", MfcFloat( desc.bottom.fStreamSpeed ) );
	SetAttr( frame, "BottomDisturbance", MfcFloat( desc.bottom.fDisturbance ) );
	SetAttr( frame, "BottomRelWidth", MfcFloat( desc.bottom.fRelWidth ) );
	NResourceXml::Node borders;
	borders.kind = NResourceXml::Node::Element;
	borders.name = "BottomBorders";
	for ( const SVectorStripeObjectDesc::SLayer &layer : desc.bottomBorders )
	{
		NResourceXml::Node item;
		item.kind = NResourceXml::Node::Element;
		item.name = "item";
		WriteLayerAttrs( item, layer );
		borders.children.push_back( std::move( item ) );
	}
	frame.children.push_back( std::move( borders ) );
	NResourceXml::Node layers;
	layers.kind = NResourceXml::Node::Element;
	layers.name = "Layers";
	for ( const SVectorStripeObjectDesc::SLayer &layer : desc.layers )
	{
		NResourceXml::Node item;
		item.kind = NResourceXml::Node::Element;
		item.name = "item";
		SetAttr( item, "RelWidth", MfcFloat( layer.fRelWidth ) );
		SetAttr( item, "NumCells", MfcInt( layer.nNumCells ) );
		layers.children.push_back( std::move( item ) );
	}
	frame.children.push_back( std::move( layers ) );
	for ( NResourceXml::Node &child : root.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == kFrameData )
		{
			child = std::move( frame );
			return;
		}
	root.children.push_back( std::move( frame ) );
}

bool ExportRiver3D( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_3DRIVER_ROOT_ITEM, "river", outcome );
	if ( !pProject )
		return false;
	SVectorStripeObjectDesc desc;
	if ( !FillRiver3DDesc( *pProject->root, desc, outcome.szError, &pProject->document.root ) )
		return false;

	const std::string szFile = StatsFileName( project, context, kRiverAddDir, false );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "VSODescription", &desc );
	}, outcome ) )
		return false;
	// The river the preview draws: the file without its extension.
	std::string szObject = szFile;
	const std::string::size_type nDot = szObject.find_last_of( '.' );
	if ( nDot != std::string::npos && nDot > szObject.find_last_of( '\\' ) + 1 )
		szObject.resize( nDot );
	outcome.szObjectName = szObject;
	return true;
}

void River3DStatsToTree( const SVectorStripeObjectDesc &desc, CTreeItem &root )
{
	CTreeItem *pBottom = MutableChild( root, ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM );
	CTreeItem *pLayers = MutableChild( root, ETIT_3DRIVER_LAYERS_ITEM );
	SetSlot( pBottom, 0, CVariant( int( desc.bottom.nNumCells ) ) );
	SetSlot( pBottom, 1, CVariant( int( desc.bottom.opacityCenter ) ) );
	SetSlot( pBottom, 2, CVariant( int( desc.bottom.opacityBorder ) ) );
	SetSlot( pBottom, 3, CVariant( float( desc.bottom.fTextureStep ) ) );
	SetSlot( pBottom, 4, CVariant( desc.bottom.szTexture ) );
	SetSlot( pBottom, 5, CVariant( desc.szAmbientSound ) );
	if ( pLayers == nullptr )
		return;
	for ( const SVectorStripeObjectDesc::SLayer &layer : desc.layers )
	{
		auto pLayer = CTreeItemFactory::Instance().Create( ETIT_3DRIVER_LAYER_PROPS_ITEM );
		pLayer->SetItemName( "Layer" );
		CTreeItem *pRaw = pLayer.get();
		pLayers->AddChild( std::move( pLayer ) );
		SetSlot( pRaw, 0, CVariant( int( layer.opacityCenter ) ) );
		SetSlot( pRaw, 1, CVariant( int( layer.opacityBorder ) ) );
		SetSlot( pRaw, 2, CVariant( float( layer.fStreamSpeed ) ) );
		SetSlot( pRaw, 3, CVariant( float( layer.fTextureStep ) ) );
		SetSlot( pRaw, 4, CVariant( layer.bAnimated ) );
		SetSlot( pRaw, 5, CVariant( layer.szTexture ) );
		SetSlot( pRaw, 6, CVariant( float( layer.fDisturbance ) ) );
	}
}

}
