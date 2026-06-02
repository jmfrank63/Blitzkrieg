#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "3DRiverFrm.h"
#include "3DRiverTreeItem.h"
#include "common.h"


void C3DRiverTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_3DRIVER_BOTTOM_LAYER_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );

	child.nChildItemType = E_3DRIVER_LAYERS_ITEM;
	child.szDefaultName = "Layers";
	child.szDisplayName = "Layers";
	defaultChilds.push_back( child );
}

void C3DRiverBottomLayerPropsItem::InitDefaultValues()
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
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Center opacity";
	prop.szDisplayName = "Center opacity";
	prop.value = 255;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Border opacity";
	prop.szDisplayName = "Border opacity";
	prop.value = 255;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Texture step";
	prop.szDisplayName = "Texture step";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_WATER_TEXTURE_REF;
	prop.szDefaultName = "Texture";
	prop.szDisplayName = "Texture";
	prop.value = "water\\bottom";
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void C3DRiverBottomLayerPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	C3DRiverFrame *pFrame = static_cast<C3DRiverFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME ) );
	
	pFrame->UpdateRiverView();
}

void C3DRiverLayersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void C3DRiverLayersItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new C3DRiverLayerPropsItem;
			string szName = "Layer";
			pItem->SetItemName( szName.c_str() );
			AddChild( pItem );
			C3DRiverFrame *pFrame = static_cast<C3DRiverFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME ) );
			pFrame->UpdateRiverView();
			pFrame->SetChangedFlag( true );
			break;
	}
}

void C3DRiverLayersItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new C3DRiverLayerPropsItem;
		string szName = "Layer";
		pItem->SetItemName( szName.c_str() );
		AddChild( pItem );
		C3DRiverFrame *pFrame = static_cast<C3DRiverFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME ) );
		pFrame->UpdateRiverView();
		pFrame->SetChangedFlag( true );
	}
}

void C3DRiverLayerPropsItem::InitDefaultValues()
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
	prop.szDefaultName = "Stream speed";
	prop.szDisplayName = "Stream speed";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Texture step";
	prop.szDisplayName = "Texture step";
	prop.value = 0.1f;
	defaultValues.push_back( prop );
	
	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Animated flag";
	prop.szDisplayName = "Animated flag";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_WATER_TEXTURE_REF;
	prop.szDefaultName = "Texture";
	prop.szDisplayName = "Texture";
	prop.value = "water\\water";
	defaultValues.push_back( prop );
	
	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Disturbance";
	prop.szDisplayName = "Disturbance";
	prop.value = 0.3f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void C3DRiverLayerPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	C3DRiverFrame *pFrame = static_cast<C3DRiverFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME ) );
	
	if ( nItemId == 5 )
	{
		SProp &prop = values[5];
		prop.szStrings.clear();
		bool bAnimated = GetAnimatedFlag();
		if ( bAnimated )
		{
			prop.nDomenType = DT_BROWSEDIR;
			prop.szStrings.push_back( theApp.GetDestDir() );
			prop.value = "water\\blick\\";
		}
		else
		{
			prop.nDomenType = DT_WATER_TEXTURE_REF;
			prop.value = "water\\water";
		}
		pFrame->UpdatePropView( this );
	}

	if ( nItemId == 6 )
	{
		bool bAnimated = GetAnimatedFlag();
		if ( bAnimated )
		{
			if ( !IsRelatedPath( value ) )
			{
				string szDataDir = theApp.GetDestDir();
				string szValue = value;
				string szRelatedPath;
				bool bRes = MakeRelativePath( szDataDir.c_str(), szValue.c_str(), szRelatedPath );
				if ( bRes )
				{
					CVariant newVal = szRelatedPath;
					CTreeItem::UpdateItemValue( nItemId, newVal );
					pFrame->UpdatePropView( this );
				}
				else
				{
					std::string szErr;
					szErr = "Error, the animation texture directory must be inside MOD directory";
					AfxMessageBox( szErr.c_str() );
					CVariant newVal = "";
					CTreeItem::UpdateItemValue( nItemId, newVal );
					pFrame->UpdatePropView( this );
				}
			}
		}
	}
	
	pFrame->UpdateRiverView();
}

void C3DRiverLayerPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->SetChangedFlag( true );
			break;
	}
}

void C3DRiverLayerPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME )->SetChangedFlag( true );
	}
}
