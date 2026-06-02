#include "StdAfx.h"

#include "..\scene\scene.h"
#include "..\common\world.h"
#include "editor.h"
#include "frames.h"
#include "CampaignFrm.h"
#include "CampaignTreeItem.h"
#include "UnitSide.h"

void CCampaignTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_CAMPAIGN_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CAMPAIGN_CHAPTERS_ITEM;
	child.szDefaultName = "Chapters";
	child.szDisplayName = "Chapters";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CAMPAIGN_TEMPLATES_ITEM;
	child.szDefaultName = "Templates";
	child.szDisplayName = "Templates";
	defaultChilds.push_back( child );
}

void CCampaignCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Header text";
	prop.szDisplayName = "Header text";
	prop.value = "header";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "SubHeader text";
	prop.szDisplayName = "SubHeader text";
	prop.value = "subheader";
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Map image";
	prop.szDisplayName = "Map image";
	prop.value = "map";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 4;
	prop.nDomenType = DT_MOVIE_REF;
	prop.szDefaultName = "Intro movie";
	prop.szDisplayName = "Intro movie";
	prop.value = "";
	defaultValues.push_back( prop );
	
	prop.nId = 5;
	prop.nDomenType = DT_MOVIE_REF;
	prop.szDefaultName = "Outro movie";
	prop.szDisplayName = "Outro movie";
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_MUSIC_REF;
	prop.szDefaultName = "Interface music";
	prop.szDisplayName = "Interface music";
	prop.value = "";
	defaultValues.push_back( prop );
	
	prop.nId = 7;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Player side";
	prop.szDisplayName = "Player side";
	prop.value = "German";
	FillVectorOfSides( prop.szStrings );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CCampaignCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 || nItemId == 3 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: file should be inside campaign editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
			}
		}
		
		if ( nItemId == 3 )
		{
			CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
			pFrame->LoadImageTexture( GetMapImage() );
		}
	}
}

void CCampaignCommonPropsItem::MyLButtonClick()
{
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->SetActiveChapter( 0 );
}

void CCampaignChaptersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CCampaignChaptersItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CCampaignChapterPropsItem;
			pItem->SetItemName( "Chapter" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCampaignChaptersItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CCampaignChapterPropsItem;
		pItem->SetItemName( "Chapter" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
	}
}

void CCampaignChaptersItem::MyLButtonClick()
{
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->SetActiveChapter( 0 );
}

void CCampaignChapterPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_CHAPTER_REF;
	prop.szDefaultName = "Chapter";
	prop.szDisplayName = "Chapter";
	prop.value = "";
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Chapter position X";
	prop.szDisplayName = "Chapter position X";
	prop.value = 0;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Chapter position Y";
	prop.szDisplayName = "Chapter position Y";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is this chapter visible?";
	prop.szDisplayName = "Is this chapter visible?";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Chapter secret flag";
	prop.szDisplayName = "Chapter secret flag";
	prop.value = false;
	defaultValues.push_back( prop );
}

void CCampaignChapterPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCampaignChapterPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
	}
}

void CCampaignChapterPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		std::string szValue = value;
		std::string szPrefix = "scenarios\\chapters\\";
		if ( strncmp( szValue.c_str(), szPrefix.c_str(), szPrefix.size() ) )
		{
			szValue = szPrefix + szValue;
			CTreeItem::UpdateItemValue( nItemId, szValue );
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
		}
	}
	
	if ( nItemId == 2 || nItemId == 3 )
	{
		CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
		pFrame->SetActiveChapter( this );
	}
}

void CCampaignChapterPropsItem::MyLButtonClick()
{
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->SetActiveChapter( this );
}



void CCampaignTemplatesItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Templates directory";
	prop.szDisplayName = "Templates directory";
	prop.value = "";
	std::string szDir = theApp.GetEditorDataDir();
	szDir += "Scenarios\\TemplateMissions\\";
	prop.szStrings.push_back( szDir.c_str() );
	defaultValues.push_back( prop );
	
	values = defaultValues;
	
	defaultChilds.clear();
}

void CCampaignTemplatesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CCampaignTemplatePropsItem;
			pItem->SetItemName( "Template" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCampaignTemplatesItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CCampaignTemplatePropsItem;
		pItem->SetItemName( "Template" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
	}
}

void CCampaignTemplatesItem::MyLButtonClick()
{
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->SetActiveChapter( 0 );
}

void CCampaignTemplatesItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		std::string szVal = value;
		string szMask = "*.xml";
		vector<string> files;
		std::string szBaseDir = theApp.GetEditorDataDir();
		szBaseDir += "Scenarios\\TemplateMissions\\";
		
		std::string szShortDirName;
		bool bRes = MakeSubRelativePath( szBaseDir.c_str(), szVal.c_str(), szShortDirName );
		if ( !bRes )
		{
			AfxMessageBox( "Error: The directory with Template Missions should be inside Data\\Scenarios\\TemplateMissions\\ directory of the game" );
			return;
		}
		
		if ( GetChildsCount() > 0 )
		{
			int nRes = AfxMessageBox( "The are already some Template Missions added, do you want to remove them first?", MB_YESNOCANCEL );
			if ( nRes == IDCANCEL )
				return;
			if ( nRes == IDYES )
				RemoveAllChilds();
		}
		
		CVariant newVal = szShortDirName;
		CTreeItem::UpdateItemValue( nItemId, newVal );
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
		
		NFile::EnumerateFiles( szVal.c_str(), szMask.c_str(), NFile::CGetAllFilesRelative( szBaseDir.c_str(), &files ), true );
		for ( int i=0; i<files.size(); i++ )
		{
			string szName = files[i];
			szName = szName.substr( 0, szName.rfind( '.' ) );
			NI_ASSERT( szName.size() > 0 );

			CCampaignTemplatePropsItem *pProps = new CCampaignTemplatePropsItem;
			pProps->SetItemName( szName.c_str() );
			szName = std::string("Scenarios\\TemplateMissions\\") + szName;
			pProps->SetTemplateName( szName.c_str() );
			AddChild( pProps );
		}
	}
}

void CCampaignTemplatePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_TEMPLATE_MISSION_REF;
	prop.szDefaultName = "Template";
	prop.szDisplayName = "Template";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
}

void CCampaignTemplatePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCampaignTemplatePropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->SetChangedFlag( true );
	}
}

void CCampaignTemplatePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		std::string szValue = value;
		std::string szPrefix = "scenarios\\templatemissions\\";
		if ( strncmp( szValue.c_str(), szPrefix.c_str(), szPrefix.size() ) )
		{
			ChangeItemName( szValue.c_str() );

			szValue = szPrefix + szValue;
			CTreeItem::UpdateItemValue( nItemId, szValue );
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
		}
		else
		{
			szValue = szValue.c_str() + szPrefix.size();
			ChangeItemName( szValue.c_str() );
			g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->UpdatePropView( this );
		}
	}
}

void CCampaignTemplatePropsItem::MyLButtonClick()
{
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->SetActiveChapter( 0 );
}
