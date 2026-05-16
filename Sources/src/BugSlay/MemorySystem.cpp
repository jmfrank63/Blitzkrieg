#include "StdAfx.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bInternal = false;
static const int nNumCallstackEntries = 20;
struct SAllocInfo
{
	DWORD dwAddresses[nNumCallstackEntries];
	int nSize;
};
typedef std::unordered_map<void*, SAllocInfo, SDefaultPtrHash> CAllocsInfoMap;
typedef std::unordered_map<DWORD, bool> CIgnoredMap;
typedef std::list<std::string> CIgnoredPathList;
static CAllocsInfoMap *pAllocs = 0;
static CIgnoredMap *pIgnored = 0;
static CIgnoredPathList *pIgnoredPathes = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMemTools
{
	inline int MSVCMustDie_tolower( int a ) { return tolower(a); } 
	inline int MSVCMustDie_toupper( int a ) { return toupper(a); }
	void ToLower( std::string &szString ) 
	{ 
		std::transform( szString.begin(), szString.end(), szString.begin(), MSVCMustDie_tolower ); 
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// special class for 'bInternal' true/false setting
class CInternalGuard
{
	bool &bInternal;
public:
	CInternalGuard( bool &_bInternal ) : bInternal( _bInternal ) { bInternal = true; }
	~CInternalGuard() { bInternal = false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL NBugSlayer::MemSystemAddIgnoredPath( const char *pszPath )
{
	if ( bInternal )
		return;
	CInternalGuard internal( bInternal );
	if ( pIgnoredPathes == 0 )
		pIgnoredPathes = new CIgnoredPathList;
	std::string szPath = pszPath;
	NMemTools::ToLower( szPath );
	//NStr::ToLower( szPath );
	for ( CIgnoredPathList::const_iterator it = pIgnoredPathes->begin(); it != pIgnoredPathes->end(); ++it )
	{
		// don't duplicate pathes
		if ( (*it) == szPath )
			return;
	}
	pIgnoredPathes->push_back( szPath );
}
struct SMemorySystemAutomatic
{
	SMemorySystemAutomatic() 
	{ 
		NBugSlayer::MemSystemAddIgnoredPath( "D:\\Program Files\\" ); 
		NBugSlayer::MemSystemAddIgnoredPath( "C:\\Program Files\\" ); 
		NBugSlayer::MemSystemAddIgnoredPath( "\\MemorySystem.cpp" );
		NBugSlayer::MemSystemAddIgnoredPath( "\\BasicShare.h" );
	}
	~SMemorySystemAutomatic()
	{
		if ( (pAllocs != 0) || (pIgnored != 0) )
		{
			OutputDebugString( "Memory tracking system still have allocated memory. forsing deallocation\n" );
			//NBugSlayer::DumpMemoryStats();
		}
		if ( pAllocs )
			delete pAllocs;
		pAllocs = 0;
		if ( pIgnored )
			delete pIgnored;
		pIgnored = 0;
		if ( pIgnoredPathes )
			delete pIgnoredPathes;
		pIgnoredPathes = 0;
	}
};
static SMemorySystemAutomatic init;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsFileIgnored( const char *pszFileName )
{
	std::string szFileName = pszFileName;
	NMemTools::ToLower( szFileName );
	
	//NStr::ToLower( szFileName );
	// check ignored pathes list for this filename
	if ( pIgnoredPathes != 0 )
	{
		for ( CIgnoredPathList::const_iterator it = pIgnoredPathes->begin(); it != pIgnoredPathes->end(); ++it )
		{
			std::string szPath = *it;
			if ( szFileName.find( *it ) != std::string::npos )
				return true;
			/*
			if ( it->compare(0, it->size(), pszFileName, it->size()) == 0 )
				return true;
			*/
		}
	}
	// also, we don't need to capture this source file
	return _stricmp( __FILE__, pszFileName ) == 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// проверить, удовлетворяет ли этот адрес возврата нашим требованиям (т.е. не находится ли он в игнорируемом файле)
static bool IsAddressFits( DWORD dwAddress )
{
	if ( pIgnored == 0 )
		pIgnored = new CIgnoredMap;
	std::unordered_map<DWORD, bool>::iterator i = pIgnored->find( dwAddress );
	if ( i == pIgnored->end() )
	{
		const char *pszFileName = 0;
		int nSourceLine;
		if ( !GetSourceLine( dwAddress, pszFileName, nSourceLine ) || (pszFileName == 0) )
		{
			(*pIgnored)[dwAddress] = false;
			return false;
		}
		// analyze source file
		return (*pIgnored)[dwAddress] = !IsFileIgnored( pszFileName );
	}
	return i->second;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL NBugSlayer::MemSystemRegister( size_t nSize, void *ptr )
{
	if ( bInternal )
		return;
	CInternalGuard internal( bInternal );
	if ( pAllocs == 0 )
		pAllocs = new CAllocsInfoMap;
	//
	SAllocInfo &info = (*pAllocs)[ptr];
	info.nSize = nSize;
	GET_CALLSTACK_ADDRS( info.dwAddresses, nNumCallstackEntries );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL NBugSlayer::MemSystemFree( void *ptr )
{
	if ( bInternal )
		return;
	CInternalGuard internal( bInternal );
	CAllocsInfoMap::iterator it = pAllocs->find( ptr );
	NI_ASSERT_T( it != pAllocs->end(), "Deallocating memory, which was allocated in unknown place and not registered" );
	if ( it != pAllocs->end() )
		pAllocs->erase( it );
	else
		assert( 0 );
	if ( pAllocs->empty() )
	{
		delete pAllocs;
		if ( pIgnored != 0 )
			delete pIgnored;
		if ( pIgnoredPathes != 0 )
			delete pIgnoredPathes;
		pAllocs = 0;
		pIgnored = 0;
		pIgnoredPathes = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAllocStats
{
	int nMax, nTotal, nNumber;
	SAllocStats() : nMax( 0 ), nTotal( 0 ), nNumber( 0 ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::pair<std::string, int> SFileLocationKey;
struct SFileLocationHashFunc
{
	int operator()( const SFileLocationKey &key ) const
	{
		std::hash<std::string> HashFunc;
		return HashFunc( key.first ) + key.second;
	}
};
struct SFileLocationEqual
{
	bool operator()( const SFileLocationKey &key1, const SFileLocationKey &key2 ) const
	{
		return (key1.first == key2.first) && (key1.second == key2.second);
	}
};
typedef std::unordered_map<SFileLocationKey, SAllocStats, SFileLocationHashFunc, SFileLocationEqual> CAllocStatsMap;
typedef std::pair<SFileLocationKey, SAllocStats> CMemStats;
typedef std::list<CMemStats> CMemStatsList;

struct SMemStatsSortFunctional
{
	bool operator()( const CMemStats &s1, const CMemStats &s2 ) const
	{
		if ( s1.second.nTotal == s2.second.nTotal )
		{
			if ( s1.second.nMax == s2.second.nMax )
				return s1.first.first > s1.first.first;
			else
				return s1.second.nMax > s2.second.nMax;
		}
		else
			return s1.second.nTotal > s2.second.nTotal;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL NBugSlayer::MemSystemDumpStats()
{
	if ( pAllocs == 0 )
		return;
	CInternalGuard internal( bInternal );
	CAllocStatsMap stats;
	OutputDebugString( "***********************************************   Memory dump begin   ********************************************************\n" );
	// form stats
	int nTotal = 0;
	char szBuf[1024];
	for ( CAllocsInfoMap::const_iterator it = pAllocs->begin(); it != pAllocs->end(); ++it )
	{
		const SAllocInfo &info = it->second;
		for ( int i=0; i<nNumCallstackEntries; ++i )
		{
			if ( info.dwAddresses[i] == 0 )
				break;
			if ( IsAddressFits( info.dwAddresses[i] ) )
			{
				const char *pszFileName = 0;
				int nLine = 0;
				GetSourceLine( info.dwAddresses[i], pszFileName, nLine );
				//
				if ( pszFileName == 0 )
				{
					sprintf_s( szBuf, sizeof(szBuf), "unknown: block = %d\n", info.nSize );
					OutputDebugString( szBuf );
					nTotal += info.nSize;
				}
				else
				{
					SAllocStats &stat = stats[ SFileLocationKey(pszFileName, nLine) ];
					stat.nMax = Max( stat.nMax, info.nSize );
					stat.nTotal += info.nSize;
					stat.nNumber += 1;
				}
				break;
			}
		}
	}
	// trace stats
	CMemStatsList sortedstats;
	for ( CAllocStatsMap::const_iterator it = stats.begin(); it != stats.end(); ++it )
		sortedstats.push_back( CMemStats(it->first, it->second) );
	sortedstats.sort( SMemStatsSortFunctional() );
	//
	for ( CMemStatsList::const_iterator it = sortedstats.begin(); it != sortedstats.end(); ++it )
	{
		sprintf_s( szBuf, sizeof(szBuf), "%s(%d): max block = %d, block(s) = %d, total bytes = %d\n", 
			it->first.first.c_str(), it->first.second, it->second.nMax, it->second.nNumber, it->second.nTotal );
		OutputDebugString( szBuf );
		nTotal += it->second.nTotal;
	}
	//
	sprintf_s( szBuf, sizeof(szBuf), "total allocated %d bytes\n", nTotal );
	OutputDebugString( szBuf );
	OutputDebugString( "***********************************************   Memory dump ends    ********************************************************\n" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
