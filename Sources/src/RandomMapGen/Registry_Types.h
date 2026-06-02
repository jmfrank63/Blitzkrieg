#if !defined(__Registry__Types__)
#define __Registry__Types__

class CRegistrySection
{
  protected:
	HKEY hRegistrySection;

  public:
  CRegistrySection( HKEY hKey, REGSAM samDesired, LPCTSTR pszRegistrySection );
  ~CRegistrySection();

	LONG LoadString( LPCTSTR pszRegistryKey, std::string *pszLoadValue, const std::string &rszDefaultValue ) const;
  LONG SaveString( LPCTSTR pszRegistryKey, const std::string &szSaveValue ) const;
 
	bool IsValid() { return ( hRegistrySection != 0 ); }
	
	template<class Type>
	LONG LoadNumber( LPCTSTR pszRegistryKey, LPCTSTR pszMask, Type *pLoadValue, const Type rDefaultValue ) const
	{
		if ( pLoadValue != 0 )
		{
			( *pLoadValue ) = rDefaultValue;
			
			std::string szBuffer;
			LONG result = LoadString( pszRegistryKey, &szBuffer, "" );
			if ( ( result == ERROR_SUCCESS ) && ( !szBuffer.empty() ) )
			{
				if ( sscanf( szBuffer.c_str(), pszMask, pLoadValue ) < 1 )
				{
					( *pLoadValue ) = rDefaultValue;
					result = ERROR_INVALID_DATA;
				}
			}
			return result;
		}
		else
		{
			return ERROR_INVALID_PARAMETER;
		}
	}

	template<class Type>
  LONG SaveNumber( LPCTSTR pszRegistryKey, LPCTSTR pszMask, const Type rSaveValue ) const
	{
		std::string szBuffer = NStr::Format( pszMask, rSaveValue );
		return SaveString( pszRegistryKey, szBuffer );
	}

	template<class Type>
	LONG LoadRect( LPCTSTR pszRegistryKey, LPCTSTR pszMask, CTRect<Type> *pLoadValue, const CTRect<Type> &rDefaultValue ) const
	{
		if ( pLoadValue != 0 )
		{
			( *pLoadValue ) = rDefaultValue;
			
			std::string szBuffer;
			LONG result = LoadString( pszRegistryKey, &szBuffer, "" );
			if ( ( result == ERROR_SUCCESS ) && ( !szBuffer.empty() ) )
			{
				if ( sscanf( szBuffer.c_str(),
										 NStr::Format( "%s %s %s %s", pszMask, pszMask, pszMask, pszMask ),
										 &( pLoadValue->minx ),
										 &( pLoadValue->miny ),
										 &( pLoadValue->maxx ),
										 &( pLoadValue->maxy ) ) < 4 )
				{
					( *pLoadValue ) = rDefaultValue;
					result = ERROR_INVALID_DATA;
				}
			}
			return result;
		}
		else
		{
			return ERROR_INVALID_PARAMETER;
		}
	}

	template<class Type>
  LONG SaveRect( LPCTSTR pszRegistryKey, LPCTSTR pszMask, const CTRect<Type> &rRect ) const
	{
		std::string szFormat = NStr::Format( "%s %s %s %s", pszMask, pszMask, pszMask, pszMask );
		std::string szBuffer = NStr::Format( szFormat.c_str(), rRect.minx, rRect.miny, rRect.maxx, rRect.maxy );
		return SaveString( pszRegistryKey, szBuffer );
	}
};
#endif //#if !defined(__Registry__Types__)


