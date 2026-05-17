#ifndef __INIFILE_H__
#define __INIFILE_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETableAccessMode
{
	TABLE_ACCESS_READ  = 0x00000001,
	TABLE_ACCESS_WRITE = 0x00000002,
	TABLE_ACCESS_SHARE = 0x00000004,
	TABLE_ACCESS_FORCE_DWORD = 0x7fffffff
};
struct SEntry
{
	typedef std::string TKey;
	typedef std::string TVal;
	TKey key;
	TVal val;
	//
	SEntry() {  }
	SEntry( TKey &_key, TVal &_val ) : key( _key ), val( _val ) {  }
	//
	SEntry& operator=( const SEntry &entry ) { key = entry.key; val = entry.val; return *this; }
	SEntry& operator=( const TVal &_val ) { val = _val; return *this; }
};
struct SRow
{
	typedef std::string TKey;
	typedef SEntry TVal;
	typedef std::list<TVal> CValList;
	typedef std::unordered_map<TVal::TKey, TVal*> CValMap;
	TKey key;
	CValList elist;
	CValMap emap;
	//
	TVal& operator[]( const TVal::TKey &key )
	{
		CValMap::iterator pos = emap.find( key );
		if ( pos != emap.end() )
			return *( pos->second );
		else
		{
			elist.push_back( TVal() );
			elist.back().key = key;
			emap[key] = &( elist.back() );
			return elist.back();
		}
	}
	//
	CValMap::iterator find( const TVal::TKey &key ) { return emap.find( key ); }
	CValMap::const_iterator find( const TVal::TKey &key ) const { return emap.find( key ); }
};
struct STable
{
	typedef std::string TKey;
	typedef SRow TVal;
	typedef std::list<TVal> CValList;
	typedef std::unordered_map<TVal::TKey, TVal*> CValMap;
	TKey key;
	CValList elist;
	CValMap emap;
	//
	TVal& operator[]( const TVal::TKey &key )
	{
		CValMap::iterator pos = emap.find( key );
		if ( pos != emap.end() )
			return *( pos->second );
		else
		{
			elist.push_back( TVal() );
			elist.back().key = key;
			emap[key] = &( elist.back() );
			return elist.back();
		}
	}
	//
	CValMap::iterator find( const TVal::TKey &key ) { return emap.find( key ); }
	CValMap::const_iterator find( const TVal::TKey &key ) const { return emap.find( key ); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SIniFileEntry
{
	int nOrder;
	std::string szData;
};
class CIniFile //: public IDataTable
{
	//OBJECT_MINIMAL_METHODS( CIniFile );
	DWORD dwAccessMode;
	//
	STable table;
	bool bChanged;
	//
	bool CanWrite() const { return dwAccessMode & TABLE_ACCESS_WRITE; }
	bool CanRead() const { return dwAccessMode & TABLE_ACCESS_READ; }
	const SRow* GetRow( const char *pszName ) const
	{
		STable::CValMap::const_iterator pos = table.find( pszName );
		return pos == table.emap.end() ? 0 : pos->second;
	}
	SRow* GetRow( const char *pszName )
	{
		STable::CValMap::const_iterator pos = table.find( pszName );
		return pos == table.emap.end() ? 0 : pos->second;
	}
	SEntry* GetEntry( const char *pszRow, const char *pszName )
	{
		SRow *pRow = GetRow( pszRow );
		if ( !pRow )
			return 0;
		SRow::CValMap::iterator pos = pRow->find( pszName );
		return pos == pRow->emap.end() ? 0 : pos->second;
	}
	const SEntry* GetEntry( const char *pszRow, const char *pszName ) const
	{
		const SRow *pRow = GetRow( pszRow );
		if ( !pRow )
			return 0;
		SRow::CValMap::const_iterator pos = pRow->find( pszName );
		return pos == pRow->emap.end() ? 0 : pos->second;
	}
public:
	CIniFile() : bChanged( false ) { }
	//
	bool Open( const std::vector<BYTE> &rData, DWORD dwAccessMode );
	//
	int STDCALL GetRowNames( char *pszBuffer, int nBufferSize );
	int STDCALL GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize );
	
	// ������� ������
	void STDCALL ClearRow( const char *pszRowName )
	{
		if ( SRow *pRow = GetRow( pszRowName ) )
		{
			pRow->emap.clear();
			pRow->elist.clear();
		}
	}
	// get
	int STDCALL GetInt( const char *pszRow, const char *pszEntry, int defval );
	double STDCALL GetDouble( const char *pszRow, const char *pszEntry, double defval );
	const char* STDCALL GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize );

	void STDCALL SetInt( const char *pszRow, const char *pszEntry, int val );
	void STDCALL SetDouble( const char *pszRow, const char *pszEntry, double val );
	void STDCALL SetString( const char *pszRow, const char *pszEntry, const char *val );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __INIFILE_H__
