#include "StdAfx.h"

#include "FilesInspector.h"

namespace
{
	void NormalizeSeparators( std::string *pPath )
	{
		for ( int i = 0; i < pPath->size(); ++i )
		{
			if ( (*pPath)[i] == '/' )
				(*pPath)[i] = '\\';
		}
	}
}

bool CFilesInspector::AddEntry( const std::string &szName, IFilesInspectorEntry *pEntry )
{
	if ( GetEntry(szName) != 0 ) 
	{
		NI_ASSERT_SLOW_TF( GetEntry(szName) == 0, NStr::Format("Can't add entry \"%s\" - one already exist", szName.c_str()), return false );
		return false;
	}
	entries.push_back( SEntry(szName, pEntry) );
	return true;
}
bool CFilesInspector::RemoveEntry( const std::string &szName )
{
	for ( CEntriesList::iterator it = entries.begin(); it != entries.end(); ++it )
	{
		if ( it->first == szName ) 
		{
			entries.erase( it );
			return true;
		}
	}
	return false;
}
IFilesInspectorEntry* CFilesInspector::GetEntry( const std::string &szName )
{
	for ( CEntriesList::iterator it = entries.begin(); it != entries.end(); ++it )
	{
		if ( it->first == szName ) 
			return it->second;
	}
	return 0;
}
bool CFilesInspector::InspectStorage( IDataStorage *pStorage )
{
	CPtr<IStorageEnumerator> pEnumerator = pStorage->CreateEnumerator();
	pEnumerator->Reset( "*.*" );
	while (	pEnumerator->Next() )
	{
		const SStorageElementStats *pStats = pEnumerator->GetStats();
		if ( pStats->type != SET_STREAM ) 
			continue;
		const std::string szName = pStats->pszName;
		for ( CEntriesList::iterator it = entries.begin(); it != entries.end(); ++it )
			it->second->InspectStream( szName );
	}
	return true;
}
void CFilesInspector::Clear()
{
	for ( CEntriesList::iterator it = entries.begin(); it != entries.end(); ++it )
		it->second->Clear();
}
int CFilesInspector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &entries );
	return 0;
}
void CFilesInspectorEntryCollector::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	std::string szConfig = pszConfig;
	NStr::ToLower( szConfig );
	std::vector<std::string> szStrings;
	NStr::SplitString( szConfig, szStrings, ';' );
	szPrefix.clear();
	szSuffix.clear();
	if ( szStrings.size() > 0 ) 
		szPrefix = szStrings[0];
	if ( szStrings.size() > 1 ) 
		szSuffix = szStrings[1];
	if ( !szPrefix.empty() && !szSuffix.empty() ) 
		nMatchType = 0;
	else if ( !szPrefix.empty() ) 
		nMatchType = 1;
	else if ( !szSuffix.empty() ) 
		nMatchType = 2;
	SetMatchFunction( nMatchType );
}
void CFilesInspectorEntryCollector::SetMatchFunction( const int _nMatchType )
{
	switch ( _nMatchType ) 
	{
		case 0:
			pfnAddIfMatched = &CFilesInspectorEntryCollector::AddIfBothMatched;
			break;
		case 1:
			pfnAddIfMatched = &CFilesInspectorEntryCollector::AddIfPrefixMatched;
			break;
		case 2:
			pfnAddIfMatched = &CFilesInspectorEntryCollector::AddIfSuffixMatched;
			break;
		default:
			pfnAddIfMatched = 0;
	}
	NI_ASSERT_SLOW_T( pfnAddIfMatched != 0, NStr::Format("Can't set match function of type %d", _nMatchType) );
}
void CFilesInspectorEntryCollector::AddIfPrefixMatched( const std::string &szName )
{
	std::string lowerName = szName;
	NStr::ToLower( lowerName );
	NormalizeSeparators( &lowerName );
	if ( lowerName.find( szPrefix ) != std::string::npos )
		szNames.push_back( szName );
}
void CFilesInspectorEntryCollector::AddIfSuffixMatched( const std::string &szName )
{
	std::string lowerName = szName;
	NStr::ToLower( lowerName );
	NormalizeSeparators( &lowerName );
	if ( lowerName.compare( lowerName.size() - szSuffix.size(), szSuffix.size(), szSuffix ) == 0 )
		szNames.push_back( szName );
}
void CFilesInspectorEntryCollector::AddIfBothMatched( const std::string &szName )
{
	std::string lowerName = szName;
	NStr::ToLower( lowerName );
	NormalizeSeparators( &lowerName );
	if ( (lowerName.find( szPrefix ) != std::string::npos) &&
	     (lowerName.compare( lowerName.size() - szSuffix.size(), szSuffix.size(), szSuffix ) == 0) )
		szNames.push_back( szName );
}
void CFilesInspectorEntryCollector::InspectStream( const std::string &szName )
{
	(this->*pfnAddIfMatched)( szName );
}
int CFilesInspectorEntryCollector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szNames );
	saver.Add( 2, &szPrefix );
	saver.Add( 3, &szSuffix );
	saver.Add( 4, &nMatchType );
	if ( saver.IsReading() ) 
		SetMatchFunction( nMatchType );
	return 0;
}
