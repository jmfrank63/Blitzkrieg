#include "stdafx.h"

#include "Registry_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRegistrySection::CRegistrySection( HKEY hKey, REGSAM samDesired, LPCTSTR pszRegistrySection )
{
  LONG result = ERROR_SUCCESS;
  DWORD dwDisposition;
  result = ::RegCreateKeyEx( hKey,
														 pszRegistrySection,
														 0,
														 0,
														 REG_OPTION_NON_VOLATILE,
														 samDesired,
														 0,
														 &hRegistrySection,
														 &dwDisposition );
  if ( result != ERROR_SUCCESS )
  {
		hRegistrySection = 0;
	}
}

CRegistrySection::~CRegistrySection()
{
  if ( hRegistrySection != 0 )
	{
		::RegCloseKey( hRegistrySection );
		hRegistrySection = 0;
	}
}

LONG CRegistrySection::LoadString( LPCTSTR pszRegistryKey, std::string *pszLoadValue, const std::string &rszDefaultValue ) const
{
  if ( ( pszLoadValue != 0 ) && ( hRegistrySection != 0 ) )
	{
		( *pszLoadValue ) = rszDefaultValue;

		DWORD dwLoadValueType;
		DWORD dwLoadValueLength = 0xFFF;
		BYTE pBuffer[0xFFF];
		LONG result = ERROR_SUCCESS;
		result = ::RegQueryValueEx( hRegistrySection,
																pszRegistryKey,
																0,
																&dwLoadValueType,
																pBuffer,
																&dwLoadValueLength );
		if( ( result != ERROR_SUCCESS ) || ( dwLoadValueType != REG_SZ ) )
		{
			( *pszLoadValue ) = rszDefaultValue;
		}
		else
		{
			( *pszLoadValue ) = std::string( reinterpret_cast<LPCTSTR>( pBuffer ) );
		}
		return result;
	}
	else
	{
		return ERROR_INVALID_PARAMETER;
	}
}

LONG CRegistrySection::SaveString( LPCTSTR pszRegistryKey, const std::string &szSaveValue ) const
{
	return ::RegSetValueEx( hRegistrySection,
													pszRegistryKey,
													0,
													REG_SZ,
													reinterpret_cast<const BYTE*>( szSaveValue.c_str() ),
													szSaveValue.size() + 1 );
}
