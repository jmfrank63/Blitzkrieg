#include "StdAfx.h"
#include "NewMapDialog.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "..\misc\FileUtils.h"
#include "..\RandomMapGen\MapInfo_Types.h"
const int CNewMapDialog::vID[] = 
{
	IDC_NEW_MAP_SIZE_X_LABEL,				//0
	IDC_NEW_MAP_SIZE_Y_LABEL,				//1
	IDC_NEW_MAP_SIZE_X_COMBO_BOX,		//2
	IDC_NEW_MAP_SIZE_Y_COMBO_BOX,		//3
	IDC_NEW_MAP_SQUARE_CHECK_BOX,		//4
	IDC_NEW_MAP_NAME_LABEL,					//5
	IDC_NEW_MAP_NAME_EDIT,					//6
	IDC_NEW_MAP_NAME_BROWSE_BUTTON,	//7
	IDC_NEW_MAP_SEASON_LABEL,				//8
	IDC_NEW_MAP_SEASON_COMBO_BOX,		//9
	IDOK,														//10
	IDCANCEL,												//11
};

CNewMapDialog::CNewMapDialog( CWnd* pParent )
	: CResizeDialog( CNewMapDialog::IDD, pParent )
{

	SetControlStyle( IDC_NEW_MAP_SIZE_X_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_NEW_MAP_SIZE_Y_LABEL, ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_NEW_MAP_SIZE_X_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_NEW_MAP_SIZE_Y_COMBO_BOX, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_NEW_MAP_SQUARE_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_NEW_MAP_NAME_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_NEW_MAP_NAME_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_NEW_MAP_NAME_BROWSE_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_NEW_MAP_SEASON_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_NEW_MAP_SEASON_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_NEW_MAP_MOD_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_NEW_MAP_MOD_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDOK, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );

}

void CNewMapDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_NEW_MAP_SIZE_X_COMBO_BOX, wndSizeXComboBox);
	DDX_Control(pDX, IDC_NEW_MAP_SIZE_Y_COMBO_BOX, wndSizeYComboBox);
	DDX_Control(pDX, IDC_NEW_MAP_NAME_EDIT, wndFileName);
	DDX_Control(pDX, IDC_NEW_MAP_SEASON_COMBO_BOX, wndSeasonComboBox);
	DDX_Control(pDX, IDC_NEW_MAP_MOD_COMBO_BOX, wndMODComboBox);
}

BEGIN_MESSAGE_MAP(CNewMapDialog, CResizeDialog)
	ON_EN_CHANGE(IDC_NEW_MAP_NAME_EDIT, OnChangeNewMapNameEdit)
	ON_BN_CLICKED(IDC_NEW_MAP_NAME_BROWSE_BUTTON, OnNewMapNameBrowseButton)
	ON_BN_CLICKED(IDC_NEW_MAP_SQUARE_CHECK_BOX, OnNewMapSquareCheckBox)
	ON_CBN_SELCHANGE(IDC_NEW_MAP_SIZE_X_COMBO_BOX, OnSelchangeNewMapSizeXComboBox)
END_MESSAGE_MAP()

void CNewMapDialog::GetAllMODs( std::vector<std::string> *pMODsList )
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

std::string CNewMapDialog::GetMODKey()
{ 
	if ( ( resizeDialogOptions.szParameters[2].empty() ) || ( resizeDialogOptions.szParameters[2] == RMGC_NO_MOD_FOLDER ) )
	{
		return "";
	}
	return resizeDialogOptions.szParameters[2];
}

void CNewMapDialog::LoadControls()
{
	for ( int nSize = 1; nSize <= 32; ++nSize )
	{
		int nStringNumber = wndSizeXComboBox.AddString( NStr::Format( "%d", nSize ) );
		wndSizeXComboBox.SetItemData( nStringNumber, nSize );
		nStringNumber = wndSizeYComboBox.AddString( NStr::Format( "%d", nSize ) );
		wndSizeYComboBox.SetItemData( nStringNumber, nSize );
	}
	if ( resizeDialogOptions.nParameters[2] > 0 )
	{
		resizeDialogOptions.nParameters[1] = resizeDialogOptions.nParameters[0];
	}
	wndSizeXComboBox.SelectString( -1, NStr::Format( "%d", resizeDialogOptions.nParameters[0] ) );
	wndSizeYComboBox.SelectString( -1, NStr::Format( "%d", resizeDialogOptions.nParameters[1] ) );
	CheckDlgButton( IDC_NEW_MAP_SQUARE_CHECK_BOX, resizeDialogOptions.nParameters[2] > 0 ? BST_CHECKED : BST_UNCHECKED );
	for ( int nSeasonIndex = 0; nSeasonIndex < CMapInfo::SEASON_COUNT; ++nSeasonIndex )
	{
		int nStringNumber = wndSeasonComboBox.AddString( CMapInfo::SEASON_NAMES[nSeasonIndex] );
		wndSeasonComboBox.SetItemData( nStringNumber, nSeasonIndex );
	}
	wndSeasonComboBox.SelectString( -1, CMapInfo::SEASON_NAMES[resizeDialogOptions.nParameters[3] ] );
	
	wndFileName.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );

	std::vector<std::string> modsFolders;
	GetAllMODs( &modsFolders );
	modsFolders.push_back( RMGC_NO_MOD_FOLDER );
	modsFolders.push_back( RMGC_CURRENT_MOD_FOLDER );
	bool bMODExists = false;
	for ( std::vector<std::string>::const_iterator modFolderIterator = modsFolders.begin(); modFolderIterator != modsFolders.end(); ++ modFolderIterator )
	{
		int nStringNumber = wndMODComboBox.AddString( modFolderIterator->c_str() );
		if ( resizeDialogOptions.szParameters[2] == ( *modFolderIterator ) )
		{
			bMODExists = true;	
		}
	}
	if ( resizeDialogOptions.szParameters[2].empty() || ( !bMODExists ) )
	{
		wndMODComboBox.SelectString( -1, RMGC_NO_MOD_FOLDER );
		resizeDialogOptions.szParameters[2].clear();
	}
	else
	{
		wndMODComboBox.SelectString( -1, resizeDialogOptions.szParameters[2].c_str() );
	}
}

void CNewMapDialog::SaveControls()
{
	int nStringNumber = wndSizeXComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		resizeDialogOptions.nParameters[0] = wndSizeXComboBox.GetItemData( nStringNumber );
	}
	nStringNumber = wndSizeYComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		resizeDialogOptions.nParameters[1] = wndSizeYComboBox.GetItemData( nStringNumber );
	}
	resizeDialogOptions.nParameters[2] = IsDlgButtonChecked( IDC_NEW_MAP_SQUARE_CHECK_BOX ) ? 1 : 0;
	nStringNumber = wndSeasonComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		resizeDialogOptions.nParameters[3] = wndSeasonComboBox.GetItemData( nStringNumber );
	}

	CString strString;
	wndFileName.GetWindowText( strString );
	resizeDialogOptions.szParameters[0] = strString;

	nStringNumber = wndMODComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		wndMODComboBox.GetLBText( nStringNumber, strString );
		resizeDialogOptions.szParameters[2] = strString;
	}
}

BOOL CNewMapDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.nParameters.size() < 4 )
	{
		resizeDialogOptions.nParameters.resize( 4 );
		resizeDialogOptions.nParameters[0] = 8;
		resizeDialogOptions.nParameters[1] = 8;
		resizeDialogOptions.nParameters[2] = 1;
		resizeDialogOptions.nParameters[3] = 0;
	}

	if ( resizeDialogOptions.szParameters.size() < 3 )
	{
		resizeDialogOptions.szParameters.resize( 3 );
		resizeDialogOptions.szParameters[0] = _T( "new_blitzkrieg_map" );
		resizeDialogOptions.szParameters[2].clear();
	}

	LoadControls();
	UpdateControls();
	return TRUE;
}

void CNewMapDialog::OnOK() 
{
	SaveControls();
	CResizeDialog::OnOK();
}

void CNewMapDialog::OnCancel() 
{
	SaveControls();
	CResizeDialog::OnCancel();
}

void CNewMapDialog::UpdateControls()
{
	CString strText;
	wndFileName.GetWindowText( strText );
	
	if ( CWnd *pWnd = GetDlgItem( IDOK ) )
	{
		pWnd->EnableWindow( !strText.IsEmpty() );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_NEW_MAP_SIZE_Y_COMBO_BOX ) )
	{
		pWnd->EnableWindow( !IsDlgButtonChecked( IDC_NEW_MAP_SQUARE_CHECK_BOX ) );
	}
}

void CNewMapDialog::OnChangeNewMapNameEdit() 
{
	UpdateControls();
}

void CNewMapDialog::OnNewMapNameBrowseButton() 
{
	std::string szInitialDir;
	int nSlashPosition = resizeDialogOptions.szParameters[1].rfind( '\\' );
	if ( nSlashPosition != std::string::npos )
	{
		szInitialDir = resizeDialogOptions.szParameters[1].substr( 0, nSlashPosition );
	}
	
	CFileDialog fileDialog( true, ".xml", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "All supported Files (*.bzm; *.xml)|*.bzm; *.xml|XML files (*.xml)|*.xml|BZM files (*.bzm)|*.bzm|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = szInitialDir.c_str();

	if ( fileDialog.DoModal() == IDOK )
	{
		resizeDialogOptions.szParameters[0] = fileDialog.GetPathName();
		resizeDialogOptions.szParameters[1] = resizeDialogOptions.szParameters[0];
		wndFileName.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );
		UpdateControls();
	}
}

void CNewMapDialog::OnSelchangeNewMapSizeXComboBox() 
{
	int nStringNumber = wndSizeXComboBox.GetCurSel();
	if ( nStringNumber >= 0 )
	{
		resizeDialogOptions.nParameters[0] = wndSizeXComboBox.GetItemData( nStringNumber );
	}
	if ( IsDlgButtonChecked( IDC_NEW_MAP_SQUARE_CHECK_BOX ) )
	{
		resizeDialogOptions.nParameters[1] = resizeDialogOptions.nParameters[0];
		wndSizeYComboBox.SelectString( -1, NStr::Format( "%d", resizeDialogOptions.nParameters[1] ) );
	}
}

void CNewMapDialog::OnNewMapSquareCheckBox() 
{
	if ( IsDlgButtonChecked( IDC_NEW_MAP_SQUARE_CHECK_BOX ) )
	{
		resizeDialogOptions.nParameters[1] = resizeDialogOptions.nParameters[0];
		wndSizeYComboBox.SelectString( -1, NStr::Format( "%d", resizeDialogOptions.nParameters[1] ) );
	}
	UpdateControls();
}
