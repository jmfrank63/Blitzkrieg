#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "SpriteCompose.h"
#include "frames.h"
#include "BridgeFrm.h"
#include "BridgeTreeItem.h"
#include "common.h"

void CBridgeTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_BRIDGE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_DEFENCES_ITEM;
	child.szDefaultName = "Defence";
	child.szDisplayName = "Defence";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_STAGE_PROPS_ITEM;
	child.szDefaultName = "Destroyed";
	child.szDisplayName = "Destroyed";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_FIRE_POINTS_ITEM;
	child.szDefaultName = "Fire points";
	child.szDisplayName = "Fire points";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_SMOKES_ITEM;
	child.szDefaultName = "Smoke points";
	child.szDisplayName = "Smoke points";
	defaultChilds.push_back( child );
}

bool CBridgeTreeRootItem::SaveShadowFile( const string &szBridgeFileName, const string &szShadowFileName, const string &szTempShadow )
{
	IImageProcessor *pIP = GetImageProcessor();

	CPtr<IDataStream> pBridgeStream = OpenFileStream( szBridgeFileName.c_str(), STREAM_ACCESS_READ );
	if ( pBridgeStream == 0 )
		return false;
	CPtr<IImage> pSpriteImage = pIP->LoadImage( pBridgeStream );
	if ( pSpriteImage == 0 )
		return false;
	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 1 );
	pInverseSprite->InvertAlpha();

	CPtr<IDataStream> pShadowStream = OpenFileStream( szShadowFileName.c_str(), STREAM_ACCESS_READ );
	if ( pShadowStream == 0 )
		return false;
	CPtr<IImage> pShadowImage = pIP->LoadImage( pShadowStream );
	if ( pShadowImage == 0 )
		return false;
	if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
	{
		string szErr = "The size of sprite is not equal the size of shadow: ";
		szErr += szBridgeFileName;
		szErr += ",  ";
		szErr += szShadowFileName;

		NI_ASSERT_T( 0, szErr.c_str() );
		return false;
	}
	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = pInverseSprite->GetSizeX();
	rc.bottom = pInverseSprite->GetSizeY();
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD(0) );

	CPtr<IDataStream> pSaveShadowStream = OpenFileStream( szTempShadow.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsTGA( pSaveShadowStream, pShadowImage );

	return true;
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

void CBridgeCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 2 )
	{
		CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
		pFrame->SetBridgeType( GetDirection() );
	}
}

bool CBridgeCommonPropsItem::GetDirection()
{
	string szTemp = values[1].value;
	if ( szTemp == "horizontal" || szTemp == "Horizontal" )
		return true;
	else
		return false;
}

void CBridgeDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;
			
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_DEFENCE_PROPS_ITEM;
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
	
	child.nChildItemType = E_BRIDGE_BEGIN_SPANS_ITEM;
	child.szDefaultName = "Begin spans";
	child.szDisplayName = "Begin spans";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_CENTER_SPANS_ITEM;
	child.szDefaultName = "Center spans";
	child.szDisplayName = "Center spans";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_END_SPANS_ITEM;
	child.szDefaultName = "End spans";
	child.szDisplayName = "End spans";
	defaultChilds.push_back( child );
}

int CBridgeStagePropsItem::GetActiveStage()
{
	CTreeItem *pRoot = GetParentTreeItem();
	NI_ASSERT( pRoot != 0 );
	CTreeItem *pCompare = pRoot->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, 0 );
	if ( pCompare == this )
		return E_WHOLE;
	
	pCompare = pRoot->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, 1 );
	if ( pCompare == this )
		return E_DAMAGED;
	
	pCompare = pRoot->GetChildItem( E_BRIDGE_STAGE_PROPS_ITEM, 2 );
	if ( pCompare == this )
		return E_DESTROYED;
	
	NI_ASSERT( 0 );
	return 0;
}

int CBridgeCommonSpansItem::GetActiveStage()
{
	CBridgeStagePropsItem *pStage = static_cast<CBridgeStagePropsItem *> ( GetParentTreeItem() );				//stage
	NI_ASSERT( pStage != 0 );
	return pStage->GetActiveStage();
}

void CBridgeCommonSpansItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CBridgePartsItem *pItem = new CBridgePartsItem;
			string szName = "Span parts";
			pItem->SetItemName( szName.c_str() );
			AddChild( pItem );
			CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
			pFrame->SetChangedFlag( true );
			pItem->nSpanIndex = pFrame->GetFreeBridgeIndex( GetActiveStage() );
			break;
	}
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
	
	child.nChildItemType = E_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Back girder";
	child.szDisplayName = "Back girder";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Front girder";
	child.szDisplayName = "Front girder";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BRIDGE_PART_PROPS_ITEM;
	child.szDefaultName = "Slab";
	child.szDisplayName = "Slab";
	defaultChilds.push_back( child );
}

void CBridgePartsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
			pFrame->RemoveBridgeIndex( GetActiveStage(), nSpanIndex );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CBridgePartsItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SetActivePartsItem( this, pFrame->GetProjectFileName().c_str() );
}

int CBridgePartsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "SpanIndex", &nSpanIndex );
	
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	if ( !saver.IsReading() )
	{
		pFrame->SaveMyData( this, saver );
	}
	else
	{
		pFrame->LoadMyData( this, saver );
	}
	
	return 0;
}

int CBridgePartsItem::GetActiveStage()
{
	CTreeItem *pPapa = GetParentTreeItem();		//begins or spans or ends
	NI_ASSERT( pPapa != 0 );
	CBridgeStagePropsItem *pStage = static_cast<CBridgeStagePropsItem *> ( pPapa->GetParentTreeItem() );				//stage
	NI_ASSERT( pStage != 0 );
	return pStage->GetActiveStage();
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

void CBridgePartPropsItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SetActivePartPropsItem( this );
}

void CBridgePartPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
		string szFull = value;
		string szProjectName = pFrame->GetProjectFileName();
		if ( !IsRelatedPath(szFull.c_str()) )
		{
			string szRelatedPath;
			bool bRes = MakeSubRelativePath( szProjectName.c_str(), szFull.c_str(), szRelatedPath );
			if ( bRes == false )
			{
				AfxMessageBox( "Error: The picture must be inside directory of project" );
				szRelatedPath = "";
			}
			
			CVariant newVal = szRelatedPath;
			CTreeItem::UpdateItemValue( nItemId, newVal );
			pFrame->UpdatePropView( this );
		}
		
		pFrame->UpdatePartPropsItem();
		return;
	}
}

void CBridgeFirePointsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBridgeFirePointsItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SetActiveMode( CBridgeFrame::E_FIRE_POINT );
	pFrame->GFXDraw();
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

void CBridgeFirePointPropsItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SelectFirePoint( this );
	pFrame->SetActiveMode( CBridgeFrame::E_FIRE_POINT );
	pFrame->GFXDraw();
}

void CBridgeFirePointPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
			pFrame->DeleteFirePoint( this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CBridgeFirePointPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
		CTreeItem *pTemp = pFrame->GetActiveFirePointItem();
		if ( pTemp != this )
		{
			pFrame->SelectFirePoint( this );
			pFrame->SetFireDirection( value );
			pFrame->SelectFirePoint( pTemp );
		}
		else
			pFrame->SetFireDirection( value );
		return;
	}
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

	child.nChildItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front left";
	child.szDisplayName = "Front left";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front right";
	child.szDisplayName = "Front right";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back right";
	child.szDisplayName = "Back right";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back left";
	child.szDisplayName = "Back left";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Top center";
	child.szDisplayName = "Top center";
	defaultChilds.push_back( child );
}

void CBridgeDirExplosionsItem::MyLButtonClick()
{
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

void CBridgeDirExplosionPropsItem::MyLButtonClick()
{
}

void CBridgeDirExplosionPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
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

void CBridgeSmokesItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SetActiveMode( CBridgeFrame::E_SMOKE_POINT );
	pFrame->GFXDraw();
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

void CBridgeSmokePropsItem::MyLButtonClick()
{
	CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
	pFrame->SelectSmokePoint( this );
	pFrame->SetActiveMode( CBridgeFrame::E_SMOKE_POINT );
	pFrame->GFXDraw();
}

void CBridgeSmokePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
		pFrame->ComputeSmokeLines();
		return;
	}
}

void CBridgeSmokePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_DELETE:
		CBridgeFrame *pFrame = static_cast<CBridgeFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BRIDGE_FRAME ) );
		pFrame->DeleteSmokePoint();
		DeleteMeInParentTreeItem();
		break;
	}
}
