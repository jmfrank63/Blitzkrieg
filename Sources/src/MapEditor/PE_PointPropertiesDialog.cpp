#include "stdafx.h"

#include "PE_PointPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CPEPointPropertiesDialog::vID[] = 
{
	IDC_PE_PP_POINT_X_COORD_LABEL_LEFT,		//0
	IDC_PE_PP_POINT_X_COORD_EDIT,					//1
	IDC_PE_PP_POINT_X_COORD_LABEL_RIGHT,	//2
	IDC_PE_PP_POINT_Y_COORD_LABEL_LEFT,		//3
	IDC_PE_PP_POINT_Y_COORD_EDIT,					//4
	IDC_PE_PP_POINT_Y_COORD_LABEL_RIGHT,	//5
	IDOK,																	//6
	IDCANCEL,															//7
};

CPEPointPropertiesDialog::CPEPointPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CPEPointPropertiesDialog::IDD, pParent )
{
	m_strXCoord = _T("");
	m_strYCoord = _T("");

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[2], ANCHORE_RIGHT_TOP );

	SetControlStyle( vID[3], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[4], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[5], ANCHORE_RIGHT_TOP );

	SetControlStyle( vID[6], ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( vID[7], ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
}

void CPEPointPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Text(pDX, IDC_PE_PP_POINT_X_COORD_EDIT, m_strXCoord);
	DDX_Text(pDX, IDC_PE_PP_POINT_Y_COORD_EDIT, m_strYCoord);
}

BEGIN_MESSAGE_MAP(CPEPointPropertiesDialog, CResizeDialog)
END_MESSAGE_MAP()
