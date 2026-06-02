#include "StdAfx.h"

#include "GameWnd.h"
#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "MiniMapBar.h"

CFrameManager g_frameManager;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__;
#endif

/**
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
/**/
/**
bool MakeSubRelativePath( const char *pszSrcName, const char *pszDstName, string &szResult )
{
	if ( MakeRelativePath( pszSrcName, pszDstName, szResult ) )
	{
		if ( szResult.empty() )
			return true;

		if ( szResult[0] == '.' )		//тогда не внутри src директории
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
/**/
/**
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
/**/
/**
bool MakeFullPath( const char *pszFullDirName, const char *pszRelName, string &szResult )
{
	NI_ASSERT( pszFullDirName != 0 && pszRelName != 0 );
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
/**/
/**
FILETIME GetFileChangeTime( const char *pszFileName )
{
	FILETIME zero;
	zero.dwHighDateTime = 0;
	zero.dwLowDateTime = 0;
	HANDLE hFile = CreateFile( pszFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( hFile == INVALID_HANDLE_VALUE )
		return zero;
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

bool operator > ( FILETIME a, FILETIME b )
{
	if ( a.dwHighDateTime > b.dwHighDateTime || (a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime > b.dwLowDateTime) )
		return true;
	else
		return false;
}

bool operator < ( FILETIME a, FILETIME b ) { return (b > a); }

bool operator == ( FILETIME a, FILETIME b ) { if ( a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime == b.dwLowDateTime ) return true; return false; }
/**/
/**
void SetDefaultCamera()
{
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetPlacement( VNULL3, 700, -ToRadian(90.0f + 30.0f), ToRadian(45.0f) );
}
/**/
/**
void SetHorizontalCamera()
{
	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetPlacement( VNULL3, 700, -ToRadian(90.0f), ToRadian(45.0f) );
}
/**/
CFrameManager::CFrameManager()
{
	activeFrameType = E_UNKNOWN_FRAME;
	pActiveFrame = 0;
	pGameWnd = 0;

	pTemplateEditorFrame = 0;
	pwndMiniMapDialog = 0;
}

void CFrameManager::SetActiveFrame( CWnd *pNewActiveFrame )
{
	if ( pActiveFrame == pNewActiveFrame )
		return;
	
	if ( !pActiveFrame )
	{
		if ( pTemplateEditorFrame )
			pTemplateEditorFrame->ShowFrameWindows( SW_HIDE );
	}
	else
	{
		if( pActiveFrame == pTemplateEditorFrame )
			pTemplateEditorFrame->ShowFrameWindows( SW_HIDE );
	}
	pActiveFrame = pNewActiveFrame;

	if ( pActiveFrame == pTemplateEditorFrame )
	{
		pTemplateEditorFrame->ShowFrameWindows( SW_SHOW );
		activeFrameType = E_TEMPLATE_FRAME;
	}

	else
	{
		activeFrameType = E_UNKNOWN_FRAME;
		ASSERT( 0 );
	}
}

CWnd *CFrameManager::GetFrameWindow( int nID )
{
	switch ( nID )
	{
		case E_TEMPLATE_FRAME:
			return pTemplateEditorFrame;
		default:
			return pTemplateEditorFrame;
	}
}
