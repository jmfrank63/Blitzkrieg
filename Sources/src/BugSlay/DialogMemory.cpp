// DialogMemory.cpp: implementation of the CDialogMemory class.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "DialogMemory.h"
#include "WndUtils.h"

/*
BOOL CALLBACK DlgProcMemory( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam );

////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDialogMemory::DoModal( HINSTANCE hInstance, HWND hWndParent )
{
	return DialogBoxParam( hInstance, "IDD_DIALOG_MEMORY", hWndParent, reinterpret_cast<DLGPROC>(DlgProcMemory), reinterpret_cast<LPARAM>(this) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK DlgProcMemory( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_CLOSE:
			EndDialog( hwndDlg, 0 );
			return true;

		case WM_COMMAND:
			return reinterpret_cast<CDialogMemory*>(lParam)->OnWndCmd( hwndDlg, LOWORD(wParam), HIWORD(wParam), lParam );
		case WM_NOTIFY:
			return reinterpret_cast<CDialogMemory*>(lParam)->OnWndNotify( hwndDlg, (int)wParam, lParam );
		default:
			return reinterpret_cast<CDialogMemory*>(lParam)->OnWndMsg( hwndDlg, message, wParam, lParam );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDialogMemory::OnWndMsg( HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_INITDIALOG:
		{
			// center dialog
			RECT rc;
			GetWindowRect( hwndDlg, &rc );
			MoveWindow( hwndDlg, (GetSystemMetrics(SM_CXFULLSCREEN) - rc.right + rc.left) / 2,
								 (GetSystemMetrics(SM_CYFULLSCREEN) - rc.bottom + rc.top) / 2,
								 rc.right - rc.left, rc.bottom - rc.top, true );

			HWND hwndListFull = GetDlgItem( hwndDlg, IDC_LIST_DETAILS );
			HWND hwndListSumm = GetDlgItem( hwndDlg, IDC_LIST_SUMMARY );

			GetClientRect( hwndListFull, &rc );
			int nPart10 = (rc.right - rc.left) / 10;
			ListView_AddColumn( hwndListFull, "File", nPart10 * 2 );
			ListView_AddColumn( hwndListFull, "Line", nPart10 );
			ListView_AddColumn( hwndListFull, "Allocs", nPart10 );
			ListView_AddColumn( hwndListFull, "Frees", nPart10 );
			ListView_AddColumn( hwndListFull, "AllocMem", nPart10 );
			ListView_AddColumn( hwndListFull, "FreedMem", nPart10 );
			std::unordered_map<std::string, SExternalDebugAllocStat> mapSumm;
			for( const SExternalDebugAllocStat *it = m_pAllocStatBegin; it != m_pAllocStatEnd; ++it )
			{
				int nNewLine = ListView_AddItem( hwndListFull, it->pszFileName, LPARAM(it), 2000000 );
				char buff[100];

				itoa( it->uLineNumber, buff, 10 );
				ListView_SetItemText( hwndListFull, nNewLine, 1, buff );
				mapSumm[it->pszFileName].uLineNumber += it->uLineNumber;

				itoa( it->uNumAllocs, buff, 10 );
				ListView_SetItemText( hwndListFull, nNewLine, 2, buff );
				mapSumm[it->pszFileName].uNumAllocs += it->uNumAllocs;

				itoa( it->uNumFrees, buff, 10 );
				ListView_SetItemText( hwndListFull, nNewLine, 3, buff );
				mapSumm[it->pszFileName].uNumFrees += it->uNumFrees;

				itoa( it->uAllocatedMemory, buff, 10 );
				ListView_SetItemText( hwndListFull, nNewLine, 4, buff );
				mapSumm[it->pszFileName].uAllocatedMemory += it->uAllocatedMemory;

				itoa( it->uFreedMemory, buff, 10 );
				ListView_SetItemText( hwndListFull, nNewLine, 5, buff );
				mapSumm[it->pszFileName].uFreedMemory += it->uFreedMemory;
			}

			GetClientRect( hwndListSumm, &rc );
			nPart10 = (rc.right - rc.left) / 10;
			ListView_AddColumn( hwndListSumm, "File", nPart10 * 2 );
			ListView_AddColumn( hwndListSumm, "Line", nPart10 );
			ListView_AddColumn( hwndListSumm, "Allocs", nPart10 );
			ListView_AddColumn( hwndListSumm, "Frees", nPart10 );
			ListView_AddColumn( hwndListSumm, "AllocMem", nPart10 );
			ListView_AddColumn( hwndListSumm, "FreedMem", nPart10 );
			for( std::unordered_map<std::string, SExternalDebugAllocStat>::const_iterator i = mapSumm.begin();
					i != mapSumm.end(); ++i )
			{
				int nNewLine = ListView_AddItem( hwndListSumm, const_cast<char*>(i->first.c_str()), 0, 2000000 );
				char buff[100];

				itoa( i->second.uLineNumber, buff, 10 );
				ListView_SetItemText( hwndListSumm, nNewLine, 1, buff );

				itoa( i->second.uNumAllocs, buff, 10 );
				ListView_SetItemText( hwndListSumm, nNewLine, 2, buff );

				itoa( i->second.uNumFrees, buff, 10 );
				ListView_SetItemText( hwndListSumm, nNewLine, 3, buff );

				itoa( i->second.uAllocatedMemory, buff, 10 );
				ListView_SetItemText( hwndListSumm, nNewLine, 4, buff );

				itoa( i->second.uFreedMemory, buff, 10 );
				ListView_SetItemText( hwndListSumm, nNewLine, 5, buff );
			}
		}
		break;			
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDialogMemory::OnWndCmd( HWND hwndDlg, WORD wCtrlID, WORD wNotifCode, LPARAM lParam )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
int CDialogMemory::OnWndNotify( HWND hwndDlg, WORD wCtrlID, LPARAM lParam )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
*/