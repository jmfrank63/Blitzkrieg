#include "StdAfx.h"

#include "RMG_TemplateVSOPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CRMGTemplateVSOPropertiesDialog::vID[] = 
{
	IDC_RMG_TVP_STATS_LABEL_LEFT,		//0
	IDC_RMG_TVP_STATS_LABEL_RIGHT,	//1
	IDC_RMG_TVP_DELIMITER_00,				//2
	IDC_RMG_TVP_PATH_LABEL_LEFT,		//3
	IDC_RMG_TVP_PATH_LABEL_RIGHT,		//4
	IDC_RMG_TVP_WEIGHT_LABEL,				//5
	IDC_RMG_TVP_WEIGHT_EDIT,				//6
	IDC_RMG_TVP_WIDTH_LABEL,				//7
	IDC_RMG_TVP_WIDTH_EDIT,					//8
	IDC_RMG_TVP_OPACITY_LABEL,			//9
	IDC_RMG_TVP_OPACITY_EDIT,				//10
	IDOK,														//11
	IDCANCEL,												//12
};

CRMGTemplateVSOPropertiesDialog::CRMGTemplateVSOPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGTemplateVSOPropertiesDialog::IDD, pParent )
{
	m_strWidth = _T("");
	m_strWeight = _T("");
	m_strStats = _T("");
	m_strPath = _T("");
	m_strOpacity = _T("");

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[2], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[3], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[4], ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( vID[5], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[6], ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( vID[7], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[8], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[9], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[10], ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( vID[11], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[12], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGTemplateVSOPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Text(pDX, IDC_RMG_TVP_WIDTH_EDIT, m_strWidth);
	DDX_Text(pDX, IDC_RMG_TVP_WEIGHT_EDIT, m_strWeight);
	DDX_Text(pDX, IDC_RMG_TVP_STATS_LABEL_RIGHT, m_strStats);
	DDX_Text(pDX, IDC_RMG_TVP_PATH_LABEL_RIGHT, m_strPath);
	DDX_Text(pDX, IDC_RMG_TVP_OPACITY_EDIT, m_strOpacity);
}

BEGIN_MESSAGE_MAP(CRMGTemplateVSOPropertiesDialog, CResizeDialog)
END_MESSAGE_MAP()
