#include "StdAfx.h"
#include "editor.h"

#include "InputNotifyShortcutBar.h"
#include "TemplateEditorFrame1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP( CInputNotifyShortcutBar, SECShortcutBar )
	ON_MESSAGE( TCM_TABSEL, OnNotify3DTabChangePage )	
END_MESSAGE_MAP()

BOOL CInputNotifyShortcutBar::OnChangeBar( int nShortcutIndex )
{
	SNotifyStruct notifyStruct;
	if ( CWnd *pParent = GetParent() )
	{
		memset( &notifyStruct, 0, sizeof( SNotifyStruct ) );
		notifyStruct.hwndFrom				= GetSafeHwnd();
		notifyStruct.idFrom					= IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00;
		notifyStruct.code						= NM_CHANGE_PAGE;
		notifyStruct.nShortcutIndex	= nShortcutIndex;
		notifyStruct.nTabIndex	= CInputStateParameter::INVALID_STATE;
		pParent->SendMessage( WM_NOTIFY, IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00, reinterpret_cast<LPARAM>( &notifyStruct ) );
	}
	return true;
}

LRESULT CInputNotifyShortcutBar::OnNotify3DTabChangePage( WPARAM wParam, LPARAM /*lParam*/ )
{
	SNotifyStruct notifyStruct;
	if ( CWnd *pParent = GetParent() )
	{
		memset( &notifyStruct, 0, sizeof( SNotifyStruct ) );
		notifyStruct.hwndFrom				= GetSafeHwnd();
		notifyStruct.idFrom					= IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00;
		notifyStruct.code						= NM_CHANGE_PAGE;
		notifyStruct.nShortcutIndex = GetActiveIndex();
		notifyStruct.nTabIndex	= wParam;
		pParent->SendMessage( WM_NOTIFY, IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00, reinterpret_cast<LPARAM>( &notifyStruct ) );
	}
	return true;
	return 0;
}
