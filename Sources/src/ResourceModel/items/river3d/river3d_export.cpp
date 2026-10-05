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
#include "../tree_item_types.h"
#include "../../../Formats/fmtVSO.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kRiverAddDir[] = "terrain\\sets\\";

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

bool FillRiver3DDesc( const CTreeItem &root, SVectorStripeObjectDesc &desc, std::string &szError )
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
	return true;
}

bool ExportRiver3D( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_3DRIVER_ROOT_ITEM, "river", outcome );
	if ( !pProject )
		return false;
	SVectorStripeObjectDesc desc;
	if ( !FillRiver3DDesc( *pProject->root, desc, outcome.szError ) )
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
