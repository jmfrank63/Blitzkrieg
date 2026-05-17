#include "stdafx.h"
#include "editor.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"
#include "TabAIGeneralDialog.h"
#include "..\Misc\FileUtils.h"
#include "TabAIGeneralEnterScriptIDDialog.h"
#include "TabAIGeneralSetPositionTypeDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char *POSITION_TYPE_LABELS[3] = { "Unknown", "Defence", "Reinforce" };

const int   REINFORCEMENTS_COLUMN_COUNT = 5;
const char *REINFORCEMENTS_COLUMN_NAME  [REINFORCEMENTS_COLUMN_COUNT] = { "N", "Script ID", "Units", "Invalid or Enemy Units", "Squads" };
const int   REINFORCEMENTS_COLUMN_FORMAT[REINFORCEMENTS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
int					REINFORCEMENTS_COLUMN_WIDTH [REINFORCEMENTS_COLUMN_COUNT] = { 20, 80, 60, 60, 60 };

const int   POSITIONS_COLUMN_COUNT = 6;
const char *POSITIONS_COLUMN_NAME  [POSITIONS_COLUMN_COUNT] = { "N", "Type", "Radius", "Center", "Direction", "Placeholders" };
const int   POSITIONS_COLUMN_FORMAT[POSITIONS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
int					POSITIONS_COLUMN_WIDTH [POSITIONS_COLUMN_COUNT] = { 20, 60, 60, 60, 60, 60 };

int CALLBACK ReinforcementsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CTabAIGeneralDialog* pAIGrDialog = reinterpret_cast<CTabAIGeneralDialog*>( lParamSort );

	CString strItem1 = pAIGrDialog->m_ReinforcementsList.GetItemText( lParam1, pAIGrDialog->nSortColumn );
	CString strItem2 = pAIGrDialog->m_ReinforcementsList.GetItemText( lParam2, pAIGrDialog->nSortColumn );
	if ( pAIGrDialog->bReinforcementsSortParam[pAIGrDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

int CALLBACK PositionsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CTabAIGeneralDialog* pAIGrDialog = reinterpret_cast<CTabAIGeneralDialog*>( lParamSort );

	CString strItem1 = pAIGrDialog->m_PositionsList.GetItemText( lParam1, pAIGrDialog->nSortColumn );
	CString strItem2 = pAIGrDialog->m_PositionsList.GetItemText( lParam2, pAIGrDialog->nSortColumn );
	if ( pAIGrDialog->bPositionsSortParam[pAIGrDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CTabAIGeneralDialog::vID[] = 
{
	IDC_AIG_SIDE_0_RADIO_BUTTON,														//0
	IDC_AIG_SIDE_1_RADIO_BUTTON,														//1
	IDC_AIG_DELIMITER_00,																		//2
	IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LABEL,						//3
	IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST,						//4
	IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON,			//5
	IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON,		//6
	IDC_AIG_POSITIONS_LABEL,																//7
	IDC_AIG_POSITIONS_LIST,																	//8
	IDC_AIG_POSITION_TYPE_BUTTON,														//9
	IDC_AIG_DELETE_POSITIONS_BUTTON,												//10
	IDC_AIG_DELIMITER_01,																		//11
	IDC_AIG_MESSAGE_LABEL,																	//12
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTabAIGeneralDialog::CTabAIGeneralDialog( CWnd* pParent )
	: CResizeDialog( CTabAIGeneralDialog::IDD, pParent ), bCreateControls( false )
{
	//{{AFX_DATA_INIT(CTabAIGeneralDialog)
	m_nSide = 0;
	//}}AFX_DATA_INIT

	SetControlStyle( IDC_AIG_SIDE_0_RADIO_BUTTON, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_AIG_SIDE_1_RADIO_BUTTON, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_AIG_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR | RESIZE_VER, 1.0f, 1.0f, 1.0f, 0.5f );
	
	SetControlStyle( IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON, ANCHORE_VER_CENTER | ANCHORE_RIGHT, 1.0f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON, ANCHORE_VER_CENTER | ANCHORE_RIGHT, 1.0f, 0.5f, 1.0f, 1.0f );
	
	SetControlStyle( IDC_AIG_POSITIONS_LABEL, ANCHORE_VER_CENTER | ANCHORE_LEFT | RESIZE_HOR, 1.0f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_AIG_POSITIONS_LIST, ANCHORE_LEFT_BOTTOM | RESIZE_VER | RESIZE_HOR, 1.0f, 1.0f, 1.0f, 0.5f );
	
	SetControlStyle( IDC_AIG_POSITION_TYPE_BUTTON, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDC_AIG_DELETE_POSITIONS_BUTTON, ANCHORE_RIGHT_BOTTOM );

	SetControlStyle( IDC_AIG_DELIMITER_01, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR );
	SetControlStyle( IDC_AIG_MESSAGE_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP( CTabAIGeneralDialog, CResizeDialog )
	//{{AFX_MSG_MAP(CTabAIGeneralDialog)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_AIG_POSITIONS_LIST, OnColumnclickPositionsList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, OnColumnclickReinforcementsList)
	ON_BN_CLICKED(IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON, OnAigDeleteMobileReinforcementScriptIdButton)
	ON_BN_CLICKED(IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON, OnAigAddMobileReinforcementScriptIdButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, OnItemchangedAigMobileReinforcementScriptIdList)
	ON_BN_CLICKED(IDC_AIG_SIDE_1_RADIO_BUTTON, OnSide1RadioButton)
	ON_BN_CLICKED(IDC_AIG_SIDE_0_RADIO_BUTTON, OnSide0RadioButton)
	ON_NOTIFY(NM_RCLICK, IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, OnRclickReinforcementsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, OnKeydownReinforcementsList)
	ON_COMMAND(IDC_TAB_AIG_ADD_REINFORCEMENT_MENU, OnAddReinforcementMenu)
	ON_COMMAND(IDC_TAB_AIG_DELETE_REINFORCEMENT_MENU, OnDeleteReinforcementMenu)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_AIG_POSITIONS_LIST, OnItemchangedPositionsList)
	ON_COMMAND(IDC_TAB_AIG_DELETE_POSITION_MENU, OnDeletePositionMenu)
	ON_COMMAND(IDC_TAB_AIG_POSITION_TYPE_MENU, OnPositionTypeMenu)
	ON_BN_CLICKED(IDC_AIG_POSITION_TYPE_BUTTON, OnPositionTypeButton)
	ON_BN_CLICKED(IDC_AIG_DELETE_POSITIONS_BUTTON, OnDeletePositionButton)
	ON_NOTIFY(NM_RCLICK, IDC_AIG_POSITIONS_LIST, OnRclickPositionsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_AIG_POSITIONS_LIST, OnKeydownPositionsList)
	ON_NOTIFY(NM_DBLCLK, IDC_AIG_POSITIONS_LIST, OnDblclkPositionsList)
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CTabAIGeneralDialog::DoDataExchange(CDataExchange* pDX)
{ 
	CResizeDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTabAIGeneralDialog)
	DDX_Control(pDX, IDC_AIG_POSITIONS_LIST, m_PositionsList);
	DDX_Control(pDX, IDC_AIG_MOBILE_REINFORCEMENT_SCRIPT_ID_LIST, m_ReinforcementsList);
	DDX_Control(pDX, IDC_AIG_MESSAGE_LABEL, m_MessageStatic);
	DDX_Radio(pDX, IDC_AIG_SIDE_0_RADIO_BUTTON, m_nSide);
	//}}AFX_DATA_MAP
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CTabAIGeneralDialog::AngleToText( WORD angle )
{
	int nAngle = angle;
	nAngle = nAngle * 360 / 0xFFFF;
	return NStr::Format( "%d", nAngle );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTabAIGeneralDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.nParameters.size() < ( REINFORCEMENTS_COLUMN_COUNT + POSITIONS_COLUMN_COUNT + 1 ) )
	{
		resizeDialogOptions.nParameters.resize( REINFORCEMENTS_COLUMN_COUNT + POSITIONS_COLUMN_COUNT + 1, 0 );
	}
	
	m_nSide = resizeDialogOptions.nParameters.back();
	CheckRadioButton( IDC_AIG_SIDE_0_RADIO_BUTTON, IDC_AIG_SIDE_1_RADIO_BUTTON, IDC_AIG_SIDE_0_RADIO_BUTTON + m_nSide );
	
	CreateControls();
	UpdateControls();
	
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::CreateControls()
{
	bCreateControls = true;
	m_ReinforcementsList.SetExtendedStyle( m_ReinforcementsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < REINFORCEMENTS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = REINFORCEMENTS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_ReinforcementsList.InsertColumn( nColumnIndex, REINFORCEMENTS_COLUMN_NAME[nColumnIndex], REINFORCEMENTS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bReinforcementsSortParam.push_back( true );
	}

	m_PositionsList.SetExtendedStyle( m_PositionsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < POSITIONS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + REINFORCEMENTS_COLUMN_COUNT] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + REINFORCEMENTS_COLUMN_COUNT] = POSITIONS_COLUMN_WIDTH[nColumnIndex];
		}

		int nNewColumn = m_PositionsList.InsertColumn( nColumnIndex, POSITIONS_COLUMN_NAME[nColumnIndex], POSITIONS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + REINFORCEMENTS_COLUMN_COUNT], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bPositionsSortParam.push_back( true );
	}
	bCreateControls = false;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	bool bFrameExists = ( pFrame && pAIEditor && pIDB && pTerrain );

	//Reinforcements buttons
	if ( pWnd = GetDlgItem( IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
	{
		pWnd->EnableWindow( bFrameExists );
	}
	if ( pWnd = GetDlgItem( IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
	{
		pWnd->EnableWindow( bFrameExists && ( m_ReinforcementsList.GetSelectedCount() > 0 ) );
	}
	
	//Positions buttons
	if ( pWnd = GetDlgItem( IDC_AIG_POSITION_TYPE_BUTTON ) )
	{
		pWnd->EnableWindow( bFrameExists && ( m_PositionsList.GetSelectedCount() > 0 ) );
	}
	if ( pWnd = GetDlgItem( IDC_AIG_DELETE_POSITIONS_BUTTON ) )
	{
		pWnd->EnableWindow( bFrameExists && ( m_PositionsList.GetSelectedCount() > 0 ) );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnColumnclickReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < REINFORCEMENTS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, REINFORCEMENTS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_ReinforcementsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_ReinforcementsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_ReinforcementsList.SortItems( ReinforcementsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bReinforcementsSortParam[nSortColumn] = !bReinforcementsSortParam[nSortColumn];
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnColumnclickPositionsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < POSITIONS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, POSITIONS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_PositionsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_PositionsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_PositionsList.SortItems( PositionsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bPositionsSortParam[nSortColumn] = !bPositionsSortParam[nSortColumn];
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTabAIGeneralDialog::LoadAIGReinforcementsInfo()
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	m_ReinforcementsList.DeleteAllItems();
	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];
			for ( int nReinforcementIndex = 0; nReinforcementIndex < rAIGeneralSideInfo.mobileScriptIDs.size(); ++nReinforcementIndex )
			{
				int nNewItem = m_ReinforcementsList.InsertItem( LVIF_TEXT, 0, NStr::Format( "%d", nReinforcementIndex ), 0, 0, 0, 0 );
				if ( nNewItem  != ( -1 ) )
				{
					SetReinforcementItem( nNewItem, rAIGeneralSideInfo.mobileScriptIDs[nReinforcementIndex] );
				}
			}
		}
	}
	UpdateControls();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTabAIGeneralDialog::LoadAIGPositionsInfo()
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	m_PositionsList.DeleteAllItems();
	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

			for ( int nPositionIndex = 0; nPositionIndex < rAIGeneralSideInfo.parcels.size(); ++nPositionIndex )
			{
				int nNewItem = m_PositionsList.InsertItem( LVIF_TEXT, 0, NStr::Format( "%d", nPositionIndex ), 0, 0, 0, 0 );
				if ( nNewItem  != ( -1 ) )
				{
					const SAIGeneralParcelInfo &rAIGeneralParcelInfo = rAIGeneralSideInfo.parcels[nPositionIndex];
					SetPositionItem( nNewItem, rAIGeneralParcelInfo );
				}
			}
		}
	}
	UpdateControls();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::SetReinforcementItem( int nNewItem, int nScriptID )
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();

	m_ReinforcementsList.SetItem( nNewItem, 1, LVIF_TEXT, NStr::Format( "%d", nScriptID ), 0, 0, 0, 0 );

	int nUnits = 0;
	int nInvalidUnits = 0;
	int nSquads = 0;

	GetUnitsCountByScriptID( nScriptID, m_nSide, pFrame, &nUnits, &nInvalidUnits, &nSquads );

	m_ReinforcementsList.SetItem( nNewItem, 2, LVIF_TEXT, NStr::Format( "%d", nUnits ), 0, 0, 0, 0 );
	m_ReinforcementsList.SetItem( nNewItem, 3, LVIF_TEXT, NStr::Format( "%d", nInvalidUnits ), 0, 0, 0, 0 );
	m_ReinforcementsList.SetItem( nNewItem, 4, LVIF_TEXT, NStr::Format( "%d", nSquads ), 0, 0, 0, 0 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::SetPositionItem( int nNewItem, const SAIGeneralParcelInfo &rAIGeneralParcelInfo )
{
	std::string szType;
	switch( rAIGeneralParcelInfo.eType )
	{
		case SAIGeneralParcelInfo::EPATCH_DEFENCE:
		{
			szType = POSITION_TYPE_LABELS[1];
			break;
		}
		case SAIGeneralParcelInfo::EPATCH_REINFORCE:
		{
			szType = POSITION_TYPE_LABELS[2];
			break;
		}
		default:
		{
			szType = POSITION_TYPE_LABELS[0];
			break;
		}
	}

	m_PositionsList.SetItem( nNewItem, 1, LVIF_TEXT, szType.c_str(), 0, 0, 0, 0 );
	m_PositionsList.SetItem( nNewItem, 2, LVIF_TEXT, NStr::Format( "%.2f", rAIGeneralParcelInfo.fRadius / ( 2 * SAIConsts::TILE_SIZE ) ), 0, 0, 0, 0 );
	m_PositionsList.SetItem( nNewItem, 3, LVIF_TEXT, NStr::Format( "(%.2f, %.2f)", rAIGeneralParcelInfo.vCenter.x / ( 2 * SAIConsts::TILE_SIZE ), rAIGeneralParcelInfo.vCenter.y / ( 2 * SAIConsts::TILE_SIZE ) ), 0, 0, 0, 0 );
	m_PositionsList.SetItem( nNewItem, 4, LVIF_TEXT, AngleToText( rAIGeneralParcelInfo.wDefenceDirection ).c_str(), 0, 0, 0, 0 );
	m_PositionsList.SetItem( nNewItem, 5, LVIF_TEXT, NStr::Format( "%d", rAIGeneralParcelInfo.reinforcePoints.size() ), 0, 0, 0, 0 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::UpdatePosition( int nPositionIndex )
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];
		
			bCreateControls = true;
			int nSelectedItem = m_PositionsList.GetNextItem( -1, LVNI_ALL );
			while ( nSelectedItem >= 0 )
			{
				std::string szKey = m_PositionsList.GetItemText( nSelectedItem, 0 );
				int nParcelIndex = -1;
				if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
						 ( nParcelIndex >= 0 ) && 
						 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
				{
					if ( nParcelIndex == nPositionIndex )
					{
						SetPositionItem( nSelectedItem, rAIGeneralSideInfo.parcels[nParcelIndex] );
						m_PositionsList.SetItemState( nSelectedItem, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED );
					}
					else
					{
						m_PositionsList.SetItemState( nSelectedItem, 0, LVNI_SELECTED | LVNI_FOCUSED );
					}
				}
				nSelectedItem = m_PositionsList.GetNextItem( nSelectedItem, LVNI_ALL );
			}
			bCreateControls = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::DeletePosition( int nPositionIndex )
{
	LoadAIGPositionsInfo();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::AddPosition( int nPositionIndex )
{
	LoadAIGPositionsInfo();

	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];
		
			bCreateControls = true;
			int nSelectedItem = m_PositionsList.GetNextItem( -1, LVNI_ALL );
			while ( nSelectedItem >= 0 )
			{
				std::string szKey = m_PositionsList.GetItemText( nSelectedItem, 0 );
				int nParcelIndex = -1;
				if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
						 ( nParcelIndex >= 0 ) && 
						 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
				{
					if ( nParcelIndex == nPositionIndex )
					{
						m_PositionsList.SetItemState( nSelectedItem, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED );
						break;
					}
				}
				nSelectedItem = m_PositionsList.GetNextItem( nSelectedItem, LVNI_ALL );
			}
			pFrame->SetMapModified();
			bCreateControls = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::GetUnitsCountByScriptID( int nScriptID, int nSide, CTemplateEditorFrame *pFrame, int *pUnits, int *pInvalidUnits, int *pSquads )
{
	( *pUnits ) = 0;
	( *pInvalidUnits ) = 0;
	( *pSquads ) = 0;

	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	std::set<IRefCount*> squads;
	
	//�� ������
	for ( std::unordered_map<SMapObject*, SEditorObjectItem*, SDefaultPtrHash>::const_iterator objectsIterator = pFrame->m_objectsAI.begin(); objectsIterator != pFrame->m_objectsAI.end(); ++objectsIterator )
	{
		IRefCount *pSquad = GetSingleton<IAIEditor>()->GetFormationOfUnit( objectsIterator->first->pAIObj );
		if ( !pSquad )
		{
			if ( objectsIterator->second->nScriptID == nScriptID )
			{
				if ( pFrame->currentMapInfo.diplomacies[objectsIterator->second->nPlayer] == nSide )
				{
					const SUnitBaseRPGStats *pUnitBaseRPGStats = dynamic_cast_gdb<const SUnitBaseRPGStats*>( objectsIterator->first->pRPG );
					if ( pUnitBaseRPGStats && ( IsArmor( pUnitBaseRPGStats->type ) || IsSPG( pUnitBaseRPGStats->type ) || IsTransport( pUnitBaseRPGStats->type ) ) )
					{
						++( *pUnits );
						continue;
					}
				}
				++( *pInvalidUnits );
			}
		}
		else
		{
			squads.insert( pSquad );
		}
	}
	
	//������� ������������ ������, ������ ���������� �� ���
	for( std::set< IRefCount* >::iterator squadIterator = squads.begin(); squadIterator != squads.end(); ++squadIterator )
	{
		IRefCount **pUnits;
		int nLength;
		pAIEditor->GetUnitsInFormation( ( *squadIterator ), &pUnits, &nLength );	
		SEditorObjectItem *tmpEditiorObj =	pFrame->m_objectsAI[ pFrame->FindByAI( pUnits[0] ) ];
		if ( tmpEditiorObj->nScriptID == nScriptID )
		{
			++( *pSquads );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnAigDeleteMobileReinforcementScriptIdButton() 
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

			int nSelectedItem = m_ReinforcementsList.GetNextItem( -1, LVNI_SELECTED );
			while ( nSelectedItem >= 0 )
			{
				std::string szKey = m_ReinforcementsList.GetItemText( nSelectedItem, 0 );
				int nScriptIDIndex = -1;
				if ( ( sscanf( szKey.c_str(), "%d", &nScriptIDIndex ) > 0 ) &&
						 ( nScriptIDIndex >= 0 ) && 
						 ( nScriptIDIndex < rAIGeneralSideInfo.mobileScriptIDs.size() ) )
				{
					rAIGeneralSideInfo.mobileScriptIDs[nScriptIDIndex] = ( -1 );
				}
				nSelectedItem = m_ReinforcementsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
			}
		
			for ( std::vector<int>::iterator scriptIDIterator = rAIGeneralSideInfo.mobileScriptIDs.begin(); scriptIDIterator != rAIGeneralSideInfo.mobileScriptIDs.end(); )
			{
				if ( ( *scriptIDIterator ) == ( -1 ) )
				{
					scriptIDIterator = rAIGeneralSideInfo.mobileScriptIDs.erase( scriptIDIterator );
				}
				else
				{
					++scriptIDIterator;
				}
			}
			pFrame->SetMapModified();
		}
		
		LoadAIGReinforcementsInfo();	
	}
	UpdateControls();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnAigAddMobileReinforcementScriptIdButton() 
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

			CTabAIGeneralEnterScriptIDDialog enterScriptIDDialog;
			if ( ( enterScriptIDDialog.DoModal() == IDOK ) && ( enterScriptIDDialog.m_ScriptID != ( -1 ) ) )
			{
				int nNewItem = m_ReinforcementsList.InsertItem( LVIF_TEXT, 0, NStr::Format( "%d", rAIGeneralSideInfo.mobileScriptIDs.size() ), 0, 0, 0, 0 );
				if ( nNewItem  != ( -1 ) )
				{
					rAIGeneralSideInfo.mobileScriptIDs.push_back( enterScriptIDDialog.m_ScriptID );
					SetReinforcementItem( nNewItem, rAIGeneralSideInfo.mobileScriptIDs.back() );
				}
			}
			pFrame->SetMapModified();
		}
	}
	UpdateControls();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnItemchangedAigMobileReinforcementScriptIdList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnItemchangedPositionsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();

	if ( !bCreateControls )
	{
		CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
		ICamera *pCamera = GetSingleton<ICamera>();
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

		if ( pFrame && pCamera && pTerrain )
		{
			const SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
			if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
			{
				const SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

				int nFocusedItem = m_PositionsList.GetNextItem( -1, LVNI_FOCUSED );
				if ( nFocusedItem >= 0 )
				{
					std::string szKey = m_PositionsList.GetItemText( nFocusedItem, 0 );
					int nParcelIndex = -1;
					if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
							 ( nParcelIndex >= 0 ) && 
							 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
					{
						CVec2 vPos = rAIGeneralSideInfo.parcels[nParcelIndex].vCenter;
						AI2Vis( &vPos );

						const CVec3 center3 = pFrame->GetScreenCenter();
						const CVec3 camera3 = pCamera->GetAnchor();
						vPos += CVec2( ( camera3.x - center3.x ), ( camera3.y - center3.y ) );
						
						pFrame->NormalizeCamera( &( CVec3( vPos, 0.0f ) ) );
						pFrame->inputStates.Update();
						pFrame->RedrawWindow();
					}
				}
			}
		}
	}
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnSide0RadioButton() 
{
	m_nSide = 0;
	LoadAIGReinforcementsInfo();
	LoadAIGPositionsInfo();
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
	resizeDialogOptions.nParameters.back() = m_nSide;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnSide1RadioButton() 
{
	m_nSide = 1;
	LoadAIGReinforcementsInfo();
	LoadAIGPositionsInfo();
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
	resizeDialogOptions.nParameters.back() = m_nSide;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnRclickReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 0 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_TAB_AIG_ADD_REINFORCEMENT_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_TAB_AIG_DELETE_REINFORCEMENT_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnKeydownReinforcementsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_ADD_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAigAddMobileReinforcementScriptIdButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_DELETE_MOBILE_REINFORCEMENT_SCRIPT_ID_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAigDeleteMobileReinforcementScriptIdButton();
			}
		}
	}
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnAddReinforcementMenu() 
{
	OnAigAddMobileReinforcementScriptIdButton();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnDeleteReinforcementMenu() 
{
	OnAigDeleteMobileReinforcementScriptIdButton();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnDeletePositionMenu() 
{
	OnDeletePositionButton();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnPositionTypeMenu() 
{
	OnPositionTypeButton();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnPositionTypeButton() 
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		int nType0Count = 0;
		int nType1Count = 0;
		
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

			int nSelectedItem = m_PositionsList.GetNextItem( -1, LVNI_SELECTED );
			while ( nSelectedItem >= 0 )
			{
				std::string szKey = m_PositionsList.GetItemText( nSelectedItem, 0 );
				int nParcelIndex = -1;
				if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
						 ( nParcelIndex >= 0 ) && 
						 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
				{
					switch( rAIGeneralSideInfo.parcels[nParcelIndex].eType )
					{
						case SAIGeneralParcelInfo::EPATCH_DEFENCE:
						{
							++nType0Count;
							break;
						}
						case SAIGeneralParcelInfo::EPATCH_REINFORCE:
						{
							++nType1Count;
							break;
						}
					}
				}
				nSelectedItem = m_PositionsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
			}

			CTabAIGeneralSetPositionTypeDialog setPositionTypeDialog;
			if ( nType0Count >= nType1Count )
			{
				setPositionTypeDialog.m_Type = 0;
			}
			else
			{
				setPositionTypeDialog.m_Type = 1;
			}
			if ( setPositionTypeDialog.DoModal() == IDOK )
			{
				nSelectedItem = m_PositionsList.GetNextItem( -1, LVNI_SELECTED );
				while ( nSelectedItem >= 0 )
				{
					std::string szKey = m_PositionsList.GetItemText( nSelectedItem, 0 );
					int nParcelIndex = -1;
					if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
							 ( nParcelIndex >= 0 ) && 
							 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
					{
						if ( setPositionTypeDialog.m_Type == 0 )
						{
							rAIGeneralSideInfo.parcels[nParcelIndex].eType = SAIGeneralParcelInfo::EPATCH_DEFENCE;
						}
						else
						{
							rAIGeneralSideInfo.parcels[nParcelIndex].eType = SAIGeneralParcelInfo::EPATCH_REINFORCE;
						}
					}
					nSelectedItem = m_PositionsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
				}
				LoadAIGPositionsInfo();
				pFrame->inputStates.Update();
				pFrame->RedrawWindow();
				pFrame->SetMapModified();
			}
		}
	}
	UpdateControls();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnDeletePositionButton()
{
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	IScene *pScene = GetSingleton<IScene>();
	IAIEditor* pAIEditor = GetSingleton<IAIEditor>();
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

	if ( pFrame && pAIEditor && pIDB && pTerrain )
	{
		SAIGeneralMapInfo &rAIGeneralMapInfo = pFrame->currentMapInfo.aiGeneralMapInfo;
		if ( ( m_nSide >= 0 ) && ( m_nSide < rAIGeneralMapInfo.sidesInfo.size() ) )
		{
			SAIGeneralSideInfo &rAIGeneralSideInfo = rAIGeneralMapInfo.sidesInfo[m_nSide];

			int nSelectedItem = m_PositionsList.GetNextItem( -1, LVNI_SELECTED );
			while ( nSelectedItem >= 0 )
			{
				std::string szKey = m_PositionsList.GetItemText( nSelectedItem, 0 );
				int nParcelIndex = -1;
				if ( ( sscanf( szKey.c_str(), "%d", &nParcelIndex ) > 0 ) &&
						 ( nParcelIndex >= 0 ) && 
						 ( nParcelIndex < rAIGeneralSideInfo.parcels.size() ) )
				{
					rAIGeneralSideInfo.parcels[nParcelIndex].eType = -1;
				}
				nSelectedItem = m_PositionsList.GetNextItem( nSelectedItem, LVNI_SELECTED );
			}
		
			for ( std::vector<SAIGeneralParcelInfo>::iterator parcelIterator = rAIGeneralSideInfo.parcels.begin(); parcelIterator != rAIGeneralSideInfo.parcels.end(); )
			{
				if ( parcelIterator->eType == ( -1 ) )
				{
					parcelIterator = rAIGeneralSideInfo.parcels.erase( parcelIterator );
				}
				else
				{
					++parcelIterator;
				}
			}
			pFrame->SetMapModified();
		}
		LoadAIGPositionsInfo();
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
	UpdateControls();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnRclickPositionsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 1 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_POSITION_TYPE_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_TAB_AIG_POSITION_TYPE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_DELETE_POSITIONS_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_TAB_AIG_DELETE_POSITION_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnDblclkPositionsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_AIG_POSITION_TYPE_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnPositionTypeButton();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnKeydownPositionsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_POSITION_TYPE_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
			OnPositionTypeButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_AIG_DELETE_POSITIONS_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeletePositionButton();
			}
		}
	}
	*pResult = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CResizeDialog ::OnLButtonUp(nFlags, point);
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CResizeDialog ::OnRButtonUp(nFlags, point);
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CResizeDialog ::OnRButtonDown(nFlags, point);
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CResizeDialog ::OnLButtonDown(nFlags, point);
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->inputStates.Update();
		pFrame->RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTabAIGeneralDialog::OnDestroy() 
{
	for ( int nColumnIndex = 0; nColumnIndex < REINFORCEMENTS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_ReinforcementsList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < POSITIONS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + REINFORCEMENTS_COLUMN_COUNT] = m_PositionsList.GetColumnWidth( nColumnIndex );
	}
	
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog ::OnDestroy();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
