#include "StdAfx.h"
#include "editor.h"
#include "TabAIGeneralSetPositionTypeDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTabAIGeneralSetPositionTypeDialog::CTabAIGeneralSetPositionTypeDialog( CWnd* pParent )
	: CDialog( CTabAIGeneralSetPositionTypeDialog::IDD, pParent )
{
	m_Type = 0;
}

void CTabAIGeneralSetPositionTypeDialog::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_TAB_AI_GENERAL_SPT_TYPE0, m_Type);
}

BEGIN_MESSAGE_MAP(CTabAIGeneralSetPositionTypeDialog, CDialog)
	ON_BN_CLICKED(IDC_TAB_AI_GENERAL_SPT_TYPE0, OnTabAiGeneralSptType0)
	ON_BN_CLICKED(IDC_TAB_AI_GENERAL_SPT_TYPE1, OnTabAiGeneralSptType1)
END_MESSAGE_MAP()

void CTabAIGeneralSetPositionTypeDialog::OnTabAiGeneralSptType0() 
{
	m_Type = 0;
}

void CTabAIGeneralSetPositionTypeDialog::OnTabAiGeneralSptType1() 
{
	m_Type = 1;
}
