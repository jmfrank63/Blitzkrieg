#include "StdAfx.h"

#include "editor.h"
#include "MainFrm.h"
#include "frames.h"
#include "GameWnd.h"
#include "PropView.h"
#include "ParentFrame.h"
#include "ETreeCtrl.h"
#include "TreeItem.h"
#include "..\Scene\Scene.h"

CFrameManager g_frameManager;

bool MakeRelativePath( const char *pszSrcName, const char *pszDstName, string &szResult )
{
	NI_ASSERT( pszSrcName != 0 && pszDstName != 0 );
	NI_ASSERT( strlen(pszSrcName) >= 3 &&  strlen(pszDstName) >= 3 );
	szResult = "";

	string szTempSrc = pszSrcName;
	string szTempDst = pszDstName;
	NStr::ToLower( szTempSrc );
	NStr::ToLower( szTempDst );

	const char *pszs = szTempSrc.c_str();
	const char *pszd = szTempDst.c_str();

	if ( pszs[0] != pszd[0] || pszs[1] != pszd[1] || pszs[2] != pszd[2] )
		return false;

	ASSERT( pszs[2] == '\\' );
	pszs += 3;
	pszd += 3;

	while ( *pszs++ == *pszd++ )
		;
	pszs--;
	pszd--;

	while ( *(--pszd) != '\\' )
		;
	pszd++;

	while ( pszs = strchr( pszs, '\\' ) )
	{
		pszs++;
		szResult += "..\\";
	}
	szResult += pszd;
	return true;
}

bool MakeSubRelativePath( const char *pszSrcName, const char *pszDstName, string &szResult )
{
	if ( MakeRelativePath( pszSrcName, pszDstName, szResult ) )
	{
		if ( szResult.size() == 0 )
			return true;

		if ( szResult[0] == '.' )		//����� �� ������ src ����������
		{
			szResult = "";
			return false;
		}
		else
			return true;
	}
	else
	{
		szResult = "";
		return false;
	}
}

string GetDirectory( const char *pszFileName )
{
	string szResult = pszFileName;
	int nPos = szResult.rfind( '\\' );
	if ( nPos != string::npos )
		szResult = szResult.substr( 0, nPos + 1 );
	else
		szResult = "";
	return szResult;
}

bool IsRelatedPath( const char *pszFileName )
{
	NI_ASSERT( pszFileName != 0 );

	if ( pszFileName[1] == ':' )
		return false;
	else
		return true;
}

bool MakeFullPath( const char *pszFullDirName, const char *pszRelName, string &szResult )
{
	NI_ASSERT( pszFullDirName != 0 && pszRelName != 0 );
	NI_ASSERT( strlen(pszFullDirName) > 0 );
	NI_ASSERT( pszFullDirName[strlen(pszFullDirName)-1] == '\\' );

	if ( strlen( pszRelName ) == 0 )
	{
		szResult = pszFullDirName;
		return true;
	}

	if ( strlen( pszRelName ) >= 2 )
	{
		if ( pszRelName[1] == ':' )
			return false;
	}

	szResult = pszFullDirName;

	const char *pszf = pszFullDirName;
	const char *pszr = pszRelName;
	const char *pszEnd = strrchr( pszRelName, '\\' );
	if ( pszEnd == 0 )
	{
		szResult += pszRelName;
		return true;
	}

	szResult = szResult.substr( 0, szResult.rfind('\\') );
	pszEnd = pszr;
	while ( pszr = strstr( pszr, "..\\" ) )
	{
		int nPos = szResult.rfind( '\\' );
		ASSERT( nPos != std::string::npos );
		if ( nPos == std::string::npos )
			return false;

		szResult = szResult.substr( 0, nPos );
		pszr += 3;
		pszEnd = pszr;
	}

	szResult += '\\';
	szResult += pszEnd;
	return true;
}

void ShowFirstChildElementInPropertyView( CETreeCtrl *pTree, CPropView *pOIDockBar )
{
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem::CTreeItemList::const_iterator it = pRootItem->GetBegin();
	NI_ASSERT( it != pRootItem->GetEnd() );
	
	CTreeItem *pFirstChild = *it;
	pOIDockBar->SetItemProperty( pFirstChild->GetItemName(), pFirstChild );
}

FILETIME GetFileChangeTime( const char *pszFileName )
{
	FILETIME zero;
	zero.dwHighDateTime = 0;
	zero.dwLowDateTime = 0;
	HANDLE hFile = CreateFile( pszFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		DWORD dwErr = GetLastError();
		return zero;
	}
	BY_HANDLE_FILE_INFORMATION fileInfo;
	bool bRes = GetFileInformationByHandle( hFile, &fileInfo );
	CloseHandle( hFile );
	if ( !bRes )
		return zero;
	
	if ( fileInfo.ftCreationTime > fileInfo.ftLastWriteTime )
		return fileInfo.ftCreationTime;
	else
		return fileInfo.ftLastWriteTime;
}

FILETIME GetTextureFileChangeTime( const char *pszFileName )
{
	FILETIME minTime, current;
	minTime.dwHighDateTime = -1;
	minTime.dwLowDateTime = -1;
	std::string szTemp;

	szTemp = pszFileName;
	szTemp += "_h.dds";
	current = GetFileChangeTime( pszFileName );
	if ( current < minTime )
		minTime = current;

	szTemp = pszFileName;
	szTemp += "_l.dds";
	current = GetFileChangeTime( pszFileName );
	if ( current < minTime )
		minTime = current;

	szTemp = pszFileName;
	szTemp += "_c.dds";
	current = GetFileChangeTime( pszFileName );
	if ( current < minTime )
		minTime = current;

	return minTime;
}

bool operator > ( FILETIME a, FILETIME b )
{
	if ( a.dwHighDateTime > b.dwHighDateTime || (a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime > b.dwLowDateTime) )
		return true;
	else
		return false;
}

bool operator < ( FILETIME a, FILETIME b ) { return (b > a); }

bool operator == ( FILETIME a, FILETIME b ) { if ( a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime == b.dwLowDateTime ) return true; return false; }

BOOL MyCopyFile( const char *pszSrc, const char *pszDest )
{
	if ( !CopyFile( pszSrc, pszDest, FALSE ) )
		return false;
	HANDLE hFile = CreateFile( pszDest, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	NI_ASSERT_T( hFile != INVALID_HANDLE_VALUE, "Error setting file time: MyCopyFile() FAILED." );
	if ( hFile == INVALID_HANDLE_VALUE )
		return false;
	FILETIME writeTime;
	SYSTEMTIME st;
	GetSystemTime( &st );
	SystemTimeToFileTime( &st, &writeTime );
	SetFileTime( hFile, 0, 0, &writeTime );
	CloseHandle( hFile );
	return true;
}

void MyCopyDir( const string szSrc, const string szDest )
{
	vector<string> files;	
	NFile::EnumerateFiles( szSrc.c_str(), "*.*", NFile::CGetAllFilesRelative( szSrc.c_str(), &files ), true );
	for ( vector<string>::const_iterator it = files.begin(); it != files.end(); ++it )
	{
		if ( it == files.begin() )
			CPtr<IDataStream> pStream = CreateFileStream( szDest + (*it), STREAM_ACCESS_WRITE );
		CopyFile( (szSrc + (*it)).c_str(), (szDest + (*it)).c_str(), FALSE );
	}
}

void SetDefaultCamera()
{
	ICamera *pCamera = GetSingleton<ICamera>();
x	if ( pCamera == 0 )
		return;
	pCamera->SetPlacement( VNULL3, 700, -ToRadian(90.0f + 30.0f), ToRadian(45.0f) );
}

void SetHorizontalCamera()
{
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetPlacement( VNULL3, 700, -ToRadian(90.0f), ToRadian(45.0f) );
}

CFrameManager::CFrameManager()
{
	activeFrameType = E_UNKNOWN_FRAME;
	pActiveFrame = 0;
	pGameWnd = 0;
}

void CFrameManager::AddFrame( CParentFrame *pFrame )
{
	frames.push_back( pFrame );
}

void CFrameManager::SetActiveFrame( CParentFrame *pNewActiveFrame )
{
	if ( pActiveFrame == pNewActiveFrame )
		return;
	
	if ( !pActiveFrame )
	{
		for ( int i=0; i<frames.size(); i++ )
			frames[i]->ShowFrameWindows( SW_HIDE );
	}
	else
		pActiveFrame->ShowFrameWindows( SW_HIDE );

	pActiveFrame = pNewActiveFrame;
	pActiveFrame->ShowFrameWindows( SW_SHOW );

	if ( activeFrameType != pActiveFrame->GetFrameType() && theApp.IsInitFinished() )
	{
		theApp.SaveNewFrameTypeToRegister();			//����� �������� ���� � �������
	}
	activeFrameType = ( CFrameManager::EFrameType ) pActiveFrame->GetFrameType();
	
	NI_ASSERT( activeFrameType != E_UNKNOWN_FRAME );
}

CParentFrame *CFrameManager::GetFrame( int nID )
{
	for ( int i=0; i<frames.size(); i++ )
	{
		if ( frames[i]->GetFrameType() == nID )
			return frames[i];
	}
	if ( frames.size() > 0 )
		return frames[0];
	return 0;
}

CParentFrame *CFrameManager::ActivateFrameByExtension( const char *pszExtension )
{
	std::string szExtension = pszExtension;
	int nTemp = szExtension.rfind( '.' );
	if ( nTemp == std::string::npos )
		return 0;

	szExtension = szExtension.substr( nTemp + 1 );
	for ( int i=0; i<g_frameManager.frames.size(); i++ )
	{
		std::string szShortExt = g_frameManager.frames[i]->GetModuleExtension() + 2;
		if ( szExtension == szShortExt )
		{
			g_frameManager.SetActiveFrame( g_frameManager.frames[i] );
			g_frameManager.frames[i]->PostMessage( WM_SETFOCUS, 0, 0 );
			return pActiveFrame;
		}
	}

	return 0;
}
