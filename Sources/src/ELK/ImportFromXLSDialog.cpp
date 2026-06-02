#include "StdAfx.h"
#include "..\Common\StingrayCompat.h"

#include "resource.h"
#include "ImportFromXLSDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CImportFromXLSDialog::vID[] = 
{
	IDC_IFX_XLS_BROWSE_LABEL,	//0
	IDC_IFX_XLS_BROWSE_EDIT,		//1
	IDC_IFX_XLS_BROWSE_BUTTON,	//2
	IDC_IFX_FILE_BROWSE_LABEL,		//3
	IDC_IFX_FILE_BROWSE_EDIT,			//4
	IDC_IFX_FILE_BROWSE_BUTTON,		//5
	IDOK,													//6
	IDCANCEL,											//7
};

CImportFromXLSDialog::CImportFromXLSDialog( CWnd* pParent )
	: CResizeDialog( CImportFromXLSDialog::IDD, pParent )
{

	SetControlStyle( IDC_IFX_XLS_BROWSE_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_IFX_XLS_BROWSE_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_IFX_XLS_BROWSE_BUTTON, ANCHORE_RIGHT_TOP );
	
	SetControlStyle( IDC_IFX_FILE_BROWSE_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_IFX_FILE_BROWSE_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_IFX_FILE_BROWSE_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

std::string CImportFromXLSDialog::GetRegistryKey()
{
	CString strPath;
	CString strProgramKey;
	CString strKey;
	strPath.LoadString( IDS_REGISTRY_PATH );
	strProgramKey.LoadString( AFX_IDS_APP_TITLE );
	strKey.LoadString( IDS_IFX_REGISTRY_KEY );
	std::string szRegistryKey = NStr::Format( _T( "Software\\%s\\%s\\%s" ), LPCTSTR( strPath ), LPCTSTR( strProgramKey ), LPCTSTR( strKey ) );
	return szRegistryKey;
}

void CImportFromXLSDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_IFX_XLS_BROWSE_EDIT, m_XLSEdit);
	DDX_Control(pDX, IDC_IFX_FILE_BROWSE_EDIT, m_FileEdit);
}

BEGIN_MESSAGE_MAP(CImportFromXLSDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_IFX_XLS_BROWSE_BUTTON, OnXLSBrowseButton)
	ON_EN_CHANGE(IDC_IFX_XLS_BROWSE_EDIT, OnChangeXLSBrowseEdit)
	ON_BN_CLICKED(IDC_IFX_FILE_BROWSE_BUTTON, OnFileBrowseButton)
	ON_EN_CHANGE(IDC_IFX_FILE_BROWSE_EDIT, OnChangeFileBrowseEdit)
END_MESSAGE_MAP()

BOOL CImportFromXLSDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.szParameters.size() < 2 )
	{
		resizeDialogOptions.szParameters.resize( 2 );
	}

	m_XLSEdit.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );
	m_FileEdit.SetWindowText( resizeDialogOptions.szParameters[1].c_str() );
	
	UpdateControls();
	return TRUE;
}

void CImportFromXLSDialog::GetXLSPath( std::string *pszXLSPath )
{
	NI_ASSERT_T( pszXLSPath != 0, NStr::Format( "CImportFromXLSDialog::GetXLSPath wrong parameter: pszXLSPath %x", pszXLSPath ) );
	if ( pszXLSPath )
	{
		( *pszXLSPath ) = resizeDialogOptions.szParameters[0];
	}
}

void CImportFromXLSDialog::GetFilePath( std::string *pszFilePath )
{
	NI_ASSERT_T( pszFilePath != 0, NStr::Format( "CImportFromXLSDialog::GetFilePath wrong parameter: pszFilePath %x", pszFilePath ) );
	if ( pszFilePath )
	{
		( *pszFilePath ) = resizeDialogOptions.szParameters[1];
	}
}

void CImportFromXLSDialog::OnXLSBrowseButton() 
{
	CString strDialogTitle;
	CString strFileName;
	CString strFolderName;

	strDialogTitle.LoadString( IDS_IFX_BROWSE_FOR_XLS_DIALOG_TITLE );

	m_XLSEdit.GetWindowText( strFileName );
	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, _T( ".xls" ), strFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T( "XLS files (*.xls)|*.xls|All Files (*.*)|*.*||" ), this );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		strFileName = fileDialog.GetPathName();

		int nSlashPos = strFileName.ReverseFind( '.' );
		if ( nSlashPos < 0 )
		{
			strFileName += _T( ".xls" );
		}

		resizeDialogOptions.szParameters[0] = strFileName;
		m_XLSEdit.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );

		UpdateControls();
	}
}

void CImportFromXLSDialog::OnFileBrowseButton() 
{
	CString strDialogTitle;
	CString strFileName;
	CString strFolderName;

	strDialogTitle.LoadString( IDS_IFX_BROWSE_FOR_FILE_DIALOG_TITLE );

	m_FileEdit.GetWindowText( strFileName );
	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, _T( ".xml" ), strFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T( "XML files (*.xml)|*.xml|All Files (*.*)|*.*||" ), this );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		strFileName = fileDialog.GetPathName();

		int nSlashPos = strFileName.ReverseFind( '.' );
		if ( nSlashPos < 0 )
		{
			strFileName += _T( ".xml" );
		}

		resizeDialogOptions.szParameters[1] = strFileName;
		m_FileEdit.SetWindowText( resizeDialogOptions.szParameters[1].c_str() );

		UpdateControls();
	}
}

void CImportFromXLSDialog::OnChangeXLSBrowseEdit() 
{
	CString strFolderName;
	m_XLSEdit.GetWindowText( strFolderName );
	resizeDialogOptions.szParameters[0] = strFolderName;
	UpdateControls();
}

void CImportFromXLSDialog::OnChangeFileBrowseEdit() 
{
	CString strFileName;
	m_FileEdit.GetWindowText( strFileName );
	resizeDialogOptions.szParameters[1] = strFileName;
	UpdateControls();
}

void	CImportFromXLSDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDOK ) )
	{
		pWnd->EnableWindow( !resizeDialogOptions.szParameters[0].empty() && !resizeDialogOptions.szParameters[1].empty() );
	}
}

void CImportFromXLSDialog::OnOK() 
{
	int nPointPos = resizeDialogOptions.szParameters[0].rfind( '.' );
	if ( nPointPos == std::string::npos )
	{
		resizeDialogOptions.szParameters[0] += std::string( ".xls" );
	}
	nPointPos = resizeDialogOptions.szParameters[1].rfind( '.' );
	if ( nPointPos == std::string::npos )
	{
		resizeDialogOptions.szParameters[1] += std::string( ".xml" );
	}
	CResizeDialog::OnOK();
}

