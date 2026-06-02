#include "StdAfx.h"
#include "resource.h"

#include "ELK_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "CreateFilterDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int SELKTextProperty::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nState );
	saver.Add( 2, &bTranslated );
	
	return 0;
}

int SELKTextProperty::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "State", &nState );
	saver.Add( "Changed", &bTranslated );

	return 0;
}

int SELKDescription::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szName );
	saver.Add( 2, &szPAKName );
	saver.Add( 3, &szUPDPrefix );
	saver.Add( 4, &bGenerateFonts );

	return 0;
}

int SELKDescription::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Name", &szName );
	saver.Add( "PAK", &szPAKName );
	saver.Add( "UPD", &szUPDPrefix );
	saver.Add( "Fonts", &bGenerateFonts );
	
	return 0;
}

int SELKElement::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &description );
	saver.Add( 2, &szPath );
	saver.Add( 3, &szVersion );
	saver.Add( 4, &nLastUpdateNumber );

	return 0;
}

int SELKElement::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Description", &description );
	saver.Add( "Path", &szPath );
	saver.Add( "Version", &szVersion );
	saver.Add( "LastUpdateNumber", &nLastUpdateNumber );
	
	return 0;
}

int SELKElementStatistic::SState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nTextsCount );
	saver.Add( 2, &nWordsCount );
	saver.Add( 3, &nWordSymbolsCount );
	saver.Add( 4, &nSymbolsCount );

	return 0;
}

int SELKElementStatistic::SState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "TextsCount", &nTextsCount );
	saver.Add( "WordCount", &nWordsCount );
	saver.Add( "WordSymbolsCount", &nWordSymbolsCount );
	saver.Add( "SymbolsCount", &nSymbolsCount );
	
	return 0;
}

int SELKElementStatistic::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &states );

	return 0;
}

int SELKElementStatistic::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "States", &states );

	return 0;
}

int SELKStatistic::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &original );
	saver.Add( 2, &translation );
	saver.Add( 3, &bValid );

	return 0;
}

int SELKStatistic::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Orininal", &original );
	saver.Add( "Translation", &translation );
	saver.Add( "Valid", &bValid );
	
	return 0;
}

int CELK::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &elements );
	saver.Add( 2, &szPath );
	saver.Add( 3, &statistic );

	return 0;
}

int CELK::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Elements", &elements );
	saver.Add( "Path", &szPath );
	saver.Add( "PreviousStatistic", &previousStatistic );
	saver.Add( "Statistic", &statistic );
	
	return 0;
}

void SELKElement::GetDataBaseFolder( std::string *pszDataBaseFolder ) const
{
	GetDataBaseFolder( szPath, pszDataBaseFolder );
}

void SELKElement::GetDataBaseReserveFolder( std::string *pszDataBaseReserveFolder ) const
{
	GetDataBaseReserveFolder( szPath, pszDataBaseReserveFolder );
}

bool CELK::Open( const std::string &rszELKPath, bool bEnumFiles )
{
	Close();
	
	CPtr<IDataStream> pStreamXML = CreateFileStream( rszELKPath.c_str(), STREAM_ACCESS_READ );
	if ( pStreamXML == 0 )
	{
		return false;
	}
	
	CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
	CTreeAccessor saver = pSaver;
	saver.AddTypedSuper( &( *this ) );
	
	szPath = rszELKPath;
	
	std::string szCurrentFolder = szPath.substr( 0, szPath.rfind( '\\' ) + 1 );
	for ( int nElementIndex = 0; nElementIndex < elements.size(); ++nElementIndex )
	{
		std::string szElementName = elements[nElementIndex].szPath.substr( elements[nElementIndex].szPath.rfind( '\\' ) + 1 );
		elements[nElementIndex].szPath = szCurrentFolder + szElementName;
		elementNames[szElementName] = nElementIndex;
	}

	if ( bEnumFiles )
	{
		for ( int nElementIndex = 0; nElementIndex < elements.size(); ++nElementIndex )
		{
			std::string szDataBaseFolder;
			elements[nElementIndex].GetDataBaseFolder( &szDataBaseFolder );
			if ( CPtr<IDataStorage> pStorage = OpenStorage( NStr::Format( _T( "%s*.pak" ), szDataBaseFolder.c_str() ), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
			{
				EnumFilesInDataStorage( 0, pStorage, &enumFolderStructureParameter );
			}
		}
	}
	return IsOpened();
}

bool CELK::Save()
{
	if ( IsOpened() )
	{
		CPtr<IDataStream> pStreamXML = CreateFileStream( szPath.c_str(), STREAM_ACCESS_WRITE );
		if ( pStreamXML == 0 )
		{
			return false;
		}
		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::WRITE );
		CTreeAccessor saver = pSaver;
		saver.AddTypedSuper( &( *this ) );
	}
	return true;
}

void CELK::Close()
{
	if ( IsOpened() )
	{
		if ( statistic.bValid )
		{
			previousStatistic = statistic;
		}
		Save();
		previousStatistic.Clear();
		statistic.Clear();
		szPath.clear();
		elements.clear();
		elementNames.clear();

		enumFolderStructureParameter.nIgnoreFolderCount = 1;
		enumFolderStructureParameter.folders.clear();
	}
}

bool SSimpleFilter::Check( const std::string &rszFolder, bool _bTranslated, int nState ) const
{
	bool bChecked = false;
	if ( !filter.empty() )
	{
		std::string szFolder = rszFolder;
		NStr::ToLower( szFolder );
		for ( TSimpleFilter::const_iterator filterIterator = filter.begin(); filterIterator != filter.end(); ++filterIterator )
		{
			bool bInnerChecked = true;
			for ( TSimpleFilterItem::const_iterator filterItemIterator = filterIterator->begin(); filterItemIterator != filterIterator->end(); ++filterItemIterator )
			{
				if ( szFolder.find( *filterItemIterator ) == std::string::npos )
				{
					bInnerChecked = false;
					break;
				}
			}
			if ( bInnerChecked )
			{
				bChecked = true;
				break;
			}
		}
	}
	else
	{
		bChecked = true;
	}
	if ( nState >= 0 && nState < states.size() )
	{
		if ( !states[nState] )
		{
			bChecked = false;
		}
	}
	if ( bTranslated && !_bTranslated )
	{
		bChecked = false;
	}
	return bChecked;
}

int SSimpleFilter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &filter );
	saver.Add( 2, &bTranslated );
	saver.Add( 3, &states );

	return 0;
}

int SSimpleFilter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Filter", &filter );
	saver.Add( "Translated", &bTranslated );
	saver.Add( "States", &states );
	
	return 0;
}

int SMainFrameParams::INVALID_FILTER_NUMBER = -1;
	
int SMainFrameParams::SSearchParam::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bFindDown );
	saver.Add( 2, &bFindMatchCase );
	saver.Add( 3, &bFindWholeWord );
	saver.Add( 4, &strFindText );
	saver.Add( 5, &nWindowType );
	saver.Add( 6, &nPosition );

	return 0;
}

int SMainFrameParams::SSearchParam::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "FindDown", &bFindDown );
	saver.Add( "FindMatchCase", &bFindMatchCase );
	saver.Add( "FindWholeWord", &bFindWholeWord );
	saver.Add( "FindText", &strFindText );
	saver.Add( "WindowType", &nWindowType );
	saver.Add( "Position", &nPosition );

	return 0;
}

int SMainFrameParams::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szZIPToolPath );
	saver.Add( 2, &szLastOpenedELKName );
	saver.Add( 3, &szLastOpenedPAKName );
	saver.Add( 4, &szLastOpenedXLSName );
	saver.Add( 5, &recentList );
	saver.Add( 6, &szPreviousPath );
	saver.Add( 7, &szLastPath );
	saver.Add( 8, &filters );
	saver.Add( 9, &szCurrentFilterName );
	saver.Add( 10, &bCollapseItem );
	saver.Add( 11, &nCodePage );
	saver.Add( 12, &bFullScreen );
	saver.Add( 13, &rect );
	saver.Add( 14, &szGameFolder );
	saver.Add( 15, &szHelpFilePath );
	saver.Add( 16, &nLastELKElement );
	saver.Add( 17, &szCurrentFolder );

	return 0;
}

int SMainFrameParams::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "ZIPToolPath", &szZIPToolPath );
	saver.Add( "LastOpenedELKName", &szLastOpenedELKName );
	saver.Add( "LastOpenedPAKName", &szLastOpenedPAKName );
	saver.Add( "LastOpenedXLSName", &szLastOpenedXLSName );
	saver.Add( "RecentList", &recentList );
	saver.Add( "PreviousPath", &szPreviousPath );
	saver.Add( "LastPath", &szLastPath );
	saver.Add( "LastELKElement", &nLastELKElement );
	saver.Add( "Filters", &filters );
	saver.Add( "CurrentFilter", &szCurrentFilterName );
	saver.Add( "CollapseItem", &bCollapseItem );
	saver.Add( "CodePage", &nCodePage );
	saver.Add( "FullScreen", &bFullScreen );
	saver.Add( "Rect", &rect );
	saver.Add( "GameFolder", &szGameFolder );
	saver.Add( "HelpFilePath", &szHelpFilePath );
	saver.Add( "CurrentFolder", &szCurrentFolder );

	return 0;
}

void SMainFrameParams::ValidatePath( std::string *pszPath, bool bFolder )
{
	if ( ( pszPath->size() < 2 ) || ( ( *pszPath )[1] != ':' ) )
	{
		if ( !pszPath->empty() )
		{
			if ( ( *pszPath )[0] == '\\' )
			{
				( *pszPath ) = pszPath->substr( 1 );
			}
		}
		( *pszPath ) = szCurrentFolder + ( *pszPath );
	}
	if ( bFolder )
	{
		if ( ( *pszPath )[pszPath->size() - 1] != '\\' )
		{
			( *pszPath ) += std::string( "\\" );	
		}
	}
	else
	{
		if ( ( *pszPath )[pszPath->size() - 1] == '\\' )
		{
			( *pszPath ) = pszPath->substr( 0, pszPath->size() - 1 );
		}
	}
	NStr::ToLower( ( *pszPath ) );
}

void SMainFrameParams::LoadFromRegistry( const std::string &rszRegistryKey, bool bShortApperence )
{
	CString strKey;
	std::string szFormat;
	int nValue = 0;
	CRegistrySection registrySection( HKEY_CURRENT_USER, KEY_READ, rszRegistryKey.c_str() );
	
	{
		char pBuffer[0xFFF + 1];
		::GetCurrentDirectory( 0xFFF, pBuffer );
		szCurrentFolder = std::string( pBuffer ) + std::string( "\\" );
	}

	szZIPToolPath = szCurrentFolder + std::string( "\\" ) + std::string( CELK::ZIP_EXE );
	szHelpFilePath = szCurrentFolder + std::string( "\\" ) + std::string( CELK::ELK_CHM );

	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_ELK_NAME );
	registrySection.LoadString( strKey, &szLastOpenedELKName, "" );

	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_PAK_NAME );
	registrySection.LoadString( strKey, &szLastOpenedPAKName, "" );

	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_XLS_NAME );
	registrySection.LoadString( strKey, &szLastOpenedXLSName, "" );

	nValue = 1;
	strKey.LoadString( IDS_REGISTRY_KEY_COLLAPSE_ITEM );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nValue, 1 );
	bCollapseItem = ( nValue  > 0 );

	strKey.LoadString( IDS_REGISTRY_LAST_PATH );
	registrySection.LoadString( strKey, &szLastPath, "" );
	
	strKey.LoadString( IDS_REGISTRY_LAST_ELK_ELEMENT );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nLastELKElement, 0 );

	nValue = 0;
	strKey.LoadString( IDS_REGISTRY_KEY_FULL_SCREEN );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nValue, 0 );
	bFullScreen = ( nValue  > 0 );

	strKey.LoadString( IDS_REGISTRY_KEY_RECT );
	registrySection.LoadRect( strKey, _T( "%d" ), &rect, CTRect<int>( 0, 0, 0, 0 ) );

	strKey.LoadString( IDS_REGISTRY_KEY_CODE_PAGE );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nCodePage, CELK::DEFAULT_CODE_PAGE );
	if ( nCodePage == CELK::DEFAULT_CODE_PAGE )
	{
		nCodePage = ::GetACP();
	}
	
	int nRecentCount = 0;
	strKey.LoadString( IDS_REGISTRY_KEY_RECENT_LIST );
	szFormat = NStr::Format( _T( "%ss" ), LPCTSTR( strKey ) );
	registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &nRecentCount, 0 );
	recentList.clear();
	for ( int nRecentIndex = 0; nRecentIndex < nRecentCount; ++nRecentIndex )
	{
		recentList.push_back( "" );
		std::string szFormat = NStr::Format( _T( "%s%d" ), LPCTSTR( strKey ), nRecentIndex );
		registrySection.LoadString( szFormat.c_str(), &( recentList.back() ), "" );
	}

	strKey.LoadString( IDS_REGISTRY_CURRENT_FILTER_NAME );
	registrySection.LoadString( strKey, &szCurrentFilterName, "" );
	{
		CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_READ, CELK::GAME_REGISTRY_FOLDER );
		registrySection.LoadString( CELK::GAME_REGISTRY_KEY, &szGameFolder, "" );
		if ( szGameFolder.rfind( CELK::GAME_FILE_NAME ) == ( szGameFolder.size() - strlen( CELK::GAME_FILE_NAME ) ) )
		{
			szGameFolder.resize( szGameFolder.rfind( CELK::GAME_FILE_NAME ) );
		}
		if ( ( !szGameFolder.empty() ) && ( szGameFolder[szGameFolder.size() - 1] != '\\' ) )
		{
			szGameFolder += "\\";
		}
	}

	{
		int nFilterCount = 0;
		strKey.LoadString( IDS_REGISTRY_FILTER );
		szFormat = NStr::Format( _T( "%ss" ), LPCTSTR( strKey ) );
		registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &nFilterCount, 0 );
		filters.clear();
		if ( nFilterCount > 0 )
		{
			for ( int nFilterIndex = 0; nFilterIndex < nFilterCount; ++nFilterIndex )
			{
				std::string szFilterName;
				szFormat = NStr::Format( _T( "%s%d_Name" ), LPCTSTR( strKey ), nFilterIndex );
				registrySection.LoadString( szFormat.c_str(), &szFilterName, "" );

				if ( !szFilterName.empty() )
				{
					SSimpleFilter &rSimpleFilter = filters[szFilterName];
					rSimpleFilter.states.resize(SELKTextProperty::STATE_COUNT, true );
				
					szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_NOT_TRANSLATED] );
					registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &( rSimpleFilter.states[SELKTextProperty::STATE_NOT_TRANSLATED] ), 1 );

					szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_OUTDATED] );
					registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &( rSimpleFilter.states[SELKTextProperty::STATE_OUTDATED] ), 1 );

					szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_TRANSLATED] );
					registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &( rSimpleFilter.states[SELKTextProperty::STATE_TRANSLATED] ), 1 );

					szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_APPROVED] );
					registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &( rSimpleFilter.states[SELKTextProperty::STATE_APPROVED] ), 1 );

					nValue = 0;
					szFormat = NStr::Format( _T( "%s%d_Changed" ), LPCTSTR( strKey ), nFilterIndex );
					registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &nValue, 0 );
					rSimpleFilter.bTranslated = ( nValue > 0 );

					int nFilterItemIndex = 0;
					while ( true )
					{
						int nFilterItemStringIndex = 0;
						TSimpleFilterItem filterItem;
						while( true )
						{
							szFormat = NStr::Format( _T( "%s%d_%d_%d" ), LPCTSTR( strKey ), nFilterIndex, nFilterItemIndex, nFilterItemStringIndex );
							std::string szFilterItemString;
							registrySection.LoadString( szFormat.c_str(), &szFilterItemString, "" );
							if ( szFilterItemString.empty() )
							{
								break;
							}
							filterItem.push_back( szFilterItemString );
							++nFilterItemStringIndex;
						}
						if ( nFilterItemStringIndex == 0 )
						{
							break;
						}
						rSimpleFilter.filter.push_back( filterItem );
						++nFilterItemIndex;
					}
				}
			}
		}
	}
	/**
	else
	{
		filters.clear();
	}
	/**/
	if ( filters.empty() )
	{
		filters.clear();
		{
			SSimpleFilter &rSimpleFilter = filters[_T( "All" )];
			rSimpleFilter.states.resize( SELKTextProperty::STATE_COUNT, 1 );
		}
		{
			SSimpleFilter &rSimpleFilter = filters[_T( "Not Translated" )];
			rSimpleFilter.states.resize( SELKTextProperty::STATE_COUNT, 0 );
			rSimpleFilter.states[SELKTextProperty::STATE_NOT_TRANSLATED] = 1;
			rSimpleFilter.states[SELKTextProperty::STATE_OUTDATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_APPROVED] = 0;
		}
		{
			SSimpleFilter &rSimpleFilter = filters[_T( "Outdated" )];
			rSimpleFilter.states.resize( SELKTextProperty::STATE_COUNT, 0 );
			rSimpleFilter.states[SELKTextProperty::STATE_NOT_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_OUTDATED] = 1;
			rSimpleFilter.states[SELKTextProperty::STATE_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_APPROVED] = 0;
		}
		{
			SSimpleFilter &rSimpleFilter = filters[_T( "Translated" )];
			rSimpleFilter.states.resize( SELKTextProperty::STATE_COUNT, 0 );
			rSimpleFilter.states[SELKTextProperty::STATE_NOT_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_OUTDATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_TRANSLATED] = 1;
			rSimpleFilter.states[SELKTextProperty::STATE_APPROVED] = 0;
		}
		{
			SSimpleFilter &rSimpleFilter = filters[_T( "Approved" )];
			rSimpleFilter.states.resize( SELKTextProperty::STATE_COUNT, 0 );
			rSimpleFilter.states[SELKTextProperty::STATE_NOT_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_OUTDATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_TRANSLATED] = 0;
			rSimpleFilter.states[SELKTextProperty::STATE_APPROVED] = 1;
		}
		szCurrentFilterName = _T( "All" );
	}

	nValue = 1;
	strKey.LoadString( IDS_REGISTRY_KEY_FIND_DOWN );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nValue, 1 );
	searchParam.bFindDown = ( nValue  > 0 );

	nValue = 0;
	strKey.LoadString( IDS_REGISTRY_KEY_FIND_MATCH_CASE );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nValue, 0 );
	searchParam.bFindMatchCase = ( nValue  > 0 );

	nValue = 0;
	strKey.LoadString( IDS_REGISTRY_KEY_FIND_WHOLE_WORD );
	registrySection.LoadNumber( strKey, _T( "%d" ), &nValue, 0 );
	searchParam.bFindWholeWord = ( nValue  > 0 );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_WINDOW );
	registrySection.LoadNumber( strKey, _T( "%d" ), &( searchParam.nWindowType ), (int)( SSearchParam::WT_ORIGINAL ) );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_POSITION );
	registrySection.LoadNumber( strKey, _T( "%d" ), &( searchParam.nPosition ), 0 );
	
	std::string szBufferFontName;
	strKey.LoadString( IDS_REGISTRY_FONT_NAME );
	registrySection.LoadString( strKey, &szBufferFontName, CFontGen::FONT_NAME );
	if ( !szBufferFontName.empty() )
	{
		strFontName = CString( szBufferFontName.c_str() );
	}
	{
		std::set<CString> fonts;
		CFontGen::GetFonts( nCodePage, &fonts );
		if ( !fonts.empty() )
		{
			if ( fonts.find( strFontName ) == fonts.end() )
			{
				strFontName = *( fonts.begin() );
			}
		}
	}

	strKey.LoadString( IDS_REGISTRY_KEY_NORMAL_FONT_SIZE );
	registrySection.LoadNumber( strKey, _T( "%d" ), &( dwNormalFontSize ), CFontGen::FONTS_SIZE[2] );

	strKey.LoadString( IDS_REGISTRY_KEY_LARGE_FONT_SIZE );
	registrySection.LoadNumber( strKey, _T( "%d" ), &( dwLargeFontSize ), CFontGen::FONTS_SIZE[3] );
}

void SMainFrameParams::SaveToRegistry( const std::string &rszRegistryKey, bool bShortApperence )
{
	CString strKey;
	std::string szFormat;
	::RegDeleteKey( HKEY_CURRENT_USER, rszRegistryKey.c_str() );
	CRegistrySection registrySection( HKEY_CURRENT_USER, KEY_WRITE, rszRegistryKey.c_str() );


	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_ELK_NAME );
	registrySection.SaveString( strKey, szLastOpenedELKName );

	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_PAK_NAME );
	registrySection.SaveString( strKey, szLastOpenedPAKName );

	strKey.LoadString( IDS_REGISTRY_KEY_LAST_OPENED_XLS_NAME );
	registrySection.SaveString( strKey, szLastOpenedXLSName );
	
	strKey.LoadString( IDS_REGISTRY_KEY_COLLAPSE_ITEM );
	registrySection.SaveNumber( strKey, _T( "%d" ), bCollapseItem );

	strKey.LoadString( IDS_REGISTRY_LAST_PATH );
	registrySection.SaveString( strKey, szLastPath );

	strKey.LoadString( IDS_REGISTRY_LAST_ELK_ELEMENT );
	registrySection.SaveNumber( strKey, _T( "%d" ), nLastELKElement );

	strKey.LoadString( IDS_REGISTRY_KEY_FULL_SCREEN );
	registrySection.SaveNumber( strKey, _T( "%d" ), bFullScreen );

	strKey.LoadString( IDS_REGISTRY_KEY_RECT );
	registrySection.SaveRect( strKey, _T( "%d" ), rect );

	strKey.LoadString( IDS_REGISTRY_KEY_CODE_PAGE );
	registrySection.SaveNumber( strKey, _T( "%d" ), nCodePage );

	int nRecentCount = recentList.size();
	strKey.LoadString( IDS_REGISTRY_KEY_RECENT_LIST );
	szFormat = NStr::Format( _T( "%ss" ), LPCTSTR( strKey ) );
	registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), nRecentCount );
	int nRecentIndex = 0;
	for ( std::list<std::string>::const_iterator recentIterator = recentList.begin(); recentIterator != recentList.end(); ++recentIterator )
	{
		std::string szFormat = NStr::Format( _T( "%s%d" ), LPCTSTR( strKey ), nRecentIndex );
		registrySection.SaveString( szFormat.c_str(), ( *recentIterator ) );
		++nRecentIndex;
	}

	{
		strKey.LoadString( IDS_REGISTRY_CURRENT_FILTER_NAME );
		registrySection.SaveString( strKey, szCurrentFilterName );

		int nFilterCount = filters.size();
		strKey.LoadString( IDS_REGISTRY_FILTER );
		szFormat = NStr::Format( _T( "%ss" ), LPCTSTR( strKey ) );
		registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), nFilterCount );
	
		int nFilterIndex = 0;
		for ( TFilterHashMap::const_iterator filtersIterator = filters.begin(); filtersIterator != filters.end(); ++filtersIterator )
		{
			szFormat = NStr::Format( _T( "%s%d_Name" ), LPCTSTR( strKey ), nFilterIndex );
			registrySection.SaveString( szFormat.c_str(), filtersIterator->first );

			szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_NOT_TRANSLATED] );
			registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), filtersIterator->second.states[SELKTextProperty::STATE_NOT_TRANSLATED] );

			szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_OUTDATED] );
			registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), filtersIterator->second.states[SELKTextProperty::STATE_OUTDATED] );

			szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_TRANSLATED] );
			registrySection.SaveNumber( szFormat.c_str(), ( "%d" ), filtersIterator->second.states[SELKTextProperty::STATE_TRANSLATED] );

			szFormat = NStr::Format( _T( "%s%d_%s" ), LPCTSTR( strKey ), nFilterIndex, SELKTextProperty::STATE_LABELS[SELKTextProperty::STATE_APPROVED] );
			registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), filtersIterator->second.states[SELKTextProperty::STATE_APPROVED] );
			
			szFormat = NStr::Format( _T( "%s%d_Changed" ), LPCTSTR( strKey ), nFilterIndex );
			registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), filtersIterator->second.bTranslated );

			int nFilterItemIndex = 0;
			for ( TSimpleFilter::const_iterator filterIterator = filtersIterator->second.filter.begin(); filterIterator != filtersIterator->second.filter.end(); ++filterIterator )
			{
				int nFilterItemStringIndex = 0;
				for ( TSimpleFilterItem::const_iterator filterItemIterator = filterIterator->begin(); filterItemIterator != filterIterator->end(); ++filterItemIterator )
				{
					szFormat = NStr::Format( _T( "%s%d_%d_%d" ), LPCTSTR( strKey ), nFilterIndex, nFilterItemIndex, nFilterItemStringIndex );
					registrySection.SaveString( szFormat.c_str(), ( *filterItemIterator ) );
					++nFilterItemStringIndex;
				}
				++nFilterItemIndex;
			}
			++nFilterIndex;
		}
	}

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_DOWN );
	registrySection.SaveNumber( strKey, _T( "%d" ), searchParam.bFindDown );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_MATCH_CASE );
	registrySection.SaveNumber( strKey, _T( "%d" ), searchParam.bFindMatchCase );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_WHOLE_WORD );
	registrySection.SaveNumber( strKey, _T( "%d" ), searchParam.bFindWholeWord );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_WINDOW );
	registrySection.SaveNumber( strKey, _T( "%d" ), searchParam.nWindowType );

	strKey.LoadString( IDS_REGISTRY_KEY_FIND_POSITION );
	registrySection.SaveNumber( strKey, _T( "%d" ), searchParam.nPosition );

	std::string szBufferFontName( strFontName );
	strKey.LoadString( IDS_REGISTRY_FONT_NAME );
	registrySection.SaveString( strKey, szBufferFontName );

	strKey.LoadString( IDS_REGISTRY_KEY_NORMAL_FONT_SIZE );
	registrySection.SaveNumber( strKey, _T( "%d" ), dwNormalFontSize );

	strKey.LoadString( IDS_REGISTRY_KEY_LARGE_FONT_SIZE );
	registrySection.SaveNumber( strKey, _T( "%d" ), dwLargeFontSize );
}

const SSimpleFilter* SMainFrameParams::GetCurrentFilter() const
{
	if ( !szCurrentFilterName.empty() )
	{
		TFilterHashMap::const_iterator filtersIterator = filters.find( szCurrentFilterName );
		if ( filtersIterator != filters.end() )
		{
			return &( filtersIterator->second );
		}
	}
	return 0;
}
