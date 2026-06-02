#include "stdafx.h"
#include "MapEditorOptions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CMapEditorOptionsDialog::vID[] = 
{
	IDC_OPTIONS_COMMAND_LINE_LABEL,		//0
	IDC_OPTIONS_COMMAND_LINE_EDIT,		//1
	IDC_OPTIONS_DEF_EXTENTION_LABEL,	//2
	IDC_OPTIONS_DEF_EXTENTION_BZM,		//3
	IDC_OPTIONS_DEF_EXTENTION_XML,		//4
	IDOK,															//5
	IDCANCEL,													//6
};

CMapEditorOptionsDialog::CMapEditorOptionsDialog( CWnd* pParent )
	: CResizeDialog( CMapEditorOptionsDialog::IDD, pParent ), bIsBZM( true )
{
	SetControlStyle( IDC_OPTIONS_COMMAND_LINE_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_OPTIONS_COMMAND_LINE_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_OPTIONS_DEF_EXTENTION_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_OPTIONS_DEF_EXTENTION_BZM, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_OPTIONS_DEF_EXTENTION_XML, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDOK, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );

}

void CMapEditorOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_OPTIONS_COMMAND_LINE_EDIT, m_Parameters);
}

BEGIN_MESSAGE_MAP(CMapEditorOptionsDialog, CResizeDialog)
END_MESSAGE_MAP()

void CMapEditorOptionsDialog::LoadControls()
{
	m_Parameters.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );
	CheckRadioButton( IDC_OPTIONS_DEF_EXTENTION_BZM, IDC_OPTIONS_DEF_EXTENTION_XML, IDC_OPTIONS_DEF_EXTENTION_BZM + resizeDialogOptions.nParameters[0] );
}

void CMapEditorOptionsDialog::SaveControls()
{
	CString strString;
	m_Parameters.GetWindowText( strString );
	resizeDialogOptions.szParameters[0] = strString;
	resizeDialogOptions.nParameters[0] = GetCheckedRadioButton( IDC_OPTIONS_DEF_EXTENTION_BZM, IDC_OPTIONS_DEF_EXTENTION_XML ) - IDC_OPTIONS_DEF_EXTENTION_BZM;
}

BOOL CMapEditorOptionsDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.nParameters.empty() )
	{
		resizeDialogOptions.nParameters.resize( 1 );
	}
	resizeDialogOptions.nParameters[0] = bIsBZM ? 0 : 1;

	if ( resizeDialogOptions.szParameters.empty() )
	{
		resizeDialogOptions.szParameters.resize( 1 );
	}
	resizeDialogOptions.szParameters[0] = szParameter;
	LoadControls();
	return TRUE;
}

void CMapEditorOptionsDialog::OnOK() 
{
	SaveControls();
	CResizeDialog::OnOK();
}

void CMapEditorOptionsDialog::OnCancel() 
{
	SaveControls();
	CResizeDialog::OnCancel();
}
