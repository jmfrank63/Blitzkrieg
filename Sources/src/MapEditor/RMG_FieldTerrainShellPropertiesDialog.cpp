#include "stdafx.h"

#include "RMG_FieldTerrainShellPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRMGFieldTerrainShellPropertiesDialog::CRMGFieldTerrainShellPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldTerrainShellPropertiesDialog::IDD, pParent )
{
	m_szWidth = _T("");

	SetControlStyle( IDC_RMG_CF_TS_WIDTH_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_WIDTH_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CF_TS_WIDTH_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	
	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGFieldTerrainShellPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Text(pDX, IDC_RMG_CF_TS_WIDTH_EDIT, m_szWidth);
}

BEGIN_MESSAGE_MAP(CRMGFieldTerrainShellPropertiesDialog, CResizeDialog)
END_MESSAGE_MAP()
