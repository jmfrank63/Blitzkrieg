
#include "StdAfx.h"
#include "CTreeItem.h"
#include "MultiTree.h"
#include "MultiTreeEditBox.h"
#include "TreeItemComboBox.h"
#include "PercentDialog.h"
#include "..\..\Common\LegacyUiCompat.h"

#include "MultiTreeSlider.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__;
#endif

bool CSTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	rect.top -= 2;
	rect.bottom += 2;
	m_editCtl = new  CMultiTreeEditBox();
	m_editCtl->Create(WS_CHILD | WS_VISIBLE | WS_BORDER  , CRect(treePtr->GetColumnWidth(0)+2, rect.top
		, treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0), rect.bottom), treePtr, 1);
	m_editCtl->SetFocus();
	m_editCtl->SetWindowText( GetNormalProperty() != "" ? GetNormalProperty() .c_str() : "" );
	return true;
}

bool CSTreeItem::KillEditor()
{
	CString str;
	COleVariant var,tmp;
	if ( ( m_editCtl != NULL ) && ( !bKilleditor ) )
	{
		bKilleditor = true;

		m_editCtl->GetWindowText( str );
		tmp = str; 
		
		var = GetOleData();
		try
		{
			tmp.ChangeType( var.vt, NULL) ; //���� �� ���������� �����������
		}
		catch(...)
		{	
		}	
		if ( tmp.vt == var.vt )
		{
			SetOleData( tmp );
		}
		m_editCtl->DestroyWindow();
		delete m_editCtl;
		m_editCtl = NULL;

		bKilleditor = false;
	}
	return bKilleditor;
}
bool CTrueFalseTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	m_ComboBoxCtrl= new CTreeItemComboBox ();
	m_ComboBoxCtrl->Create( WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | WS_THICKFRAME | WS_BORDER   ,
      CRect(treePtr->GetColumnWidth(0) + 2, rect.top - 3
		,treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0), rect.bottom + 3 * (rect.bottom - rect.top )), treePtr, 1);
	m_ComboBoxCtrl->SetFocus();
	m_ComboBoxCtrl->AddString( "true"  );
	m_ComboBoxCtrl->AddString( "false" );
	if( m_var ) { m_ComboBoxCtrl->SetCurSel( 0 ); }
	else { m_ComboBoxCtrl->SetCurSel( 1 ); }
	return  true;
}

bool CTrueFalseTreeItem::KillEditor()
{
	if ( ( m_ComboBoxCtrl != NULL ) && ( !bKilleditor ) )
	{
		bKilleditor = true;

		if( m_ComboBoxCtrl->GetCurSel() == 0) { m_var = true; }
		else { m_var = false; }
		
		m_ComboBoxCtrl->DestroyWindow();
		delete m_ComboBoxCtrl;
		m_ComboBoxCtrl = NULL;	
		
		bKilleditor = false;
	}
	return bKilleditor;
}
bool CProcentTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	CRect rect2;
	m_SliderDlg= new CPercentDialog;
  m_SliderDlg->Create(IDD_DIALOG1,treePtr );
	m_SliderDlg->m_value = m_procent ;
	m_SliderDlg->m_variable = m_procent ;
	m_SliderDlg->UpdateData( false );
	m_SliderDlg->GetWindowRect( &rect2 );
	m_SliderDlg->SetWindowPos(NULL , treePtr->GetColumnWidth(0) , rect.top , 
	rect2.right - rect2.left , rect2.bottom- rect2.top
	, SWP_SHOWWINDOW   );
 	/*m_SliderCtrl->Create( WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_THICKFRAME |TBS_HORZ   ,
      CRect(treePtr->GetColumnWidth(0)+2, rect.top
		,treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0), rect.bottom + 1 * (rect.bottom - rect.top )), treePtr, 1);
	*/
	m_SliderDlg->SetFocus();
	return  true;
}

bool CProcentTreeItem::KillEditor()
{
	if ( ( m_SliderDlg != NULL ) && ( !bKilleditor ) )
	{
		bKilleditor = true;

		m_procent = m_SliderDlg->m_variable;
		m_SliderDlg->DestroyWindow();
		delete m_SliderDlg;
		m_SliderDlg = NULL;	
		
		bKilleditor = false;
	}
	return bKilleditor;
}
bool CNumComboBoxTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	m_ComboBoxCtrl= new CTreeItemComboBox ();
	m_ComboBoxCtrl->Create( WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST/*| WS_BORDER */  ,
		CRect(treePtr->GetColumnWidth(0) + 2, rect.top - 3
		,treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0), rect.bottom + 3 * (rect.bottom - rect.top )), treePtr, 1);
	m_ComboBoxCtrl->SetFocus();
	m_ComboBoxCtrl->AddString( "0" );
	m_ComboBoxCtrl->AddString( "1" );
	m_ComboBoxCtrl->AddString( "2" );
	m_ComboBoxCtrl->AddString( "3" );
	m_ComboBoxCtrl->AddString( "4" );
	m_ComboBoxCtrl->AddString( "5" );
	
	m_ComboBoxCtrl->SetCurSel( m_var ); 
	return  true;

}

bool CNumComboBoxTreeItem::KillEditor()
{
	if ( ( m_ComboBoxCtrl != NULL ) && ( !bKilleditor ) )
	{
		bKilleditor = true;

		m_var = m_ComboBoxCtrl->GetCurSel() ;
		m_ComboBoxCtrl->DestroyWindow();
		delete m_ComboBoxCtrl;
		m_ComboBoxCtrl = NULL;	

		bKilleditor = false;
	}
	return bKilleditor;
}

std::string			  CNumComboBoxTreeItem::GetNormalProperty()
{
	CString rString;
	rString.Format( "%d", m_var );
	return std::string(rString);
}

bool CEmptyTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	treePtr->SendMessage( WM_USER + 2);
	return  false;
}

bool CEmptyTreeItem::KillEditor()
{
	return false;
}
bool CPropertieTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	rect.top -= 2;
	rect.bottom += 2;
	m_editCtl = new  CMultiTreeEditBox();
	m_editCtl->Create(WS_CHILD | WS_VISIBLE | WS_BORDER  , CRect(treePtr->GetColumnWidth(0)+2, rect.top
		, treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0), rect.bottom), treePtr, 1);
	m_editCtl->SetFocus();
	m_editCtl->SetWindowText( GetNormalProperty() != "" ? GetNormalProperty() .c_str() : "" );
	return  true;

}

bool CPropertieTreeItem::KillEditor()
{
	if ( ( m_editCtl != 0 ) && ( !bKilleditor ) )
	{
		bKilleditor = true;

		CString strValue;
		m_editCtl->GetWindowText( strValue );
		const SPropertyDesc	*pDesc = m_pManipulator->GetPropertyDesc( m_propName.c_str() );
		COleVariant value;
		bool bConverted = false;
		try
		{
			switch ( pDesc->ePropType )
			{
				case SPropertyDesc::EPropertyType::VAL_FLOAT:
				{
					strValue.Replace( ',', '.' );
					float fValue = 0.0f;
					if ( sscanf( LPCTSTR( strValue ), "%g", &fValue ) >= 1 )
					{
						value = fValue;
						value.ChangeType( VT_R4, NULL ); 
						bConverted = true;
					}
					break;
				}
				case SPropertyDesc::EPropertyType::VAL_INT:
				{
					strValue.Replace( ',', '.' );
					long nValue = 0;
					if ( sscanf( LPCTSTR( strValue ), "%d", &nValue ) >= 1 )
					{
						value = nValue;
						value.ChangeType( VT_INT, NULL ) ; 
						bConverted = true;
					}
					break;
				}
				case SPropertyDesc::EPropertyType::VAL_COMBO:
				case SPropertyDesc::EPropertyType::VAL_BROWSEDIR:
				case SPropertyDesc::EPropertyType::VAL_BROWSEFILE:
				{	
					value = strValue;
					value.ChangeType( VT_BSTR, NULL ); 
					bConverted = true;
					break;
				}
			}
		}
		catch ( ... )
		{
		}

		if ( bConverted )
		{
			m_pManipulator->SetValue( m_propName.c_str(), value );
		}

		m_editCtl->DestroyWindow();
		delete m_editCtl;
		m_editCtl = 0;
		
		bKilleditor = false;
	}
	return bKilleditor;
}
bool CComboBoxTreeItemPropertieTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CRect rect;
	treePtr->GetItemRect( *item , &rect, FALSE );
	m_ComboBoxCtrl= new CTreeItemComboBox ();
	m_ComboBoxCtrl->Create( WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_SORT | CBS_DROPDOWNLIST/*| WS_BORDER */  ,
		CRect(treePtr->GetColumnWidth(0) + 2, rect.top - 3
		, rect.right/*treePtr->GetColumnWidth(1) + treePtr->GetColumnWidth(0)*/, rect.bottom + 7 * (rect.bottom - rect.top )), treePtr, 1);
/*	m_ComboBoxCtrl->SetFocus();
	m_ComboBoxCtrl->AddString( "0" );
	m_ComboBoxCtrl->AddString( "1" );

	m_ComboBoxCtrl->SetCurSel( m_var ); */
		if( m_ComboBoxCtrl )
		{
			m_ComboBoxCtrl->SetFocus();
			if ( m_pManipulator->GetPropertyDesc(m_propName.c_str())->VAL_COMBO )	
			{
				variant_t tmp_t;
				COleVariant tmpVal;
				m_pManipulator->GetValue( m_propName.c_str(), &tmp_t );
				tmpVal = tmp_t;
				tmpVal.ChangeType(VT_BSTR , NULL);
				CString strInitialBuffer = tmpVal.bstrVal;	
				
				for ( int i = 0; i != m_pManipulator->GetPropertyDesc(m_propName.c_str())->values.size(); ++i )
				{
					COleVariant tmp;
					tmp = m_pManipulator->GetPropertyDesc( m_propName.c_str() )->values[i];
					tmp.ChangeType( VT_BSTR, NULL );
					CString strBuffer = tmp.bstrVal;	
					m_ComboBoxCtrl->AddString( strBuffer );
				}

				m_ComboBoxCtrl->SelectString( CB_ERR, strInitialBuffer );
			}
		}
		return true;
}

/*void	CComboBoxTreeItemPropertieTreeItem::Setup( std::string &name, IManipulator *ptr )
{	
	m_propName = name ;
	m_pManipulator = ptr;
}*/
bool CComboBoxTreeItemPropertieTreeItem::KillEditor()
{
	if ( ( m_ComboBoxCtrl != NULL ) && ( !bKilleditor ) )
	{
		bKilleditor = true;
		
		CString str;
		COleVariant var, tmp;
		variant_t var_t;
		m_ComboBoxCtrl->GetWindowText( str );
		tmp = str; 	
		m_pManipulator->GetValue( m_propName.c_str(), &var_t );
		var = var_t;
		try
		{
			tmp.ChangeType( var.vt, NULL ); //���� �� ���������� �����������
		}
		catch(...)
		{	
		}	
		if ( tmp.vt == var.vt )
		{
			m_pManipulator->SetValue( m_propName.c_str(), tmp );				
		}

		m_ComboBoxCtrl->DestroyWindow();
		delete m_ComboBoxCtrl;
		m_ComboBoxCtrl = NULL;
		
		bKilleditor = false;
	}
	return bKilleditor;
}
bool CFileChosePropertieTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	CFileDialog dlg( true );//. dlg( "Select Directory" );
	CString str;
	if ( dlg.DoModal() == IDOK )
	{
		str = dlg.GetPathName(  );
		COleVariant var, tmp;
		variant_t var_t;
		tmp = str;
		m_pManipulator->GetValue( m_propName.c_str(), &var_t );
		var = var_t;
		try
		{
			tmp.ChangeType( var.vt, NULL) ; //���� �� ���������� �����������
		}
		catch(...)
		{	
		}	
		m_pManipulator->SetValue( m_propName.c_str(), tmp );				

	}

	return false;
}
bool CFileChosePropertieTreeItem::KillEditor()
{
	return false;
}
bool CDirChosePropertieTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	SECDirSelectDlg dlg;//. dlg( "Select Directory" );
	CString str;
	if ( dlg.DoModal() == IDOK )
	{
		str = dlg.GetPathName(  );
		COleVariant var,tmp;
		variant_t var_t;
		tmp = str;
		m_pManipulator->GetValue( m_propName.c_str(), &var_t );
		var = var_t;
		try
		{
			tmp.ChangeType( var.vt, NULL) ; //���� �� ���������� �����������
		}
		catch(...)
		{	
		}	
		m_pManipulator->SetValue( m_propName.c_str(), tmp );				

	}
	return  false;
}
bool CDirChosePropertieTreeItem::KillEditor()
{
	return false;
}
bool CUnitsPropertieTreeItem::BuildEditor( HTREEITEM *item , CMultiTree *treePtr)
{
	COleVariant tmp;
	m_pManipulator->SetValue( m_propName.c_str(),  tmp );			
		
	/*
	SECDirSelectDlg dlg;//. dlg( "Select Directory" );
	CString str;
	if ( dlg.DoModal() == IDOK )
	{
		str = dlg.GetPathName(  );
		COleVariant var,tmp;
		tmp = str;
		m_pManipulator->GetValue( m_propName.c_str(), &var );
		try
		{
			tmp.ChangeType( var.vt, NULL) ; //���� �� ���������� �����������
		}
		catch(...)
		{	
		}	
		m_pManipulator->SetValue( m_propName.c_str(), tmp );				

	}*/
	return  false;
}
bool CUnitsPropertieTreeItem::KillEditor()
{
	return true;
}