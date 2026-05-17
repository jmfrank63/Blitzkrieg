#include "StdAfx.h"

#include "ELK_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\RandomMapGen\Registry_Types.h"
#include "..\StreamIO\StreamIO.h"
#include "..\Misc\FileUtils.h"
#include "ProgressDialog.h"
#include "ELK_TreeWindow.h"
#include "SpellChecker.h"
#include "..\Image\Image.h"

#include <afxdb.h> 
#include <odbcinst.h> 
#include "BlitzkriegELKDatabase.h"

#ifdef _DEBUG

#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SELKElement::GetDataBaseFolder( const std::string &rszELKPath, std::string *pszDataBaseFolder )
{
	NI_ASSERT_T( pszDataBaseFolder != 0, NStr::Format( _T( "CELK::GetDataBaseFolder() wrong parameter: pszDataBaseFolder %x" ), pszDataBaseFolder ) );
	if ( pszDataBaseFolder )
	{
		//( *pszDataBaseFolder ) = rszELKPath.substr( 0, rszELKPath.rfind( '.' ) );
		//( *pszDataBaseFolder ) +=	DATA_BASE_FOLDER;
		( *pszDataBaseFolder ) = rszELKPath + std::string( DATA_BASE_FOLDER );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SELKElement::GetDataBaseReserveFolder( const std::string &rszELKPath, std::string *pszDataBaseReserveFolder )
{
	NI_ASSERT_T( pszDataBaseReserveFolder != 0, NStr::Format( _T( "CELK::GetDataBaseReserveFolder() wrong parameter: pszDataBaseReserveFolder %x" ), pszDataBaseReserveFolder ) );
	if ( pszDataBaseReserveFolder )
	{
		//( *pszDataBaseReserveFolder ) = rszELKPath.substr( 0, rszELKPath.rfind( '.' ) );
		//( *pszDataBaseReserveFolder ) +=	DATA_BASE_RESERVE_FOLDER;
		( *pszDataBaseReserveFolder ) =	rszELKPath + std::string( DATA_BASE_RESERVE_FOLDER );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// = _T( "MICROSOFT EXCEL DRIVER (*.XLS)" );
bool GetExcelODBCDriverName( CString *pstrExcelODBCDriverName )
{
	if ( pstrExcelODBCDriverName )
	{
		pstrExcelODBCDriverName->Empty();
		TCHAR szBuf[2001];
		const WORD cbBufMax = 2000;
		WORD cbBufOut;
		LPTSTR pszBuf = szBuf;

		if ( SQLGetInstalledDrivers( szBuf, cbBufMax, &cbBufOut ) )
		{
			do
			{
				if( strstr( pszBuf, _T( "Excel" ) ) != 0 )
				{
					( *pstrExcelODBCDriverName ) = pszBuf;
					return true;
				}
				pszBuf = strchr( pszBuf, '\0' ) + 1;
			}
			while( pszBuf[1] != '\0' );
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::ToText( const std::vector<BYTE> &rBuffer, CString *pstrText, int nCodePage, bool bRemove_0D )
{
	NI_ASSERT_T( pstrText != 0, NStr::Format( _T( "CELK::ToText() wrong parameter: pstrText %x" ), pstrText ) );
	NI_ASSERT_T( ( rBuffer.size() > 3 ) && ( rBuffer[0] == 0xFF ) && ( rBuffer[1] == 0xFE ), NStr::Format( _T( "CELK::ToText() wrong parameter: rBuffer" ) ) );
	if ( ( pstrText != 0 ) && ( rBuffer.size() > 3 ) && ( rBuffer[0] == 0xFF ) && ( rBuffer[1] == 0xFE ) )
	{
		pstrText->Empty();
		std::wstring wszText;
		wszText.resize( ( rBuffer.size() - 2 ) / sizeof( wchar_t ) );
		memcpy( &( wszText[0] ), &( rBuffer[0] ) + 2, wszText.size() * sizeof( wchar_t ) );

		if ( bRemove_0D )
		{
			wszText.erase( std::remove_if( wszText.begin(), wszText.end(), std::bind2nd( std::equal_to<wchar_t>(), 0x0D ) ),
										 wszText.end() );
		}
		
		int nLastIndex = 0;
		for ( nLastIndex = ( wszText.size() - 1 ); nLastIndex >= 0; --nLastIndex )
		{
			if ( ( wszText[nLastIndex] != 0x0A ) && ( wszText[nLastIndex] != 0x0D ) )
			{
				break;
			}
		}
		if ( nLastIndex < 0 )
		{
			wszText.clear();
		}
		else if ( nLastIndex < ( wszText.size() - 1 ) )
		{
			wszText = wszText.substr( 0, nLastIndex + 1 );
		}

		const int nBufferLength = ::WideCharToMultiByte( nCodePage, 0, wszText.c_str(), wszText.length(), 0, 0, 0, 0 );
		LPTSTR lptStr = pstrText->GetBuffer( nBufferLength );
		::WideCharToMultiByte( nCodePage, 0, wszText.c_str(), wszText.length(), lptStr, nBufferLength, 0, 0 );
		pstrText->ReleaseBuffer();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::FromText( const CString &rstrText, std::vector<BYTE> *pBuffer, int nCodePage, bool bAdd_0D )
{
	NI_ASSERT_T( pBuffer != 0, NStr::Format( _T( "CELK::ToText() wrong parameter: pBuffer %x" ), pBuffer ) );
	std::wstring wszText;
	
	pBuffer->clear();

	const int nBufferLength = ::MultiByteToWideChar( nCodePage, 0, rstrText, -1, 0, 0 );
	wszText.resize( nBufferLength );
	::MultiByteToWideChar( nCodePage, 0, rstrText, -1, &( wszText[0] ), wszText.size() );

	if ( bAdd_0D )
	{
		for ( int nIndex = 0; nIndex < wszText.size(); ++nIndex )
		{
			if ( wszText[nIndex] == 0x0A )
			{
				if ( ( nIndex == 0 ) || ( wszText[nIndex - 1] != 0x0D ) )
				{
					wszText.insert( wszText.begin() + nIndex, 0x0D );
				}
			}
		}
	}

	int nLastIndex = 0;
	for ( nLastIndex = ( wszText.size() - 1 ); nLastIndex >= 0; --nLastIndex )
	{
		if ( ( wszText[nLastIndex] != 0x0A ) && ( wszText[nLastIndex] != 0x0D ) )
		{
			break;
		}
	}
	if ( nLastIndex < 0 )
	{
		wszText.clear();
	}
	else if ( nLastIndex < ( wszText.size() - 1 ) )
	{
		wszText = wszText.substr( 0, nLastIndex + 1 );
	}
	
	//����� 0 �� �����
	pBuffer->resize( 2 + ( wszText.size() - 1 ) * sizeof( wchar_t ) );
	( *pBuffer )[0] = 0xFF;
	( *pBuffer )[1] = 0xFE;
	memcpy( &( ( *pBuffer )[2] ), &( wszText[0] ), ( wszText.size() - 1 ) * sizeof( wchar_t ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::GetOriginalText( const std::string &rszTextPath, CString *pstrText, int nCodePage, bool bRemove_0D )
{
	NI_ASSERT_T( pstrText != 0, NStr::Format( _T( "CELK::GetOriginalText() wrong parameter: pstrText %x" ), pstrText ) );
	if ( pstrText )
	{
		pstrText->Empty();
		CPtr<IDataStream> pFileStream = 0;
		if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), ELK_EXTENTION ), STREAM_ACCESS_READ ) )
		{
			std::vector<BYTE> fileBuffer;
			fileBuffer.resize( pFileStream->GetSize() );
			pFileStream->Read( &( fileBuffer[0] ), fileBuffer.size() );

			ToText( fileBuffer, pstrText, nCodePage, bRemove_0D );
		}
	}	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::GetTranslatedText( const std::string &rszTextPath, CString *pstrText,  int nCodePage, bool bRemove_0D )
{
	NI_ASSERT_T( pstrText != 0, NStr::Format( _T( "CELK::GetTranslatedText() wrong parameter: pstrText %x" ), pstrText ) );
	if ( pstrText )
	{
		pstrText->Empty();
		CPtr<IDataStream> pFileStream = 0;
		if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), TXT_EXTENTION ), STREAM_ACCESS_READ ) )
		{
			std::vector<BYTE> fileBuffer;
			fileBuffer.resize( pFileStream->GetSize() );
			pFileStream->Read( &( fileBuffer[0] ), fileBuffer.size() );

			ToText( fileBuffer, pstrText, nCodePage, bRemove_0D );
		}
	}	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::GetDescription( const std::string &rszTextPath, CString *pstrText, int nCodePage, bool bRemove_0D )
{
	NI_ASSERT_T( pstrText != 0, NStr::Format( _T( "CELK::GetDescription() wrong parameter: pstrText %x" ), pstrText ) );
	if ( pstrText )
	{
		pstrText->Empty();
		std::string szFileName = NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), DSC_EXTENTION );
		std::string szFileFolder = szFileName.substr( 0, szFileName.rfind( '\\' ) );
		while ( !NFile::IsFileExist( szFileName.c_str() ) )
		{
			int nPosition = szFileFolder.rfind( '\\' );
			if ( nPosition == std::string::npos )
			{
				szFileName.clear();
				break;
			}
			szFileFolder = szFileFolder.substr( 0, nPosition );
			szFileName = szFileFolder + std::string( _T( "\\" ) ) + std::string( FOLDER_DESC_FILE_NAME ) + DSC_EXTENTION;
		}
		if ( !szFileName.empty() )
		{
			CPtr<IDataStream> pFileStream = 0;
			if ( pFileStream = CreateFileStream( szFileName.c_str(), STREAM_ACCESS_READ ) )
			{
				std::vector<BYTE> fileBuffer;
				fileBuffer.resize( pFileStream->GetSize() );
				pFileStream->Read( &( fileBuffer[0] ), fileBuffer.size() );

				ToText( fileBuffer, pstrText, nCodePage, bRemove_0D );
			}
		}
	}	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CELK::GetState( const std::string &rszTextPath, bool *pbTranslated )
{
	SELKTextProperty textProperty;
	
	CPtr<IDataStream> pFileStream = 0;
	if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), XML_EXTENTION ), STREAM_ACCESS_READ ) )
	{
		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::READ );
		CTreeAccessor saver = pSaver;
		saver.AddTypedSuper( &textProperty );
	}
	
	if ( pbTranslated )
	{
		( *pbTranslated ) = textProperty.bTranslated;
	}
	return textProperty.nState;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::SetTranslatedText( const std::string &rszTextPath, const CString &rstrText, int nCodePage, bool bAdd_0D )
{
	CPtr<IDataStream> pFileStream = 0;
	if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), TXT_EXTENTION ), STREAM_ACCESS_WRITE ) )
	{
		std::vector<BYTE> fileBuffer;
		FromText( rstrText, &fileBuffer, nCodePage, bAdd_0D );
		pFileStream->Write( &( fileBuffer[0] ), fileBuffer.size() );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CELK::SetState( const std::string &rszTextPath, int nState, bool *pbTranslated )
{
	SELKTextProperty textProperty;

	CPtr<IDataStream> pFileStream = 0;
	if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), XML_EXTENTION ), STREAM_ACCESS_READ ) )
	{
		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::READ );
		CTreeAccessor saver = pSaver;
		saver.AddTypedSuper( &textProperty );
	}
	
	int nPreviousState = textProperty.nState;
	textProperty.nState = nState;
	if ( nState != SELKTextProperty::STATE_NOT_TRANSLATED )
	{
		textProperty.bTranslated = true;
	}
	
	if ( nPreviousState != nState )
	{
		pFileStream = 0;
		if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), rszTextPath.c_str(), XML_EXTENTION ), STREAM_ACCESS_WRITE ) )
		{
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &textProperty );
		}
	}
	if ( pbTranslated )
	{
		( *pbTranslated ) = textProperty.bTranslated;
	}
	
	return nPreviousState;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::CreatePAK( const std::string &rszGamePath, const std::string &rszFilePath, const std::string &rszZIPToolPath, CProgressDialog* pwndProgressDialog )
{
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( NStr::Format( _T ( "Create PAK file: %s" ), rszFilePath.c_str() ) );
	}

	const std::string szTempPath = NStr::Format( _T( "%s%s" ), rszFilePath.c_str(), TEMP_FOLDER );
	{
		NFile::DeleteDirectory( szTempPath.c_str() );
	}

	if ( CPtr<IDataStorage> pStorage = OpenStorage( NStr::Format( _T( "%s*.pak" ), rszGamePath.c_str() ), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
	{
		std::vector<SEnumFilesInDataStorageParameter> enumFilesInDataStorageParameter;
	
		if ( enumFilesInDataStorageParameter.empty() )
		{
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = TXT_EXTENTION;

			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = DSC_EXTENTION;
			
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = PAK_DESCRIPTION_EXTENTION;

			EnumFilesInDataStorage( &enumFilesInDataStorageParameter, pStorage );
		}
		
		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0,
																						enumFilesInDataStorageParameter[0].fileNames.size() + 
																						enumFilesInDataStorageParameter[1].fileNames.size() +
																						enumFilesInDataStorageParameter[2].fileNames.size() );
			pwndProgressDialog->SetProgressPosition( 0 );
		}
		
		for ( int nParameterIndex = 0; nParameterIndex < enumFilesInDataStorageParameter.size(); ++nParameterIndex )
		{
			for ( std::list<std::string>::const_iterator nameIterator = enumFilesInDataStorageParameter[nParameterIndex].fileNames.begin(); nameIterator != enumFilesInDataStorageParameter[nParameterIndex].fileNames.end(); ++nameIterator )
			{
				if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
				{
					pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Copying: %s..." ), nameIterator->c_str() ) );
				}
				if ( CPtr<IDataStream> pStream = pStorage->OpenStream( nameIterator->c_str(), STREAM_ACCESS_READ ) )
				{
					if ( CPtr<IDataStream> pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), szTempPath.c_str(), nameIterator->c_str() ), STREAM_ACCESS_WRITE ) )
					{
						pStream->CopyTo( pFileStream, pStream->GetSize() );
					}
				}
				if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
				{
					pwndProgressDialog->IterateProgressPosition();
				}
			}
		}
	}
	
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{		
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Creating PAK: %s..." ), rszFilePath.c_str() ) );
	}
	
	ExecuteProcess( rszZIPToolPath, NStr::Format( _T( " -9 -R -D \"%s\" *.*" ), rszFilePath.c_str() ), szTempPath.c_str() );
	
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{		
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Finalizing..." ) ) );
	}
	
	{
		NFile::DeleteDirectory( szTempPath.c_str() );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::ExportToPAK( const std::string &rszELKPath,
												const std::string &rszPAKPath,
												const std::string &rszZIPToolPath,
												class CELKTreeWindow *pwndELKTreeWindow,
												bool bOnlyFilled,
												bool bGenerateFonts,
												const CString &rstrFontName,
												DWORD dwNormalFontSize,
												DWORD dwLargeFontSize,
												int nCodePage,
												class CProgressDialog* pwndProgressDialog,
												const struct SSimpleFilter *pELKFilter )
{
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( NStr::Format( _T( "Create PAK file: %s" ), rszPAKPath.c_str() ) );
	}

	::DeleteFile( rszPAKPath.c_str() );
	std::string szTempPath = NStr::Format( _T( "%s%s" ), rszPAKPath.c_str(), TEMP_FOLDER );
	{
		NFile::DeleteDirectory( szTempPath.c_str() );
	}

	std::string szDataBaseFolder;
	SELKElement::GetDataBaseFolder( rszELKPath, &szDataBaseFolder );
	
	if ( CPtr<IDataStorage> pStorage = OpenStorage( NStr::Format( _T( "%s*.pak" ), szDataBaseFolder.c_str() ), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
	{
		std::vector<SEnumFilesInDataStorageParameter> enumFilesInDataStorageParameter;
	
		if ( enumFilesInDataStorageParameter.empty() )
		{
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = ELK_EXTENTION;

			EnumFilesInDataStorage( &enumFilesInDataStorageParameter, pStorage );
		}
		
		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0, enumFilesInDataStorageParameter[0].fileNames.size() );
			pwndProgressDialog->SetProgressPosition( 0 );
		}
		
		std::set<WORD> symbols;
		bool bSingleByte = true;
		for ( std::list<std::string>::const_iterator nameIterator = enumFilesInDataStorageParameter[0].fileNames.begin(); nameIterator != enumFilesInDataStorageParameter[0].fileNames.end(); ++nameIterator )
		{
			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Copying: %s..." ), nameIterator->c_str() ) );
			}
			const std::string szFileName = nameIterator->substr( 0, nameIterator->rfind( '.' ) );
			const std::string szTempFileName = NStr::Format( _T( "%s%s" ), szTempPath.c_str(), szFileName.c_str() );
			
			SELKTextProperty textProperty;
			CPtr<IDataStream> pFileStream = 0;
			if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s%s" ), szDataBaseFolder.c_str(), szFileName.c_str(), XML_EXTENTION ), STREAM_ACCESS_READ ) )
			{
				CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::READ );
				CTreeAccessor saver = pSaver;
				saver.AddTypedSuper( &textProperty );
			}
			
			pFileStream = 0;
			bool bTranslationIsValid = true;
			if ( pELKFilter )
			{
				bTranslationIsValid = pELKFilter->Check( szFileName, textProperty.bTranslated, textProperty.nState );
			}
			if ( bTranslationIsValid )
			{
				bTranslationIsValid = false;
				if ( pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), szTempFileName.c_str(), TXT_EXTENTION ), STREAM_ACCESS_WRITE ) )
				{
					if ( ( textProperty.nState != SELKTextProperty::STATE_NOT_TRANSLATED ) )// || textProperty.bTranslated )
					{
						if ( CPtr<IDataStream> pStream = pStorage->OpenStream( NStr::Format( _T( "%s%s" ), szFileName.c_str(), TXT_EXTENTION ), STREAM_ACCESS_READ ) )
						{
							if ( !bOnlyFilled || ( pStream->GetSize() > 2 ) )
							{
								pStream->CopyTo( pFileStream, pStream->GetSize() );
								bTranslationIsValid = true;
							}
						}
					}
					if ( !bTranslationIsValid )
					{
						if ( CPtr<IDataStream> pStream = pStorage->OpenStream( NStr::Format( _T( "%s%s" ), szFileName.c_str(), ELK_EXTENTION ), STREAM_ACCESS_READ ) )
						{
							pStream->CopyTo( pFileStream, pStream->GetSize() );
						}
					}
				}
			}
			pFileStream = 0;
			
			//��������� ����� ��������
			CString strSymbols;
			GetTranslatedText( szTempFileName, &strSymbols, nCodePage, true );
			if ( !strSymbols.IsEmpty() )
			{
				LPTSTR pSymbols = (LPTSTR)(LPCTSTR)( strSymbols );
				const int nSymbolsCount = _mbstrlen( pSymbols );
				for ( int nSymbolIndex = 0; nSymbolIndex < nSymbolsCount; ++nSymbolIndex )
				{
					LPCTSTR pSymbol = pSymbols;
					pSymbols = _tcsinc( pSymbols );
					WORD wSymbol = 0;
					if ( ( pSymbols - pSymbol ) > 1 )
					{
						const TCHAR b0 = ( *pSymbol );
						const TCHAR b1 = ( *( pSymbol + 1 ) );

						wSymbol = ( ( b0 << 8 ) & 0xFF00 ) + ( b1 & 0xFF );
						bSingleByte = false;
					}
					else
					{
						const TCHAR b0 = ( *pSymbol );
						wSymbol = b0 & 0xFF;
					}
					symbols.insert( wSymbol );
				}
			}

			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->IterateProgressPosition();
			}
		}

		if ( bGenerateFonts )
		{
			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Creating Font Images..." ) ) );
			}

			CFontGen::GenerateFont( szTempPath,
															symbols,
															bSingleByte,
															rstrFontName,
															dwNormalFontSize,
															dwLargeFontSize,
															nCodePage );
		}
	}
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{		
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Creating PAK: %s..." ), rszPAKPath.c_str() ) );
	}
	
	ExecuteProcess( rszZIPToolPath, NStr::Format( _T( " -9 -R -D \"%s\" *.*" ), rszPAKPath.c_str() ), szTempPath.c_str() );
	
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{		
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Finalizing..." ) ) );
	}
	
	{
		NFile::DeleteDirectory( szTempPath.c_str() );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CImportFromPAKEraseFile
{
	std::unordered_set<std::string> *pUsedPaths;

public:
	CImportFromPAKEraseFile( std::unordered_set<std::string> *_pUsedPaths ) : pUsedPaths( _pUsedPaths )
	{
		NI_ASSERT_T( pUsedPaths != 0,
								 NStr::Format( _T( "CImportFromPAKEraseFile, invalid parameters: %x" ), pUsedPaths ) );
	}

	void operator()( const NFile::CFileIterator &fileIterator )
	{
		if ( !fileIterator.IsDirectory() && !fileIterator.IsDots() )
		{
			std::string szBaseFilePath = fileIterator.GetFilePath();
			NStr::ToLower( szBaseFilePath );
			szBaseFilePath = szBaseFilePath.substr ( 0, szBaseFilePath.rfind( '.' ) );
			if ( pUsedPaths->find( szBaseFilePath ) == pUsedPaths->end() )
			{
				::DeleteFile( fileIterator.GetFilePath().c_str() );
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::ImportFromPAK( const std::string &rszPAKPath, const std::string &rszELKPath, bool bAbsolute, std::string *pszNewVersion, CProgressDialog* pwndProgressDialog )
{
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( NStr::Format( _T( "New ELK Update found: %s" ), rszPAKPath.c_str() )  );
	}

	std::string szDataBaseFolder;
	SELKElement::GetDataBaseFolder( rszELKPath, &szDataBaseFolder );

	SStorageElementStats statsPAK;
	{
		Zero( statsPAK );
		if ( CPtr<IDataStream> pPAKStream = CreateFileStream( rszPAKPath.c_str(), STREAM_ACCESS_READ ) )
		{
			pPAKStream->GetStats( &statsPAK );
		}
	}	

	if ( CPtr<IDataStorage> pStorage = OpenStorage( rszPAKPath.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_ZIP ) )
	{
		std::vector<SEnumFilesInDataStorageParameter> enumFilesInDataStorageParameter;
		if ( enumFilesInDataStorageParameter.empty() )
		{
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = TXT_EXTENTION;

			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = DSC_EXTENTION;
			
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			enumFilesInDataStorageParameter.back().szExtention = PAK_DESCRIPTION_EXTENTION;

			EnumFilesInDataStorage( &enumFilesInDataStorageParameter, pStorage );
		}
		
		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0,
																						enumFilesInDataStorageParameter[0].fileNames.size() + 
																						enumFilesInDataStorageParameter[1].fileNames.size() +
																						enumFilesInDataStorageParameter[2].fileNames.size() );
			pwndProgressDialog->SetProgressPosition( 0 );
			pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Updating ELK Database..." ) ) );
		}

		std::unordered_set<std::string> usedPaths;

		//��������� TXT � ELK
		for ( std::list<std::string>::const_iterator nameIterator = enumFilesInDataStorageParameter[0].fileNames.begin(); nameIterator != enumFilesInDataStorageParameter[0].fileNames.end(); ++nameIterator )
		{
			if ( CPtr<IDataStream> pStream = pStorage->OpenStream( nameIterator->c_str(), STREAM_ACCESS_READ ) )
			{
				const std::string szFileName = nameIterator->substr( 0, nameIterator->rfind( '.' ) );
				std::string szBaseFilePath = szDataBaseFolder + szFileName;
				NStr::ToLower( szBaseFilePath );
				usedPaths.insert( szBaseFilePath );

				std::vector<BYTE> buffer0;
				if ( pStream->GetSize() > 0 )
				{
					buffer0.resize( pStream->GetSize() );
					pStream->Read( &( buffer0[0] ), buffer0.size() );
					pStream->Seek( 0, STREAM_SEEK_SET );
				}

				SELKTextProperty textProperty;
				CPtr<IDataStream> pFileStream = 0;
				if ( pFileStream = CreateFileStream( ( szBaseFilePath + XML_EXTENTION ).c_str(), STREAM_ACCESS_READ ) )
				{
					CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::READ );
					CTreeAccessor saver = pSaver;
					saver.AddTypedSuper( &textProperty );
				}
				if ( ( textProperty.nState < 0 ) || ( textProperty.nState >= SELKTextProperty::STATE_COUNT ) )
				{
					textProperty.nState = SELKTextProperty::STATE_NOT_TRANSLATED;
				}

				std::vector<BYTE> buffer1;
				bool bFileExists = false;
				if ( pFileStream = CreateFileStream( ( szBaseFilePath + ELK_EXTENTION ).c_str(), STREAM_ACCESS_READ ) )
				{
					if ( pFileStream->GetSize() > 0 )
					{
						buffer1.resize( pFileStream->GetSize() );
						pFileStream->Read( &( buffer1[0] ), buffer1.size() );
					}
					bFileExists = true;
				}
				if ( !bFileExists || ( buffer1.size() != buffer0.size() ) || ( memcmp( &( buffer1[0] ), &( buffer0[0] ), buffer1.size() > buffer0.size() ? buffer0.size() : buffer1.size() ) != 0 ) )
				{
					if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
					{
						pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Copying: %s..." ), nameIterator->c_str() ) );
					}

					pFileStream = 0;
					if ( pFileStream = CreateFileStream( ( szBaseFilePath + ELK_EXTENTION ).c_str(), STREAM_ACCESS_WRITE ) )
					{
						pStream->CopyTo( pFileStream, pStream->GetSize() );
					}
					if ( bFileExists && ( textProperty.nState > SELKTextProperty::STATE_OUTDATED ) )
					{
						textProperty.nState = SELKTextProperty::STATE_OUTDATED;
					}
					else
					{
						textProperty.nState = SELKTextProperty::STATE_NOT_TRANSLATED;
					}
					
					pFileStream = 0;
					if ( pFileStream = CreateFileStream( ( szBaseFilePath + XML_EXTENTION ).c_str(), STREAM_ACCESS_WRITE ) )
					{
						CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::WRITE );
						CTreeAccessor saver = pSaver;
						saver.AddTypedSuper( &textProperty );
					}
				}
			}
			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->IterateProgressPosition();
			}
		}

		//��������� ����������
		for ( int nParameterIndex = 1; nParameterIndex < enumFilesInDataStorageParameter.size(); ++nParameterIndex )
		{
			for ( std::list<std::string>::const_iterator nameIterator = enumFilesInDataStorageParameter[nParameterIndex].fileNames.begin(); nameIterator != enumFilesInDataStorageParameter[nParameterIndex].fileNames.end(); ++nameIterator )
			{
				if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
				{
					pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Copying: %s..." ), nameIterator->c_str() ) );
				}
				if ( CPtr<IDataStream> pStream = pStorage->OpenStream( nameIterator->c_str(), STREAM_ACCESS_READ ) )
				{
					if ( CPtr<IDataStream> pFileStream = CreateFileStream( NStr::Format( _T( "%s%s" ), szDataBaseFolder.c_str(), nameIterator->c_str() ), STREAM_ACCESS_WRITE ) )
					{
						pStream->CopyTo( pFileStream, pStream->GetSize() );
					}
				}
				if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
				{
					pwndProgressDialog->IterateProgressPosition();
				}
			}
		}

		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Finalising..." ) ) );
		}

		if ( bAbsolute )
		{
			NFile::EnumerateFiles( szDataBaseFolder.c_str(), NStr::Format( _T( "*%s" ), ELK_EXTENTION ), CImportFromPAKEraseFile( &usedPaths ), true );
		}

		if ( pszNewVersion )
		{
			const std::string szPAKName = rszPAKPath.substr( rszELKPath.rfind( '\\' ) + 1 );
			( *pszNewVersion ) = NStr::Format( _T( "%s, [%02d:%02d:%04d, %02d.%02d.%02d]" ),
																					szPAKName.c_str(),
																					statsPAK.mtime.day,
																					statsPAK.mtime.month,
																					statsPAK.mtime.year + 1980,
																					statsPAK.mtime.hours,
																					statsPAK.mtime.minutes,
																					statsPAK.mtime.seconds * 2 );
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::ExportToXLS( const CELK &rELK, const std::string &rszXLSPath, CELKTreeWindow *pwndELKTreeWindow, int nCodePage, CProgressDialog* pwndProgressDialog )
{
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( NStr::Format( _T( "Export to XLS file: %s" ), rszXLSPath.c_str() ) );
	}

	::DeleteFile( rszXLSPath.c_str() );

	if ( pwndELKTreeWindow )
	{
		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0, pwndELKTreeWindow->GetItemsCountInternal() );
			pwndProgressDialog->SetProgressPosition( 0 );
		}
		
		CDatabase database;
		CString strDriver;
		GetExcelODBCDriverName( &strDriver );
		CString strSql;
    
		TRY
		{
			strSql.Format( _T( "DRIVER={%s};DSN='';FIRSTROWHASNAMES=1;READONLY=FALSE;CREATE_DB=\"%s\";DBQ=%s" ), LPCTSTR( strDriver ), rszXLSPath.c_str(), rszXLSPath.c_str() );

			if ( database.OpenEx( strSql, CDatabase::noOdbcDialog ) )
			{
				strSql = _T( "CREATE TABLE BlitzkriegELK ([Path] TEXT,[Original] LONGTEXT,[Translation] LONGTEXT,[State] TEXT, [Description] LONGTEXT)" );
				database.ExecuteSQL( strSql );
				HTREEITEM item = pwndELKTreeWindow->GetFirstItemInternal(); 
				while ( item )
				{
					std::string szFileRelPath;
					std::string szFilePath;
					pwndELKTreeWindow->GetXLSPathInternal( item, &szFileRelPath );
					pwndELKTreeWindow->GetItemPathInternal( item, &szFilePath, true );

					if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
					{
						pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Exporting: %s..." ), szFileRelPath.c_str() ) );
					}
					
					CString strOriginalText;
					GetOriginalText( szFilePath, &strOriginalText, nCodePage, true );
					strOriginalText.Replace( '\'', '`');
					strOriginalText.TrimRight( "\t\r\n " );

					if ( strOriginalText.IsEmpty() )
					{
						NStr::DebugTrace( _T( "EMPTY ORIGINAL! %s\n" ), szFileRelPath );
					}

					CString strDescription;
					GetDescription( szFilePath, &strDescription, nCodePage, true );
					strDescription.Replace( '\'', '`');
					strDescription.TrimRight( "\t\r\n " );
					
					CString strTranslatedText;
					GetTranslatedText( szFilePath, &strTranslatedText, nCodePage, true );
					strTranslatedText.Replace( '\'', '`');
					strTranslatedText.TrimRight( "\t\r\n " );

					bool bTranslated = false;
					int nState = GetState( szFilePath, &bTranslated );
					
					strSql.Format( _T( "INSERT INTO BlitzkriegELK ([Path], [Original], [Translation], [State], [Description]) VALUES ('%s','%s','%s','%s','%s')" ),
												 szFileRelPath.c_str(),
												 LPCTSTR( strOriginalText ),
												 LPCTSTR( strTranslatedText ),
												 SELKTextProperty::STATE_NAMES[nState],
												 LPCTSTR( strDescription ) );
					database.ExecuteSQL(strSql);
					if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
					{
						pwndProgressDialog->IterateProgressPosition();
					}
					item = pwndELKTreeWindow->GetNextItemInternal( item );
				}
			}
		}
		CATCH(CDBException , pEx)
		{
			pEx->ReportError();
		}
		AND_CATCH(CMemoryException, pEx)
		{
			pEx->ReportError();
		}
		END_CATCH
		database.Close();
	}
	return true;	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::ImportFromXLS( const CELK &rELK, const std::string &rszXLSPath, std::string *pszNewVersion, int nCodePage, CProgressDialog* pwndProgressDialog )
{
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( NStr::Format( _T( "Import from XLS file: %s" ), rszXLSPath.c_str() ) );
	}

	SStorageElementStats statsXLS;
	{
		Zero( statsXLS );
		if ( CPtr<IDataStream> pXLSStream = CreateFileStream( rszXLSPath.c_str(), STREAM_ACCESS_READ ) )
		{
			pXLSStream->GetStats( &statsXLS );
		}
	}	
		
	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->SetProgressRange( 0, 1 );
		pwndProgressDialog->SetProgressPosition( 0 );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Updating ELK Database..." ) ) );
	}

  CDatabase database;
	CString strDriver;
	GetExcelODBCDriverName( &strDriver );
	//CString strSql;
	CString strDsn;

	TRY
	{
		strDsn.Format( _T( "ODBC;DRIVER={%s};DSN='';DBQ=%s;MAXSCANROWS=0" ), LPCTSTR( strDriver ), rszXLSPath.c_str() );
		database.Open( NULL, false, false, strDsn );
		CBlitzkriegELKRecordset recset( &database );
		//recset.m_strSort = "Path";
		recset.Open( CRecordset::forwardOnly, 0, CRecordset::readOnly );

		int nOverallStatesCount = 0;
		while( !recset.IsEOF() )
		{
			++nOverallStatesCount;
			recset.MoveNext();
		}

		recset.Close();
		recset.Open( CRecordset::forwardOnly, 0, CRecordset::readOnly );

		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0, nOverallStatesCount );
			pwndProgressDialog->SetProgressPosition( 0 );
		}

		int nElementIndex = 0;
		while( !recset.IsEOF() )
		{
			CString strFileName = recset.m_Path;
			//recset.GetFieldValue( _T( "Path" ), strFileName );
			strFileName.Replace( '`', '\'' );
			std::string szText = strFileName;

			CString strOriginalText = recset.m_Original;
			//recset.GetFieldValue( _T( "Original" ), strOriginalText );
			strOriginalText.Replace( '`', '\'' );
			strOriginalText.TrimRight( "\t\r\n " );

			szText = strOriginalText;

			if ( strOriginalText.IsEmpty() )
			{
				NStr::DebugTrace( _T( "EMPTY ORIGINAL! %s\n" ), strFileName );
			}
			
			CString strTranslatedText = recset.m_Translation;
			//recset.GetFieldValue( _T( "Translation" ), strTranslatedText );
			strTranslatedText.Replace( '`', '\'' );
			szText = LPCTSTR( strTranslatedText );

			if ( !strTranslatedText.IsEmpty() )
			{
				std::string szELKElementName = strFileName;
				szELKElementName = szELKElementName.substr( 0, szELKElementName.find( '\\' ) );
				std::unordered_map<std::string, int>::const_iterator elkElementNameIterator = rELK.elementNames.find( szELKElementName );
				if ( elkElementNameIterator != rELK.elementNames.end() )
				{
					nElementIndex = elkElementNameIterator->second; 
					if ( nElementIndex >= 0 && nElementIndex < rELK.elements.size() )
					{
						std::string szELKElementPath = strFileName;
						szELKElementPath = szELKElementPath.substr( szELKElementPath.find( '\\' ) + 1 );

						std::string szDataBaseFolder;
						rELK.elements[nElementIndex].GetDataBaseFolder( &szDataBaseFolder );
						std::string szBaseFilePath = szDataBaseFolder + szELKElementPath;

						CString strOriginalTextOnDisk;
						GetOriginalText( szBaseFilePath, &strOriginalTextOnDisk, nCodePage, true );
						strOriginalTextOnDisk.TrimRight( "\t\r\n " );

						bool bOrifginal = false;
						int nState = SELKTextProperty::STATE_TRANSLATED;
						CPtr<IDataStream> pFileStream = 0;
						std::vector<BYTE> buffer0;
						std::vector<BYTE> buffer1;

						if ( strOriginalTextOnDisk != strOriginalText )
						{
							nState = SELKTextProperty::STATE_OUTDATED;
							bOrifginal = true;
						}

						/**
						buffer0.clear();
						if ( !strOriginalText.IsEmpty() )
						{
							FromText( strOriginalText, &buffer0, nCodePage, true );
						}
						
						pFileStream = 0;
						buffer1.clear();
						if ( pFileStream = CreateFileStream( ( szBaseFilePath + ELK_EXTENTION ).c_str(), STREAM_ACCESS_READ ) )
						{
							if ( pFileStream->GetSize() > 0 )
							{
								buffer1.resize( pFileStream->GetSize() );
								pFileStream->Read( &( buffer1[0] ), pFileStream->GetSize() );
							}
							if ( ( buffer0.size() != buffer1.size() ) ||
									 ( memcmp( &( buffer0[0] ), &( buffer1[0] ), buffer0.size() > buffer1.size() ? buffer1.size() : buffer0.size() ) != 0 ) )
							{
								nState = SELKTextProperty::STATE_OUTDATED;
							}
						}
						/**/

						bool bTranslation = false;
				
						buffer0.clear();
						FromText( strTranslatedText, &buffer0, nCodePage, true );
						pFileStream = 0;
						buffer1.clear();
						if ( pFileStream = CreateFileStream( ( szBaseFilePath + TXT_EXTENTION ).c_str(), STREAM_ACCESS_READ ) )
						{
							if ( pFileStream->GetSize() > 0 )
							{
								buffer1.resize( pFileStream->GetSize() );
								pFileStream->Read( &( buffer1[0] ), pFileStream->GetSize() );
							}
						}
						if ( ( buffer0.size() != buffer1.size() ) || 
								 ( memcmp( &( buffer0[0] ), &( buffer1[0] ), buffer0.size() > buffer1.size() ? buffer1.size() : buffer0.size() ) != 0 ) )
						{
							if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
							{
								pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Copying: %s..." ), LPCTSTR( strFileName ) ) );
							}
							
							pFileStream = 0;
							if ( pFileStream = CreateFileStream( ( szBaseFilePath + TXT_EXTENTION ).c_str(), STREAM_ACCESS_WRITE ) )
							{
								pFileStream->Write( &( buffer0[0] ), buffer0.size() );
								bTranslation = true;
							}
						}

						pFileStream = 0;
						SELKTextProperty textProperty;
						if ( pFileStream = CreateFileStream( ( szBaseFilePath + XML_EXTENTION ).c_str(), STREAM_ACCESS_READ ) )
						{
							CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::READ );
							CTreeAccessor saver = pSaver;
							saver.AddTypedSuper( &textProperty );
						}
						if ( ( textProperty.nState < 0 ) || ( textProperty.nState >= SELKTextProperty::STATE_COUNT ) )
						{
							textProperty.nState = SELKTextProperty::STATE_NOT_TRANSLATED;
						}
						if ( bTranslation || bOrifginal )
						{
							textProperty.nState = nState;
							pFileStream = 0;
							if ( pFileStream = CreateFileStream( ( szBaseFilePath + XML_EXTENTION ).c_str(), STREAM_ACCESS_WRITE ) )
							{
								CPtr<IDataTree> pSaver = CreateDataTreeSaver( pFileStream, IDataTree::WRITE );
								CTreeAccessor saver = pSaver;
								saver.AddTypedSuper( &textProperty );
							}
						}
					}
				}
			}
			recset.MoveNext();
			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->IterateProgressPosition();
			}
		}
	}
	CATCH(CDBException , pEx)
	{
		pEx->ReportError();
	}
	AND_CATCH(CMemoryException, pEx)
	{
		pEx->ReportError();
	}
	END_CATCH
	database.Close();

	if ( pszNewVersion )
	{
		std::string szXLSName = rszXLSPath.substr( rszXLSPath.rfind( '\\' ) + 1 );
		( *pszNewVersion )	= NStr::Format( _T( "%s, [%02d:%02d:%04d, %02d.%02d.%02d]" ),
																				szXLSName.c_str(),
																				statsXLS.mtime.month,
																				statsXLS.mtime.year + 1980,
																				statsXLS.mtime.hours,
																				statsXLS.mtime.minutes,
																				statsXLS.mtime.seconds * 2 );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::CreateStatistic( SELKStatistic *pStatistic, class CELKTreeWindow *pwndELKTreeWindow, int nCodePage, CProgressDialog* pwndProgressDialog )
{
	NI_ASSERT_T( pStatistic != 0, NStr::Format( _T( "CELK::CreateStatistic() wrong parameter: pStatistic %x" ), pStatistic ) );

	if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
	{
		pwndProgressDialog->ShowWindow( SW_SHOW );
		pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Getting file structure..." ) ) );
		pwndProgressDialog->SetWindowText( _T( "Creating Statistics" ) );
	}

	pStatistic->Clear();
	if ( pwndELKTreeWindow )
	{
		if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
		{
			pwndProgressDialog->SetProgressRange( 0, pwndELKTreeWindow->GetItemsCountInternal() );
			pwndProgressDialog->SetProgressPosition( 0 );
		}
		HTREEITEM item = pwndELKTreeWindow->GetFirstItemInternal(); 
		while ( item )
		{
			int nELKElementNumber = pwndELKTreeWindow->GetELKElementNumberInternal( item );
			if ( nELKElementNumber >= 0 )
			{
				while ( pStatistic->original.size() <= nELKElementNumber )
				{
					pStatistic->original.push_back( SELKElementStatistic() );	
				}
				while ( pStatistic->translation.size() <= nELKElementNumber )
				{
					pStatistic->translation.push_back( SELKElementStatistic() );	
				}
				SELKElementStatistic &rOriginal = pStatistic->original[nELKElementNumber];
				SELKElementStatistic &rTranslation = pStatistic->translation[nELKElementNumber];

				std::string szFileRelPath;
				std::string szFilePath;
				pwndELKTreeWindow->GetXLSPathInternal( item, &szFileRelPath );
				pwndELKTreeWindow->GetItemPathInternal( item, &szFilePath, true );

				if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
				{
					pwndProgressDialog->SetProgressMessage( NStr::Format( _T( "Processing Item: %s..." ), szFileRelPath.c_str() ) );
				}

				bool bTranslated = false;
				int nState = GetState( szFilePath, &bTranslated );

				CString strOriginalText;
				GetOriginalText( szFilePath, &strOriginalText, nCodePage, true );

				if ( strOriginalText.IsEmpty() )
				{
					NStr::DebugTrace( _T( "EMPTY ORIGINAL! %s\n" ), szFilePath );
				}
				int nWordsCount = 0;
				int nWordSymbolsCount = 0;
				int nSymbolsCount = 0;
				CSpellChecker::GetTextCounts( strOriginalText, &nWordsCount, &nWordSymbolsCount, &nSymbolsCount );

				rOriginal.states[nState].nTextsCount += 1;
				rOriginal.states[nState].nWordsCount += nWordsCount;
				rOriginal.states[nState].nWordSymbolsCount += nWordSymbolsCount;
				rOriginal.states[nState].nSymbolsCount += nSymbolsCount;

				CString strTranslatedText;
				GetTranslatedText( szFilePath, &strTranslatedText, nCodePage, true  );
				CSpellChecker::GetTextCounts( strTranslatedText, &nWordsCount, &nWordSymbolsCount, &nSymbolsCount );
				
				rTranslation.states[nState].nTextsCount += 1;
				rTranslation.states[nState].nWordsCount += nWordsCount;
				rTranslation.states[nState].nWordSymbolsCount += nWordSymbolsCount;
				rTranslation.states[nState].nSymbolsCount += nSymbolsCount;
			}
			
			if ( pwndProgressDialog && ( pwndProgressDialog->GetSafeHwnd() != 0 ) )
			{
				pwndProgressDialog->IterateProgressPosition();
			}
			item = pwndELKTreeWindow->GetNextItemInternal( item );
		}
		pStatistic->bValid = true;
	}
	return true;	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CELK::UpdateELK( const std::string &rszPath, const std::string &rszPAKFileName, class CProgressDialog* pwndProgressDialog )
{
/**
		bool bAbsolute = false;
		std::string szPAKExtention = szPAKPath.substr( szPAKPath.rfind( '.') );
		NStr::ToLower( szPAKExtention );
		if ( szPAKExtention == std::string( _T( ".upd" ) ) )
		{
			bAbsolute = true;	
		}
/**/

	CELK elk;
	elk.Open( rszPath, false );
	bool bUpdateFileIsUPD = false;

	std::string szFolder = rszPath.substr( 0, rszPath.rfind( '\\' ) + 1 );
	
	std::unordered_map<std::string, int> files;
	
	if ( !rszPAKFileName.empty() )
	{
		std::string szFileTitle = rszPAKFileName.substr( rszPAKFileName.rfind( '\\' ) + 1 );
		if ( szFileTitle.rfind( UPD_EXTENTION ) == ( szFileTitle.size() - strlen( UPD_EXTENTION ) ) )
		{
			szFileTitle = szFileTitle.substr(	0, szFileTitle.rfind( UPD_EXTENTION ) );
			bUpdateFileIsUPD = true;
		}
		else if ( szFileTitle.rfind( PAK_EXTENTION ) == ( szFileTitle.size() - strlen( PAK_EXTENTION ) ) )
		{
			szFileTitle = szFileTitle.substr(	0, szFileTitle.rfind( PAK_EXTENTION ) );
			bUpdateFileIsUPD = false;
		}

		int nNumber = -1;
		int nPosition = szFileTitle.rfind( '_' );
		if ( nPosition != std::string::npos )
		{
			std::string szNumber = szFileTitle.substr( nPosition + 1 );
			szFileTitle = szFileTitle.substr( 0, nPosition );
			if ( sscanf( szNumber.c_str(), _T( "%d" ), &nNumber ) <= 0 )
			{
				nNumber = -1;
			}
		}
		files[szFileTitle] = nNumber;
	}
	else
	{
		//���� ��� ����� ������� � �� ��������� ������
		for ( NFile::CFileIterator _NFileIterator( NStr::Format( _T( "%s*%s" ), szFolder.c_str(), UPD_EXTENTION ) ); !_NFileIterator.IsEnd(); ++_NFileIterator )
		{
			int nNumber = -1;
			if ( !_NFileIterator.IsDirectory() && !_NFileIterator.IsDots() )
			{
				std::string szFileTitle = _NFileIterator.GetFileTitle();
				int nPosition = szFileTitle.rfind( '_' );
				if ( nPosition != std::string::npos )
				{
					std::string szNumber = szFileTitle.substr( nPosition + 1 );
					szFileTitle = szFileTitle.substr( 0, nPosition );
					if ( sscanf( szNumber.c_str(), _T( "%d" ), &nNumber ) <= 0 )
					{
						nNumber = -1;
					}
				}
				std::unordered_map<std::string, int>::const_iterator fileIterator = files.find( szFileTitle );
				if ( fileIterator != files.end() )
				{
					if ( fileIterator->second < nNumber )
					{
						files[szFileTitle] = nNumber;
					}
				}
				else
				{
					files[szFileTitle] = nNumber;
				}
			}
		}
	}

	//Update Existing ELK Elements
	for ( std::vector<SELKElement>::iterator elementIterator = elk.elements.begin(); elementIterator != elk.elements.end(); )
	{
		std::string szFileName = elementIterator->szPath.substr( elementIterator->szPath.rfind( '\\' ) + 1 );
		std::unordered_map<std::string, int>::iterator fileIterator = files.find( szFileName );
		if ( fileIterator != files.end() )
		{
			if ( ( !rszPAKFileName.empty() ) || ( elementIterator->nLastUpdateNumber < fileIterator->second ) )
			{
				//Update ELKElement
				elementIterator->nLastUpdateNumber = fileIterator->second;
				std::string szPAKFile;
				bool bUPDFile = true;
				if ( !rszPAKFileName.empty() )
				{
					szPAKFile = rszPAKFileName;
					bUPDFile = bUpdateFileIsUPD;
				}
				else
				{
					if (  fileIterator->second < 0 )
					{
						szPAKFile = NStr::Format( _T( "%s%s%s" ), szFolder.c_str(), fileIterator->first.c_str(), UPD_EXTENTION );
					}
					else
					{
						szPAKFile = NStr::Format( _T( "%s%s_%d%s" ), szFolder.c_str(), fileIterator->first.c_str(), fileIterator->second, UPD_EXTENTION );
					}
				}
				ImportFromPAK( szPAKFile,
											 elementIterator->szPath,
											 bUPDFile,
											 &( elementIterator->szVersion ),
											 pwndProgressDialog );
				//Update ELKElement Description
				{
					std::string szDataBaseFolder;
					elementIterator->GetDataBaseFolder( &szDataBaseFolder );
					CPtr<IDataStream> pStreamDESCRIPTION = CreateFileStream( NStr::Format( _T( "%s%s%s" ), szDataBaseFolder.c_str(), CELK::PAK_DESCRIPTION, CELK::PAK_DESCRIPTION_EXTENTION ), STREAM_ACCESS_READ );
					if ( pStreamDESCRIPTION != 0 )
					{
						CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamDESCRIPTION, IDataTree::READ );
						CTreeAccessor saver = pSaver;
						saver.AddTypedSuper( &( elementIterator->description ) );
					}
				}
			}
			files.erase( fileIterator );
			++elementIterator;
		}
		else
		{
			if ( rszPAKFileName.empty() )
			{
				elementIterator = elk.elements.erase( elementIterator );
			}
			else
			{
				++elementIterator;
			}
		}
	}

	//Add New ELK Elements
	for ( std::unordered_map<std::string, int>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
	{
		elk.elements.push_back( SELKElement() );
		SELKElement &rELKElement = elk.elements.back();
		rELKElement.szPath = szFolder + fileIterator->first;
		rELKElement.nLastUpdateNumber = fileIterator->second;
		std::string szPAKFile;
		bool bUPDFile = true;
		if ( !rszPAKFileName.empty() )
		{
			szPAKFile = rszPAKFileName;
			bUPDFile = bUpdateFileIsUPD;
		}
		else
		{
			if (  fileIterator->second < 0 )
			{
				szPAKFile = NStr::Format( _T( "%s%s%s" ), szFolder.c_str(), fileIterator->first.c_str(), UPD_EXTENTION );
			}
			else
			{
				szPAKFile = NStr::Format( _T( "%s%s_%d%s" ), szFolder.c_str(), fileIterator->first.c_str(), fileIterator->second, UPD_EXTENTION );
			}
		}
		ImportFromPAK( szPAKFile,
									 rELKElement.szPath,
									 bUPDFile,
									 &( rELKElement.szVersion ),
									 pwndProgressDialog );
		//Update ELKElement Description
		{
			std::string szDataBaseFolder;
			rELKElement.GetDataBaseFolder( &szDataBaseFolder );
			CPtr<IDataStream> pStreamDESCRIPTION = CreateFileStream( NStr::Format( _T( "%s%s%s" ), szDataBaseFolder.c_str(), CELK::PAK_DESCRIPTION, CELK::PAK_DESCRIPTION_EXTENTION ), STREAM_ACCESS_READ );
			if ( pStreamDESCRIPTION != 0 )
			{
				CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamDESCRIPTION, IDataTree::READ );
				CTreeAccessor saver = pSaver;
				saver.AddTypedSuper( &( rELKElement.description ) );
			}
		}
	}

	elk.szPath = rszPath;
	if ( !elk.elements.empty() )
	{
		elk.Save(); 
	}
	return ( !elk.elements.empty() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CELK::UpdateGame( const CELK &rELK,
											 const std::string &rszZIPToolPath,
											 class CELKTreeWindow *pwndELKTreeWindow,
											 bool bRunGame,
											 const CString &rstrFontName,
											 DWORD dwNormalFontSize,
											 DWORD dwLargeFontSize,
											 int nCodePage,
											 class CProgressDialog* pwndProgressDialog )
{
	std::string szGameFolder;
	CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_READ, GAME_REGISTRY_FOLDER );
	registrySection.LoadString( GAME_REGISTRY_KEY, &szGameFolder, "" );
	if ( !szGameFolder.empty() )
	{
		if ( szGameFolder.rfind( CELK::GAME_FILE_NAME ) == ( szGameFolder.size() - strlen( CELK::GAME_FILE_NAME ) ) )
		{
			szGameFolder.resize( szGameFolder.rfind( CELK::GAME_FILE_NAME ) );
		}
		if ( ( !szGameFolder.empty() ) && ( szGameFolder[szGameFolder.size() - 1] != '\\' ) )
		{
			szGameFolder += "\\";
		}
		
		for ( std::vector<SELKElement>::const_iterator elementIterator = rELK.elements.begin(); elementIterator != rELK.elements.end(); ++elementIterator )
		{
			std::string szFileName = elementIterator->szPath.substr( elementIterator->szPath.rfind( '\\' ) + 1 );

			std::string szPAKPath = szGameFolder + TEXTS_PAK_FILE_NAME + PAK_EXTENTION;
			ExportToPAK( elementIterator->szPath,
									 szPAKPath,
									 rszZIPToolPath,
									 pwndELKTreeWindow,
									 true,
									 elementIterator->description.bGenerateFonts,
									 rstrFontName,
									 dwNormalFontSize,
									 dwLargeFontSize,
									 nCodePage,
									 pwndProgressDialog, 0 );
		}
		ExecuteProcess( ( szGameFolder + std::string( GAME_FILE_NAME ) ).c_str(), GAME_PARAMETERS,  szGameFolder.c_str(), false );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFontGen::IsFit( const SFontInfo &fi, DWORD dwNumChars, DWORD dwSizeX, DWORD dwSizeY )
{
	return ( dwSizeX / ( fi.tm.tmAveCharWidth + 2 ) ) * ( dwSizeY / fi.tm.tmHeight ) >= dwNumChars;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFontGen::EstimateTextureSize( SFontInfo &fi, DWORD dwNumChars )
{
  for ( int i = 6; i < 13; ++i )
  {
    // first, try to estimate 2:1 size
    if ( IsFit( fi, dwNumChars, 1L << i, 1L << ( i - 1 ) ) )
    {
      fi.nTextureSizeX = 1L << i;
      fi.nTextureSizeY = 1L << ( i - 1 );
      return true;
    }
    // then, try to estimate 1:1 size
    else if ( IsFit( fi, dwNumChars, 1L << i, 1L << i ) )
    {
      fi.nTextureSizeX = fi.nTextureSizeY = 1L << i;
      return true;
    }
  }
  // too big texture!!!
  return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFontGen::MeasureFont( HDC hdc, SFontInfo &fi, std::vector<WORD> &chars, bool bSingleByte, int nCodePage )
{
  GetTextMetrics( hdc, &fi.tm );
	//����������
	if ( std::find( chars.begin(), chars.end(), fi.tm.tmDefaultChar ) == chars.end() )
	{
		chars.push_back( fi.tm.tmDefaultChar );
	}
	std::sort( chars.begin(), chars.end() );
  
	// translate chars to UNICODE and re-map kerns and chars
	{
		std::wstring wszText;
		{
			int nActualSize = 0;
			LPSTR pMBCSString = new TCHAR[chars.size() * 2];

			if ( bSingleByte )
			{
				for ( int nCharIndex = 0; nCharIndex < chars.size(); ++nCharIndex )
				{
					pMBCSString[nActualSize] = ( chars[nCharIndex] & 0xFF );
					++nActualSize;
				}
			}
			else
			{
				for ( int nCharIndex = 0; nCharIndex < chars.size(); ++nCharIndex )
				{
					const BYTE b0 = ( chars[nCharIndex] >> 8 ) & 0xFF;
					const BYTE b1 = ( chars[nCharIndex] & 0xFF );

					if ( b0 > 0 )
					{
						pMBCSString[nActualSize] = b0;
						pMBCSString[nActualSize + 1] = b1;
						nActualSize += 2;
					}
					else
					{
						pMBCSString[nActualSize] = b1;
						++nActualSize;
					}
				}

			}
			
			const int nBufferLength = ::MultiByteToWideChar( nCodePage, 0, pMBCSString, nActualSize, 0, 0 );
			wszText.resize( nBufferLength );
			::MultiByteToWideChar( nCodePage, 0, pMBCSString, nActualSize, &( wszText[0] ), wszText.size() );

			delete pMBCSString;
			pMBCSString = 0;
		}
		
		for ( int nCharIndex = 0; nCharIndex < chars.size(); ++nCharIndex )
		{
			if ( nCharIndex < wszText.size() ) 
			{
				fi.translate[ chars[nCharIndex] ]= wszText[nCharIndex];
			}
			else
			{
				fi.translate[ chars[nCharIndex] ]= 0x0000;
			}
		}
	}
	
	// Measure TrueType fonts with GetCharABCWidths:
	fi.abc.resize( chars.size() );
	int nkpsCount = GetKerningPairs( hdc, 0, 0 );
	fi.kps.resize( nkpsCount );
	if ( !GetCharABCWidths( hdc, chars[0], chars[0], &( fi.abc[0] ) ) )
  {
		// 
		ABC abc;
		Zero( abc );
		std::fill( fi.abc.begin(), fi.abc.end(), abc );
    // If it's not a TT font, use GetTextExtentPoint32 to fill array abc:
    SIZE size;
		for ( int i = 0; i < chars.size(); ++i )
		{
      // get width of character...
      GetTextExtentPoint32( hdc, (LPCTSTR)&( chars[i] ), 1, &size );
      // ...and store it in abcB:
      fi.abc[i].abcB = size.cx;
		}
  }
	else
	{
		for ( int i = 0; i < chars.size(); ++i )
		{
			GetCharABCWidths( hdc, chars[i], chars[i], &( fi.abc[i] ) );
		}
	}

  // get kerning pairs
	KERNINGPAIR kernpair;
	Zero( kernpair );
	std::fill( fi.kps.begin(), fi.kps.end(), kernpair );
  GetKerningPairs( hdc, nkpsCount, &( fi.kps[0] ) );
  // remove kerning pairs with '0' kern value
  fi.kps.erase( std::remove_if( fi.kps.begin(), fi.kps.end(), SKPZeroFunctional( &( fi.translate ) ) ), fi.kps.end() );
  // estimate texture size
  if ( EstimateTextureSize( fi, chars.size() ) == false )
  {
		throw 1; // too large texture !!!
	}
  // check and correct size estimating
  int x = 0, y = 0;
	for ( int i = 0; i < chars.size(); ++i )
	{
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + LEADING_PIXELS > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
      {
        if ( fi.nTextureSizeX == fi.nTextureSizeY ) // if we have 1:1 sizes, make it 2:1
          fi.nTextureSizeX <<= 1;
        else                                   // else, if we have 2:1 already, make it 2:2 :)
          fi.nTextureSizeY = fi.nTextureSizeX;
        break;
      }
    }
    x += LEADING_PIXELS;
    x += nNextCharShift;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFontGen::LoadFont( HWND hWnd,
												 SFontInfo &fi,
												 int nHeight,
												 int nWeight,
												 bool bItalic,
												 int nCodePage, 
												 bool bAntialias,
												 DWORD dwPitch,
												 const CString &strFaceName,
												 bool bSingleByte,
												 std::vector<WORD> &chars )
{
  // create an HFONT:
  if ( fi.hFont )
  { 
    DeleteObject( fi.hFont ); 
    fi.hFont = 0;
  }
  // create font (in this version this will be with the hardcoded height)
  // in the next version I want completely remove 'ChooseFont' dialog and take all info from the .ini file
  fi.hFont = ::CreateFont( nHeight, 0, 0, 0, nWeight, bItalic, FALSE, FALSE, 
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                           bAntialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
                           dwPitch, LPCTSTR( strFaceName ) );
  // retrieve logfont
//  ::GetObject( fi.hFont, sizeof(fi.lf), &fi.lf );
  // get HDC:
  HDC hdc = GetDC( hWnd );
  // select font:
  HFONT hOldFont = (HFONT)::SelectObject( hdc, fi.hFont );
	//
  // get text metrics and char widths:
  MeasureFont( hdc, fi, chars, bSingleByte, nCodePage );

  // select old font
  ::SelectObject( hdc, hOldFont );
  // release HDC:
  ReleaseDC( hWnd, hdc );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFontGen::DrawFont( HDC hdc, const SFontInfo &fi, const std::vector<WORD> &chars )
{
  // Draw characters:
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
    WORD wChar = chars[i];
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + LEADING_PIXELS > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
        return false;
    }
    x += LEADING_PIXELS;
		WORD wSymbol = fi.Translate( chars[i] );
		if ( wSymbol > 0 )
		{
			TextOutW( hdc, x - fi.abc[i].abcA, y*fi.tm.tmHeight, (LPCWSTR)&( wSymbol ), 1 );
		}
    x += nNextCharShift;
	}
  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* CFontGen::CreateFontImage( const SFontInfo &fi, const std::vector<WORD> &chars )
{
  // Create an offscreen bitmap:
  int width = fi.nTextureSizeX;//16 * fi.tm.tmMaxCharWidth;
  int height = fi.nTextureSizeY;//14 * fi.tm.tmHeight;
  // Prepare to create a bitmap
  BYTE *pBitmapBits = 0;
  BITMAPINFO bmi;
  memset( &bmi.bmiHeader, 0, sizeof(bmi.bmiHeader) );
  bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
  bmi.bmiHeader.biWidth       = fi.nTextureSizeX;
  bmi.bmiHeader.biHeight      = fi.nTextureSizeY;
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biBitCount    = 24;
  bmi.bmiHeader.biSizeImage   = abs( bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * bmi.bmiHeader.biBitCount / 8 );
  // Create a DC and a bitmap for the font
  HDC hDC = CreateCompatibleDC( 0 );
  HBITMAP hbmBitmap = CreateDIBSection( hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, 0, 0 );
  HBITMAP hOldBmp = (HBITMAP)SelectObject( hDC, hbmBitmap );
  HFONT hOldFont = (HFONT)SelectObject( hDC, fi.hFont );
  // Clear background to black:
  SelectObject( hDC, GetStockObject(BLACK_BRUSH) );
  Rectangle( hDC, 0, 0, width, height );
  SetBkMode( hDC, TRANSPARENT );           // do not fill character background
  SetTextColor( hDC, RGB(255, 255, 255) ); // text color white
  SetTextAlign( hDC, TA_TOP );
  // Draw characters:
  DrawFont( hDC, fi, chars );
  //
  SelectObject( hDC, hOldFont );
  SelectObject( hDC, hOldBmp );
  //
  // create image.
	// use only one color component due to gray-scale image
  std::vector<DWORD> imagedata( fi.nTextureSizeX * fi.nTextureSizeY );
  for ( int i=0, j=0; i<fi.nTextureSizeX * fi.nTextureSizeY * 3; i+=3, ++j )
  {
		DWORD a = pBitmapBits[i];
    DWORD c = pBitmapBits[i];//( a != 0 ? 255 : pBitmapBits[i] );
    imagedata[j] = (a << 24) | (c << 16) | (c << 8) | c;
  }
	IImage *pImage = GetSingleton<IImageProcessor>()->CreateImage( fi.nTextureSizeX, fi.nTextureSizeY, &(imagedata[0]) );
	pImage->FlipY(); // flip image due to bottom-left bitmap orientation

	return pImage;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SFontFormat* CFontGen::CreateFontFormat( const std::string &szFaceName, const SFontInfo &fi, const std::vector<WORD> &chars )
{
	SFontFormat *pFF = new SFontFormat();
	// face name
	pFF->szFaceName = szFaceName;
	NStr::ToLower( pFF->szFaceName );
	// metrics
	pFF->metrics.nHeight = fi.tm.tmHeight;
	pFF->metrics.nAscent = fi.tm.tmAscent;
	pFF->metrics.nDescent = fi.tm.tmDescent;
	pFF->metrics.nInternalLeading = fi.tm.tmInternalLeading;
	pFF->metrics.nExternalLeading = fi.tm.tmExternalLeading;
	pFF->metrics.nAveCharWidth = fi.tm.tmAveCharWidth;
	pFF->metrics.nMaxCharWidth = fi.tm.tmMaxCharWidth;
	pFF->metrics.wDefaultChar = fi.Translate( fi.tm.tmDefaultChar );
	pFF->metrics.cCharSet = fi.tm.tmCharSet;
	// pFF->metrics.fSpaceWidth will be filled later 
	// kerning pairs
	for ( int i=0; i<fi.kps.size(); ++i )
	{
		DWORD dwFirst = fi.Translate( fi.kps[i].wFirst );
		DWORD dwSecond = fi.Translate( fi.kps[i].wSecond );
		pFF->kerns[ (dwFirst << 16) | dwSecond ] = fi.kps[i].iKernAmount;
	}
	// chars
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
		BYTE ansicode = chars[i];
		WORD unicode = fi.Translate( chars[i] );
		SFontFormat::SCharDesc &chardesc = pFF->chars[unicode];
		//
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + LEADING_PIXELS > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
    }
    x += LEADING_PIXELS;
    // char ABC parameters in the texture's respective size
    chardesc.fA = fi.abc[i].abcA;
    chardesc.fB = fi.abc[i].abcB;
    chardesc.fC = fi.abc[i].abcC;
    chardesc.fWidth = fi.abc[i].abcB + ( fi.abc[i].abcC > 0 ? fi.abc[i].abcC : 0 );
    // character rect in the texture's coords
    // add '0.5f' to all coords to achive an excellent letter quality (due to texel center in (0.5,0.5) with respect to pixel center)
    chardesc.x1 = float( x + 0.5f ) / fi.nTextureSizeX;
    chardesc.y1 = float( y * fi.tm.tmHeight + 0.5f ) / fi.nTextureSizeY;
    chardesc.x2 = float( x + chardesc.fWidth + 0.5f ) / fi.nTextureSizeX;
    chardesc.y2 = float( ( y + 1 ) * fi.tm.tmHeight + 0.5f ) / fi.nTextureSizeY;
    //
    x += nNextCharShift;
	}
  //
	pFF->metrics.fSpaceWidth = pFF->chars[ fi.Translate(32) ].fWidth;
	//
	return pFF;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFontGen::GenerateFont( const std::string &rszFolder,
														 const std::set<WORD> &rSymbols,
														 bool bSingleByte,
														 const CString &rstrFontName,
														 DWORD dwNormalFontSize,
														 DWORD dwLargeFontSize,
														 int nCodePage )
{
	std::vector<WORD> chars;
	for ( WORD nCharIndex = 32; nCharIndex < 128; ++nCharIndex ) 
	{
		chars.push_back( nCharIndex );
	}
	if ( bSingleByte || rSymbols.empty() )
	{
		for ( WORD nCharIndex = 128; nCharIndex < 256; ++nCharIndex ) 
		{
			chars.push_back( nCharIndex );
		}
	}
	else
	{
		for ( std::set<WORD>::const_iterator symbolIterator = rSymbols.begin(); symbolIterator != rSymbols.end(); ++symbolIterator )
		{
			if ( ( *symbolIterator ) >= 128 )
			{
				chars.push_back( *symbolIterator );
			}
		}
	}
	//
	DWORD fontsSize[FONTS_COUNT];
	for ( int nFontIndex = 0; nFontIndex < FONTS_COUNT; ++nFontIndex )
	{
		fontsSize[nFontIndex] = FONTS_SIZE[nFontIndex];
		if ( nFontIndex == 2 )
		{
			if ( ( dwNormalFontSize >= FONTS_SIZE[1] ) && ( dwNormalFontSize <= FONTS_SIZE[2] ) )
			{
				fontsSize[nFontIndex] = dwNormalFontSize;
			}
		}
		else if ( nFontIndex == 3 )
		{
			if ( ( dwLargeFontSize >= FONTS_SIZE[1] ) && ( dwLargeFontSize <= FONTS_SIZE[3] ) )
			{
				fontsSize[nFontIndex] = dwLargeFontSize;
			}
		}
	}
	//
	CString strFontName = rstrFontName;
	{
		HFONT hFont = ::CreateFont( FONTS_SIZE[2], 0, 0, 0, FW_NORMAL, false, FALSE, FALSE, 
																DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
																ANTIALIASED_QUALITY,
																VARIABLE_PITCH, LPCTSTR( strFontName ) );
		if ( hFont == 0 )
		{
			std::set<CString> fonts;
			CFontGen::GetFonts( nCodePage, &fonts );
			if ( !fonts.empty() )
			{
				strFontName = *( fonts.begin() );
			}
		}
	}
	for ( int nFontIndex = 0; nFontIndex < FONTS_COUNT; ++nFontIndex )
	{
		SFontInfo fi;
		LoadFont( GetDesktopWindow(),
							fi,
							fontsSize[nFontIndex],
							FW_NORMAL,
							false,
							nCodePage,
							true,
							VARIABLE_PITCH,
							strFontName,
							bSingleByte,
							chars );
		
		CPtr<IImage> pImage = CreateFontImage( fi, chars );
		SFontFormat *pFF = CreateFontFormat( "", fi, chars );

		std::string szFontFilePath = rszFolder + std::string( FONTS_FOLDER[nFontIndex] ) + std::string( FONT_FILE_NAME );
		
		if ( !SaveImageToDDSImageResource( pImage, szFontFilePath, GFXPF_DXT3, GFXPF_ARGB4444, GFXPF_ARGB8888 ) )
		{
			delete pFF;
			pFF = 0;
			return false;
		}
		CPtr<IDataStream> pStream = CreateFileStream( ( szFontFilePath + std::string( FONT_FILE_EXTENTION ) ).c_str(), STREAM_ACCESS_WRITE );
		if ( pStream != 0 )
		{
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, pFF );
		}
		else
		{
			delete pFF;
			pFF = 0;
			return false;
		}

		delete pFF;
		pFF = 0;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CALLBACK EnumAllFontsCallback( ENUMLOGFONTEX *lpelfe,    // logical-font data
																	 NEWTEXTMETRICEX *lpntme,  // physical-font data
																	 DWORD dwFontType,         // type of font
																	 LPARAM lParam )           // application-defined data
{
	std::set<CString> *pFontsList = reinterpret_cast<std::set<CString>*>( lParam );
	if ( pFontsList )
	{
		if ( dwFontType == TRUETYPE_FONTTYPE )
		{
			pFontsList->insert( CString( lpelfe->elfFullName ) );
		}
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CFontGen::GetFonts( DWORD nCodePage, std::set<CString> *pFontsList )
{
	CHARSETINFO cs;
	if ( !TranslateCharsetInfo( (DWORD*)nCodePage, &cs, TCI_SRCCODEPAGE ) )
	{
		cs.ciCharset = DEFAULT_CHARSET;
	}

	LOGFONT logFont;
	logFont.lfCharSet = cs.ciCharset;
	logFont.lfFaceName[0] = 0;
	logFont.lfPitchAndFamily = 0;
	HDC hDC = ::GetDC( 0 );
	::EnumFontFamiliesEx( hDC, &logFont, (FONTENUMPROC)EnumAllFontsCallback,  reinterpret_cast<LPARAM>( pFontsList ), 0 );
	::ReleaseDC( 0, hDC );

	return pFontsList->size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
