#include "StdAfx.h"

#include "RMG_PatchPropertiesDialog.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CRMGPatchPropertiesDialog::vID[] = 
{
	IDC_RMG_PP_PATH_LABEL_LEFT,		//0
	IDC_RMG_PP_PATH_LABEL_RIGHT,	//1
	IDC_RMG_PP_SIZE_LABEL_LEFT,		//2
	IDC_RMG_PP_SIZE_LABEL_RIGHT,	//3
	IDC_RMG_PP_CHECKBOXES_LABEL,	//4
	IDC_RMG_PP_CHECKBOX_0,				//5
	IDC_RMG_PP_CHECKBOX_90,				//6
	IDC_RMG_PP_CHECKBOX_180,			//7
	IDC_RMG_PP_CHECKBOX_270,			//8
	IDOK,													//9
	IDCANCEL,											//10
};

CRMGPatchPropertiesDialog::CRMGPatchPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGPatchPropertiesDialog::IDD, pParent )
{
	m_strPath = _T("");
	m_strSize = _T("");
	m_n0 = FALSE;
	m_n180 = FALSE;
	m_n270 = FALSE;
	m_n90 = FALSE;

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[2], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[3], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[4], ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( vID[5], ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[6], ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[7], ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[8], ANCHORE_TOP | ANCHORE_HOR_CENTER );
	
	SetControlStyle( vID[9], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[10], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGPatchPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_PP_PLACE_COMBO_BOX, m_Places);
	DDX_Text(pDX, IDC_RMG_PP_PATH_LABEL_RIGHT, m_strPath);
	DDX_Text(pDX, IDC_RMG_PP_SIZE_LABEL_RIGHT, m_strSize);
	DDX_Check(pDX, IDC_RMG_PP_CHECKBOX_0, m_n0);
	DDX_Check(pDX, IDC_RMG_PP_CHECKBOX_180, m_n180);
	DDX_Check(pDX, IDC_RMG_PP_CHECKBOX_270, m_n270);
	DDX_Check(pDX, IDC_RMG_PP_CHECKBOX_90, m_n90);
}

BEGIN_MESSAGE_MAP(CRMGPatchPropertiesDialog, CResizeDialog)
	ON_CBN_SELCHANGE(IDC_RMG_PP_PLACE_COMBO_BOX, OnSelchangeRmgPpPlaceComboBox)
END_MESSAGE_MAP()

BOOL CRMGPatchPropertiesDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	for ( std::vector<std::string>::const_iterator placeIterator = szPlaces.begin(); placeIterator != szPlaces.end(); ++placeIterator )
	m_Places.AddString( placeIterator->c_str() );
	m_Places.SelectString( -1, szPlace.c_str() );
	return TRUE;
}

void CRMGPatchPropertiesDialog::OnSelchangeRmgPpPlaceComboBox() 
{
	CString szBuffer;
	m_Places.GetWindowText( szBuffer );
	szPlace = szBuffer;
}
