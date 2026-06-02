#if !defined(__Values_Collector__)
#define __Values_Collector__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

template< class T> 
class CValuesCollector
{
	std::string szMultipleValue;
	std::string szValue;
	bool bMultilpe;
	T lastValue;
	T initialValue;
public:
	CValuesCollector( const std::string &rszMultipleValue, const T &rInitialValue )
		:	szMultipleValue( rszMultipleValue ), bMultilpe( false ), initialValue( rInitialValue ) {}
	void Clear() { szValue.clear(); bMultilpe = false; lastValue = initialValue; }
	bool AddValue( const T &rValue, const std::string &rszFormatString )
	{
		lastValue = rValue;
		if( szValue.empty() )
		{
			szValue = NStr::Format( rszFormatString.c_str(), rValue );
		}
		else
		{
			const std::string szNewValue = NStr::Format( rszFormatString.c_str(), rValue );
			if ( szNewValue != szValue )
			{
				bMultilpe = true;
			}
		}
		return bMultilpe;
	}
	std::string GetStringValue() { return ( bMultilpe ? szMultipleValue : szValue ); }
	T GetValue() { return bMultilpe ? initialValue : lastValue; }
};

template<>
class CValuesCollector<std::string>
{
	std::string szMultipleValue;
	std::string szValue;
	bool bMultilpe;
	std::string lastValue;
	std::string initialValue;
public:
	CValuesCollector<std::string>( const std::string &rszMultipleValue, const std::string &rszInitialValue )
		:	szMultipleValue( rszMultipleValue ), bMultilpe( false ), initialValue( rszInitialValue ) {}
	void Clear() { szValue.clear(); bMultilpe = false; lastValue = initialValue; }
	bool AddValue( const std::string &rszValue, const std::string &rszFormatString )
	{
		lastValue = rszValue;
		if( szValue.empty() )
		{
			szValue = NStr::Format( rszFormatString.c_str(), lastValue.c_str() );
		}
		else
		{
			const std::string szNewValue = NStr::Format( rszFormatString.c_str(), lastValue.c_str() );
			if ( szNewValue != szValue )
			{
				bMultilpe = true;
			}
		}
		return bMultilpe;
	}
	std::string GetStringValue() { return ( bMultilpe ? szMultipleValue : szValue ); }
	std::string GetValue() { return bMultilpe ? initialValue : lastValue; }
};
#endif // !defined(__Values_Collector__)
