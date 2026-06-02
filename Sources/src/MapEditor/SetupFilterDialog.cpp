#include "stdafx.h"
#include "editor.h"
#include "SetupFilterDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CSetupFilterDialog::vID[] = 
{
	IDC_SO_SET_FILTER_LIST_LABEL,	//0
	IDC_SO_SET_FILTER_LIST,				//1
	IDOK,													//2
	IDCANCEL,											//3
};

const char CSetupFilterDialog::SELECTED_UNITS[] = "SELECTION";
CSetupFilterDialog::CSetupFilterDialog(CWnd* pParent )
	: CResizeDialog( CSetupFilterDialog::IDD, pParent )
{

	SetControlStyle( IDC_SO_SET_FILTER_LIST_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SO_SET_FILTER_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CSetupFilterDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SO_SET_FILTER_LIST, m_filtersNames);
}

BEGIN_MESSAGE_MAP(CSetupFilterDialog, CResizeDialog)
	ON_LBN_SELCHANGE(IDC_SO_SET_FILTER_LIST, OnSelchangeList1)
	ON_LBN_DBLCLK(IDC_SO_SET_FILTER_LIST, OnDblclkSoSetFilterList)
END_MESSAGE_MAP()

BOOL CSetupFilterDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	for( TFilterHashMap::iterator it = m_allFilters.begin(); it != m_allFilters.end(); ++it )
	{
		m_filtersNames.AddString( it->first.c_str() );
	}
	m_filtersNames.SelectString( -1, m_filterName );
	UpdateControls();
	return TRUE;
}

void CSetupFilterDialog::OnSelchangeList1() 
{
	UpdateControls();
}

void CSetupFilterDialog::OnOK() 
{
	if ( m_filtersNames.GetCurSel() != ( -1 ) )
	{
		m_filtersNames.GetText( m_filtersNames.GetCurSel(), m_filterName );
	}
	else
	{
		m_filterName = "";
	}
	CResizeDialog::OnOK();
}
void CSetupFilterDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDOK ) )
	{
		pWnd->EnableWindow( m_filtersNames.GetCurSel() != ( -1 ) );
	}
}

void CSetupFilterDialog::OnDblclkSoSetFilterList() 
{
	OnOK();
}
