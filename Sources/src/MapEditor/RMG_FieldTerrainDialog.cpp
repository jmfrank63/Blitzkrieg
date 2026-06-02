#include "StdAfx.h"

#include "frames.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "TabTileEditDialog.h"

#include "ValuesCollector.h"

#include "RMG_CreateFieldDialog.h"
#include "RMG_FieldTerrainDialog.h"
#include "RMG_FieldTerrainShellPropertiesDialog.h"
#include "RMG_FieldTilePropertiesDialog.h"

#include "..\RandomMapGen\RMG_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int   CFT_SHELLS_COLUMN_START = 0;
const int   CFT_SHELLS_COLUMN_COUNT = 3;
const char *CFT_SHELLS_COLUMN_NAME  [CFT_SHELLS_COLUMN_COUNT] = { "N", "Tiles Count", "Size" };
const int   CFT_SHELLS_COLUMN_FORMAT[CFT_SHELLS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT };
const int		CFT_SHELLS_COLUMN_WIDTH [CFT_SHELLS_COLUMN_COUNT] = { 30, 80, 80 };

const int CRMGFieldTerrainDialog::DEFAULT_TILE_WEIGHT = 1;
const float CRMGFieldTerrainDialog::DEFAULT_SHELL_WIDTH = 2.0f;
const char CRMGFieldTerrainDialog::UNKNOWN_TILE[] = "Unknown";
const char CRMGFieldTerrainDialog::MULTIPLE_SELECTION[] = "Multiple selection...";

int CALLBACK CFT_ShellsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGFieldTerrainDialog* pFieldTerrainDialog = reinterpret_cast<CRMGFieldTerrainDialog*>( lParamSort );

	CString strItem1 = pFieldTerrainDialog->m_ShellsList.GetItemText( lParam1, pFieldTerrainDialog->nSortColumn );
	CString strItem2 = pFieldTerrainDialog->m_ShellsList.GetItemText( lParam2, pFieldTerrainDialog->nSortColumn );

	if ( pFieldTerrainDialog->bShellsSortParam[pFieldTerrainDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

CRMGFieldTerrainDialog::CRMGFieldTerrainDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldTerrainDialog::IDD, pParent ), nSortColumn( -1 ), pRMGCreateFieldDialog( 0 ), bCreateControls( true ), pRMFieldSet( 0 ), nSelectedSeason( 0 ), nOldSelectedSeason( CB_ERR ), nCurrentShell( CB_ERR )
{

	SetControlStyle( IDC_CF_TS_SEASON_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_TS_SEASON_COMBO, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_CF_TS_AVAILABLE_TILES_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR | RESIZE_VER, 0.5f, 0.5f, 0.5f, 1.0f );

	SetControlStyle( IDC_CF_TS_SHELLS_LABEL, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_TS_SHELLS_LIST, ANCHORE_RIGHT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 0.5f );
	SetControlStyle( IDC_CF_TS_TILES_LABEL, ANCHORE_RIGHT | ANCHORE_VER_CENTER | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CF_TS_TILES_LIST, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 0.5f );

	SetControlStyle( IDC_CF_TS_ADD_SHELL, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_TS_REMOVE_SHELL, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_TS_SHELL_PROPERTIES, ANCHORE_HOR_CENTER | ANCHORE_TOP, 0.5f, 0.5f, 1.0f, 1.0f );
	
	SetControlStyle( IDC_CF_TS_ADD_TILE, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_TS_REMOVE_TILE, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_CF_TS_TILE_PROPERTIES, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER, 0.5f, 0.5f, 1.0f, 1.0f );

	SetControlStyle( IDOK, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
	SetControlStyle( IDCANCEL, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
}

void CRMGFieldTerrainDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CF_TS_SEASON_COMBO, m_SeasonComboBox);
	DDX_Control(pDX, IDC_CF_TS_SHELLS_LIST, m_ShellsList);
	DDX_Control(pDX, IDC_CF_TS_TILES_LIST, m_TilesList);
	DDX_Control(pDX, IDC_CF_TS_AVAILABLE_TILES_LIST, m_AvailableTilesList);
}

BEGIN_MESSAGE_MAP(CRMGFieldTerrainDialog, CResizeDialog)
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_CF_TS_SEASON_COMBO, OnSelchangeSeasonCombo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_TS_SHELLS_LIST, OnItemchangedShellsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_TS_TILES_LIST, OnItemchangedTilesList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CF_TS_AVAILABLE_TILES_LIST, OnItemchangedAvailableTilesList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_CF_TS_SHELLS_LIST, OnColumnclickShellsList)
	ON_BN_CLICKED(IDC_CF_TS_TILE_PROPERTIES, OnTileProperties)
	ON_BN_CLICKED(IDC_CF_TS_REMOVE_TILE, OnRemoveTile)
	ON_BN_CLICKED(IDC_CF_TS_ADD_TILE, OnAddTile)
	ON_BN_CLICKED(IDC_CF_TS_ADD_SHELL, OnAddShell)
	ON_BN_CLICKED(IDC_CF_TS_REMOVE_SHELL, OnRemoveShell)
	ON_BN_CLICKED(IDC_CF_TS_SHELL_PROPERTIES, OnShellProperties)
	ON_COMMAND(IDC_CF_TS_ADD_SHELL_MENU, OnAddShellMenu)
	ON_COMMAND(IDC_CF_TS_REMOVE_SHELL_MENU, OnRemoveShellMenu)
	ON_COMMAND(IDC_CF_TS_SHELL_PROPERTIES_MENU, OnShellPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_TS_SHELLS_LIST, OnDblclkShellsList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_TS_SHELLS_LIST, OnRclickShellsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_TS_SHELLS_LIST, OnKeydownShellsList)
	ON_COMMAND(IDC_CF_TS_ADD_TILE_MENU, OnAddTileMenu)
	ON_COMMAND(IDC_CF_TS_REMOVE_TILE_MENU, OnRemoveTileMenu)
	ON_COMMAND(IDC_CF_TS_TILE_PROPERTIES_MENU, OnTilePropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_TS_TILES_LIST, OnDblclkTilesList)
	ON_COMMAND(IDC_CF_TS_AVAILABLE_TILE_PROPERTIES_MENU, OnAvailableTilePropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_CF_TS_AVAILABLE_TILES_LIST, OnDblclkAvailableTilesList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_TS_TILES_LIST, OnRclickTilesList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_TS_TILES_LIST, OnKeydownTilesList)
	ON_NOTIFY(NM_RCLICK, IDC_CF_TS_AVAILABLE_TILES_LIST, OnRclickAvailableTilesList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_CF_TS_AVAILABLE_TILES_LIST, OnKeydownAvailableTilesList)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CRMGFieldTerrainDialog::OnInitDialog()
{
  CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.nParameters.size() < CFT_SHELLS_COLUMN_COUNT )
	{
		resizeDialogOptions.nParameters.resize( CFT_SHELLS_COLUMN_COUNT, 0 );
	}

	CreateControls();
	return true;
}

void CRMGFieldTerrainDialog::CreateControls()
{
	bCreateControls = true;

	m_SeasonComboBox.ResetContent();
	for ( int nSeason = 0; nSeason < CMapInfo::SEASON_COUNT; ++nSeason )
	{
		const int nItem = m_SeasonComboBox.AddString( CMapInfo::SEASON_NAMES[nSeason] );
		if ( nItem >= 0 )
		{
			m_SeasonComboBox.SetItemData( nItem, nSeason );
		}
	}
	
	CTabTileEditDialog *pTabTileEditDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetTabTileEditDialog();

	m_AvailableTilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
																			 TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
	m_AvailableTilesList.Arrange( LVA_DEFAULT );
	m_AvailableTilesList.SetImageList( &( pTabTileEditDialog->tilesImageList ), LVSIL_NORMAL );

	m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
															TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
	m_TilesList.Arrange( LVA_DEFAULT );
	m_TilesList.SetImageList( &( pTabTileEditDialog->tilesImageList ), LVSIL_NORMAL );
	
	m_ShellsList.SetExtendedStyle( m_ShellsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CFT_SHELLS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CFT_SHELLS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CFT_SHELLS_COLUMN_START] = CFT_SHELLS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_ShellsList.InsertColumn( nColumnIndex, CFT_SHELLS_COLUMN_NAME[nColumnIndex], CFT_SHELLS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CFT_SHELLS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bShellsSortParam.push_back( true );
	}

	bCreateControls = false;
}

void CRMGFieldTerrainDialog::OnSize( UINT nType, int cx, int cy ) 
{
	CResizeDialog::OnSize( nType, cx, cy );

	m_AvailableTilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
																			 TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
	m_AvailableTilesList.Arrange( LVA_DEFAULT );
	m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
															TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
	m_TilesList.Arrange( LVA_DEFAULT );
}

void CRMGFieldTerrainDialog::FillAvailableTiles( int nSeason )
{
	CTabTileEditDialog *pTabTileEditDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetTabTileEditDialog();
	m_AvailableTilesList.DeleteAllItems();
	if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( LoadDataResource( NStr::Format( "%stileset", CMapInfo::SEASON_FOLDERS[nSeason] ), "", false, 0, "tileset", tilesetDesc ) )
		{
			for ( int nTileIndex = 0; nTileIndex < tilesetDesc.terrtypes.size(); ++nTileIndex )
			{
				const int nImageIndex = pTabTileEditDialog->seasonTilesIndices[MAKELPARAM( nSeason, nTileIndex )];
				const int nInsertedItem = m_AvailableTilesList.InsertItem( nTileIndex, tilesetDesc.terrtypes[nTileIndex].szName.c_str() , nImageIndex );
				m_AvailableTilesList.SetItemData( nInsertedItem, nTileIndex );
			}
		}
	}
	m_AvailableTilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
																			 TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
	m_AvailableTilesList.Arrange( LVA_DEFAULT );
}

void CRMGFieldTerrainDialog::LoadFieldToControls()
{
	bCreateControls = true;
	if ( pRMFieldSet )
	{
		std::string szSeasonName;
		RMGGetSeasonNameString( pRMFieldSet->nSeason, pRMFieldSet->szSeasonFolder, &szSeasonName );

		nOldSelectedSeason = m_SeasonComboBox.GetCurSel();
		if ( nOldSelectedSeason != CB_ERR )
		{
			nOldSelectedSeason = m_SeasonComboBox.GetItemData( nOldSelectedSeason );
		}

		m_SeasonComboBox.SelectString( CB_ERR, szSeasonName.c_str() );

		nSelectedSeason = m_SeasonComboBox.GetCurSel();
		if ( nSelectedSeason != CB_ERR )
		{
			nSelectedSeason = m_SeasonComboBox.GetItemData( nSelectedSeason );
		}
		else
		{
			nSelectedSeason = 0;
			m_SeasonComboBox.SelectString( CB_ERR, CMapInfo::SEASON_NAMES[nSelectedSeason] );
			pRMFieldSet->nSeason = CMapInfo::REAL_SEASONS[nSelectedSeason];
			pRMFieldSet->szSeasonFolder = CMapInfo::SEASON_FOLDERS[nSelectedSeason];
		}
		
		if ( nSelectedSeason != nOldSelectedSeason )
		{
			nOldSelectedSeason = nSelectedSeason;
			FillAvailableTiles( nSelectedSeason );
		}

		m_ShellsList.DeleteAllItems();
		if ( ( nCurrentShell < 0 ) || ( nCurrentShell >= pRMFieldSet->tilesShells.size() ) )
		{
			nCurrentShell = CB_ERR;
		}
		for ( int nShellIndex = 0; nShellIndex < pRMFieldSet->tilesShells.size(); ++nShellIndex )
		{
			const SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nShellIndex];
			int nNewItem = m_ShellsList.InsertItem( LVIF_TEXT, nShellIndex, NStr::Format( "%2d", nShellIndex ), 0, 0, 0, 0 );
			if ( nNewItem != ( CB_ERR ) )
			{
				SetShellItem( nNewItem, rTileSetShell );
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
				m_ShellsList.SortItems( CFT_ShellsCompareFunc, reinterpret_cast<LPARAM>( this ) );
			}
		}
		FillShellTilesList( nSelectedSeason, nCurrentShell );
	}
	else
	{
		m_TilesList.DeleteAllItems();
		m_ShellsList.DeleteAllItems();
	}
	UpdateControls();
	bCreateControls = false;
}

void CRMGFieldTerrainDialog::UpdateControls()
{
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_SHELLS_LIST ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_TILES_LIST ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) && ( m_ShellsList.GetSelectedCount() > 0 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_ADD_SHELL ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_REMOVE_SHELL ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_SHELL_PROPERTIES ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_ADD_TILE ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_AvailableTilesList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_REMOVE_TILE ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_TilesList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_CF_TS_TILE_PROPERTIES ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) && ( m_ShellsList.GetSelectedCount() > 0 ) && ( m_TilesList.GetSelectedCount() > 0 ) && ( fieldSets.size() < 2 ) );
	}
}

void CRMGFieldTerrainDialog::SetShellItem( int nItem, const SRMTileSetShell &rTileSetShell )
{
	m_ShellsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%2d", rTileSetShell.tiles.size() ), 0, 0, 0, 0 );
	m_ShellsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%.2f", rTileSetShell.fWidth ), 0, 0, 0, 0 );
}

void CRMGFieldTerrainDialog::FillShellTilesList( int nSeason, int nSelectedShell )
{
	if ( pRMFieldSet )
	{
		m_TilesList.DeleteAllItems();
		if ( ( nSelectedShell >= 0 )  && ( nSelectedShell < pRMFieldSet->tilesShells.size() ) )
		{
			CTabTileEditDialog *pTabTileEditDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetTabTileEditDialog();
			const SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nSelectedShell];

			for ( int nTileIndex = 0; nTileIndex < rTileSetShell.tiles.size(); ++nTileIndex )
			{
				const int nSelectedTileIndex = rTileSetShell.tiles[nTileIndex];
				int nImageIndex = 0;
				std::string szTileName = UNKNOWN_TILE;
				if ( ( nSelectedTileIndex >= 0 ) && ( nSelectedTileIndex < tilesetDesc.terrtypes.size() ) )
				{
					nImageIndex = pTabTileEditDialog->seasonTilesIndices[MAKELPARAM( nSeason, nSelectedTileIndex )];
					szTileName = tilesetDesc.terrtypes[nSelectedTileIndex].szName;
				}
				const int nInsertedItem = m_TilesList.InsertItem( nTileIndex, NStr::Format( "(%d) %s", rTileSetShell.tiles.GetWeight( nTileIndex ), szTileName.c_str() ), nImageIndex );
				m_TilesList.SetItemData( nInsertedItem, nTileIndex );
			}
		}
		m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X,
																TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y );
		m_TilesList.Arrange( LVA_DEFAULT );
	}
}

void CRMGFieldTerrainDialog::OnDestroy() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CFT_SHELLS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CFT_SHELLS_COLUMN_START] = m_ShellsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog ::OnDestroy();
}

void CRMGFieldTerrainDialog::OnOK() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnOK();
	}
}

void CRMGFieldTerrainDialog::OnCancel() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnCancel();
	}
}

void CRMGFieldTerrainDialog::OnItemchangedShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
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
				FillShellTilesList( nSelectedSeason, nCurrentShell );
			}
		}
		else
		{
			m_TilesList.DeleteAllItems();
		}
		UpdateControls();
		bCreateControls = false;
	}

	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnSelchangeSeasonCombo() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		nSelectedSeason = m_SeasonComboBox.GetCurSel();
		if ( nSelectedSeason == CB_ERR )
		{
			nSelectedSeason = 0;
			m_SeasonComboBox.SelectString( CB_ERR, CMapInfo::SEASON_NAMES[nSelectedSeason] );
		}
		else
		{
			nSelectedSeason = m_SeasonComboBox.GetItemData( nSelectedSeason );
		}
		
		if ( fieldSets.size() > 1 )
		{
			for ( int nFieldIndex = 0; nFieldIndex < fieldSets.size(); ++nFieldIndex )	
			{
				if ( fieldSets[nFieldIndex] != pRMFieldSet )
				{
					fieldSets[nFieldIndex]->nSeason = CMapInfo::REAL_SEASONS[nSelectedSeason];
					fieldSets[nFieldIndex]->szSeasonFolder = CMapInfo::SEASON_FOLDERS[nSelectedSeason];
					pRMGCreateFieldDialog->UpdateFieldList( fieldSets[nFieldIndex] );
				}
			}
		}
		if ( pRMFieldSet )
		{
			pRMFieldSet->nSeason = CMapInfo::REAL_SEASONS[nSelectedSeason];
			pRMFieldSet->szSeasonFolder = CMapInfo::SEASON_FOLDERS[nSelectedSeason];
			pRMGCreateFieldDialog->UpdateFieldList( pRMFieldSet );
		}
		
		if ( nSelectedSeason != nOldSelectedSeason )
		{
			nOldSelectedSeason = nSelectedSeason;
			FillAvailableTiles( nSelectedSeason );
			if ( pRMFieldSet )
			{
				FillShellTilesList( nSelectedSeason, nCurrentShell );
			}
		}
		UpdateControls();
		bCreateControls = false;
	}
}

void CRMGFieldTerrainDialog::OnItemchangedTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
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

void CRMGFieldTerrainDialog::OnItemchangedAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
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

void CRMGFieldTerrainDialog::OnColumnclickShellsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CFT_SHELLS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CFT_SHELLS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_ShellsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		bShellsSortParam[nSortColumn] = !bShellsSortParam[nSortColumn];
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_ShellsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_ShellsList.SortItems( CFT_ShellsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnTileProperties() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->tilesShells.size() ) )
		{
			int nSelectedItem = m_TilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
			if ( nSelectedItem != CB_ERR )
			{
				bCreateControls = true;
				
				CTabTileEditDialog *pTabTileEditDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetTabTileEditDialog();
				SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nCurrentShell];

				CValuesCollector<std::string> nameCollector( MULTIPLE_SELECTION, "" );
				CValuesCollector<int> variantsCollector( MULTIPLE_SELECTION, 0 );
				CValuesCollector<int> weightCollector( "...", 0 );
				CValuesCollector<int> imageCollector( "", -1 );
				
				while ( nSelectedItem != CB_ERR )
				{
					const int nSelectedShellElement = m_TilesList.GetItemData( nSelectedItem );
					const int nSelectedTileIndex = rTileSetShell.tiles[nSelectedShellElement];
					
					int nImageIndex = 0;
					std::string szTileName = UNKNOWN_TILE;
					int nTileVariants = 0;
					if ( ( nSelectedTileIndex >= 0 ) && ( nSelectedTileIndex < tilesetDesc.terrtypes.size() ) )
					{
						nImageIndex = pTabTileEditDialog->seasonTilesIndices[MAKELPARAM( nSelectedSeason, nSelectedTileIndex )];
						szTileName = tilesetDesc.terrtypes[nSelectedTileIndex].szName;
						nTileVariants = tilesetDesc.terrtypes[nSelectedTileIndex].tiles.size();
					}
					
					nameCollector.AddValue( szTileName, "%s" );
					variantsCollector.AddValue( nTileVariants, "%d" );
					weightCollector.AddValue( rTileSetShell.tiles.GetWeight( nSelectedShellElement ), "%d" );
					imageCollector.AddValue( nImageIndex, "%d" );
					
					nSelectedItem = m_TilesList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}
			
				CRMGFieldTilePropertiesDialog fieldTilePropertiesDialog;
				fieldTilePropertiesDialog.m_szStats.Format( "Overall weight: %d, average weight: %.2f", rTileSetShell.tiles.weight(), ( 1.0f * rTileSetShell.tiles.weight() ) / rTileSetShell.tiles.size() );
				fieldTilePropertiesDialog.bDisableEditWeight = false;

				fieldTilePropertiesDialog.m_szName = nameCollector.GetStringValue().c_str();
				fieldTilePropertiesDialog.m_szVariants = variantsCollector.GetStringValue().c_str();
				fieldTilePropertiesDialog.m_szWeight = weightCollector.GetStringValue().c_str();
				if ( imageCollector.GetValue() >= 0 )
				{
					fieldTilePropertiesDialog.hIcon = pTabTileEditDialog->tilesImageList.ExtractIcon( imageCollector.GetValue() );
				}
				else
				{
					fieldTilePropertiesDialog.hIcon = 0;
				}
				if ( fieldTilePropertiesDialog.DoModal() == IDOK )
				{
					int nNewWeight = 0;
					if ( ( sscanf( fieldTilePropertiesDialog.m_szWeight, "%d", &nNewWeight ) == 1 ) && ( nNewWeight >= 0 ) )
					{
						nSelectedItem = m_TilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
						while ( nSelectedItem != CB_ERR )
						{
							const int nSelectedShellElement = m_TilesList.GetItemData( nSelectedItem );
							const int nSelectedTileIndex = rTileSetShell.tiles[nSelectedShellElement];

							std::string szTileName = UNKNOWN_TILE;
							if ( ( nSelectedTileIndex >= 0 ) && ( nSelectedTileIndex < tilesetDesc.terrtypes.size() ) )
							{
								szTileName = tilesetDesc.terrtypes[nSelectedTileIndex].szName;
							}

							rTileSetShell.tiles.SetWeight( nSelectedShellElement, nNewWeight );
							m_TilesList.SetItemText( nSelectedItem, 0, NStr::Format( "(%d) %s", rTileSetShell.tiles.GetWeight( nSelectedShellElement ), szTileName.c_str() ) );
							nSelectedItem = m_TilesList.GetNextItem( nSelectedItem, LVNI_SELECTED );
						}
					}
				}
				bCreateControls = false;
			}
		}
	}
}

void CRMGFieldTerrainDialog::OnAvailableTileProperties()
{
	int nSelectedItem = m_AvailableTilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
	if ( nSelectedItem != CB_ERR )
	{
		CTabTileEditDialog *pTabTileEditDialog = g_frameManager.GetTemplateEditorFrame()->m_mapEditorBarPtr->GetTabTileEditDialog();

		CValuesCollector<std::string> nameCollector( MULTIPLE_SELECTION, "" );
		CValuesCollector<int> variantsCollector( MULTIPLE_SELECTION, 0 );
		CValuesCollector<int> imageCollector( "", -1 );

		while ( nSelectedItem != CB_ERR )
		{
			const int nSelectedTileIndex = m_AvailableTilesList.GetItemData( nSelectedItem );
			
			int nImageIndex = 0;
			std::string szTileName = UNKNOWN_TILE;
			int nTileVariants = 0;
			if ( ( nSelectedTileIndex >= 0 ) && ( nSelectedTileIndex < tilesetDesc.terrtypes.size() ) )
			{
				nImageIndex = pTabTileEditDialog->seasonTilesIndices[MAKELPARAM( nSelectedSeason, nSelectedTileIndex )];
				szTileName = tilesetDesc.terrtypes[nSelectedTileIndex].szName;
				nTileVariants = tilesetDesc.terrtypes[nSelectedTileIndex].tiles.size();
			}
			
			nameCollector.AddValue( szTileName, "%s" );
			variantsCollector.AddValue( nTileVariants, "%d" );
			imageCollector.AddValue( nImageIndex, "%d" );

			nSelectedItem = m_AvailableTilesList.GetNextItem( nSelectedItem, LVNI_SELECTED );
		}
		CRMGFieldTilePropertiesDialog fieldTilePropertiesDialog;
		fieldTilePropertiesDialog.m_szWeight = "";
		fieldTilePropertiesDialog.bDisableEditWeight = true;

		fieldTilePropertiesDialog.m_szName = nameCollector.GetStringValue().c_str();
		fieldTilePropertiesDialog.m_szVariants = variantsCollector.GetStringValue().c_str();
		if ( imageCollector.GetValue() >= 0 )
		{
			fieldTilePropertiesDialog.hIcon = pTabTileEditDialog->tilesImageList.ExtractIcon( imageCollector.GetValue() );
		}
		else
		{
			fieldTilePropertiesDialog.hIcon = 0;
		}
		fieldTilePropertiesDialog.DoModal();
	}
}

void CRMGFieldTerrainDialog::OnRemoveTile() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_TilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->tilesShells.size() ) )
			{
				SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nCurrentShell];
				
				bCreateControls = true;
				while ( nSelectedItem != CB_ERR )
				{
					const int nSelectedShellElement = m_TilesList.GetItemData( nSelectedItem );
					rTileSetShell.tiles.Set( nSelectedShellElement, -1 );
					nSelectedItem = m_TilesList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}

				for ( int nItemToDelete = 0; nItemToDelete < rTileSetShell.tiles.size(); )
				{
					if ( rTileSetShell.tiles[nItemToDelete] < 0 )
					{
						rTileSetShell.tiles.erase( nItemToDelete );
					}
					else
					{
						++nItemToDelete;
					}
				}

				const int nSelectedShell = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
				if ( nSelectedShell != ( CB_ERR ) )
				{
					SetShellItem( nSelectedShell, rTileSetShell );
				}
				FillShellTilesList( nSelectedSeason, nCurrentShell );
				bCreateControls = false;
			}
		}
	}
}

void CRMGFieldTerrainDialog::OnAddTile() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_AvailableTilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			if ( ( nCurrentShell >= 0 ) && ( nCurrentShell < pRMFieldSet->tilesShells.size() ) )
			{
				SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nCurrentShell];
				
				bCreateControls = true;
				bool bSomeAdded = false;
				while ( nSelectedItem != CB_ERR )
				{
					const int nSelectedTileIndex = m_AvailableTilesList.GetItemData( nSelectedItem );
					bool bNotExists = true;
					for ( int nShellElement = 0; nShellElement < rTileSetShell.tiles.size(); ++nShellElement )
					{
						 if ( nSelectedTileIndex == rTileSetShell.tiles[nShellElement] )
						 {
								bNotExists = false;
								break;
						 }
					}
					if ( bNotExists )
					{
						bSomeAdded = true;
						rTileSetShell.tiles.push_back( nSelectedTileIndex, DEFAULT_TILE_WEIGHT );
					}
					nSelectedItem = m_AvailableTilesList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}

				if ( bSomeAdded )
				{
					const int nSelectedShell = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
					if ( nSelectedShell != ( CB_ERR ) )
					{
						SetShellItem( nSelectedShell, rTileSetShell );
					}
					FillShellTilesList( nSelectedSeason, nCurrentShell );
				}
				bCreateControls = false;
			}
		}
	}
}

void CRMGFieldTerrainDialog::OnShellProperties() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		int nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if ( nSelectedItem != CB_ERR )
		{
			bCreateControls = true;

			CValuesCollector<float> widthCollector( "...", 0.0f );
			while ( nSelectedItem != CB_ERR )
			{
				std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
				int nSelectedShellElement = CB_ERR;
				if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
				{
					SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nSelectedShellElement];

					if ( widthCollector.AddValue( rTileSetShell.fWidth, "%.2f" ) )
					{
						break;
					}
				}
				nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
			}

			CRMGFieldTerrainShellPropertiesDialog fieldTerrainShellPropertiesDialog;
			fieldTerrainShellPropertiesDialog.m_szWidth = widthCollector.GetStringValue().c_str();
			
			if ( fieldTerrainShellPropertiesDialog.DoModal() == IDOK )
			{
				float fNewWidth = -1.0f;
				if ( ( sscanf( fieldTerrainShellPropertiesDialog.m_szWidth, "%f", &fNewWidth ) == 1 ) && ( fNewWidth >= 0.0f ) )
				{
					nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
					while ( nSelectedItem != CB_ERR )
					{
						std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
						int nSelectedShellElement = CB_ERR;
						if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
						{
							SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nSelectedShellElement];
							rTileSetShell.fWidth = fNewWidth;
							SetShellItem( nSelectedItem, rTileSetShell );
						}
						nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
					}
				}
			}

			bCreateControls = false;
		}
	}
}

void CRMGFieldTerrainDialog::OnRemoveShell() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		if ( m_ShellsList.GetSelectedCount() > 0 )
		{
			CString strTitle;
			strTitle.LoadString( IDR_EDITORTYPE );
			if ( MessageBox( "Do you really want to DELETE selected terrain shells?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
			{
				int nSelectedItem = m_ShellsList.GetNextItem( CB_ERR, LVNI_SELECTED );
				while ( nSelectedItem != CB_ERR )
				{
					std::string szKey = m_ShellsList.GetItemText( nSelectedItem, 0 );
					int nSelectedShellElement = CB_ERR;
					if ( ( sscanf( szKey.c_str(), "%d", &nSelectedShellElement ) == 1 ) && ( nSelectedShellElement >= 0 ) )
					{
						SRMTileSetShell &rTileSetShell = pRMFieldSet->tilesShells[nSelectedShellElement];
						rTileSetShell.fWidth = -1.0f;
					}
					nSelectedItem = m_ShellsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}
				
				for ( CRMTileSet::iterator tileShellIterator = pRMFieldSet->tilesShells.begin(); tileShellIterator != pRMFieldSet->tilesShells.end(); )
				{
					if ( tileShellIterator->fWidth < 0.0f )
					{
						pRMFieldSet->tilesShells.erase( tileShellIterator );
					}
					else
					{
						++tileShellIterator;
					}
				}
				pRMGCreateFieldDialog->UpdateFieldList( pRMFieldSet );

				nCurrentShell = -1;
				LoadFieldToControls();
			}
		}
	}
}

void CRMGFieldTerrainDialog::OnAddShell() 
{
	if ( pRMFieldSet && ( fieldSets.size() < 2 ) )
	{
		SRMTileSetShell tileSetShell;
		tileSetShell.fWidth = DEFAULT_SHELL_WIDTH;
		pRMFieldSet->tilesShells.push_back( tileSetShell );
		pRMGCreateFieldDialog->UpdateFieldList( pRMFieldSet );
		nCurrentShell = ( pRMFieldSet->tilesShells.size() - 1 );
		LoadFieldToControls();
	}
}

void CRMGFieldTerrainDialog::OnAddShellMenu() 
{
	OnAddShell();
}

void CRMGFieldTerrainDialog::OnRemoveShellMenu() 
{
	OnRemoveShell();
}

void CRMGFieldTerrainDialog::OnShellPropertiesMenu() 
{
	OnShellProperties();
}

void CRMGFieldTerrainDialog::OnDblclkShellsList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	OnShellProperties();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnRclickShellsList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 12 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_ADD_SHELL ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_ADD_SHELL_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_REMOVE_SHELL ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_REMOVE_SHELL_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_SHELL_PROPERTIES ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_SHELL_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnKeydownShellsList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_ADD_SHELL ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddShell();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_REMOVE_SHELL ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnRemoveShell();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_SHELL_PROPERTIES ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnShellProperties();
			}
		}
	}
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnAddTileMenu() 
{
	OnAddTile();
}

void CRMGFieldTerrainDialog::OnRemoveTileMenu() 
{
	OnRemoveTile();
}

void CRMGFieldTerrainDialog::OnTilePropertiesMenu() 
{
	OnTileProperties();
}

void CRMGFieldTerrainDialog::OnAvailableTilePropertiesMenu() 
{
	OnAvailableTileProperties();
}

void CRMGFieldTerrainDialog::OnDblclkTilesList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	OnTileProperties();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnDblclkAvailableTilesList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	OnAddTile();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnRclickTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 11 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_REMOVE_TILE ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_REMOVE_TILE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_TILE_PROPERTIES ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_TILE_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnRclickAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 10 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_ADD_TILE ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_ADD_TILE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_AVAILABLE_TILES_LIST ) )
		{
			pMenu->EnableMenuItem( IDC_CF_TS_AVAILABLE_TILE_PROPERTIES_MENU, ( pWnd->IsWindowEnabled() && ( m_AvailableTilesList.GetSelectedCount() > 0 ) ) ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnKeydownTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_REMOVE_TILE ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnRemoveTile();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_TILE_PROPERTIES ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnTileProperties();
			}
		}
	}
	*pResult = 0;
}

void CRMGFieldTerrainDialog::OnKeydownAvailableTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_ADD_TILE ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddTile();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_CF_TS_AVAILABLE_TILES_LIST ) )
		{
			if ( pWnd->IsWindowEnabled() && ( m_AvailableTilesList.GetSelectedCount() > 0 ) )
			{
				OnAvailableTileProperties();
			}
		}
	}
	*pResult = 0;
}
