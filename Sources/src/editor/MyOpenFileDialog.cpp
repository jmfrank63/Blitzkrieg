#include "StdAfx.h"
#include "editor.h"
#include "MyOpenFileDialog.h"
#include "frames.h"

/////////////////////////////////////////////////////////////////////////////
// CMyOpenFileDialog

IMPLEMENT_DYNAMIC(CMyOpenFileDialog, CFileDialog)

BEGIN_MESSAGE_MAP(CMyOpenFileDialog, CFileDialog)
//{{AFX_MSG_MAP(CMyOpenFileDialog)
ON_WM_LBUTTONDBLCLK()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CMyOpenFileDialog::PreTranslateMessage( MSG* pMsg )
{
/*
	switch ( pMsg->message )
	{
		case WM_USERCHANGEPARAM:
			SetChangedFlag( true );
			return true;
	}
*/
	
	return CFileDialog::PreTranslateMessage(pMsg);
}

void CMyOpenFileDialog::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	// CFileDialog::OnLButtonDblClk(nFlags, point);
	AfxMessageBox( "Fuck off" );
	int i = 0;
}

//�������� ���������� ������ ��� FileDialog
typedef std::unordered_map<std::string, std::string> CExtensionToFile;
CExtensionToFile extensionToFileMap;

void SaveFileDialogRegisterData()
{
	theApp.MyWriteProfileInt( "FileDialog", "Size", extensionToFileMap.size() );
	int nIndex = 0;
	for ( CExtensionToFile::iterator it=extensionToFileMap.begin(); it!=extensionToFileMap.end(); ++it )
	{
		std::string szStrToWrite = it->first;
		szStrToWrite += ";";
		szStrToWrite += it->second;
		theApp.MyWriteProfileString( "FileDialog", NStr::Format("%03d", nIndex), szStrToWrite.c_str() );
		nIndex++;
	}
}

void LoadFileDialogRegisterData()
{
	extensionToFileMap.clear();
	int nSize = theApp.MyGetProfileInt( "FileDialog", "Size", extensionToFileMap.size() );
	for ( int i=0; i<nSize; i++ )
	{
		std::string szTemp = theApp.MyGetProfileString( "FileDialog", NStr::Format("%03d", i), "" );
		if ( szTemp.size() > 0 )
		{
			int nPos = szTemp.find( ';' );
			if ( nPos == std::string::npos )
				continue;
			
			std::string szExt = szTemp.substr( 0, nPos );
			std::string szVal = szTemp.substr( nPos+1 );
			if ( szExt.size() > 0 )
				extensionToFileMap[szExt] = szVal;
		}
	}
}

bool GetDirectoryFromExtensionTable( std::string &szRes, const char *pszExtension )
{
	const string szExt = pszExtension;
	CExtensionToFile::iterator findIt = extensionToFileMap.find( szExt );
	if ( findIt == extensionToFileMap.end() )
		return false;

	szRes = findIt->second;
	return true;
}

BOOL ShowFileDialog( std::string &szResult, LPCTSTR lpszInitDir, LPCTSTR lpszTitle, BOOL bOpen, LPCTSTR lpszDefExt,
										LPCTSTR lpszFileName, LPCTSTR lpszFilter )
{
	CMyOpenFileDialog dlg( bOpen, lpszDefExt, lpszFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, lpszFilter );
	//	CFileDialog dlg( bOpen, lpszDefExt, lpszFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, lpszFilter );
	
	std::string szExtension;
	if ( lpszDefExt )
		szExtension = lpszDefExt;
	else if ( lpszFilter )
	{
		//������� extension �� �������
		szExtension = lpszFilter;
		int nPos = szExtension.find( "(" );
		if ( nPos != std::string::npos )
		{
			szExtension = szExtension.substr( nPos+1 );
			nPos = szExtension.find( ")" );
			if ( nPos != std::string::npos )
				szExtension = szExtension.substr( 0, nPos );
			else
				szExtension = "";
		}
		else
			szExtension = "";
	}

	std::string szInitDir;
	if ( szExtension.size() > 0 && GetDirectoryFromExtensionTable( szInitDir, szExtension.c_str() ) )
	{
		//��������, ����� szInitDir ��� ����� �������� lpszInitDir
		int nPos = szInitDir.find( lpszInitDir );
		if ( nPos == std::string::npos )
			szInitDir = lpszInitDir;
	}
	if ( szInitDir.size() == 0 )
		szInitDir = lpszInitDir;

	dlg.m_ofn.lpstrInitialDir = szInitDir.c_str();
	dlg.m_ofn.lpstrTitle = lpszTitle;
	if ( dlg.DoModal() == IDOK )
	{
		szResult = dlg.GetPathName();
		NStr::ToLower( szResult );

		//������� ��������
		extensionToFileMap[szExtension] = GetDirectory( szResult.c_str() );
		return TRUE;
	}
	return FALSE;
}
