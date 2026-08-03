#include "StdAfx.h"

#include "frames.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "TabSimpleObjectsDialog.h"

#include "../RandomMapGen/Resource_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"

#include "RMG_CreateFieldDialog.h"
#include "RMG_FieldTerrainDialog.h"
#include "RMG_FieldObjectsDialog.h"
#include "RMG_FieldHeightsDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char *CF_FIELDS_XML_NAME = "Fields";
const char *CF_FIELDS_FILE_NAME = "Editor\\DefaultFields";
const char *CF_FIELDS_DIALOG_TITLE = "Fields Composer";

const int   CF_FIELDS_COLUMN_START = 1;
const int   CF_FIELDS_COLUMN_COUNT = 8;
const char *CF_FIELDS_COLUMN_NAME  [CF_FIELDS_COLUMN_COUNT] = { "Path", "Season", "Terrain Shells", "Objects Shells", "Profile", "Height", "Pattern Size", "Positive Ratio %" };
const int   CF_FIELDS_COLUMN_FORMAT[CF_FIELDS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
const int		CF_FIELDS_COLUMN_WIDTH [CF_FIELDS_COLUMN_COUNT] = { 200, 80, 60, 60, 100, 60, 60, 60 };

const char* CRMGCreateFieldDialog::FIELD_TAB_LABELS[FIELD_TAB_COUNT] =
{
	"Terrain",
	"Objects",
	"Heights",
};

int CALLBACK CF_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateFieldDialog* pFieldDialog = reinterpret_cast<CRMGCreateFieldDialog*>( lParamSort );

	CString strItem1 = pFieldDialog->m_FieldsList.GetItemText( lParam1, pFieldDialog->nSortColumn );
	CString strItem2 = pFieldDialog->m_FieldsList.GetItemText( lParam2, pFieldDialog->nSortColumn );
	if ( pFieldDialog->bFieldsSortParam[pFieldDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CRMGCreateFieldDialog::vID[] = 
{
	IDC_RMG_CF_DELIMITER_00,

	IDC_RMG_CF_FIELDS_LABEL,
	IDC_RMG_CF_FIELDS_LIST,
	IDC_RMG_CF_ADD_FIELD_BUTTON,
	IDC_RMG_CF_DELETE_FIELD_BUTTON,
	IDC_RMG_CF_CHECK_FIELDS_BUTTON,

	IDC_RMG_CF_FIELD_PROPERTIES_PLACEHOLDER,

	IDC_RMG_CF_SAVE_BUTTON,
	IDOK,
	IDCANCEL,
};

CRMGCreateFieldDialog::CRMGCreateFieldDialog( CWnd* pParent )
	: CResizeDialog( CRMGCreateFieldDialog::IDD, pParent ), nSortColumn( 0 ), bCreateControls( true ), pInput3DTabWindow( 0 ) 
{

	pInput3DTabWindow = new CInput3DTabWindow();

	SetControlStyle( IDC_RMG_CF_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_FIELDS_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CF_FIELDS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CF_ADD_FIELD_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CF_DELETE_FIELD_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CF_CHECK_FIELDS_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_RMG_CF_FIELD_PROPERTIES_PLACEHOLDER, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( IDC_RMG_CF_FIELD_PROPERTIES_TAB, ANCHORE_LEFT_TOP | RESIZE_HOR_VER ); 
	SetControlStyle( IDC_RMG_CF_SAVE_BUTTON, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDOK, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_RIGHT_BOTTOM );
}

CRMGCreateFieldDialog::~CRMGCreateFieldDialog()
{
	if ( pInput3DTabWindow != 0 )
	{
		delete pInput3DTabWindow;
		pInput3DTabWindow = 0;
	}
}

void CRMGCreateFieldDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RMG_CF_FIELDS_LIST, m_FieldsList);
}

BEGIN_MESSAGE_MAP(CRMGCreateFieldDialog, CResizeDialog)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CF_FIELDS_LIST, OnColumnclickFieldsList)
	ON_BN_CLICKED(IDC_RMG_CF_ADD_FIELD_BUTTON, OnAddFieldButton)
	ON_BN_CLICKED(IDC_RMG_CF_DELETE_FIELD_BUTTON, OnDeleteFieldButton)
	ON_BN_CLICKED(IDC_RMG_CF_CHECK_FIELDS_BUTTON, OnCheckFieldsButton)
	ON_COMMAND(IDC_RMG_CF_ADD_FIELD_MENU, OnAddFieldMenu)
	ON_COMMAND(IDC_RMG_CF_DELETE_FIELD_MENU, OnDeleteFieldMenu)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CF_FIELDS_LIST, OnItemchangedFieldsList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CF_FIELDS_LIST, OnRclickFieldsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CF_FIELDS_LIST, OnKeydownFieldsList)
	ON_BN_CLICKED(IDC_RMG_CF_SAVE_BUTTON, OnSaveButton)
END_MESSAGE_MAP()

BOOL CRMGCreateFieldDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.szParameters.size() < 5 )
	{
		resizeDialogOptions.szParameters.resize( 5, "" );
	}
	if ( resizeDialogOptions.szParameters[2].empty() )
	{
		resizeDialogOptions.szParameters[2] = CF_FIELDS_FILE_NAME;
	}
	if ( resizeDialogOptions.nParameters.size() < ( CF_FIELDS_COLUMN_COUNT + 1 ) )
	{
		resizeDialogOptions.nParameters.resize( CF_FIELDS_COLUMN_COUNT + 1, 0 );
		resizeDialogOptions.nParameters[0] = 1;
	}
	
	CreateControls();
	LoadFieldsList();
	UpdateControls();
	return true;
}	

bool CRMGCreateFieldDialog::LoadFieldsList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CF_FIELDS_DIALOG_TITLE, resizeDialogOptions.szParameters[2] ) );
	BeginWaitCursor();
	LoadDataResource( resizeDialogOptions.szParameters[2], "", false, 0, CF_FIELDS_XML_NAME, fields );
	
	m_FieldsList.DeleteAllItems();
	for ( CRMFieldSetsHashMap::const_iterator fieldIterator = fields.begin();  fieldIterator != fields.end(); ++fieldIterator )
	{
		int nNewItem = m_FieldsList.InsertItem( LVIF_TEXT, 0, fieldIterator->first.c_str(), 0, 0, 0, 0 );
		if ( nNewItem == ( CB_ERR ) )
		{
			EndWaitCursor();
			return false;
		}

		SetFieldItem( nNewItem, fieldIterator->second );
	}
	LoadFieldToControls();
	EndWaitCursor();
	return true;
}

bool CRMGCreateFieldDialog::SaveFieldsList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CF_FIELDS_DIALOG_TITLE, resizeDialogOptions.szParameters[2] ) );
	BeginWaitCursor();
	for ( CRMFieldSetsHashMap::const_iterator fieldIterator = fields.begin();  fieldIterator != fields.end(); ++fieldIterator )
	{
		SRMFieldSet rmField = fieldIterator->second;
		if ( !SaveDataResource( fieldIterator->first, "", false, 1, RMGC_FIELDSET_XML_NAME, rmField ) )
		{
			return false;
		}
	}
	if ( !SaveDataResource( resizeDialogOptions.szParameters[2], "", false, 0, CF_FIELDS_XML_NAME, fields ) )
	{
		EndWaitCursor();
		return false;
	}
	EndWaitCursor();
	return true;
}

bool CRMGCreateFieldDialog::LoadFieldToControls()
{
	bCreateControls = true;

	GetRMGFieldTerrainDialog()->pRMFieldSet = 0;
	GetRMGFieldObjectsDialog()->pRMFieldSet = 0;
	GetRMGFieldHeightsDialog()->pRMFieldSet = 0;
	
	GetRMGFieldTerrainDialog()->fieldSets.clear();
	GetRMGFieldObjectsDialog()->fieldSets.clear();
	GetRMGFieldHeightsDialog()->fieldSets.clear();

	if ( m_FieldsList.GetSelectedCount() > 0 )
	{
		int nSelectedItem = m_FieldsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		std::string szKey = m_FieldsList.GetItemText( nSelectedItem, 0 );
		SRMFieldSet &rField = fields[szKey];
		
		GetRMGFieldTerrainDialog()->pRMFieldSet = &rField;
		GetRMGFieldObjectsDialog()->pRMFieldSet = &rField;
		GetRMGFieldHeightsDialog()->pRMFieldSet = &rField;

		while ( nSelectedItem != CB_ERR )
		{
			szKey = m_FieldsList.GetItemText( nSelectedItem, 0 );
			SRMFieldSet &rSelectedField = fields[szKey];

			GetRMGFieldTerrainDialog()->fieldSets.push_back( &rSelectedField );
			GetRMGFieldObjectsDialog()->fieldSets.push_back( &rSelectedField );
			GetRMGFieldHeightsDialog()->fieldSets.push_back( &rSelectedField );
			
			nSelectedItem = m_FieldsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
		}
	}
	GetRMGFieldTerrainDialog()->LoadFieldToControls();
	GetRMGFieldObjectsDialog()->LoadFieldToControls();
	GetRMGFieldHeightsDialog()->LoadFieldToControls();
	
	bCreateControls = false;
	return true;
}

bool CRMGCreateFieldDialog::UpdateFieldList( const SRMFieldSet *pRMFieldSet )
{
	{
		int nSelectedItem = m_FieldsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		while ( nSelectedItem != CB_ERR )
		{
			std::string szKey = m_FieldsList.GetItemText( nSelectedItem, 0 );
			SRMFieldSet &rField = fields[szKey];
			if ( &rField == pRMFieldSet )
			{
				SetFieldItem( nSelectedItem, rField );
				return true;
			}
			nSelectedItem = m_FieldsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
		}
	}
	return false;
}

void CRMGCreateFieldDialog::SetFieldItem( int nItem, const SRMFieldSet &rField )
{
	std::string szSeasonName;
	RMGGetSeasonNameString( rField.nSeason, rField.szSeasonFolder, &szSeasonName );
	m_FieldsList.SetItem( nItem, 1, LVIF_TEXT, szSeasonName.c_str(), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%d", rField.tilesShells.size() ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%d", rField.objectsShells.size() ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 4, LVIF_TEXT, rField.szProfileFileName.c_str(), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 5, LVIF_TEXT, NStr::Format( "%.2f", rField.fHeight ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 6, LVIF_TEXT, NStr::Format( "%d - %d", rField.patternSize.min, rField.patternSize.max ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 7, LVIF_TEXT, NStr::Format( "%.2f", rField.fPositiveRatio * 100 ), 0, 0, 0, 0 );
}

void CRMGCreateFieldDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CF_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CF_FIELDS_COLUMN_START] = m_FieldsList.GetColumnWidth( nColumnIndex );
	}
	SaveFieldsList();
	CResizeDialog::OnOK();
}

void CRMGCreateFieldDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CF_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CF_FIELDS_COLUMN_START] = m_FieldsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnCancel();
}

void CRMGCreateFieldDialog::CreateControls()
{
	bCreateControls = true;
	
	m_FieldsList.SetExtendedStyle( m_FieldsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CF_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CF_FIELDS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CF_FIELDS_COLUMN_START] = CF_FIELDS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_FieldsList.InsertColumn( nColumnIndex, CF_FIELDS_COLUMN_NAME[nColumnIndex], CF_FIELDS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CF_FIELDS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bFieldsSortParam.push_back( true );
	}

	pInput3DTabWindow->Create( this, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_TOP | TWS_DRAW_3D_NORMAL, IDC_RMG_CF_FIELD_PROPERTIES_TAB );
	if ( CWnd *pWnd = GetDlgItem( IDC_RMG_CF_FIELD_PROPERTIES_PLACEHOLDER ) )
	{
		CRect rect;
		pWnd->GetWindowRect( &rect );
		ScreenToClient( &rect );
		pInput3DTabWindow->MoveWindow( &rect );
	}

	if ( CRMGFieldTerrainDialog *pRMGFieldTerrainDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CRMGFieldTerrainDialog*>( 0 ) ) )
	{
		pRMGFieldTerrainDialog->pRMGCreateFieldDialog = this;
		pRMGFieldTerrainDialog->Create( CRMGFieldTerrainDialog::IDD, pInput3DTabWindow );
		pInput3DTabWindow->AddTab( pRMGFieldTerrainDialog, FIELD_TAB_LABELS[FIELD_TAB_TERRAIN] );
	}

	if ( CRMGFieldObjectsDialog *pRMGFieldObjectsDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CRMGFieldObjectsDialog*>( 0 ) ) )
	{
		pRMGFieldObjectsDialog->pRMGCreateFieldDialog = this;
		pRMGFieldObjectsDialog->bInitialPictures = ( resizeDialogOptions.nParameters[0] > 0 );
		pRMGFieldObjectsDialog->szCurrentFilter = resizeDialogOptions.szParameters[3];
		pRMGFieldObjectsDialog->Create( CRMGFieldObjectsDialog::IDD, pInput3DTabWindow );
		pInput3DTabWindow->AddTab( pRMGFieldObjectsDialog, FIELD_TAB_LABELS[FIELD_TAB_OBJECTS] );
	}

	if ( CRMGFieldHeightsDialog *pRMGFieldHeightsDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CRMGFieldHeightsDialog*>( 0 ) ) )
	{
		pRMGFieldHeightsDialog->pRMGCreateFieldDialog = this;
		pRMGFieldHeightsDialog->Create( CRMGFieldHeightsDialog::IDD, pInput3DTabWindow );
		pInput3DTabWindow->AddTab( pRMGFieldHeightsDialog, FIELD_TAB_LABELS[FIELD_TAB_HEIGHTS] );
	}

	NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == FIELD_TAB_COUNT ) &&
							 ( pInput3DTabWindow->inputTabWindows.size() == FIELD_TAB_COUNT ),
							 NStr::Format( "Wrong tab number: %d\n", pInput3DTabWindow->GetTabCount() ) );

	pInput3DTabWindow->SetTabIcon( FIELD_TAB_TERRAIN,	IDI_RMG_CF_TERRAIN_TAB );
	pInput3DTabWindow->SetTabIcon( FIELD_TAB_OBJECTS,	IDI_RMG_CF_OBJECTS_TAB );
	pInput3DTabWindow->SetTabIcon( FIELD_TAB_HEIGHTS,	IDI_RMG_CF_HEIGHTS_TAB );

	pInput3DTabWindow->ActivateTab( FIELD_TAB_TERRAIN );
	UpdateControlPositions();

	bCreateControls = false;
}

CRMGFieldTerrainDialog* CRMGCreateFieldDialog::GetRMGFieldTerrainDialog()
{
	if ( !pInput3DTabWindow->inputTabWindows.empty() )
	{
		return dynamic_cast<CRMGFieldTerrainDialog*>( pInput3DTabWindow->inputTabWindows[FIELD_TAB_TERRAIN] );
	}
	else
	{
		return 0;
	}
}

CRMGFieldObjectsDialog* CRMGCreateFieldDialog::GetRMGFieldObjectsDialog()
{
	if ( !pInput3DTabWindow->inputTabWindows.empty() )
	{
		return dynamic_cast<CRMGFieldObjectsDialog*>( pInput3DTabWindow->inputTabWindows[FIELD_TAB_OBJECTS] );
	}
	else
	{
		return 0;
	}
}

CRMGFieldHeightsDialog* CRMGCreateFieldDialog::GetRMGFieldHeightsDialog()
{
	if ( !pInput3DTabWindow->inputTabWindows.empty() )
	{
		return dynamic_cast<CRMGFieldHeightsDialog*>( pInput3DTabWindow->inputTabWindows[FIELD_TAB_HEIGHTS] );
	}
	else
	{
		return 0;
	}
}

void CRMGCreateFieldDialog::ClearControls()
{
}

void CRMGCreateFieldDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	if ( pWnd = GetDlgItem( IDC_RMG_CT_DELETE_FIELD_BUTTON ) )
	{
		pWnd->EnableWindow( m_FieldsList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CF_CHECK_FIELDS_BUTTON ) )
	{
		pWnd->EnableWindow( m_FieldsList.GetItemCount() > 0 );
	}
}

void CRMGCreateFieldDialog::OnColumnclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CF_FIELDS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CF_FIELDS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_FieldsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_FieldsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_FieldsList.SortItems( CF_FieldsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bFieldsSortParam[nSortColumn] = !bFieldsSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateFieldDialog::OnFileNew() 
{
	SaveFieldsList();
	resizeDialogOptions.szParameters[2] = CF_FIELDS_FILE_NAME;
	LoadFieldsList();
	UpdateControls();
}

void CRMGCreateFieldDialog::OnFileOpen() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	SaveFieldsList();

	CFileDialog fileDialog( true, ".xml", "", 0, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
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
		resizeDialogOptions.szParameters[2] = szFilePath;
		
		LoadFieldsList();
		UpdateControls();
	}
}

void CRMGCreateFieldDialog::OnFileSave() 
{
	SaveFieldsList();
}

void CRMGCreateFieldDialog::OnSaveButton() 
{
	SaveFieldsList();
}

void CRMGCreateFieldDialog::OnFileSaveas() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( false, ".xml", "", OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
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
		resizeDialogOptions.szParameters[2] = szFilePath;
		SaveFieldsList();
	}
}

void CRMGCreateFieldDialog::OnFileExit() 
{
	OnOK();
}

void CRMGCreateFieldDialog::OnAddFieldButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( true, ".xml", "", OFN_ALLOWMULTISELECT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[1].c_str();

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
				resizeDialogOptions.szParameters[1] = szFilePath.substr( 0, nSlashIndex );
			}

			szFilePath = szFilePath.substr( szStorageName.size() );
			
			SRMFieldSet fieldToAdd;

			fieldToAdd.fHeight = 2.0f;
			fieldToAdd.fPositiveRatio = 0.5f;
			fieldToAdd.nSeason = CMapInfo::REAL_SEASONS[CMapInfo::SEASON_SUMMER];
			fieldToAdd.szSeasonFolder = CMapInfo::SEASON_FOLDERS[CMapInfo::SEASON_SUMMER];
			fieldToAdd.patternSize.min = 3;
			fieldToAdd.patternSize.max = 5;
			fieldToAdd.szProfileFileName = "scenarios\\profiles\\profile";
			fieldToAdd.tilesShells.clear();
			fieldToAdd.objectsShells.clear();
			LoadDataResource( szFilePath, "", false, 0, RMGC_FIELDSET_XML_NAME, fieldToAdd );
			
			LVFINDINFO findInfo;
			findInfo.flags = LVFI_STRING;
			findInfo.psz = szFilePath.c_str();

			int nOldItem = m_FieldsList.FindItem( &findInfo, CB_ERR );
			if ( nOldItem != ( CB_ERR ) )
			{
				m_FieldsList.DeleteItem( nOldItem );
			}
			
			int nNewItem = m_FieldsList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
			if ( nNewItem != ( CB_ERR ) )
			{
				fields[szFilePath] = fieldToAdd;
				SetFieldItem( nNewItem, fieldToAdd );
			}
		}		
		LoadFieldToControls();
		UpdateControls();
		EndWaitCursor();
	}
	delete[] fileDialog.m_ofn.lpstrFile;
}

void CRMGCreateFieldDialog::OnDeleteFieldButton() 
{
	CString strTitle;
	strTitle.LoadString( IDR_EDITORTYPE );
	if ( MessageBox( "Do you really want to DELETE selected Fields?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
	{
		int nSelectedItem = m_FieldsList.GetNextItem( CB_ERR, LVIS_SELECTED );
		if ( nSelectedItem != ( CB_ERR ) )
		{
			bCreateControls = true;
			while ( nSelectedItem != ( CB_ERR ) )
			{
				std::string szKey = m_FieldsList.GetItemText( nSelectedItem, 0 );
				m_FieldsList.DeleteItem( nSelectedItem );
				fields.erase( szKey );
				nSelectedItem = m_FieldsList.GetNextItem( CB_ERR, LVIS_SELECTED );
			}
			bCreateControls = false;
			LoadFieldToControls();
			UpdateControls();
		}
	}
}

void CRMGCreateFieldDialog::OnRclickFieldsList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 9 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CF_ADD_FIELD_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CF_ADD_FIELD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CF_DELETE_FIELD_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CF_DELETE_FIELD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateFieldDialog::OnKeydownFieldsList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CF_ADD_FIELD_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddFieldButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CF_DELETE_FIELD_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteFieldButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateFieldDialog::OnAddFieldMenu() 
{
	OnAddFieldButton();
}

void CRMGCreateFieldDialog::OnDeleteFieldMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CF_DELETE_FIELD_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteFieldButton();
		}
	}
}

void CRMGCreateFieldDialog::OnCheckFieldsButton() 
{
	if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
	{
		BeginWaitCursor();
		std::vector<STilesetDesc> tileSetDescs;
		tileSetDescs.reserve( CMapInfo::SEASON_COUNT );
		for ( int nSeasonIndex = 0; nSeasonIndex < CMapInfo::SEASON_COUNT; ++nSeasonIndex )
		{
			tileSetDescs.push_back( STilesetDesc() );
			LoadDataResource( NStr::Format( "%stileset", CMapInfo::SEASON_FOLDERS[nSeasonIndex] ), "", false, 0, "tileset", tileSetDescs[nSeasonIndex] );
		}
		CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();

		for ( CRMFieldSetsHashMap::iterator fieldSetIterator = fields.begin(); fieldSetIterator != fields.end(); ++fieldSetIterator )
		{
			SRMFieldSet &rFieldSet = fieldSetIterator->second;
			if ( ( rFieldSet.nSeason < 0 ) || ( rFieldSet.nSeason >= CMapInfo::REAL_SEASONS_COUNT ) )
			{
				rFieldSet.nSeason = CMapInfo::REAL_SEASONS[CMapInfo::SEASON_SUMMER];
				rFieldSet.szSeasonFolder = CMapInfo::SEASON_FOLDERS[CMapInfo::SEASON_SUMMER];
			}
			const STilesetDesc &rTileSetDesc = tileSetDescs[CMapInfo::GetSelectedSeason( rFieldSet.nSeason, rFieldSet.szSeasonFolder )];

			if ( rFieldSet.fHeight < 0.0f )
			{
				rFieldSet.fHeight = 0.0f;
			}
			else if ( rFieldSet.fHeight > 5.0f )
			{
				rFieldSet.fHeight = 5.0f;
			}
			if ( rFieldSet.fPositiveRatio < 0.0f )
			{
				rFieldSet.fPositiveRatio = 0.0f;
			}
			else if ( rFieldSet.fPositiveRatio > 1.0f )
			{
				rFieldSet.fPositiveRatio = 1.0f;
			}
			{
				CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( rFieldSet.szProfileFileName + ".tga" ).c_str(), STREAM_ACCESS_READ );
				if ( pStream == 0 )
				{
					rFieldSet.szProfileFileName = "scenarios\\profiles\\profile";
				}
			}
			if ( rFieldSet.patternSize.min < 1 )
			{
				rFieldSet.patternSize.min = 1;
			}
			else if ( rFieldSet.patternSize.min > 16 )
			{
				rFieldSet.patternSize.min = 16;
			}
			if ( rFieldSet.patternSize.max < 1 )
			{
				rFieldSet.patternSize.max = 1;
			}
			else if ( rFieldSet.patternSize.max > 16 )
			{
				rFieldSet.patternSize.max = 16;
			}
			if ( rFieldSet.patternSize.min > rFieldSet.patternSize.max )
			{
				const int nMin = rFieldSet.patternSize.min;
				rFieldSet.patternSize.min = rFieldSet.patternSize.max;
				rFieldSet.patternSize.max = nMin;
			}
			for ( CRMTileSet::iterator tileShellIterator = rFieldSet.tilesShells.begin(); tileShellIterator != rFieldSet.tilesShells.end(); ++tileShellIterator )
			{
				SRMTileSetShell &rTileSetShell = ( *tileShellIterator );
				if ( rTileSetShell.fWidth < 0.0f )
				{
					rTileSetShell.fWidth = 0.0f;
				}
				else if ( rTileSetShell.fWidth > 512.0f )
				{
					rTileSetShell.fWidth = 512.0f;
				}
				for ( int nShellElementIndex = 0; nShellElementIndex < rTileSetShell.tiles.size(); )
				{
					if ( ( rTileSetShell.tiles[nShellElementIndex] < 0 ) ||
							 ( rTileSetShell.tiles[nShellElementIndex] >= rTileSetDesc.terrtypes.size() ) )
					{
						rTileSetShell.tiles.erase( nShellElementIndex );
					}
					else if ( rTileSetShell.tiles.GetWeight( nShellElementIndex ) < 0 )
					{
						rTileSetShell.tiles.SetWeight( nShellElementIndex, 0 );
						++nShellElementIndex;
					}
					else
					{
						++nShellElementIndex;
					}
				}
			}
			for ( CRMObjectSet::iterator objectShellIterator = rFieldSet.objectsShells.begin(); objectShellIterator != rFieldSet.objectsShells.end(); ++objectShellIterator )
			{
				SRMObjectSetShell &rObjectSetShell = ( *objectShellIterator );
				if ( rObjectSetShell.fWidth < 0.0f )
				{
					rObjectSetShell.fWidth = 0.0f;
				}
				else if ( rObjectSetShell.fWidth > 512.0f )
				{
					rObjectSetShell.fWidth = 512.0f;
				}
				if ( rObjectSetShell.fRatio < 0.0f )
				{
					rObjectSetShell.fRatio = 0.0f;
				}
				else if ( rObjectSetShell.fRatio > 1.0f )
				{
					rObjectSetShell.fRatio = 1.0f;
				}
				if ( rObjectSetShell.nBetweenDistance <= 0 )
				{
					rObjectSetShell.nBetweenDistance = 1;
				}
				for ( int nShellElementIndex = 0; nShellElementIndex < rObjectSetShell.objects.size(); )
				{
					if ( pTabSimpleObjectsDialog->objectsImageIndices.find( rObjectSetShell.objects[nShellElementIndex] ) == pTabSimpleObjectsDialog->objectsImageIndices.end() )
					{
						rObjectSetShell.objects.erase( nShellElementIndex );
					}
					else if ( rObjectSetShell.objects.GetWeight( nShellElementIndex ) < 0 )
					{
						rObjectSetShell.objects.SetWeight( nShellElementIndex, 0 );
						++nShellElementIndex;
					}
					else
					{
						++nShellElementIndex;
					}
				}
			}
		}
		LoadFieldToControls();
		UpdateControls();
		EndWaitCursor();
	}
}

void CRMGCreateFieldDialog::OnItemchangedFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bCreateControls )
	{
		LoadFieldToControls();
		UpdateControls();
	}
	*pResult = 0;
}
