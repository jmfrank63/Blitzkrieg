#include "stdafx.h"

#include "RMG_GraphNodePropertiesDialog.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CRMGGraphNodePropertiesDialog::vID[] = 
{
	IDC_RMG_GNP_SIZE_LABEL_LEFT,					//0
	IDC_RMG_GNP_SIZE_LABEL_RIGHT,					//1
	IDC_RMG_GNP_CONTAINER_LABEL,					//2
	IDC_RMG_GNP_CONTAINER_EDIT,						//3
	IDC_RMG_GNP_CONTAINER_BROWSE_BUTTON,	//4
	IDOK,																	//5		
	IDCANCEL,															//6
};

CRMGGraphNodePropertiesDialog::CRMGGraphNodePropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGGraphNodePropertiesDialog::IDD, pParent )
{
	m_strSize = _T("");
	SetControlStyle( vID[0], ANCHORE_LEFT_TOP );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[2], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[3], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[4], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[5], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( vID[6], ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGGraphNodePropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_GNP_CONTAINER_EDIT, m_ContainerPathEdit);
	DDX_Text(pDX, IDC_RMG_GNP_SIZE_LABEL_RIGHT, m_strSize);
}

BEGIN_MESSAGE_MAP(CRMGGraphNodePropertiesDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_RMG_GNP_CONTAINER_BROWSE_BUTTON, OnContainerBrowseButton)
	ON_EN_CHANGE(IDC_RMG_GNP_CONTAINER_EDIT, OnChangeContainerEdit)
END_MESSAGE_MAP()

BOOL CRMGGraphNodePropertiesDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.szParameters.size() < 1 )
	{
		resizeDialogOptions.szParameters.resize( 1, "" );
	}

	m_ContainerPathEdit.SetWindowText( szContainerInitialFileName.c_str() );
	return TRUE;
}

void CRMGGraphNodePropertiesDialog::OnContainerBrowseButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();
	if ( fileDialog.DoModal() == IDOK )
	{
		BeginWaitCursor();
		POSITION position = fileDialog.GetStartPosition();
		while ( position )
		{
			std::string szFilePath = fileDialog.GetNextPathName( position );
			std::string szStorageName = pDataStorage->GetName();
			NStr::ToLower( szFilePath );
			NStr::ToLower( szStorageName );
			if ( szFilePath.find( szStorageName ) != 0 )
			{
				return;
			}

			int nPointIndex = szFilePath.rfind( "." );
			if ( nPointIndex >= 0 )
			{
				szFilePath = szFilePath.substr( 0, nPointIndex );
			}

			int nSlashIndex = szFilePath.rfind( "\\" );
			if ( nSlashIndex >= 0 )
			{
				resizeDialogOptions.szParameters[0] = szFilePath.substr( 0, nSlashIndex );
			}

			szFilePath = szFilePath.substr( szStorageName.size() );
			szContainerInitialFileName = szFilePath;
			m_ContainerPathEdit.SetWindowText( szFilePath.c_str() );
		}
		CResizeDialog::OnOK();
	}
}

void CRMGGraphNodePropertiesDialog::OnChangeContainerEdit() 
{
	CString szBuffer;
	m_ContainerPathEdit.GetWindowText( szBuffer );
	szContainerInitialFileName = szBuffer;
}
