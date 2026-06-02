
#include "stdafx.h"
#include "OIEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


COIEdit::COIEdit()
{
}

COIEdit::~COIEdit()
{
}


BEGIN_MESSAGE_MAP(COIEdit, CEdit)
	ON_WM_KILLFOCUS()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()


void COIEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch ( nChar )
	{
		case VK_DOWN:
		case VK_RETURN:
		case VK_UP:
			GetParent()->PostMessage( WM_USER + 1, nChar );
			break;

		case VK_ESCAPE:
			GetParent()->PostMessage( WM_USER + 1, nChar );
			break;

		default:
			CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
			break;
	}
}

void COIEdit::OnKillFocus(CWnd* pNewWnd) 
{
	CEdit::OnKillFocus(pNewWnd);
	if ( pNewWnd && pNewWnd != GetParent() )
		GetParent()->PostMessage( WM_USER + 2, (WPARAM)pNewWnd->m_hWnd );
}
