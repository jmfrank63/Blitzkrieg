
#include "StdAfx.h"
#include "MultiTreeEditBox.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMultiTreeEditBox::CMultiTreeEditBox()
{
}

CMultiTreeEditBox::~CMultiTreeEditBox()
{
}


BEGIN_MESSAGE_MAP(CMultiTreeEditBox, CEdit)
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

int CMultiTreeEditBox::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CEdit::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	SetFont( GetParent()->GetParent()->GetParent()->GetFont(), false );
	return 0;
}


void CMultiTreeEditBox::OnKillFocus(CWnd* pNewWnd) 
{
	GetParent()->SendMessage( WM_USER + 1);
}




BOOL CMultiTreeEditBox::PreTranslateMessage(MSG* pMsg) 
{
		if (pMsg->message == WM_KEYDOWN &&
			pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
		{
			GetParent()->SendMessage( WM_USER + 1);
			return TRUE;
		}
		return CEdit::PreTranslateMessage(pMsg);
}
