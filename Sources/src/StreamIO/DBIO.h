#ifndef __DBIO_H__
#define __DBIO_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETableAccessMode
{
	TABLE_ACCESS_READ  = 0x00000001,
	TABLE_ACCESS_WRITE = 0x00000002,
	TABLE_ACCESS_SHARE = 0x00000004,
	TABLE_ACCESS_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EDataBaseType
{
	DB_TYPE_INI = 1,
	DB_TYPE_REG = 2,
	DB_TYPE_MDB = 3,
	DB_TYPE_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IDataTable : public IRefCount
{
	// получить имена строк таблицы. каждое имя заканчивается на '\0', с строка в целом на '\0\0'
	virtual int STDCALL GetRowNames( char *pszBuffer, int nBufferSize ) = 0;
	// получить имена колонок таблицы в данной строке. каждое имя заканчивается на '\0', с строка в целом на '\0\0'
	virtual int STDCALL GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize ) = 0;
	// очистка секции
	virtual void STDCALL ClearRow( const char *pszRowName ) = 0;
	// complete element access
	// get
	virtual int STDCALL GetInt( const char *pszRow, const char *pszEntry, int defval ) = 0;
	virtual double STDCALL GetDouble( const char *pszRow, const char *pszEntry, double defval ) = 0;
	virtual const char* STDCALL GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize ) = 0;
	virtual int STDCALL GetRawData( const char *pszRow, const char *pszEntry, void *pBuffer, int nBufferSize ) = 0;
	// set
	virtual void STDCALL SetInt( const char *pszRow, const char *pszEntry, int val ) = 0;
	virtual void STDCALL SetDouble( const char *pszRow, const char *pszEntry, double val ) = 0;
	virtual void STDCALL SetString( const char *pszRow, const char *pszEntry, const char *val ) = 0;
	virtual void STDCALL SetRawData( const char *pszRow, const char *pszEntry, const void *pBuffer, int nBufferSize ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IDataBase : public IRefCount
{
	// создать и открыть таблицу с указанным именем и правами доступа
	virtual IDataTable* STDCALL CreateTable( const char *pszName, DWORD dwAccessMode ) = 0;
	// открыть существующую таблицу с указанным именем и правами доступа
	virtual IDataTable* STDCALL OpenTable( const char *pszName, DWORD dwAccessMode ) = 0;
	// убить элемент хранилища
	virtual bool STDCALL DestroyElement( const char *pszName ) = 0;
	// переименовать элемент
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** table accessor helper class for easy and intuitive use of the data tables
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DEFAULT_SEPARATOR ','
class CTableAccessor
{
	CPtr<IDataTable> pTable;
	//
	template <class TFunc, class TYPE>
		TYPE GetVal( TFunc func, const char *pszRow, const char *pszEntry, TYPE defval )
	{
		return (TYPE)(pTable->*func)( pszRow, pszEntry, defval );
	}
	template <class TFunc, class TYPE>
		void SetVal( TFunc func, const char *pszRow, const char *pszEntry, TYPE val )
	{
		(pTable->*func)( pszRow, pszEntry, val );
	}
	template <class TFunc, class TYPE>
		bool GetValArray( TFunc func, const char *pszRow, const char *pszEntry, std::vector<TYPE> &array, const char cSeparator = DEFAULT_SEPARATOR )
	{
		std::vector<std::string> strings;
		if ( !GetArray(pszRow, pszEntry, strings, cSeparator) )
			return false;
		array.resize( strings.size() );
		for ( int i=0; i<strings.size(); ++i )
			array[i] = (TYPE)(*func)( strings[i].c_str() );
		return true;
	}
	template <class TYPE>
		void SetValArray( const char *pszFormat, const char *pszRow, const char *pszEntry, std::vector<TYPE> &array, const char cSeparator = DEFAULT_SEPARATOR )
	{
		std::vector<std::string> strings( array.size() );
		for ( int i=0; i<array.size(); ++i )
			strings[i] = NStr::Format( pszFormat, array[i] );
		SetArray( pszRow, pszEntry, strings, cSeparator );
	}
	void ParseNames( const char *pszNames, int nSize, std::vector<std::string> &szNames )
	{
		const char *pos = pszNames;
		while ( (*pos != 0) && (pos - pszNames <= nSize) )
		{
			szNames.push_back( pos );
			pos = std::find( pos, pszNames + nSize, '\0' ) + 1;
		}
	}
public:
	CTableAccessor() {  }
	CTableAccessor( const CTableAccessor &accessor ) : pTable( accessor.pTable ) {  }
	CTableAccessor( IDataTable *_pTable ) : pTable( _pTable ) {  }
	CTableAccessor( IDataBase *pDB, const char *pszName, DWORD dwAccessMode = TABLE_ACCESS_READ )
		: pTable( pDB->OpenTable( pszName, dwAccessMode ) ) {  }
	// assigning and extracting
	const CTableAccessor& operator=( IDataTable *_pTable ) { pTable = _pTable; return *this; }
	const CTableAccessor& operator=( const CTableAccessor &accessor ) { pTable = accessor.pTable; return *this; }
	operator IDataTable*() const { return pTable; }
	IDataTable* operator->() const { return pTable; }
	// comparison operators
	bool operator==( const CTableAccessor &ptr ) const { return ( pTable == ptr.pTable ); }
	bool operator==( const IDataTable *pNewObject ) const { return ( pTable == pNewObject ); }
	bool operator!=( const CTableAccessor &ptr ) const { return ( pTable != ptr.pTable ); }
	bool operator!=( const IDataTable *pNewObject ) const { return ( pTable != pNewObject ); }
	//
	//
	bool GetRowNames( std::vector<std::string> &szNames )
	{
		char buffer[65536];
		szNames.clear();
		int nSize = pTable->GetRowNames( buffer, 65536 );
		if ( nSize <= 1 )
			return false;
		ParseNames( buffer, nSize, szNames );
		return true;
	}
	bool GetEntryNames( const char *pszRow, std::vector<std::string> &szNames )
	{
		char buffer[65536];
		szNames.clear();
		int nSize = pTable->GetEntryNames( pszRow, buffer, 65536 );
		if ( nSize <= 1 )
			return false;
		ParseNames( buffer, nSize, szNames );
		return true;
	}
	//
	// read simply data
	char GetChar( const char *pszRow, const char *pszEntry, char defval );
	unsigned char GetUChar( const char *pszRow, const char *pszEntry, unsigned char defval );
	short GetShort( const char *pszRow, const char *pszEntry, short defval );
	unsigned short GetUShort( const char *pszRow, const char *pszEntry, unsigned short defval );
	long GetLong( const char *pszRow, const char *pszEntry, long defval );
	unsigned long GetULong( const char *pszRow, const char *pszEntry, unsigned long defval );
	int GetInt( const char *pszRow, const char *pszEntry, int defval );
	unsigned int GetUInt( const char *pszRow, const char *pszEntry, unsigned int defval );
	float GetFloat( const char *pszRow, const char *pszEntry, float defval );
	double GetDouble( const char *pszRow, const char *pszEntry, double defval );
	std::string GetString( const char *pszRow, const char *pszEntry, const char *defval );
	void GetString( const char *pszRow, const char *pszEntry, const char *defval, std::string &szString );
	//
	// read array data
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<std::string> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<char> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned char> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<short> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned short> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<long> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned long> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<int> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned int> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<float> &array, const char cSeparator = DEFAULT_SEPARATOR );
	bool GetArray( const char *pszRow, const char *pszEntry, std::vector<double> &array, const char cSeparator = DEFAULT_SEPARATOR );
	//
	// write simply data
	void SetChar( const char *pszRow, const char *pszEntry, char val );
	void SetUChar( const char *pszRow, const char *pszEntry, unsigned char val );
	void SetShort( const char *pszRow, const char *pszEntry, short val );
	void SetUShort( const char *pszRow, const char *pszEntry, unsigned short val );
	void SetLong( const char *pszRow, const char *pszEntry, long val );
	void SetULong( const char *pszRow, const char *pszEntry, unsigned long val );
	void SetInt( const char *pszRow, const char *pszEntry, int val );
	void SetUInt( const char *pszRow, const char *pszEntry, unsigned int val );
	void SetFloat( const char *pszRow, const char *pszEntry, float val );
	void SetDouble( const char *pszRow, const char *pszEntry, double val );
	void SetString( const char *pszRow, const char *pszEntry, const char *val );
	void SetString( const char *pszRow, const char *pszEntry, const std::string &val );
	//
	// write array data
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<std::string> &array, const char cSeparator = DEFAULT_SEPARATOR  );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<char> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned char> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<short> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned short> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<long> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned long> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<int> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned int> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<float> &array, const char cSeparator = DEFAULT_SEPARATOR );
	void SetArray( const char *pszRow, const char *pszEntry, std::vector<double> &array, const char cSeparator = DEFAULT_SEPARATOR );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline char CTableAccessor::GetChar( const char *pszRow, const char *pszEntry, char defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline unsigned char CTableAccessor::GetUChar( const char *pszRow, const char *pszEntry, unsigned char defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline short CTableAccessor::GetShort( const char *pszRow, const char *pszEntry, short defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline unsigned short CTableAccessor::GetUShort( const char *pszRow, const char *pszEntry, unsigned short defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline long CTableAccessor::GetLong( const char *pszRow, const char *pszEntry, long defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline unsigned long CTableAccessor::GetULong( const char *pszRow, const char *pszEntry, unsigned long defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline int CTableAccessor::GetInt( const char *pszRow, const char *pszEntry, int defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline unsigned int CTableAccessor::GetUInt( const char *pszRow, const char *pszEntry, unsigned int defval )
{
	return GetVal( &IDataTable::GetInt, pszRow, pszEntry, defval );
}
inline float CTableAccessor::GetFloat( const char *pszRow, const char *pszEntry, float defval )
{
	return GetVal( &IDataTable::GetDouble, pszRow, pszEntry, defval );
}
inline double CTableAccessor::GetDouble( const char *pszRow, const char *pszEntry, double defval )
{
	return GetVal( &IDataTable::GetDouble, pszRow, pszEntry, defval );
}
inline std::string CTableAccessor::GetString( const char *pszRow, const char *pszEntry, const char *defval )
{
	char buff[1024];
	pTable->GetString( pszRow, pszEntry, defval, buff, 1024 );
	return buff;
}
inline void CTableAccessor::GetString( const char *pszRow, const char *pszEntry, const char *defval, std::string &szString )
{
	char buff[1024];
	pTable->GetString( pszRow, pszEntry, defval, buff, 1024 );
	szString = buff;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<std::string> &array, const char cSeparator )
{
	std::string szString;
	array.clear();
	GetString( pszRow, pszEntry, "", szString );
	if ( szString.empty() )
		return false;
	NStr::SplitStringWithMultipleBrackets( szString, array, cSeparator );
	return true;
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<char> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned char> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<short> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned short> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<long> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned long> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<int> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned int> &array, const char cSeparator )
{
	return GetValArray( atoi, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<float> &array, const char cSeparator )
{
	return GetValArray( atof, pszRow, pszEntry, array, cSeparator );
}
inline bool CTableAccessor::GetArray( const char *pszRow, const char *pszEntry, std::vector<double> &array, const char cSeparator )
{
	return GetValArray( atof, pszRow, pszEntry, array, cSeparator );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CTableAccessor::SetChar( const char *pszRow, const char *pszEntry, char val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetUChar( const char *pszRow, const char *pszEntry, unsigned char val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetShort( const char *pszRow, const char *pszEntry, short val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetUShort( const char *pszRow, const char *pszEntry, unsigned short val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetLong( const char *pszRow, const char *pszEntry, long val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetULong ( const char *pszRow, const char *pszEntry, unsigned long val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetInt( const char *pszRow, const char *pszEntry, int val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetUInt( const char *pszRow, const char *pszEntry, unsigned int val )
{
	SetVal( &IDataTable::SetInt, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetFloat( const char *pszRow, const char *pszEntry, float val )
{
	SetVal( &IDataTable::SetDouble, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetDouble( const char *pszRow, const char *pszEntry, double val )
{
	SetVal( &IDataTable::SetDouble, pszRow, pszEntry, val );
}
inline void CTableAccessor::SetString( const char *pszRow, const char *pszEntry, const char *val )
{
	pTable->SetString( pszRow, pszEntry, val );
}
inline void CTableAccessor::SetString( const char *pszRow, const char *pszEntry, const std::string &val )
{
	pTable->SetString( pszRow, pszEntry, val.c_str() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<std::string> &array, const char cSeparator )
{
	if ( array.empty() )
		return;
	std::string szString;
	for ( int i=0; i<array.size() - 1; ++i )
		szString += array[i] + cSeparator;
	szString += array[array.size() - 1];
	SetString( pszRow, pszEntry, szString );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<char> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned char> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<short> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned short> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<long> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned long> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<int> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<unsigned int> &array, const char cSeparator )
{
	SetValArray( "%d", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<float> &array, const char cSeparator )
{
	SetValArray( "%g", pszRow, pszEntry, array, cSeparator );
}
inline void CTableAccessor::SetArray( const char *pszRow, const char *pszEntry, std::vector<double> &array, const char cSeparator )
{
	SetValArray( "%g", pszRow, pszEntry, array, cSeparator );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DBIO_H__
