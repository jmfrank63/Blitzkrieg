
#include "stdafx.h"
#include "editor.h"
#include "MultySelDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CMultySelDialog::CMultySelDialog(CWnd* pParent /*=NULL*/)
: CDialog(CMultySelDialog::IDD, pParent), nValue( 0 )
{
}


void CMultySelDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MULTY_LIST, m_multyList);
}


BEGIN_MESSAGE_MAP(CMultySelDialog, CDialog)
END_MESSAGE_MAP()


void CMultySelDialog::Init( int nRefId, int64 nVal )
{
	nReferenceType = (EReferenceType) nRefId;
	nValue = nVal;
}

std::string CMultySelDialog::GetValue()
{
	return szResult;
}

BOOL CMultySelDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	int height = m_multyList.GetItemHeight( 0 );
  m_multyList.SetItemHeight( 0, height - 2 );
	
	for ( int i=0; i<m_multyList.GetCount(); i++)
		m_multyList.DeleteString( i );
	
	switch ( nReferenceType )
	{
		case E_ACTIONS_REF:
			LoadActions();
			break;
		default:
			NI_ASSERT( 0 );
			break;
	}
	m_multyList.SetCurSel( 0 );
	
	return TRUE;  // return TRUE unless you set the focus to a control
}

void CMultySelDialog::LoadActions()
{
	std::string szFileName = theApp.GetEditorDataDir();
	CPtr<IDataBase> pDB = OpenDataBase( 0, TABLE_ACCESS_READ );	// to work with packed resources
	CTableAccessor table = pDB->OpenTable( "editor\\actions.ini", TABLE_ACCESS_READ );
	std::vector<std::string> szRowNames, szEntryNames;
	table.GetRowNames( szRowNames );
	NI_ASSERT( szRowNames.size() == 1 );
	table.GetEntryNames( szRowNames[0].c_str(), szEntryNames );
	
	for ( int i=0; i<szEntryNames.size(); i++ )
	{
		int nIndex = m_multyList.AddString( szEntryNames[i].c_str() );
		int nData = table.GetInt( szRowNames[0].c_str(), szEntryNames[i].c_str(), -1 );
		NI_ASSERT( nData != -1 );
		m_multyList.SetItemData( nIndex, nData );

		if ( nValue & ((int64) 1 << nData) )
			m_multyList.SetCheck( nIndex, 1 );
	}
}

void CMultySelDialog::OnOK() 
{
	int64 nRes = 0;
	
  UpdateData();
	for ( int i=0; i<m_multyList.GetCount(); ++i )
	{
		if ( m_multyList.GetCheck( i ) )
		{
			int nData = m_multyList.GetItemData( i );
			nRes |= (int64) 1 << nData;
		}
	}
	
	char temp[255];
	_ui64toa( nRes, temp, 16 );
	szResult = temp;
	
	CDialog::OnOK();
}
