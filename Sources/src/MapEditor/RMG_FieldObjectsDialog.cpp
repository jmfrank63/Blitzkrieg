#include "StdAfx.h"

#include "frames.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "TabSimpleObjectsDialog.h"

#include "ValuesCollector.h"

#include "RMG_CreateFieldDialog.h"
#include "RMG_FieldObjectsDialog.h"
#include "RMG_FieldObjectsShellPropertiesDialog.h"
#include "RMG_FieldObjectPropertiesDialog.h"

#include "..\RandomMapGen\RMG_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int   CFO_SHELLS_COLUMN_START = 0;
const int   CFO_SHELLS_COLUMN_COUNT = 5;
const char *CFO_SHELLS_COLUMN_NAME  [CFO_SHELLS_COLUMN_COUNT] = { "N", "Objects Count", "Size", "Step", "Probability %" };
const int   CFO_SHELLS_COLUMN_FORMAT[CFO_SHELLS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
const int		CFO_SHELLS_COLUMN_WIDTH [CFO_SHELLS_COLUMN_COUNT] = { 30, 80, 80, 80, 80 };

const int CRMGFieldObjectsDialog::DEFAULT_OBJECT_WEIGHT = 1;
const float CRMGFieldObjectsDialog::DEFAULT_SHELL_WIDTH = 2.0f;
const int CRMGFieldObjectsDialog::DEFAULT_SHELL_STEP = 4;
const float CRMGFieldObjectsDialog::DEFAULT_SHELL_RATIO = 0.3f;
const char CRMGFieldObjectsDialog::UNKNOWN_OBJECT[] = "Unknown";
const char CRMGFieldObjectsDialog::MULTIPLE_SELECTION[] = "Multiple selection...";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CALLBACK CFO_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGFieldObjectsDialog* pFieldObjectsDialog = reinterpret_cast<CRMGFieldObjectsDialog*>( lParamSort );

	CString strItem1 = pFieldObjectsDialog->m_ShellsList.GetItemText( lParam1, pFieldObjectsDialog->nSortColumn );
	CString strItem2 = pFieldObjectsDialog->m_ShellsList.GetItemText( lParam2, pFieldObjectsDialog->nSortColumn );

	if ( pFieldObjectsDialog->bShellsSortParam[pFieldObjectsDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CRMGFieldObjectsDialog::CRMGFieldObjectsDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldObjectsDialog::IDD, pParent ), nSortColumn( -1 ), pRMGCreateFieldDialog( 0 ), bCreateControls( true ), pRMFieldSet( 0 ), nCurrentShell( CB_ERR ), bInitialPictures( true )
{
	//{{AFX_DATA_INIT(CRMGFieldObjectsDialog)
	//}}AFX_DATA_INIT

	SetControlStyle( IDC_CF_OS_OBJECTS_FILTER_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_OS_OBJECTS_FILTER_COMBO, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_CF_OS_LIST_RADIO, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_OS_THUMBNAILS_RADIO, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_OS_AVAILABLE_OBJECTS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR | RESIZE_VER, 0.5f, 0.5f, 0.5f, 1.0f );

	SetControlStyle( IDC_CF_OS_SHELLS_LABEL, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_OS_SHELLS_LIST, ANCHORE_RIGHT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 0.5f );
	SetControlStyle( IDC_CF_OS_OBJECTS_LABEL, ANCHORE_RIGHT | ANCHORE_VER_CENTER | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_OS_OBJECTS_LIST, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 0.5f );

	SetControlStyle( IDC_CF_OS_ADD_SHELL, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_OS_REMOVE_SHELL, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_OS_SHELL_PROPERTIES, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	
	SetControlStyle( IDC_CF_OS_ADD_OBJECT, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_OS_REMOVE_OBJECT, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_OS_OBJECT_PROPERTIES, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );

	SetControlStyle( IDOK, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
	SetControlStyle( IDCANCEL, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRMGFieldObjectsDialog)
	DDX_Control(pDX, IDC_CF_OS_OBJECTS_FILTER_COMBO, m_FilterComboBox);
	DDX_Control(pDX, IDC_CF_OS_SHELLS_LIST, m_ShellsList);
	DDX_Control(pDX, IDC_CF_OS_OBJECTS_LIST, m_ObjectsList);
	DDX_Control(pDX, IDC_CF_OS_AVAILABLE_OBJECTS_LIST, m_AvailableObjectsList);
	//}}AFX_DATA_MAP
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CRMGFieldObjectsDialog, CResizeDialog)
	//{{AFX_MSG_MAP(CRMGFieldObjectsDialog)
	ON_BN_CLICKED(IDC_CF_OS_LIST_RADIO, OnListRadio)
	ON_BN_CLICKED(IDC_CF_OS_THUMBNAILS_RADIO, OnThumbnailsRadio)
	ON_CBN_SELCHANGE(IDC_CF_OS_OBJECTS_FILTER_COMBO, OnSelchangeObjectsFilterCombo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_OS_SHELLS_LIST, OnItemchangedShellsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_OS_AVAILABLE_OBJECTS_LIST, OnItemchangedAvailableObjectsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_OS_OBJECTS_LIST, OnItemchangedObjectsList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_CF_OS_SHELLS_LIST, OnColumnclickShellsList)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_CF_OS_OBJECT_PROPERTIES, OnObjectProperties)
	ON_BN_CLICKED(IDC_CF_OS_ADD_OBJECT, OnAddObject)
	ON_BN_CLICKED(IDC_CF_OS_REMOVE_OBJECT, OnRemoveObject)
	ON_BN_CLICKED(IDC_CF_OS_SHELL_PROPERTIES, OnShellProperties)
	ON_BN_CLICKED(IDC_CF_OS_REMOVE_SHELL, OnRemoveShell)
	ON_BN_CLICKED(IDC_CF_OS_ADD_SHELL, OnAddShell)
	ON_COMMAND(IDC_CF_OS_ADD_SHELL_MENU, OnAddShellMenu)
	ON_COMMAND(IDC_CF_OS_REMOVE_SHELL_MENU, OnRemoveShellMenu)
	ON_COMMAND(IDC_CF_OS_SHELL_PROPERTIES_MENU, OnShellPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_OS_SHELLS_LIST, OnDblclkShellsList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_OS_SHELLS_LIST, OnRclickShellsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_OS_SHELLS_LIST, OnKeydownShellsList)
	ON_COMMAND(IDC_CF_OS_ADD_OBJECT_MENU, OnAddObjectMenu)
	ON_COMMAND(IDC_CF_OS_REMOVE_OBJECT_MENU, OnRemoveObjectMenu)
	ON_COMMAND(IDC_CF_OS_OBJECT_PROPERTIES_MENU, OnObjectPropertiesMenu)
	ON_COMMAND(IDC_CF_OS_AVAILABLE_OBJECT_PROPERTIES_MENU, OnAvailableObjectPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_OS_OBJECTS_LIST, OnDblclkObjectsList)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_OS_AVAILABLE_OBJECTS_LIST, OnDblclkAvailableObjectsList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_OS_OBJECTS_LIST, OnRclickObjectsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_OS_OBJECTS_LIST, OnKeydownObjectsList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_OS_AVAILABLE_OBJECTS_LIST, OnRclickAvailableObjectsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_OS_AVAILABLE_OBJECTS_LIST, OnKeydownAvailableObjectsList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CRMGFieldObjectsDialog::OnInitDialog()
{
  CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.nParameters.size() < CFO_SHELLS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( CFO_SHELLS_COLUMN_COUNT, 0 );
	}
	
	CreateControls();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::SetObjectsListsStyle( bool bPictures )
{
	if ( ::IsWindow( m_AvailableObjectsList.m_hWnd ) )
	{
		if ( bPictures )
		{
			m_AvailableObjectsList.ModifyStyle( LVS_LIST, LVS_ICON );
		}
		else
		{
			m_AvailableObjectsList.ModifyStyle( LVS_ICON, LVS_LIST );
		}
		
		m_AvailableObjectsList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																					 TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
																					 TEFConsts::THUMBNAILTILE_HEIGHT +
																					 TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
		m_AvailableObjectsList.Arrange( LVA_DEFAULT );
	}
	if ( ::IsWindow( m_ObjectsList.m_hWnd ) )
	{
		if ( bPictures )
		{
			m_ObjectsList.ModifyStyle( LVS_LIST, LVS_ICON );
		}
		else
		{
			m_ObjectsList.ModifyStyle( LVS_ICON, LVS_LIST );
		}
		
		m_ObjectsList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																	TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
																	TEFConsts::THUMBNAILTILE_HEIGHT +
																	TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
		m_ObjectsList.Arrange( LVA_DEFAULT );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::UpdateObjectsListsStyle()
{
	bCreateControls = true;
	int nCheckedButton = GetCheckedRadioButton( IDC_CF_OS_LIST_RADIO, IDC_CF_OS_THUMBNAILS_RADIO );
	SetObjectsListsStyle( nCheckedButton == IDC_CF_OS_THUMBNAILS_RADIO );

	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->resizeDialogOptions.nParameters[0] = ( nCheckedButton - IDC_CF_OS_LIST_RADIO );
	}
	bCreateControls = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::CreateControls()
{
	bCreateControls = true;

	CheckRadioButton( IDC_CF_OS_LIST_RADIO, IDC_CF_OS_THUMBNAILS_RADIO, bInitialPictures ? IDC_CF_OS_THUMBNAILS_RADIO : IDC_CF_OS_LIST_RADIO );
	
	CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();
	const TFilterHashMap &rAllFilters = pTabSimpleObjectsDialog->m_allFilters;

	m_FilterComboBox.ResetContent();
	for( TFilterHashMap::const_iterator filtersIterator = rAllFilters.begin(); filtersIterator != rAllFilters.end(); ++filtersIterator )
	{
		const std::string szFilter = filtersIterator->first;
		if ( szFilter != "" )
		{
			m_FilterComboBox.AddString( szFilter.c_str() );		
		}
	}
	m_FilterComboBox.SelectString( CB_ERR, szCurrentFilter.c_str() );

	UpdateObjectsListsStyle();

	m_AvailableObjectsList.SetImageList( &( pTabSimpleObjectsDialog->objectsImageList ), LVSIL_NORMAL );
	m_ObjectsList.SetImageList( &( pTabSimpleObjectsDialog->objectsImageList ), LVSIL_NORMAL );

	m_ShellsList.SetExtendedStyle( m_ShellsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CFO_SHELLS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CFO_SHELLS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CFO_SHELLS_COLUMN_START] = CFO_SHELLS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_ShellsList.InsertColumn( nColumnIndex, CFO_SHELLS_COLUMN_NAME[nColumnIndex], CFO_SHELLS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CFO_SHELLS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bShellsSortParam.push_back( true );
	}

	UpdateObjectsListsStyle();

	if ( !szCurrentFilter.empty() )
	{
		FillAvailableObjects( szCurrentFilter );
	}

	bCreateControls = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnSize(UINT nType, int cx, int cy) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	UpdateObjectsListsStyle();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::FillAvailableObjects( const std::string &rszFilter )
{
	CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();
	const TFilterHashMap &rAllFilters = pTabSimpleObjectsDialog->m_allFilters;

	m_AvailableObjectsList.DeleteAllItems();
	UpdateObjectsListsStyle();
	TFilterHashMap::const_iterator filterIterator = rAllFilters.find( rszFilter );
	if ( filterIterator != rAllFilters.end() )
	{
		if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
		{
			const int nObjectsCount = pODB->GetNumDescs();
			const SGDBObjectDesc *pAllDesc = pODB->GetAllDescs(); 

			std::vector<std::string> objects;
			std::unordered_map<std::string, int> objectsIndices;
			
			for ( int nObjectIndex = 0; nObjectIndex < nObjectsCount; ++nObjectIndex )
			{
				std::string szObjectPath = pAllDesc[nObjectIndex].szPath;
				NStr::ToLower( szObjectPath );
				if ( pTabSimpleObjectsDialog->CommonFilterName( szObjectPath ) )
				{
					if ( filterIterator->second.Check( szObjectPath ) )
					{
						objects.push_back( pAllDesc[nObjectIndex].GetName() );
						objectsIndices[pAllDesc[nObjectIndex].GetName()] = nObjectIndex;
					}
				}
			}		
			std::sort( objects.begin(), objects.end() );
			
			int nObjectsAdded = 0;
			for ( std::vector<std::string>::const_iterator objectIterator = objects.begin(); objectIterator != objects.end(); ++objectIterator )
			{
				const int nImageIndex = pTabSimpleObjectsDialog->objectsImageIndices[*objectIterator];
				const int nObjectsIndex = m_AvailableObjectsList.InsertItem( nObjectsAdded, objectIterator->c_str(), nImageIndex );
				m_AvailableObjectsList.SetItemData( nObjectsIndex, objectsIndices[*objectIterator] );
				++nObjectsAdded;
			}
		}		
	}
	UpdateObjectsListsStyle();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::LoadFieldToControls()
{
	bCreateControls = true;
	if ( pRMFieldSet )
	{
		m_ShellsList.DeleteAllItems();
		if ( ( nCurrentShell < 0 ) || ( nCurrentShell >= pRMFieldSet->objectsShells.size() ) )
		{
			nCurrentShell = CB_ERR;
		}
		for ( int nShellIndex = 0; nShellIndex < pRMFieldSet->objectsShells.size(); ++nShellIndex )
		{
			const SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nShellIndex];
			int nNewItem = m_ShellsList.InsertItem( LVIF_TEXT, nShellIndex, NStr::Format( "%2d", nShellIndex ), 0, 0, 0, 0 );
			if ( nNewItem != ( CB_ERR ) )
			{
				SetShellItem( nNewItem, rObjectSetShell );
				if ( nShellIndex == nCurrentShell )
				{
					m_ShellsList.SetItemState( nNewItem, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED );
				}
			}
		}
		if ( nSortColumn >= 0 )
		{
			int nItemCount = m_ShellsList.GetItemCount();
			if ( nItemCount > 0 )
			{
				for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
				{
					m_ShellsList.SetItemData( nItemIndex, nItemIndex );	
				}
				m_ShellsList.SortItems( CFO_ShellsCompareFunc, reinterpret_cast<LPARAM>( this ) );
			}
		}
		FillShellObjectsList( nCurrentShell );
	}
	else
	{
		m_ObjectsList.DeleteAllItems();
		m_ShellsList.DeleteAllItems();
	}
	UpdateControls();
	bCreateControls = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_SHELLS_LIST ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_OBJECTS_LIST ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) && ( m_ShellsList.GetSelectedCount() > 0 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_ADD_SHELL ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_REMOVE_SHELL ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_SHELL_PROPERTIES ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_ADD_OBJECT ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_AvailableObjectsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_REMOVE_OBJECT ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_ObjectsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_OS_OBJECT_PROPERTIES ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_ObjectsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::SetShellItem( int nItem, const SRMObjectSetShell &rObjectSetShell )
{
	m_ShellsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%2d", rObjectSetShell.objects.size() ), 0, 0, 0, 0 );
	m_ShellsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%.2f", rObjectSetShell.fWidth ), 0, 0, 0, 0 );
	m_ShellsList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%d", rObjectSetShell.nBetweenDistance ), 0, 0, 0, 0 );
	m_ShellsList.SetItem( nItem, 4, LVIF_TEXT, NStr::Format( "%.2f%", rObjectSetShell.fRatio * 100.0f ), 0, 0, 0, 0 );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::FillShellObjectsList( int nSelectedShell )
{
	if ( pRMFieldSet )
	{
		CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();

		m_ObjectsList.DeleteAllItems();
		UpdateObjectsListsStyle();

		if ( ( nSelectedShell >= 0 )  && ( nSelectedShell < pRMFieldSet->objectsShells.size() ) )
		{
			const SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nSelectedShell];

			//�� �������� ���������
			for ( int nObjectIndex = 0; nObjectIndex < rObjectSetShell.objects.size(); ++nObjectIndex )
			{
				const std::string szSelectedObjectName = rObjectSetShell.objects[nObjectIndex];
				int nImageIndex = 0;
				std::string szObjectName = UNKNOWN_OBJECT;
				if ( pTabSimpleObjectsDialog->objectsImageIndices.find( szSelectedObjectName ) != pTabSimpleObjectsDialog->objectsImageIndices.end() )
				{
					nImageIndex = pTabSimpleObjectsDialog->objectsImageIndices[szSelectedObjectName];
					szObjectName = szSelectedObjectName;
				}
				const int nInsertedItem = m_ObjectsList.InsertItem( nObjectIndex, NStr::Format( "(%d) %s", rObjectSetShell.objects.GetWeight( nObjectIndex ), szObjectName.c_str() ), nImageIndex );
				m_ObjectsList.SetItemData( nInsertedItem, nObjectIndex );
			}
		}
		UpdateObjectsListsStyle();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnDestroy() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CFO_SHELLS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CFO_SHELLS_COLUMN_START] = m_ShellsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog ::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnOK() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnOK();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnCancel() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnCancel();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnListRadio() 
{
	UpdateObjectsListsStyle();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnThumbnailsRadio() 
{
	UpdateObjectsListsStyle();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnItemchangedShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	if ( !bCreateControls )
	{
		bCreateControls = true;
		int nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != ( CB_ERR ) )
		{
			std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
			nCurrentShell = CB_ERR;
			if ( ( sscanf( szKey.c_str(), "%d", &nCurrentShell ) == 1 ) && ( nCurrentShell >= 0 ) )
			{
				FillShellObjectsList( nCurrentShell );
			}
		}
		else
		{
			m_ObjectsList.DeleteAllItems();
		}
		UpdateControls();
		bCreateControls = false;
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnSelchangeObjectsFilterCombo() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		int nSelectedFilter = m_FilterComboBox.GetCurSel();
		std::string szNewFilter;
		if ( nSelectedFilter != CB_ERR )
		{
			CString strBuffer;
			m_FilterComboBox.GetLBText( nSelectedFilter, strBuffer );
			szNewFilter = strBuffer;
		}
		if ( szCurrentFilter != szNewFilter )
		{
			szCurrentFilter = szNewFilter;
			if ( pRMGCreateFieldDialog )
			{
				pRMGCreateFieldDialog->resizeDialogOptions.szParameters[3] = szCurrentFilter;
			}
			FillAvailableObjects( szCurrentFilter );
		}
		UpdateControls();
		bCreateControls = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnItemchangedAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	
	if ( !bCreateControls )
	{
		bCreateControls = true;
		UpdateControls();
		bCreateControls = false;
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnItemchangedObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	
	if ( !bCreateControls )
	{
		bCreateControls = true;
		UpdateControls();
		bCreateControls = false;
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnColumnclickShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CFO_SHELLS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CFO_SHELLS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_ShellsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		bShellsSortParam[nSortColumn] = !bShellsSortParam[nSortColumn];
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_ShellsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_ShellsList.SortItems( CFO_ShellsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnObjectProperties() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->objectsShells.size() ) )
		{
			int nSelectedItem = m_ObjectsList.GetNextItem( CB_ERR, LVNI_SELECTED );
			if ( nSelectedItem != CB_ERR )
			{
				bCreateControls = true;

				SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nCurrentShell];
				CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();
			
				CValuesCollector<std::string> nameCollector( MULTIPLE_SELECTION, "" );
				CValuesCollector<int> weightCollector( "...", 0 );
				CValuesCollector<std::string> pathCollector( MULTIPLE_SELECTION, "" );
				CValuesCollector<std::string> visTypeCollector( MULTIPLE_SELECTION, "" );
				CValuesCollector<std::string> gameTypeCollector( MULTIPLE_SELECTION, "" );
				CValuesCollector<int> imageCollector( "", -1 );
				
				while ( nSelectedItem != CB_ERR )
				{
					const int nSelectedShellElement = m_ObjectsList.GetItemData( nSelectedItem );
					const std::string szSelectedObjectName = rObjectSetShell.objects[nSelectedShellElement];

					int nImageIndex = 0;
					std::string szObjectName = UNKNOWN_OBJECT;
					std::string szPath = UNKNOWN_OBJECT;
					std::string szVisType = UNKNOWN_OBJECT;
					std::string szGameType = UNKNOWN_OBJECT;

					if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
					{
						if ( pTabSimpleObjectsDialog->objectsImageIndices.find( szSelectedObjectName ) != pTabSimpleObjectsDialog->objectsImageIndices.end() )
						{
							if ( const SGDBObjectDesc *pObjectDesc = pODB->GetDesc( szSelectedObjectName.c_str() ) )
							{
								nImageIndex = pTabSimpleObjectsDialog->objectsImageIndices[szSelectedObjectName];
								szObjectName = szSelectedObjectName;

								szPath = pObjectDesc->szPath;
								if ( ( pObjectDesc->eVisType >= 0) && ( pObjectDesc->eVisType < CRMGFieldObjectPropertiesDialog::VIS_TYPES_COUNT ) )
								{
									szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[pObjectDesc->eVisType];
								}
								else
								{
									szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[0];
								}
								if ( ( pObjectDesc->eGameType >= 0) && ( pObjectDesc->eGameType < CRMGFieldObjectPropertiesDialog::GAME_TYPES_COUNT ) )
								{
									szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[pObjectDesc->eGameType];
								}
								else
								{
									szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[0];
								}
							}
						}
					}

					nameCollector.AddValue( szObjectName, "%s" );
					pathCollector.AddValue( szPath, "%s" );
					visTypeCollector.AddValue( szVisType, "%s" );
					gameTypeCollector.AddValue( szGameType, "%s" );
					imageCollector.AddValue( nImageIndex, "%d" );

					weightCollector.AddValue( rObjectSetShell.objects.GetWeight( nSelectedShellElement ), "%d" );

					nSelectedItem = m_ObjectsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}

				CRMGFieldObjectPropertiesDialog fieldObjectPropertiesDialog;
				fieldObjectPropertiesDialog.m_szStats.Format( "Overall weight: %d, average weight: %.2f", rObjectSetShell.objects.weight(), ( 1.0f * rObjectSetShell.objects.weight() ) / rObjectSetShell.objects.size() );
				fieldObjectPropertiesDialog.bDisableEditWeight = false;

				fieldObjectPropertiesDialog.m_szName = nameCollector.GetStringValue().c_str();
				fieldObjectPropertiesDialog.m_szWeight = weightCollector.GetStringValue().c_str();
				fieldObjectPropertiesDialog.m_szPath = pathCollector.GetStringValue().c_str();
				fieldObjectPropertiesDialog.m_szVisType = visTypeCollector.GetStringValue().c_str();
				fieldObjectPropertiesDialog.m_szGameType = gameTypeCollector.GetStringValue().c_str();

				if ( imageCollector.GetValue() >= 0 )
				{
					fieldObjectPropertiesDialog.hIcon = pTabSimpleObjectsDialog->objectsImageList.ExtractIcon( imageCollector.GetValue() );
				}
				else
				{
					fieldObjectPropertiesDialog.hIcon = 0;
				}

				if ( fieldObjectPropertiesDialog.DoModal() == IDOK )
				{
					int nNewWeight = 0;
					if ( ( sscanf( fieldObjectPropertiesDialog.m_szWeight, "%d", &nNewWeight ) == 1 ) && ( nNewWeight >= 0 ) )
					{
						nSelectedItem = m_ObjectsList.GetNextItem( CB_ERR, LVNI_SELECTED );
						while ( nSelectedItem != CB_ERR )
						{
							const int nSelectedShellElement = m_ObjectsList.GetItemData( nSelectedItem );
							const std::string szSelectedObjectName = rObjectSetShell.objects[nSelectedShellElement];

							std::string szObjectName = UNKNOWN_OBJECT;
							if ( pTabSimpleObjectsDialog->objectsImageIndices.find( szSelectedObjectName ) != pTabSimpleObjectsDialog->objectsImageIndices.end() )
							{
								szObjectName = szSelectedObjectName;
							}

							rObjectSetShell.objects.SetWeight( nSelectedShellElement, nNewWeight );
							m_ObjectsList.SetItemText( nSelectedItem, 0, NStr::Format( "(%d) %s", rObjectSetShell.objects.GetWeight( nSelectedShellElement ), szObjectName.c_str() ) );
							nSelectedItem = m_ObjectsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
						}
					}
				}
				bCreateControls = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAvailableObjectProperties()
{
	int nSelectedItem = m_AvailableObjectsList.GetNextItem( CB_ERR, LVNI_SELECTED );
	if ( nSelectedItem != CB_ERR )
	{
		CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetObjectWnd();

		CValuesCollector<std::string> nameCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<std::string> pathCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<std::string> visTypeCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<std::string> gameTypeCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<std::string> statsCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<int> imageCollector( "", -1 );
		
		while ( nSelectedItem != CB_ERR )
		{
			const std::string szSelectedObjectName = m_AvailableObjectsList.GetItemText( nSelectedItem, 0 );

			int nImageIndex = 0;
			std::string szObjectName = UNKNOWN_OBJECT;
			std::string szPath = UNKNOWN_OBJECT;
			std::string szVisType = UNKNOWN_OBJECT;
			std::string szGameType = UNKNOWN_OBJECT;
			std::string szStats;

			if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
			{
				if ( pTabSimpleObjectsDialog->objectsImageIndices.find( szSelectedObjectName ) != pTabSimpleObjectsDialog->objectsImageIndices.end() )
				{
					if ( const SGDBObjectDesc *pObjectDesc = pODB->GetDesc( szSelectedObjectName.c_str() ) )
					{
						nImageIndex = pTabSimpleObjectsDialog->objectsImageIndices[szSelectedObjectName];
						szObjectName = szSelectedObjectName;

						szPath = pObjectDesc->szPath;
						if ( ( pObjectDesc->eVisType >= 0) && ( pObjectDesc->eVisType < CRMGFieldObjectPropertiesDialog::VIS_TYPES_COUNT ) )
						{
							szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[pObjectDesc->eVisType];
						}
						else
						{
							szVisType = CRMGFieldObjectPropertiesDialog::VIS_TYPES[0];
						}
						if ( ( pObjectDesc->eGameType >= 0) && ( pObjectDesc->eGameType < CRMGFieldObjectPropertiesDialog::GAME_TYPES_COUNT ) )
						{
							szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[pObjectDesc->eGameType];
						}
						else
						{
							szGameType = CRMGFieldObjectPropertiesDialog::GAME_TYPES[0];
						}
						szStats = CRMGFieldObjectPropertiesDialog::GetObjectStats( pODB, pObjectDesc );
						
					}
				}
			}

			nameCollector.AddValue( szObjectName, "%s" );
			pathCollector.AddValue( szPath, "%s" );
			visTypeCollector.AddValue( szVisType, "%s" );
			gameTypeCollector.AddValue( szGameType, "%s" );
			imageCollector.AddValue( nImageIndex, "%d" );
			statsCollector.AddValue( szStats, "%s" );

			nSelectedItem = m_AvailableObjectsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
		}		

		CRMGFieldObjectPropertiesDialog fieldObjectPropertiesDialog;
		fieldObjectPropertiesDialog.bDisableEditWeight = true;

		fieldObjectPropertiesDialog.m_szName = nameCollector.GetStringValue().c_str();
		fieldObjectPropertiesDialog.m_szPath = pathCollector.GetStringValue().c_str();
		fieldObjectPropertiesDialog.m_szVisType = visTypeCollector.GetStringValue().c_str();
		fieldObjectPropertiesDialog.m_szGameType = gameTypeCollector.GetStringValue().c_str();
		fieldObjectPropertiesDialog.m_szStats = statsCollector.GetStringValue().c_str();

		if ( imageCollector.GetValue() >= 0 )
		{
			fieldObjectPropertiesDialog.hIcon = pTabSimpleObjectsDialog->objectsImageList.ExtractIcon( imageCollector.GetValue() );
		}
		else
		{
			fieldObjectPropertiesDialog.hIcon = 0;
		}

		fieldObjectPropertiesDialog.DoModal();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRemoveObject() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_ObjectsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->objectsShells.size() ) )
			{
				SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nCurrentShell];
				
				bCreateControls = true;
				while ( nSelectedItem != CB_ERR )
				{
					const int nSelectedShellElement = m_ObjectsList.GetItemData( nSelectedItem );
					rObjectSetShell.objects.Set( nSelectedShellElement, "" );
					nSelectedItem = m_ObjectsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}

				for ( int nItemToDelete = 0; nItemToDelete < rObjectSetShell.objects.size(); )
				{
					if ( rObjectSetShell.objects[nItemToDelete].empty() )
					{
						rObjectSetShell.objects.erase( nItemToDelete );
					}
					else
					{
						++nItemToDelete;
					}
				}

				const int nSelectedShell = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
				if ( nSelectedShell != ( CB_ERR ) )
				{
					SetShellItem( nSelectedShell, rObjectSetShell );
				}
				FillShellObjectsList( nCurrentShell );
				bCreateControls = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAddObject() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_AvailableObjectsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->objectsShells.size() ) )
			{
				SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nCurrentShell];
				
				bCreateControls = true;
				bool bSomeAdded = false;
				while ( nSelectedItem != CB_ERR )
				{
					const std::string szSelectedObjectName = m_AvailableObjectsList.GetItemText( nSelectedItem, 0 );
					bool bNotExists = true;
					for ( int nShellElement = 0; nShellElement < rObjectSetShell.objects.size(); ++nShellElement )
					{
						 if ( szSelectedObjectName == rObjectSetShell.objects[nShellElement] )
						 {
								bNotExists = false;
								break;
						 }
					}
					if ( bNotExists )
					{
						bSomeAdded = true;
						rObjectSetShell.objects.push_back( szSelectedObjectName, DEFAULT_OBJECT_WEIGHT );
					}
					nSelectedItem = m_AvailableObjectsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}

				if ( bSomeAdded )
				{
					const int nSelectedShell = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
					if ( nSelectedShell != ( CB_ERR ) )
					{
						SetShellItem( nSelectedShell, rObjectSetShell );
					}
					FillShellObjectsList( nCurrentShell );
				}
				bCreateControls = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnShellProperties() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			bCreateControls = true;
			
			CValuesCollector<float> widthCollector( "...", 0.0f );
			CValuesCollector<int> stepCollector(  "...", 0 );
			CValuesCollector<float> ratioCollector(  "...", 0.0f );

			while ( nSelectedItem != CB_ERR )
			{
				std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
				int nSelectedShellElement = CB_ERR;
				if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
				{
					SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nSelectedShellElement];

					widthCollector.AddValue( rObjectSetShell.fWidth, "%.2f" );
					stepCollector.AddValue( rObjectSetShell.nBetweenDistance, "%d" );
					ratioCollector.AddValue( rObjectSetShell.fRatio * 100.f, "%.2f" );
				}
				nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
			}
			CRMGFieldObjectsShellPropertiesDialog fieldObjectsShellPropertiesDialog;
			fieldObjectsShellPropertiesDialog.m_szWidth = widthCollector.GetStringValue().c_str();
			fieldObjectsShellPropertiesDialog.m_szStep = stepCollector.GetStringValue().c_str();
			fieldObjectsShellPropertiesDialog.m_szRatio = ratioCollector.GetStringValue().c_str();

			if ( fieldObjectsShellPropertiesDialog.DoModal() == IDOK )
			{
				nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
				while ( nSelectedItem != CB_ERR )
				{
					std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
					int nSelectedShellElement = CB_ERR;
					if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
					{
						SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nSelectedShellElement];
					
						float fNewWidth = -1.0f;
						if ( ( sscanf( fieldObjectsShellPropertiesDialog.m_szWidth, "%f", &fNewWidth ) == 1 ) && ( fNewWidth >= 0.0f ) )
						{
							rObjectSetShell.fWidth = fNewWidth;
						}
						int nNewStep = 0;
						if ( ( sscanf( fieldObjectsShellPropertiesDialog.m_szStep, "%d", &nNewStep ) == 1 ) && ( nNewStep > 0 ) )
						{
							rObjectSetShell.nBetweenDistance = nNewStep;
						}
						float fNewRatio = -1.0f;
						if ( ( sscanf( fieldObjectsShellPropertiesDialog.m_szRatio, "%f", &fNewRatio ) == 1 ) && ( fNewRatio >= 0.0f ) && ( fNewRatio <= 100.0f ) )
						{
							rObjectSetShell.fRatio = ( fNewRatio / 100.0f );
						}
						SetShellItem( nSelectedItem, rObjectSetShell );
					}
					nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}
			}
			
			bCreateControls = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRemoveShell() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		if ( m_ShellsList.GetSelectedCount() > 0 )
		{
			CString strTitle;
			strTitle.LoadString( IDR_EDITORTYPE );
			if ( MessageBox( "Do you really want to DELETE selected objects shells?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
			{
				int nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
				while ( nSelectedItem != CB_ERR )
				{
					std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
					int nSelectedShellElement = CB_ERR;
					if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
					{
						SRMObjectSetShell &rObjectSetShell = pRMFieldSet->objectsShells[nSelectedShellElement];
						rObjectSetShell.fWidth = -1.0f;
					}
					nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}
				
				for ( CRMObjectSet::iterator objectShellIterator = pRMFieldSet->objectsShells.begin(); objectShellIterator != pRMFieldSet->objectsShells.end(); )
				{
					if ( objectShellIterator->fWidth < 0.0f )
					{
						pRMFieldSet->objectsShells.erase( objectShellIterator );
					}
					else
					{
						++objectShellIterator;
					}
				}
				pRMGCreateFieldDialog->UpdateFieldList( pRMFieldSet );

				nCurrentShell = -1;
				LoadFieldToControls();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAddShell() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		SRMObjectSetShell objectSetShell;
		objectSetShell.fWidth = DEFAULT_SHELL_WIDTH;
		objectSetShell.nBetweenDistance = DEFAULT_SHELL_STEP;
		objectSetShell.fRatio = DEFAULT_SHELL_RATIO;
		pRMFieldSet->objectsShells.push_back( objectSetShell );
		pRMGCreateFieldDialog->UpdateFieldList( pRMFieldSet );
		nCurrentShell = ( pRMFieldSet->objectsShells.size() - 1 );
		LoadFieldToControls();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAddShellMenu() 
{
	OnAddShell();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRemoveShellMenu() 
{
	OnRemoveShell();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnShellPropertiesMenu() 
{
	OnShellProperties();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnDblclkShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnShellProperties();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRclickShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 15 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_ADD_SHELL ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_ADD_SHELL_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_REMOVE_SHELL ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_REMOVE_SHELL_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_SHELL_PROPERTIES ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_SHELL_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnKeydownShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_ADD_SHELL ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddShell();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_REMOVE_SHELL ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnRemoveShell();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_SHELL_PROPERTIES ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnShellProperties();
			}
		}
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAddObjectMenu() 
{
	OnAddObject();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRemoveObjectMenu() 
{
	OnRemoveObject();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnObjectPropertiesMenu() 
{
	OnObjectProperties();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnAvailableObjectPropertiesMenu() 
{
	OnAvailableObjectProperties();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnDblclkObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnObjectProperties();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnDblclkAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnAddObject();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRclickObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 14 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_REMOVE_OBJECT ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_REMOVE_OBJECT_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_OBJECT_PROPERTIES ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_OBJECT_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnRclickAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 13 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_ADD_OBJECT ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_ADD_OBJECT_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_AVAILABLE_OBJECTS_LIST ) )
		{
			pMenu->EnableMenuItem( IDC_CF_OS_AVAILABLE_OBJECT_PROPERTIES_MENU, ( pWnd->IsWindowEnabled() && ( m_AvailableObjectsList.GetSelectedCount() > 0 ) ) ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnKeydownObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_REMOVE_OBJECT ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnRemoveObject();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_OBJECT_PROPERTIES ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnObjectProperties();
			}
		}
	}
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMGFieldObjectsDialog::OnKeydownAvailableObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_ADD_OBJECT ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddObject();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_OS_AVAILABLE_OBJECTS_LIST ) )
		{
			if ( pWnd->IsWindowEnabled() && ( m_AvailableObjectsList.GetSelectedCount() > 0 ) )
			{
				OnAvailableObjectProperties();
			}
		}
	}
	*pResult = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
