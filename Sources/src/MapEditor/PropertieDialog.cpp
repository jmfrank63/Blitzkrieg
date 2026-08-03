#include "StdAfx.h"
#include "editor.h"
#include "PropertieDialog.h"
#include "frames.h"

#include "../Misc/Manipulator.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPropertieDialog::CPropertieDialog( CWnd* pParent )
	: CResizeDialog( CPropertieDialog::IDD, pParent ), m_pCurrentObject( 0 ) 
{
	m_checkButton.SetBitmapIDs( IDB_PUSHPIN );

}

void CPropertieDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX); 
	DDX_Control(pDX, IDC_PIN_BUTTON, m_checkButton);
}

BEGIN_MESSAGE_MAP(CPropertieDialog, CResizeDialog)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_PIN_BUTTON, OnPinButton)
END_MESSAGE_MAP()

const int nButtonShift = 25;

const int   VALUE_COLUMN_COUNT = 2;
const char *VALUE_COLUMN_NAME  [VALUE_COLUMN_COUNT] = { "Name", "Value" };
int					VALUE_COLUMN_WIDTH [VALUE_COLUMN_COUNT] = { 220, 250 };

BOOL CPropertieDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	if ( resizeDialogOptions.nParameters.size() < 3 )
	{
		resizeDialogOptions.nParameters.resize( VALUE_COLUMN_COUNT + 1, 0 );
		resizeDialogOptions.nParameters[0] = 1;
		resizeDialogOptions.nParameters[1] = VALUE_COLUMN_WIDTH[0];
		resizeDialogOptions.nParameters[2] = VALUE_COLUMN_WIDTH[1];
	}

	RECT r;
	GetClientRect( &r );
	r.left	+= nButtonShift;
	m_tree.Create( NULL, "MultiTreeCtrl", WS_CHILD | WS_VISIBLE, r, this, 0 );
	m_tree.ModifyStyleEx( 0 , WS_EX_CLIENTEDGE );

	HD_ITEM hdi;
	hdi.mask = HDI_TEXT | HDI_FORMAT;
	hdi.mask |= HDI_WIDTH;
	hdi.cxy = resizeDialogOptions.nParameters[1];
	hdi.fmt = HDF_STRING | HDF_CENTER;
	hdi.pszText = const_cast<char *>( VALUE_COLUMN_NAME[0] );
	int m_nReturn = m_tree.m_tree.m_wndHeader.InsertItem( 0, &hdi );

	hdi.cxy = resizeDialogOptions.nParameters[2];
	hdi.pszText = const_cast<char *>( VALUE_COLUMN_NAME[1] );
	m_nReturn = m_tree.m_tree.m_wndHeader.InsertItem( 1, &hdi );

	m_checkButton.SetPinned( resizeDialogOptions.nParameters[0] > 0 );

	if( m_tree.ifInit )
	{
		GetClientRect( &r );
		r.left += nButtonShift + 2;
		r.right -= 7;
		r.top += 2;
		r.bottom -= 7;
		m_tree.MoveWindow( &r );
	}

	return TRUE;
}

void CPropertieDialog::AddRootVariable( std::string &str, int variable)
{
	if( str == "Player" )
	{
		HTREEITEM hPA = m_tree.m_tree.InsertItemEx( str.c_str(), TVI_ROOT, TVI_LAST, numComboBoxItem );
		CNumComboBoxTreeItem *ptr = reinterpret_cast< CNumComboBoxTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->SetNumData( variable );
		m_varHandles.insert( std::make_pair( str, hPA ) );
		
	}
	else
	{
		HTREEITEM hPA = m_tree.m_tree.InsertItem(TVIF_TEXT, _T( str.c_str() ), 0, 0, 0, 0, 0, 0 , NULL);
		CSTreeItem* ptr = reinterpret_cast< CSTreeItem *>( m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->SetOleData( long( variable ) );
		m_varHandles.insert( std::make_pair( str, hPA ) );
		
	}
}

HTREEITEM CPropertieDialog::AddEmptyNode( std::string &str, HTREEITEM hPARoot )
{
		HTREEITEM hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, emptyItem );
		CEmptyTreeItem *ptr = reinterpret_cast< CEmptyTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		m_varHandles.insert( std::make_pair( str, hPA ) );
		return hPA;
}

int CPropertieDialog::GetVariable( std::string &name)
{

	if( m_varHandles.find( name ) != m_varHandles.end() )
	{
		if( name == "Player" )
		{
			CNumComboBoxTreeItem* ptr = reinterpret_cast< CNumComboBoxTreeItem *>(m_tree.m_tree.GetTreeItemPtr( m_varHandles[name] ));	
			if( ptr != 0 ) 
			{
				return	ptr->GetNumData();
			}
		}
		else
		{
			CSTreeItem* ptr = reinterpret_cast< CSTreeItem *>(m_tree.m_tree.GetTreeItemPtr( m_varHandles[name] ));	
			if( ptr != 0 ) 
			{
				return	int(ptr->GetOleData().intVal);
			}
		}
	}
	return -1;
}

void CPropertieDialog::ClearVariables()
{
	m_tree.m_tree.SafeDeleteAllItems();
	m_varHandles.clear();
	m_pCurrentObject = 0;
}

void CPropertieDialog::OnOK() 
{

}

void CPropertieDialog::OnCancel() 
{
	resizeDialogOptions.nParameters[1] = m_tree.m_tree.GetColumnWidth( 0 );
	resizeDialogOptions.nParameters[2] = m_tree.m_tree.GetColumnWidth( 1 );

	reinterpret_cast<CWnd *>( g_frameManager.GetTemplateEditorFrame() )->PostMessage( WM_USER + 5 );
}

LRESULT CPropertieDialog::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( ::IsWindow(m_checkButton.m_hWnd) )
	{
		if( message == (WM_USER + 1) && !m_checkButton.IsPinned() )
		{
			reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 6 );
			reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->PostMessage( WM_USER + 5 );
		}
		if(message == (WM_USER + 1) && m_checkButton.IsPinned() )
		{
			reinterpret_cast<CWnd *>(g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 6 );
		}
		
	}
	return CResizeDialog::DefWindowProc(message, wParam, lParam);
}

void CPropertieDialog::OnDestroy() 
{
	CRect rect;
	GetWindowRect( &rect );
	resizeDialogOptions.rect = rect;
	SaveResizeDialogOptions();

	ClearVariables();
}


void	CPropertieDialog::AddManipulatorVariable( std::string &str, IManipulator *ptr )
{
	std::vector<std::string> szVector;
	NStr::SplitString( str, szVector, '.' );
	HTREEITEM hPARoot = TVI_ROOT;
	std::string tmpStr;
	if( szVector.size() != 1 )
	{
		for( std::vector<std::string>::iterator it = szVector.begin(); it != szVector.end() - 1; ++it )
		{
			tmpStr += (*it);
			if( m_insertedNodes.find( tmpStr ) == m_insertedNodes.end() )
			{
				hPARoot = AddEmptyNode( *it, hPARoot);
				m_insertedNodes.insert( std::make_pair( tmpStr, hPARoot ) );
			}
			else
			{
				hPARoot = m_insertedNodes[ tmpStr ];
			}
			tmpStr += std::string(".");
		}
	}	
	AddPropertieNode( *(szVector.end() - 1), str, ptr, hPARoot);
}

void CPropertieDialog::OnSize(UINT nType, int cx, int cy ) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	
	if( m_tree.ifInit )
	{
		RECT r;
		GetClientRect( &r );
		r.left += nButtonShift + 2;
		r.right -= 7;
		r.top += 2;
		r.bottom -= 7;
		m_tree.MoveWindow( &r );
	}
}

HTREEITEM	CPropertieDialog::AddPropertieNode( std::string &str, std::string &propName, IManipulator *pManipulator, HTREEITEM hPARoot )
{
	CPropertieTreeItem *ptr ;	
	CComboBoxTreeItemPropertieTreeItem *ptr2;
	CDirChosePropertieTreeItem	*ptr3;
	CFileChosePropertieTreeItem	*ptr4;

	CUnitsPropertieTreeItem *ptr5;


	HTREEITEM hPA;
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_COMBO
			&& pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_BROWSEDIR 
			&& pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_BROWSEFILE 
			&& pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_UNITS )
	{
	 hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItem );
		ptr	= reinterpret_cast< CPropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_COMBO )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemCombo );
		ptr2 = reinterpret_cast< CComboBoxTreeItemPropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr2 != 0 ) ptr2->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_BROWSEDIR )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemDir );
		ptr3 = reinterpret_cast< CDirChosePropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr3 != 0 ) ptr3->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_BROWSEFILE )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemFile );
		ptr4 = reinterpret_cast< CFileChosePropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr4 != 0 ) ptr4->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_UNITS )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemUnits );
		ptr5 = reinterpret_cast< CUnitsPropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr5 != 0 ) ptr5->Setup( propName, pManipulator );
	}

	m_varHandles.insert( std::make_pair( str, hPA ) );
	return hPA;
}

void CPropertieDialog::AddObjectWithProp( IManipulator *pMan )
{
	m_insertedNodes.clear();
	m_pCurrentObject = pMan;
	for ( CPtr<IManipulatorIterator> pIt = pMan->Iterate(); !pIt->IsEnd(); pIt->Next() )
	{
		const SPropertyDesc *pDesc = pIt->GetPropertyDesc();
		AddManipulatorVariable( std::string(pDesc->pszName), pMan );
	}
}

void CPropertieDialog::UpdateObjectProp()
{
	m_tree.m_tree.SafeDeleteAllItems();
	m_varHandles.clear();
	m_insertedNodes.clear();

	for ( CPtr<IManipulatorIterator> pIt = m_pCurrentObject->Iterate(); !pIt->IsEnd(); pIt->Next() )
	{
		const SPropertyDesc *pDesc = pIt->GetPropertyDesc();
		AddManipulatorVariable( std::string(pDesc->pszName), m_pCurrentObject );
	}
}

IManipulator* CPropertieDialog::GetCurrentManipulator()
{
	return m_pCurrentObject;
}

void CPropertieDialog::OnPinButton() 
{
	resizeDialogOptions.nParameters[0] = m_checkButton.IsPinned() ? 1 : 0;
}
