#include "StdAfx.h"
#include "editor.h"

#include "PropertieDialog.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"

#include "AIStartCommand.h"
#include "AIStartCommandsDialog.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CAIStartCommandsDialog::vID[] = 
{
	IDC_SCP_COMMANDS_LIST,								//0
	IDC_SCP_DELETE_BUTTON,								//1
	IDC_SCP_EDIT_BUTTON,									//2
	IDOK,																	//2
	IDCANCEL,															//3
};

CAIStartCommandsDialog::CAIStartCommandsDialog(CWnd* pParent /*=NULL*/)
	: CResizeDialog(CAIStartCommandsDialog::IDD, pParent),
		m_frame( 0 ), bAddCommand( false )
{

	SetControlStyle( IDC_SCP_COMMANDS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );

	SetControlStyle( IDC_SCP_DELETE_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SCP_EDIT_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDOK, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_RIGHT_BOTTOM );
}

void CAIStartCommandsDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAIStartCommandsDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_SCP_DELETE_BUTTON, OnScpDeleteButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_SCP_COMMANDS_LIST, OnItemchangedScpCommandsList)
	ON_BN_CLICKED(IDC_SCP_EDIT_BUTTON, OnScpEditButton)
	ON_NOTIFY(NM_RCLICK, IDC_SCP_COMMANDS_LIST, OnRclickScpCommandsList)
	ON_NOTIFY(NM_DBLCLK, IDC_SCP_COMMANDS_LIST, OnDblclkScpCommandsList)
	ON_COMMAND(IDC_SCP_DELETE_MENU, OnScpDeleteMenu)
	ON_COMMAND(IDC_SCP_EDIT_MENU, OnScpEditMenu)
	ON_NOTIFY(LVN_KEYDOWN, IDC_SCP_COMMANDS_LIST, OnKeydownScpCommandsList)
	ON_BN_CLICKED(IDC_SCP_ADD_BUTTON, OnAddButton)
	ON_COMMAND(IDC_SCP_ADD_MENU, OnAddMenu)
END_MESSAGE_MAP()

const int   PROPERTIES_COLUMN_COUNT = 4;
const char *PROPERTIES_COLUMN_NAME  [PROPERTIES_COLUMN_COUNT] = { "Type", "Units", "Position", "Parameter" };
const int   PROPERTIES_COLUMN_FORMAT[PROPERTIES_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT };
int					PROPERTIES_COLUMN_WIDTH [PROPERTIES_COLUMN_COUNT] = { 250, 60, 100, 100 };

BOOL CAIStartCommandsDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	resizeDialogOptions.nParameters.resize( PROPERTIES_COLUMN_COUNT, 0 );

	AddColumns();
	AddElements();

	return TRUE;
}

void CAIStartCommandsDialog::AddColumns()
{
	if ( m_frame )
	{
		CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
		if ( pListCtrl )
		{
			pListCtrl->SetExtendedStyle( pListCtrl->GetExtendedStyle() | LVS_EX_FULLROWSELECT );
			for ( int nColumnIndex = 0; nColumnIndex < PROPERTIES_COLUMN_COUNT; ++nColumnIndex )
			{
				if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
				{
					resizeDialogOptions.nParameters[nColumnIndex] = PROPERTIES_COLUMN_WIDTH[nColumnIndex];
				}
				
				pListCtrl->InsertColumn( nColumnIndex,
																 PROPERTIES_COLUMN_NAME[nColumnIndex],
																 PROPERTIES_COLUMN_FORMAT[nColumnIndex],
																 resizeDialogOptions.nParameters[nColumnIndex],
																 nColumnIndex );
			}
		}
	}
}

void CAIStartCommandsDialog::AddElements()
{
	if ( m_frame )
	{
		CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
		if ( pListCtrl )
		{
			pListCtrl->DeleteAllItems();
			int nIndex = 0;
			for ( TMutableAIStartCommandList::const_iterator it = m_startCommands.begin(); it != m_startCommands.end(); ++it )
			{
				int item = -1;

				std::string szCommandName = m_frame->aiscHelper.commands[CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX].szName;
				for( int nCommandIndex = 0; nCommandIndex < m_frame->aiscHelper.commands.size(); ++nCommandIndex )
				{
					if ( it->cmdType == m_frame->aiscHelper.commands[nCommandIndex].nID )
					{
						szCommandName = m_frame->aiscHelper.commands[nCommandIndex].szName;
					}
				}
				
				item = pListCtrl->InsertItem( nIndex, szCommandName.c_str() );
				if ( item != ( -1 ) )
				{
					pListCtrl->SetItemData( item, nIndex );

					
					int nUnitsNumber = 0;
					std::set<IRefCount*> squads;
					for ( std::list<SMapObject*>::const_iterator objectIterator = it->pMapObjects.begin(); objectIterator != it->pMapObjects.end(); ++objectIterator )
					{
						IRefCount* pSquad = GetSingleton<IAIEditor>()->GetFormationOfUnit( ( *objectIterator )->pAIObj ) ;
						if ( pSquad )
						{
							squads.insert( pSquad );
						}
						else
						{
							++nUnitsNumber;
						}
					}
					for( std::set< IRefCount* >::const_iterator squadIterator = squads.begin(); squadIterator != squads.end(); ++squadIterator )
					{
						++nUnitsNumber;
					}

					pListCtrl->SetItem( item, 1, LVIF_TEXT, NStr::Format("%d", nUnitsNumber ), 0, 0, 0, 0 );
					pListCtrl->SetItem( item, 2, LVIF_TEXT, NStr::Format("(%.2f, %.2f)", it->vPos.x / ( 2 * SAIConsts::TILE_SIZE ), it->vPos.y / ( 2 * SAIConsts::TILE_SIZE ) ), 0, 0, 0, 0 );
					pListCtrl->SetItem( item, 3, LVIF_TEXT, NStr::Format("%.2f", it->fNumber ), 0, 0, 0, 0 );
				}
				++nIndex;
			}
		}
	}
}

void CAIStartCommandsDialog::OnOK() 
{
	if ( m_frame )
	{
		for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); ++startCommandIterator )
		{
			m_frame->m_startCommands.push_back( *startCommandIterator );
		}
	}
	CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
	if ( pListCtrl )
	{		
		for ( int nColumnIndex = 0; nColumnIndex < PROPERTIES_COLUMN_COUNT; ++nColumnIndex )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = pListCtrl->GetColumnWidth( nColumnIndex );
		}
	}
	CResizeDialog::OnOK();
}

void CAIStartCommandsDialog::OnCancel() 
{
	if ( m_frame )
	{
		for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommandsUndo.begin(); startCommandIterator != m_startCommandsUndo.end(); ++startCommandIterator )
		{
			m_frame->m_startCommands.push_back( *startCommandIterator );
		}
	}
	CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
	if ( pListCtrl )
	{		
		for ( int nColumnIndex = 0; nColumnIndex < PROPERTIES_COLUMN_COUNT; ++nColumnIndex )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = pListCtrl->GetColumnWidth( nColumnIndex );
		}
	}
	CResizeDialog::OnCancel();
}

void CAIStartCommandsDialog::OnScpEditButton() 
{
	if ( m_frame )
	{
		CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
		if ( pListCtrl )
		{
			int item = -1;

			m_frame->dlg = new CPropertieDialog;
			m_frame->dlg->Create( CPropertieDialog::IDD, m_frame );
			m_frame->dlg->ClearVariables();
			m_frame->isStartCommandPropertyActive = true;
			m_frame->m_PointForAIStartCommand.clear();
			
			CPtr<IMultiManipulator> pMan = new CMultiManipulator;
		
			while ( ( item = pListCtrl->GetNextItem( item, LVNI_ALL ) ) != ( -1 ) )
			{
				int nCount = pListCtrl->GetItemData( item );
				int nIndex = 0;
				TMutableAIStartCommandList::iterator it = m_startCommands.begin();
				for ( ; it != m_startCommands.end(); ++it )
				{
					if ( nIndex == nCount )
					{
						break;
					}
					++nIndex;
				}
				if ( it != m_startCommands.end() )
				{
					m_frame->m_startCommands.push_back( *it );
					if ( pListCtrl->GetItemState( item, LVNI_SELECTED ) == LVNI_SELECTED )
					{
						pMan->AddManipulator( ( --( m_frame->m_startCommands.end() ) )->GetManipulator() );
						m_frame->AddStartCommandRedLines( --( m_frame->m_startCommands.end() ) );
					}
				}
			}
			m_frame->dlg->AddObjectWithProp( pMan );
			m_frame->RedrawWindow();

			CResizeDialog::OnOK();
		}
	}
}

void CAIStartCommandsDialog::OnScpDeleteButton() 
{
	if ( m_frame )
	{
		CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
		if ( pListCtrl )
		{
			int item = -1;
			while ( ( item = pListCtrl->GetNextItem( item, LVNI_SELECTED ) ) != ( -1 ) )
			{
				int nCount = pListCtrl->GetItemData( item );
				int nIndex = 0;
				for ( TMutableAIStartCommandList::iterator it = m_startCommands.begin(); it != m_startCommands.end(); ++it )
				{
					if ( nIndex == nCount )
					{
						it->pMapObjects.clear();
						break;
					}
					++nIndex;
				}
			}
			for ( TMutableAIStartCommandList::iterator it = m_startCommands.begin(); it != m_startCommands.end(); )
			{
				if ( it->pMapObjects.empty() )
				{
					it = m_startCommands.erase( it );
				}
				else
				{
					++it;
				}
			}
			AddElements();
			UpdateButtons();
		}
	}
}

void CAIStartCommandsDialog::UpdateButtons()
{
	if ( m_frame )
	{
		CListCtrl* pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_SCP_COMMANDS_LIST ) );
		if ( pListCtrl )
		{
			CWnd* pWnd = GetDlgItem( IDC_SCP_DELETE_BUTTON );
			if ( pWnd )
			{
				pWnd->EnableWindow( pListCtrl->GetSelectedCount() > 0 );
			}
			pWnd = GetDlgItem( IDC_SCP_EDIT_BUTTON );
			if ( pWnd )
			{
				pWnd->EnableWindow( pListCtrl->GetSelectedCount() > 0 );
			}
		}
	}	
}

void CAIStartCommandsDialog::OnItemchangedScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	HD_NOTIFY *phdn = (HD_NOTIFY *) pNMHDR;
	UpdateButtons();
	*pResult = 0;
}

void CAIStartCommandsDialog::OnRclickScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 3 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SCP_EDIT_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_SCP_EDIT_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_SCP_DELETE_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_SCP_DELETE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_SCP_ADD_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_SCP_ADD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

void CAIStartCommandsDialog::OnDblclkScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SCP_EDIT_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnScpEditButton();
		}
	}
	*pResult = 0;
}

void CAIStartCommandsDialog::OnScpDeleteMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SCP_DELETE_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnScpDeleteButton();
		}
	}
}

void CAIStartCommandsDialog::OnScpEditMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SCP_EDIT_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnScpEditButton();
		}
	}
}

void CAIStartCommandsDialog::OnKeydownScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SCP_DELETE_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnScpDeleteButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_SCP_EDIT_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnScpEditButton();
			}
		}
	}
	*pResult = 0;
}

void CAIStartCommandsDialog::OnAddButton() 
{
	bAddCommand = true;
	OnOK();
}

void CAIStartCommandsDialog::OnAddMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_SCP_ADD_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddButton();
		}
	}
}
