#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "3DRoadFrm.h"
#include "3DRoadTreeItem.h"
#include "common.h"

void C3DRoadTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_3DROAD_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_3DROAD_LAYER_PROPS_ITEM;
	child.szDefaultName = "Central layer";
	child.szDisplayName = "Central layer";
	defaultChilds.push_back( child );
}

void C3DRoadCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Bottom width";
	prop.szDisplayName = "Bottom width";
	prop.value = 4;
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Has borders?";
	prop.szDisplayName = "Has borders?";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative border width";
	prop.szDisplayName = "Relative border width";
	prop.value = 0.1f;
	defaultValues.push_back( prop );
	
	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Visual priority";
	prop.szDisplayName = "Visual priority";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability coefficient";
	prop.szDisplayName = "Passability coefficient";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Road type";
	prop.szDisplayName = "Road type";
	prop.value = "road";
	prop.szStrings.push_back( "road" );
	prop.szStrings.push_back( "railroad" );
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_HEX;
	prop.szDefaultName = "Minimap center color";
	prop.szDisplayName = "Minimap center color";
	prop.value = (int)0xff808080;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_HEX;
	prop.szDefaultName = "Minimap border color";
	prop.szDisplayName = "Minimap border color";
	prop.value = (int)0xff808080;
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Has dust effect";
	prop.szDisplayName = "Has dust effect";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 14;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Units leave tracks";
	prop.szDisplayName = "Units leave tracks";
	prop.value = false;
	defaultValues.push_back( prop );

	values = defaultValues;
}

int C3DRoadCommonPropsItem::GetRoadType()
{
	std::string szVal = values[9].value;
	if ( szVal == "road" || szVal == "Road" )
		return SVectorStripeObjectDesc::TYPE_ROAD;
	if ( szVal == "railroad" || szVal == "RailRoad" )
		return SVectorStripeObjectDesc::TYPE_RAILROAD;
	NI_ASSERT_T( 0, "Unknown road type" );
	return SVectorStripeObjectDesc::TYPE_ROAD;
}

void C3DRoadCommonPropsItem::SetRoadType( int nVal )
{
	std::string szVal;
	switch( nVal )
	{
		case SVectorStripeObjectDesc::TYPE_ROAD:
			szVal = "road";
			break;
		case SVectorStripeObjectDesc::TYPE_RAILROAD:
			szVal = "railroad";
			break;
		default:
			NI_ASSERT_T( 0, "Unknown road type" );
	}

	values[9].value = szVal;
}

void C3DRoadCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	C3DRoadFrame *pFrame = static_cast<C3DRoadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DROAD_FRAME ) );
	bool bOldBordersFlag = HasBorders();
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 2 )
	{
		bool bVal = HasBorders();
		if ( bVal == true && bOldBordersFlag == false )
		{
			C3DRoadLayerPropsItem *pLayerProps = new C3DRoadLayerPropsItem;
			pLayerProps->SetItemName( "Border layer" );
			GetParentTreeItem()->AddChild( pLayerProps );
		}
		
		if ( bVal == false && bOldBordersFlag == true )
		{
			CTreeItem *pLayer = GetParentTreeItem()->GetChildItem( E_3DROAD_LAYER_PROPS_ITEM, 1 );
			NI_ASSERT( pLayer != 0 );
			pLayer->DeleteMeInParentTreeItem();
		}
	}
	
	if ( nItemId == 3 )
	{
		float fVal = GetBorderRelativeWidth();
		bool bMsg = true;
		if ( fVal > 1.0f )
			fVal = 1.0f;
		else if ( fVal < 0 )
			fVal = 0;
		else bMsg = false;
		
		if ( bMsg )
		{
			AfxMessageBox( "The relative width was truncated to [0..1]" );
			CVariant newVal = fVal;
			CTreeItem::UpdateItemValue( nItemId, newVal );
			pFrame->UpdatePropView( this );
		}
	}

	pFrame->UpdateRoadView();
}

void C3DRoadLayerPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Center opacity";
	prop.szDisplayName = "Center opacity";
	prop.value = 255;
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Border opacity";
	prop.szDisplayName = "Border opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Texture step";
	prop.szDisplayName = "Texture step";
	prop.value = 0.1f;
	defaultValues.push_back( prop );
/*
	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative width";
	prop.szDisplayName = "Relative width";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
*/
	prop.nId = 5;
	prop.nDomenType = DT_ROAD_TEXTURE_REF;
	prop.szDefaultName = "Texture";
	prop.szDisplayName = "Texture";
	prop.value = "terrain\\sets\\1\\roads3d\\road_asphalt01";
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void C3DRoadLayerPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	C3DRoadFrame *pFrame = static_cast<C3DRoadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DROAD_FRAME ) );
	
	pFrame->UpdateRoadView();
}
BYTE C3DRoadCommonPropsItem::GetSoilParams()
{
	BYTE res = 0x00;
	if ( values[12].value )
		res = res | SVectorStripeObjectDesc::ESP_DUST;
	if ( values[13].value )
		res = res | SVectorStripeObjectDesc::ESP_TRACE;
	return res;
}

