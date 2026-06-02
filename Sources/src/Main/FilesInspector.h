#ifndef __FILESINSPECTOR_H__
#define __FILESINSPECTOR_H__
#pragma ONCE
#include "iMain.h"
class CFilesInspector : public CTRefCount<IFilesInspector>
{
	OBJECT_SERVICE_METHODS( CFilesInspector );
	DECLARE_SERIALIZE;
	typedef std::pair< std::string, CObj<IFilesInspectorEntry> > SEntry;
	typedef std::list<SEntry> CEntriesList;
	CEntriesList entries;
public:
	bool STDCALL AddEntry( const std::string &szName, IFilesInspectorEntry *pEntry );
	bool STDCALL RemoveEntry( const std::string &szName );
	IFilesInspectorEntry* STDCALL GetEntry( const std::string &szName );
	bool STDCALL InspectStorage( IDataStorage *pStorage );
	void STDCALL Clear();
};
class CFilesInspectorEntryGDB : public CTRefCount<IFilesInspectorEntry>
{
	OBJECT_SERVICE_METHODS( CFilesInspectorEntryGDB );
public:
	void STDCALL InspectStream( const std::string &szName ) {  }
	void STDCALL Clear() {  }
};
class CFilesInspectorEntryCollector : public CTRefCount<IFilesInspectorEntryCollector>
{
	OBJECT_SERVICE_METHODS( CFilesInspectorEntryCollector );
	DECLARE_SERIALIZE;
	std::vector<std::string> szNames;			// collected names
	std::string szPrefix;									// prefix name
	std::string szSuffix;									// suffix name
	typedef void (CFilesInspectorEntryCollector::*ADD_IF_MATCHED)( const std::string &szName );
	ADD_IF_MATCHED pfnAddIfMatched;				// match function
	int nMatchType;												// match function type
	void AddIfPrefixMatched( const std::string &szName );
	void AddIfSuffixMatched( const std::string &szName );
	void AddIfBothMatched( const std::string &szName );
	void SetMatchFunction( const int _nMatchType );
public:
	CFilesInspectorEntryCollector()
		: pfnAddIfMatched( 0 ), nMatchType( -1 ) {  }
	void STDCALL Configure( const char *pszConfig );
	const std::vector<std::string>& STDCALL GetCollected() const { return szNames; }
	void STDCALL InspectStream( const std::string &szName );
	void STDCALL Clear() { szNames.clear(); }
};
#endif // __FILESINSPECTOR_H__
