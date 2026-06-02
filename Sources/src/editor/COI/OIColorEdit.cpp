#include "StdAfx.h"
#include "OIColorEdit.h"

BEGIN_MESSAGE_MAP(COIColorEdit, COIBrowseEdit)
	ON_WM_CREATE()
END_MESSAGE_MAP()

void COIColorEdit::OnBrowse()
{
	CString szColor;
  m_Edit.GetWindowText( szColor );	
	DWORD col = atoi( szColor );

	CColorDialog dlg( col );

	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( dlg.m_cc.lpCustColors )
	{
		dlg.m_cc.lpCustColors[0] = col;
	}
	if ( IDOK != dlg.DoModal() )
		return;
	col = dlg.GetColor();
	char buf[32];
	itoa( col, buf, 10 );
	m_Edit.SetWindowText( buf );
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}

int COIColorEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_Edit.SetReadOnly();
	return 0;
}
