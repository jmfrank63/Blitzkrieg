#include "StdAfx.h"
#include "OpenMapDialog.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "../misc/FileUtils.h"
#include "../RandomMapGen/MapInfo_Types.h"
const int COpenMapDialog::vID[] = 
{
	IDC_OPEN_MAP_NAME_LABEL,					//0
	IDC_OPEN_MAP_NAME_COMBO_BOX,			//1
	IDC_OPEN_MAP_NAME_BROWSE_BUTTON,	//2
	IDC_OPEN_MAP_MOD_LABEL,						//3
	IDC_OPEN_MAP_MOD_COMBO_BOX,				//4
	IDOK,															//5
	IDCANCEL,													//6
};

COpenMapDialog::COpenMapDialog( CWnd* pParent )
	: CResizeDialog( COpenMapDialog::IDD, pParent )
{


	SetControlStyle( IDC_OPEN_MAP_NAME_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_OPEN_MAP_NAME_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_OPEN_MAP_NAME_BROWSE_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_OPEN_MAP_MOD_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_OPEN_MAP_MOD_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDOK, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );

}

void COpenMapDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_OPEN_MAP_NAME_COMBO_BOX, wndFileName);
	DDX_Control(pDX, IDC_OPEN_MAP_MOD_COMBO_BOX, wndMODComboBox);
}

BEGIN_MESSAGE_MAP(COpenMapDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_OPEN_MAP_NAME_BROWSE_BUTTON, OnOpenMapNameBrowseButton)
	ON_CBN_EDITCHANGE(IDC_OPEN_MAP_NAME_COMBO_BOX, OnEditChangeOpenMapNameComboBox)
	ON_CBN_SELCHANGE(IDC_OPEN_MAP_NAME_COMBO_BOX, OnSelChangeOpenMapNameComboBox)
END_MESSAGE_MAP()

void COpenMapDialog::GetAllMODs( std::vector<std::string> *pMODsList )
{
	if ( pMODsList )
	{
		pMODsList->clear();
		const CMODCollector &rMODCollector = g_frameManager.GetTemplateEditorFrame()->modCollector;
		for ( CMODCollector::TMODNodesList::const_iterator modNodeIterator = rMODCollector.availableMODs.begin(); modNodeIterator != rMODCollector.availableMODs.end(); ++modNodeIterator )
		{
			pMODsList->push_back( modNodeIterator->first );
		}
		std::sort( pMODsList->begin(), pMODsList->end() );
	}
}

std::string COpenMapDialog::GetMODKey()
{ 
	if ( ( resizeDialogOptions.szParameters[1].empty() ) || ( resizeDialogOptions.szParameters[1] == RMGC_NO_MOD_FOLDER ) )
	{
		return "";
	}
	return resizeDialogOptions.szParameters[1];
}

void COpenMapDialog::LoadControls()
{
	const std::string szRivers3DName( CTemplateEditorFrame::RIVERS_3D_MAP_NAME );
	const std::string szRoads3DName( CTemplateEditorFrame::ROADS_3D_MAP_NAME );

	for ( std::list<std::string>::const_iterator mapNameIterator = mapNames.begin(); mapNameIterator != mapNames.end(); ++mapNameIterator )
	{
		if ( ( ( *mapNameIterator ) != szRivers3DName ) &&
				 ( ( *mapNameIterator ) != szRoads3DName ) )
		{		
			wndFileName.AddString( mapNameIterator->c_str() );
		}
	}
	wndFileName.SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );

	std::vector<std::string> modsFolders;
	GetAllMODs( &modsFolders );
	modsFolders.push_back( RMGC_NO_MOD_FOLDER );
	modsFolders.push_back( RMGC_CURRENT_MOD_FOLDER );
	modsFolders.push_back( RMGC_OWN_MOD_FOLDER );
	bool bMODExists = false;
	for ( std::vector<std::string>::const_iterator modFolderIterator = modsFolders.begin(); modFolderIterator != modsFolders.end(); ++modFolderIterator )
	{
		int nStringNumber = wndMODComboBox.AddString( modFolderIterator->c_str() );
		if ( resizeDialogOptions.szParameters[1] == ( *modFolderIterator ) )
		{
			bMODExists = true;	
		}
	}
	if ( resizeDialogOptions.szParameters[1].empty() || ( !bMODExists ) )
	{
		wndMODComboBox.SelectString( -1, RMGC_OWN_MOD_FOLDER );
		resizeDialogOptions.szParameters[1].clear();
	}
	else
	{
		wndMODComboBox.SelectString( -1, resizeDialogOptions.szParameters[1].c_str() );
	}
}

void COpenMapDialog::SaveControls()
{
	CString strString;
	wndFileName.GetWindowText( strString );
	resizeDialogOptions.szParameters[0] = strString;

	int nStringNumber = wndMODComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		wndMODComboBox.GetLBText( nStringNumber, strString );
		resizeDialogOptions.szParameters[1] = strString;
	}
}

BOOL COpenMapDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.szParameters.size() < 2 )
	{
		resizeDialogOptions.szParameters.resize( 2, "" );
		resizeDialogOptions.szParameters[0].clear();
		resizeDialogOptions.szParameters[1].clear();
	}

	LoadControls();
	UpdateControls();
	return TRUE;
}

void COpenMapDialog::OnOK() 
{
	SaveControls();
	CResizeDialog::OnOK();
}

void COpenMapDialog::OnCancel() 
{
	SaveControls();
	CResizeDialog::OnCancel();
}

void COpenMapDialog::UpdateControls()
{
	CString strText;
	wndFileName.GetWindowText( strText );
	
	if ( CWnd *pWnd = GetDlgItem( IDOK ) )
	{
		pWnd->EnableWindow( !strText.IsEmpty() );
	}
}

void COpenMapDialog::OnOpenMapNameBrowseButton() 
{
	std::string szInitialDir;
	int nSlashPosition = resizeDialogOptions.szParameters[0].rfind( '\\' );
	if ( nSlashPosition != std::string::npos )
	{
		szInitialDir =  resizeDialogOptions.szParameters[0].substr( 0, nSlashPosition );
	}
	
	CFileDialog fileDialog( true, ".xml", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "All supported Files (*.bzm; *.xml)|*.bzm; *.xml|XML files (*.xml)|*.xml|BZM files (*.bzm)|*.bzm|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = szInitialDir.c_str();

	if ( fileDialog.DoModal() == IDOK )
	{
		resizeDialogOptions.szParameters[0] = fileDialog.GetPathName();
		wndFileName.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );
		UpdateControls();
	}
}

void COpenMapDialog::OnEditChangeOpenMapNameComboBox() 
{
	UpdateControls();
}

void COpenMapDialog::OnSelChangeOpenMapNameComboBox() 
{
	int nStringNumber = wndFileName.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		CString strText;
		wndFileName.GetLBText( nStringNumber, strText );
		wndFileName.SetWindowText( strText );
	}

	UpdateControls();
}
