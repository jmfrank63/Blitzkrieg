
#include "StdAfx.h"
#include "TreeItemComboBox.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CTreeItemComboBox::CTreeItemComboBox()
{
}

CTreeItemComboBox::~CTreeItemComboBox()
{
}


BEGIN_MESSAGE_MAP(CTreeItemComboBox, CComboBox)
	ON_WM_KILLFOCUS() 
	ON_WM_CREATE()
END_MESSAGE_MAP()


void CTreeItemComboBox::OnKillFocus(CWnd* pNewWnd) 
{
		GetParent()->SendMessage( WM_USER + 1);
	
}

int CTreeItemComboBox::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CComboBox::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	SetFont( GetParent()->GetParent()->GetParent()->GetFont(), false );
	return 0;
}


BOOL CTreeItemComboBox::PreTranslateMessage(MSG* pMsg) 
{
		if (pMsg->message == WM_KEYDOWN &&
			pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
		{
			GetParent()->SendMessage( WM_USER + 1);
			return TRUE;
		}
		return CComboBox::PreTranslateMessage(pMsg);
}
