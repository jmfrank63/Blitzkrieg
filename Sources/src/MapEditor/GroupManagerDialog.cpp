#include "stdafx.h"
#include "editor.h"
#include "GroupManagerDialog.h"
#include "EnterScriptIDDialog.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"
#include "..\Formats\fmtMap.h"
#include "GetGroupID.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CGroupManagerDialog::vID[] = 
{
	IDC_GROUPS_LABEL,										//0
	IDC_GROUPS_LIST,										//1
	IDC_GROUPS_DELETE_BUTTON,						//2
	IDC_GROUPS_ADD_BUTTON,							//3
	IDC_GROUPS_PROPERTY_LABEL,					//4
	IDC_GROUPS_PROPERTY_LIST,						//5
	IDC_GROUPS_DELETE_PROPERTY_BUTTON,	//6
	IDC_GROUPS_ADD_PROPERTY_BUTTON,			//7
};

CGroupManagerDialog::CGroupManagerDialog( CWnd* pParent )
	: CResizeDialog( CGroupManagerDialog::IDD, pParent )
{

	SetControlStyle( IDC_GROUPS_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_GROUPS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 1.0f, 0.5f );
	SetControlStyle( IDC_GROUPS_DELETE_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER );
	SetControlStyle( IDC_GROUPS_ADD_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_VER_CENTER );
	SetControlStyle( IDC_GROUPS_PROPERTY_LABEL, ANCHORE_LEFT | ANCHORE_VER_CENTER );
	SetControlStyle( IDC_GROUPS_PROPERTY_LIST, ANCHORE_LEFT_BOTTOM | RESIZE_HOR_VER, 0.5f, 0.5f, 1.0f, 0.5f );
	SetControlStyle( IDC_GROUPS_DELETE_PROPERTY_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDC_GROUPS_ADD_PROPERTY_BUTTON,  ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
}

void CGroupManagerDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GROUPS_PROPERTY_LIST, m_groupInfo);
	DDX_Control(pDX, IDC_GROUPS_LIST, m_groupList);
}


BEGIN_MESSAGE_MAP(CGroupManagerDialog, CResizeDialog)
	ON_LBN_SELCHANGE(IDC_GROUPS_LIST, OnSelchangeList1)
	ON_BN_CLICKED(IDC_GROUPS_ADD_BUTTON, OnNewGroup)
	ON_BN_CLICKED(IDC_GROUPS_DELETE_BUTTON, OnDeleteGroup)
	ON_BN_CLICKED(IDC_GROUPS_ADD_PROPERTY_BUTTON, OnAddIDGroupForCurrentRefGroup)
	ON_BN_CLICKED(IDC_GROUPS_DELETE_PROPERTY_BUTTON, OnDeleteScirptIDItem)
	ON_WM_TIMER()
	ON_LBN_SELCHANGE(IDC_GROUPS_PROPERTY_LIST, OnSelchangeGroupsPropertyList)
END_MESSAGE_MAP()

void CGroupManagerDialog::OnSelchangeList1() 
{
	reinterpret_cast<CWnd *>( g_frameManager.GetTemplateEditorFrame())->SendMessage( WM_USER + 7 );
	RedrawGroup();
	UpdateControls();
}

void CGroupManagerDialog::OnSelchangeGroupsPropertyList() 
{
	UpdateControls();
}

bool CGroupManagerDialog::IfIDChecked(int id)
{
	bool retVal = false;
	for( int i = 0; i != m_groupList.GetCount(); ++i )
	{
		if( m_groupList.GetItemData( i ) == id && m_groupList.GetCheck( i ) == 1 )
			retVal = true;
	}
	return retVal;
}

void CGroupManagerDialog::OnNewGroup() 
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		SReinforcementGroupInfo info = pFrame->GetGroupInfo();
		CGetGroupID dlg;
		if ( dlg.DoModal() == IDOK )
		{
			int nId = dlg.m_id;
			while ( info.groups.find( nId ) != info.groups.end() )
			{
				++nId;
			}
			SReinforcementGroupInfo::SGroupsVector vecTmp;
			info.groups.insert( std::make_pair( nId, vecTmp ) );
			int num = m_groupList.AddString( NStr::Format( "Group N:%d", nId ) );	
			m_groupList.SetItemData ( num , nId );	
			UpdateControls();
			pFrame->SetGroupInfo( info );
			pFrame->SetMapModified();
		}
		else
		{
			return;
		}
	}
}

void CGroupManagerDialog::OnDeleteGroup() 
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if( m_groupList.GetCurSel() != -1 )
		{
			SReinforcementGroupInfo info = pFrame->GetGroupInfo();
			int nId = m_groupList.GetItemData( m_groupList.GetCurSel() );
			info.groups.erase( nId );
			m_groupList.DeleteString( m_groupList.GetCurSel() );
			m_groupInfo.ResetContent();
			pFrame->SetGroupInfo( info );
			pFrame->SetMapModified();
			UpdateControls();
		}
	}
}

void CGroupManagerDialog::OnAddIDGroupForCurrentRefGroup() 
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if( m_groupList.GetCurSel() != -1 )
		{
			SReinforcementGroupInfo info = pFrame->GetGroupInfo();
			int nId = m_groupList.GetItemData( m_groupList.GetCurSel() );
			CEnterScriptIDDialog dlg;
			if ( dlg.DoModal() == IDOK )
			{
				int nScriptId = dlg.m_id;
				if( info.groups.find( nId ) != info.groups.end() )
				{
					bool bNotFound = true;
					for ( int nScriptIDIndex = 0; nScriptIDIndex < info.groups[ nId ].ids.size(); ++nScriptIDIndex )
					{
						if ( 	info.groups[ nId ].ids[nScriptIDIndex] == nScriptId )
						{
							bNotFound = false;
							break;
						}
					}
					if ( bNotFound )
					{
						info.groups[ nId ].ids.push_back( nScriptId ); 
						pFrame->SetGroupInfo( info );
						pFrame->SetMapModified();
						RedrawGroup();
					}
				}
			}
			UpdateControls();
		}
	}
}

void CGroupManagerDialog::RedrawGroup()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		m_groupInfo.ResetContent();
		if( m_groupList.GetCurSel() != -1 )
		{
			SReinforcementGroupInfo info = pFrame->GetGroupInfo();
			int nId = m_groupList.GetItemData( m_groupList.GetCurSel() );
			if( info.groups.find( nId ) != info.groups.end() )
			{
				int nIndex = 0;
				for( std::vector<int>::iterator it = info.groups[nId].ids.begin(); it != info.groups[nId].ids.end(); ++it )
				{
					int num =  m_groupInfo.AddString( NStr::Format( "Script Id:%d", (*it) ) );
	 				m_groupInfo.SetItemData ( num , nIndex );	
					++nIndex;
				}
			}
			pFrame->SetGroupInfo( info );
		}
	}
}

void CGroupManagerDialog::OnDeleteScirptIDItem() 
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if( m_groupList.GetCurSel() != -1 && m_groupInfo.GetCurSel() != -1 )
		{
			SReinforcementGroupInfo info = pFrame->GetGroupInfo();
			int nId = m_groupList.GetItemData( m_groupList.GetCurSel() );
			int scriptIdIndex = m_groupInfo.GetItemData( m_groupInfo.GetCurSel() );
			if( info.groups.find( nId ) != info.groups.end() )
			{
				if( scriptIdIndex >= 0 && scriptIdIndex < info.groups[ nId ].ids.size() )
				{
					info.groups[ nId ].ids.erase( info.groups[ nId ].ids.begin() + scriptIdIndex );	
					m_groupInfo.DeleteString( m_groupInfo.GetCurSel() );
				}
			}
			pFrame->SetGroupInfo( info );
			pFrame->SetMapModified();
			m_groupInfo.SetCurSel( -1 );
			RedrawGroup();
			UpdateControls();
		}
	}
}

void CGroupManagerDialog::UpdateControls()
{
	bool bEnabled = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnabled = true;
			}
		}
	}
	
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_LIST ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_DELETE_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_groupList.GetCurSel() != -1 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_ADD_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_PROPERTY_LIST ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_groupList.GetCurSel() != -1 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_ADD_PROPERTY_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_groupList.GetCurSel() != -1 ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_GROUPS_DELETE_PROPERTY_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_groupList.GetCurSel() != -1 ) && ( m_groupInfo.GetCurSel() != -1  ) );
	}
}

BOOL CGroupManagerDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	UpdateControls();
	return TRUE;
}
