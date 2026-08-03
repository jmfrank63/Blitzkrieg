#include "StdAfx.h"
#include "../Misc/fileutils.h"
#include "GUITreeItem.h"
#include "RefDlg.h"
#include "editor.h"
#include "frames.h"
#include "GUIFrame.h"

void CGUITreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_GUI_MOUSE_SELECT_ITEM;
	child.szDefaultName = "Mouse select";
	child.szDisplayName = "Mouse select";
	defaultChilds.push_back( child );

	child.nChildItemType = E_STATICS_TREE_ITEM;
	child.szDefaultName = "Statics";
	child.szDisplayName = "Statics";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUTTONS_TREE_ITEM;
	child.szDefaultName = "Buttons";
	child.szDisplayName = "Buttons";
	defaultChilds.push_back( child );

	child.nChildItemType = E_SLIDERS_TREE_ITEM;
	child.szDefaultName = "Sliders";
	child.szDisplayName = "Sliders";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_SCROLLBARS_TREE_ITEM;
	child.szDefaultName = "Scrollbars";
	child.szDisplayName = "Scrollbars";
	defaultChilds.push_back( child );

	child.nChildItemType = E_STATUSBARS_TREE_ITEM;
	child.szDefaultName = "Statusbars";
	child.szDisplayName = "Statusbars";
	defaultChilds.push_back( child );

	child.nChildItemType = E_LISTS_TREE_ITEM;
	child.szDefaultName = "Lists";
	child.szDisplayName = "Lists";
	defaultChilds.push_back( child );

	child.nChildItemType = E_DIALOGS_TREE_ITEM;
	child.szDefaultName = "Dialogs";
	child.szDisplayName = "Dialogs";
	defaultChilds.push_back( child );
}

void CGUIMouseSelectItem::MyLButtonClick()
{
	CGUIFrame *pFrame = static_cast<CGUIFrame *> ( g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME ) );
	pFrame->SetActiveTemplatePropsItem( 0 );
}

int GetTreeItemTypeByWindowType( int nWindowType )
{
	switch ( nWindowType )
	{
	case UI_STATIC:
		return E_STATIC_PROPS_TREE_ITEM;
	case UI_BUTTON:
		return E_BUTTON_PROPS_TREE_ITEM;
	case UI_SLIDER:
		return E_SLIDER_PROPS_TREE_ITEM;
	case UI_SCROLLBAR:
		return E_SCROLLBAR_PROPS_TREE_ITEM;
	case UI_STATUS_BAR:
		return E_STATUSBAR_PROPS_TREE_ITEM;
	case UI_LIST:
		return E_LIST_PROPS_TREE_ITEM;
	case UI_DIALOG:
		return E_DIALOG_PROPS_TREE_ITEM;
	default:
		NI_ASSERT_T( 0, "Unknown window type, cannot convert it to TemplateTreeItem type" );
		return -1;
	}
}

int CTemplatesTreeItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	return 0;

/*
	CTreeAccessor saver = pFile;
	if ( saver.IsReading() )
	{
		if ( GetParentTreeItem()->GetItemType() != E_GUI_ROOT_ITEM )
			return;

		string szMask = "*.xml";
		vector<string> files;
		string szFullDirectory = theApp.GetEditorDataDir();
		szFullDirectory += "editor\\UI\\";
		szFullDirectory += szDirectory;
		
		NFile::EnumerateFiles( szFullDirectory.c_str(), szMask.c_str(), CGetAllFiles( &files ), false );
		for ( int i=0; i<files.size(); i++ )
		{
			string szName = files[i];
			if ( szName == "1.xml" )
				continue;
		}
	}
*/
}

void CTemplatesTreeItem::InsertChildItems()
{
	string szMask = "*.xml";
	vector<string> files;
	string szFullDirectory = theApp.GetEditorDataDir();
	szFullDirectory += "editor\\UI\\";
	szFullDirectory += szDirectory;
	
	NFile::EnumerateFiles( szFullDirectory.c_str(), szMask.c_str(), NFile::CGetAllFiles( &files ), false );
	IObjectFactory *pFactory = GetCommonFactory();
	for ( int i=0; i<files.size(); i++ )
	{
		string szName = files[i];
/*
		if ( szName == "1.xml" )
			continue;
*/
		CPtr<IDataStream> pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_READ );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
		CTreeAccessor saver = pDT;
		int nClassTypeID = -1;
		saver.Add( "ClassTypeID", &nClassTypeID );
		NI_ASSERT_T( nClassTypeID == nWindowType, NStr::Format( "Invalid %s XML file: %s", szDirectory.c_str(), szName.c_str() ) );
		
		int nTreeItemType = GetTreeItemTypeByWindowType( nClassTypeID );
		if ( nTreeItemType == -1 )				//ERROR
			continue;
		
		CTemplatePropsTreeItem *pTemplatePropsItem = ( CTemplatePropsTreeItem *) pFactory->CreateObject( nTreeItemType );
		pTemplatePropsItem->SetWindowType( nWindowType );
		pTemplatePropsItem->SetXMLFile( szName.c_str() );
		string szShortName = szName.substr( szName.rfind( '\\' ) + 1 );
		szShortName = szShortName.substr( 0, szShortName.rfind( '.' ) );
		pTemplatePropsItem->SetItemName( szShortName.c_str() );
		AddChild( pTemplatePropsItem );
	}
}

int CTemplatePropsTreeItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	return 0;
}

void CTemplatePropsTreeItem::MyLButtonClick()
{
	CGUIFrame *pFrame = static_cast<CGUIFrame *> ( g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME ) );
	pFrame->SetActiveTemplatePropsItem( this );
}

void CTemplatePropsTreeItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			if ( szDisplayName == "Default" || szDisplayName == "default" )
			{
				AfxMessageBox( "The Default template item can not be deleted" );
				return;
			}

			int nRes = AfxMessageBox( "Do you want to delete template item?", MB_YESNO );
			if ( nRes == IDYES )
			{
				remove( szXMLFile.c_str() );
				DeleteMeInParentTreeItem();
			}
			break;
	}
}
