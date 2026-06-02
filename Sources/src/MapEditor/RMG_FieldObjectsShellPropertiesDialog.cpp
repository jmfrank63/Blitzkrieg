#include "stdafx.h"

#include "RMG_FieldObjectsShellPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRMGFieldObjectsShellPropertiesDialog::CRMGFieldObjectsShellPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldObjectsShellPropertiesDialog::IDD, pParent )
{
	m_szWidth = _T("");
	m_szStep = _T("");
	m_szRatio = _T("");

	SetControlStyle( IDC_RMG_CF_TS_WIDTH_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_TS_WIDTH_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CF_TS_WIDTH_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	
	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGFieldObjectsShellPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Text(pDX, IDC_RMG_CF_OS_WIDTH_EDIT, m_szWidth);
	DDX_Text(pDX, IDC_RMG_CF_OS_STEP_EDIT, m_szStep);
	DDX_Text(pDX, IDC_RMG_CF_OS_PROBABILITY_EDIT, m_szRatio);
}

BEGIN_MESSAGE_MAP(CRMGFieldObjectsShellPropertiesDialog, CResizeDialog)
END_MESSAGE_MAP()
