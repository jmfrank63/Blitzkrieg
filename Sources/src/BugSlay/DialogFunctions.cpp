#include "StdAfx.h"
#include "DialogFunctions.h"
#include "WndUtils.h"

#include <io.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline char *CC( const std::string &str )
{
	return const_cast<char*>(str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillStackList( HWND hwndCallStack, const CCallStackEntryList &aStack )
{
	SendMessage( hwndCallStack, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT );
	RECT rc;
	GetClientRect( hwndCallStack, &rc );
	int nPart10 = (rc.right - rc.left) / 10;
	ListView_AddColumn( hwndCallStack, "File", nPart10 * 3 );	// 0
	ListView_AddColumn( hwndCallStack, "Method", nPart10 * 5 );	// 1
	ListView_AddColumn( hwndCallStack, "Line", nPart10 * 2 );	// 2
	ListView_AddColumn( hwndCallStack, "Module", nPart10 * 3 );	// 3
	ListView_AddColumn( hwndCallStack, "Param1", nPart10 * 2);	// 4
	ListView_AddColumn( hwndCallStack, "Param2", nPart10 * 2 );
	ListView_AddColumn( hwndCallStack, "Param3", nPart10 * 2 );
	ListView_AddColumn( hwndCallStack, "Param4", nPart10 * 2 );
	for ( CCallStackEntryList::const_iterator pos = aStack.begin(); pos != aStack.end(); ++pos )
	{
		char buf[100];
		int nNewLine = ListView_AddItem( hwndCallStack, GetFileName(pos->szFileName), LPARAM(&(*pos)), 2000000 );
		ListView_SetItemText( hwndCallStack, nNewLine, 1, CC(pos->szFunctionName) );
		_itoa_s( pos->nLineNumber, buf, sizeof(buf), 10 );
		ListView_SetItemText( hwndCallStack, nNewLine, 2, buf );
		ListView_SetItemText( hwndCallStack, nNewLine, 3, CC(pos->szModuleName) );
		buf[0] = '0';
		buf[1] = 'x';
		_itoa_s( pos->params[0], &buf[2], sizeof(buf) - 2, 16 );
		ListView_SetItemText( hwndCallStack, nNewLine, 4, buf );
		_itoa_s( pos->params[1], &buf[2], sizeof(buf) - 2, 16 );
		ListView_SetItemText( hwndCallStack, nNewLine, 5, buf );
		_itoa_s( pos->params[2], &buf[2], sizeof(buf) - 2, 16 );
		ListView_SetItemText( hwndCallStack, nNewLine, 6, buf );
		_itoa_s( pos->params[3], &buf[2], sizeof(buf) - 2, 16 );
		ListView_SetItemText( hwndCallStack, nNewLine, 7, buf );
	}			
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddListBoxItem( HWND hWnd, const char *pszString, const void *pItemData )
{
  int nItem = SendMessage( hWnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>( pszString ) );
  SendMessage( hWnd, LB_SETITEMDATA, nItem, reinterpret_cast<LPARAM>( pItemData ) );
}
void AddCallStackItem( HWND hWnd, const SCallStackEntry *pEntry )
{
	AddListBoxItem( hWnd, pEntry->szFunctionName.c_str(), pEntry );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const void* GetListBoxItem( HWND hWnd, int nItemIndex )
{
	long nVal = SendMessage( hWnd, LB_GETITEMDATA, nItemIndex, 0 );
	if ( nVal == LB_ERR )
		return 0;
	else
		return reinterpret_cast<const void*>( nVal );
}
const void* GetListBoxCurrentItem( HWND hWnd )
{
	int nItem = SendMessage( hWnd, LB_GETCURSEL, 0, 0 );
	if ( nItem == LB_ERR )
		return 0;
	else
		return GetListBoxItem( hWnd, nItem );
}
const SCallStackEntry* GetCallStackItem( HWND hWnd, int nItemIndex )
{
	return reinterpret_cast<const SCallStackEntry*>( GetListBoxItem(hWnd, nItemIndex) );
}
const SCallStackEntry* GetCurrentCallStackItem( HWND hWnd )
{
	return reinterpret_cast<const SCallStackEntry*>( GetListBoxCurrentItem(hWnd) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetWindowText( HWND hwndDlg, const int nElementID, const char *pszString )
{
	return SetWindowText( GetDlgItem(hwndDlg, nElementID), pszString ) != FALSE;
}
bool SetWindowText( HWND hwndDlg, const int nElementID, const int nValue )
{
	char buff[32];
	sprintf_s( buff, sizeof(buff), "%d", nValue );
	return SetWindowText( hwndDlg, nElementID, buff );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* SetWindowUserData( HWND hwndDlg, const int nElementID, void *pUserData )
{
	return reinterpret_cast<void*>( SetWindowLong( GetDlgItem(hwndDlg, nElementID), 
		                                             GWL_USERDATA, reinterpret_cast<LONG>(pUserData) ) );
}
void* GetWindowUserData( HWND hwndDlg, const int nElementID )
{
	return reinterpret_cast<void*>( GetWindowLong( GetDlgItem(hwndDlg, nElementID), GWL_USERDATA ) );
}
void* SetDlgUserData( HWND hwndDlg, void *pUserData )
{
	return reinterpret_cast<void*>( SetWindowLong( hwndDlg, DWL_USER, reinterpret_cast<LONG>(pUserData) ) );
}
void* GetDlgUserData( HWND hwndDlg )
{
	return reinterpret_cast<void*>( GetWindowLong( hwndDlg, DWL_USER ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetCheckButtonState( HWND hwndDlg, const int nElementID )
{
	return SendMessage( GetDlgItem(hwndDlg, nElementID), BM_GETCHECK, 0, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteReportToFile( const char *pszFileName, const char *pszCondition, const char *pszDescription, 
												const CCallStackEntryList &entries )
{
	std::string szFileName = ".\\";
	// create file name
	{
		char buffer[1024];
		::GetModuleFileName( 0, buffer, 1024 );
		szFileName = buffer;
		const int nPos = szFileName.rfind( '\\' );
		if ( nPos != std::string::npos ) 
			szFileName = szFileName.substr( 0, nPos + 1 );
		szFileName += pszFileName;
	}
	//
	if ( _access(szFileName.c_str(), 0) == 0 ) 
		return;
	//
	FILE *file = 0;
	if ( fopen_s(&file, szFileName.c_str(), "at") == 0 && file != 0 ) 
	{
		fprintf( file, "%s\n", pszCondition );
		fprintf( file, "%s\n\n", pszDescription );
		for ( CCallStackEntryList::const_iterator it = entries.begin(); it != entries.end(); ++it )
			fprintf( file, "%s(%d): %s\n", it->szFileName.c_str(), it->nLineNumber, it->szFunctionName.c_str() );
		fprintf( file, "\n\n" );
		//
		fflush( file );
		fclose( file );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
