#include "StdAfx.h"
#include "editor.h"
#include "TabAIGeneralEnterScriptIDDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTabAIGeneralEnterScriptIDDialog::CTabAIGeneralEnterScriptIDDialog( CWnd* pParent )
	: CDialog( CTabAIGeneralEnterScriptIDDialog::IDD, pParent )
{
	m_ScriptID = 0;
}

void CTabAIGeneralEnterScriptIDDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TAB_AI_GENERAL_ENTER_SCRIPT_ID_EDIT, m_ScriptID);
}

BEGIN_MESSAGE_MAP(CTabAIGeneralEnterScriptIDDialog, CDialog)
END_MESSAGE_MAP()
