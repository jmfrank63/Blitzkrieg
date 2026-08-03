#include "StdAfx.h"

#include "RMG_FieldTilePropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRMGFieldTilePropertiesDialog::CRMGFieldTilePropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldTilePropertiesDialog::IDD, pParent ), bDisableEditWeight( false ), hIcon( 0 )
{
	m_szName = _T("");
	m_szStats = _T("");
	m_szVariants = _T("");
	m_szWeight = _T("");

	SetControlStyle( IDC_RMG_CF_TS_TP_STATS_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_TP_STATS_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_RMG_CF_TS_TP_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_RMG_CF_TS_TP_ICON, ANCHORE_LEFT_TOP );
	
	SetControlStyle( IDC_RMG_CF_TS_TP_NAME_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_TP_NAME_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_TS_TP_WEIGHT_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_TP_WEIGHT_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_TS_TP_DELIMITER_01, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_TS_TP_VARIANTS_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_TP_VARIANTS_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGFieldTilePropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_CF_TS_TP_ICON, m_Icon);
	DDX_Text(pDX, IDC_RMG_CF_TS_TP_NAME_LABEL_RIGHT, m_szName);
	DDX_Text(pDX, IDC_RMG_CF_TS_TP_STATS_LABEL_RIGHT, m_szStats);
	DDX_Text(pDX, IDC_RMG_CF_TS_TP_VARIANTS_LABEL_RIGHT, m_szVariants);
	DDX_Text(pDX, IDC_RMG_CF_TS_TP_WEIGHT_EDIT, m_szWeight);
}

BEGIN_MESSAGE_MAP(CRMGFieldTilePropertiesDialog, CResizeDialog)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CRMGFieldTilePropertiesDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( bDisableEditWeight )
	{
		if ( CWnd *pWnd = GetDlgItem( IDC_RMG_CF_TS_TP_WEIGHT_LABEL ) )
		{
			pWnd->ShowWindow( SW_HIDE );
			pWnd->EnableWindow( false );
		}
		if ( CWnd *pWnd = GetDlgItem( IDC_RMG_CF_TS_TP_WEIGHT_EDIT ) )
		{
			pWnd->ShowWindow( SW_HIDE );
			pWnd->EnableWindow( false );
		}
		if ( CWnd *pWnd = GetDlgItem( IDCANCEL ) )
		{
			pWnd->ShowWindow( SW_HIDE );
		}
		if ( CWnd *pWnd = GetDlgItem( IDOK ) )
		{
			pWnd->SetWindowText( "&Close" );

			CRect buttonRect;
			pWnd->GetWindowRect( &buttonRect );
			ScreenToClient( &buttonRect );

			CRect clientRect;
			GetClientRect( &clientRect );
			pWnd->MoveWindow( ( clientRect.Width() - buttonRect.Width() ) / 2,
												buttonRect.top,
												buttonRect.Width(),
												buttonRect.Height() );
		}
		UpdateControlPositions();
	}

	if ( hIcon != 0 )
	{
		m_Icon.SetIcon( hIcon );
	}
	else
	{
		m_Icon.ShowWindow( SW_HIDE );
		m_Icon.EnableWindow( false );
	}

	return TRUE;
}

void CRMGFieldTilePropertiesDialog::OnDestroy() 
{
	CResizeDialog::OnDestroy();

	if ( hIcon != 0 )
	{
		::DestroyIcon( hIcon );
		hIcon = 0;
	}
}
