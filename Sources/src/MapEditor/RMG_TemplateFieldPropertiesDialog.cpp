#include "StdAfx.h"

#include "RMG_TemplateFieldPropertiesDialog.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CRMGTemplateFieldPropertiesDialog::vID[] = 
{
	IDC_RMG_TFP_STATS_LABEL_LEFT,		//0
	IDC_RMG_TFP_STATS_LABEL_RIGHT,	//1
	IDC_RMG_TFP_DELIMITER_00,				//2
	IDC_RMG_TFP_PATH_LABEL_LEFT,		//3
	IDC_RMG_TFP_PATH_LABEL_RIGHT,		//4
	IDC_RMG_TFP_WEIGHT_LABEL,				//5
	IDC_RMG_TFP_WEIGHT_EDIT,				//6
	IDC_RMG_TFP_DEFAULT_CHECK_BOX,	//7
	IDOK,														//8
	IDCANCEL,												//9
};

CRMGTemplateFieldPropertiesDialog::CRMGTemplateFieldPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGTemplateFieldPropertiesDialog::IDD, pParent )
{
	m_bDefault = FALSE;
	m_strWeight = _T("");
	m_strStats = _T("");
	m_strPath = _T("");

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[2], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[3], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[4], ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( vID[5], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[6], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[8], ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( vID[8], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[9], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGTemplateFieldPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Check(pDX, IDC_RMG_TFP_DEFAULT_CHECK_BOX, m_bDefault);
	DDX_Text(pDX, IDC_RMG_TFP_WEIGHT_EDIT, m_strWeight);
	DDX_Text(pDX, IDC_RMG_TFP_STATS_LABEL_RIGHT, m_strStats);
	DDX_Text(pDX, IDC_RMG_TFP_PATH_LABEL_RIGHT, m_strPath);
}

BEGIN_MESSAGE_MAP(CRMGTemplateFieldPropertiesDialog, CResizeDialog)
END_MESSAGE_MAP()
