#include "StdAfx.h"

#include "StrProc.h"

#include <unordered_map>
#include <stack>
#include <math.h>
#include <stdlib.h>

namespace
{
	typedef std::basic_string<unsigned short, std::char_traits<unsigned short>, std::allocator<unsigned short> > TUnsignedShortString;

	template <class TWideString>
	void ToAsciiImpl( std::string *pRes, const TWideString &szSrc, int nCodePage )
	{
		std::wstring szWide;
		szWide.reserve( szSrc.length() );
		for ( typename TWideString::const_iterator it = szSrc.begin(); it != szSrc.end(); ++it )
			szWide += static_cast<wchar_t>( *it );

		const int N_STACK_BUFF_SIZE = 1024;
		char static_buff[N_STACK_BUFF_SIZE];
		int nBufLeng = szWide.length() * 2 + 10;
		char *pszBuf;
		if ( nBufLeng < N_STACK_BUFF_SIZE )
			pszBuf = static_buff;
		else
			pszBuf = new char[ nBufLeng ];
		int nRes = WideCharToMultiByte( nCodePage, 0, szWide.c_str(), szWide.length(), pszBuf, nBufLeng, 0, 0 );
		pszBuf[nRes] = 0;
		*pRes = pszBuf;
		if ( nBufLeng >= N_STACK_BUFF_SIZE )
			delete[] pszBuf;
	}

	template <class TChar>
	void ToUnicodeImpl( std::basic_string<TChar, std::char_traits<TChar>, std::allocator<TChar> > *pRes, const std::string &szSrc, int nCodePage )
	{
		const int N_STACK_BUFF_SIZE = 1024;
		WCHAR static_buff[N_STACK_BUFF_SIZE];
		int nBufLeng = szSrc.length() + 3;
		WCHAR *pszBuf;
		if ( nBufLeng < N_STACK_BUFF_SIZE )
			pszBuf = static_buff;
		else
			pszBuf = new WCHAR[ nBufLeng ];
		int nRes = MultiByteToWideChar( nCodePage, 0, szSrc.c_str(), szSrc.length(), pszBuf, nBufLeng );
		pszBuf[nRes] = 0;

		pRes->clear();
		pRes->reserve( nRes );
		for ( int i = 0; i < nRes; ++i )
			pRes->push_back( static_cast<TChar>( pszBuf[i] ) );

		if ( nBufLeng >= N_STACK_BUFF_SIZE )
			delete[] pszBuf;
	}
}
namespace NStr
{
	static std::unordered_map<char, char> brackets;   // map with open bracket <=> close bracket respection
	static char cBracketTypes[8] = "({[\" ";     // all available brackets (open)
	static const int NUM_BRACKET_TYPES = 4;      // number of available brackets
	static int nCodePage = GetACP();
	void InitStringProcessor();
};
void NStr::InitStringProcessor()
{
	brackets['('] = ')';
	brackets['['] = ']';
	brackets['{'] = '}';
	brackets['\"'] = '\"';
}
struct SStrProcInit
{
	SStrProcInit() { NStr::InitStringProcessor(); }
};
static SStrProcInit spInit;
bool NStr::IsOpenBracket( const char cSymbol )
{
	return brackets.find( cSymbol ) != brackets.end();
}
void NStr::AddBrackets( const char cOpenBracket, const char cCloseBracket )
{
	brackets[cOpenBracket] = cCloseBracket;
}
void NStr::RemoveBrackets( const char cOpenBracket, const char cCloseBracket )
{
	brackets.erase( cOpenBracket );
}
const char NStr::GetCloseBracket( const char cOpenBracket )
{
	return brackets[cOpenBracket];
}
template <class T1, class T2, class T3>
static void SplitStringT( const std::basic_string<T1, T2, T3> &szString, std::vector< std::basic_string<T1, T2, T3> > &szVector, const T1 tSeparator )
{
	int nPos = 0, nLastPos = 0;
	do
	{
		nPos = szString.find( tSeparator, nLastPos );
		szVector.push_back( szString.substr( nLastPos, nPos - nLastPos ) );
		nLastPos = nPos + 1;//szString.find_first_not_of( cSeparator, nPos );
	} while( nPos != std::basic_string<T1, T2, T3>::npos );
}
void NStr::SplitString( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	SplitStringT( szString, szVector, cSeparator );
}
void NStr::SplitString( const std::wstring &szString, std::vector<std::wstring> &szVector, const wchar_t cSeparator )
{
	SplitStringT( szString, szVector, cSeparator );
}
void NStr::SplitStringWithBrackets( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	int nPos = 0, nLastPos = 0;
	cBracketTypes[NUM_BRACKET_TYPES] = cSeparator;
	do
	{
		nPos = szString.find_first_of( cBracketTypes, nLastPos );
		if ( nPos != std::string::npos )
		{
			if ( szString[nPos] != cSeparator )      // this is a bracket
			{
				nPos = szString.find( brackets[szString[nPos]], nPos + 1 );
				continue;
			}
		}
		szVector.push_back( szString.substr( nLastPos, nPos - nLastPos ) );
		nLastPos = nPos + 1;//szString.find_first_not_of( cSeparator, nPos );
	} while( nPos != std::string::npos );
}
void NStr::SplitStringWithMultipleBrackets( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	std::stack<char> stackBrackets;
	int nLastPos = 0;
  int i;
	for ( i=0; i<szString.size(); ++i )
	{
		char c = szString[i];
		if ( stackBrackets.empty() )
		{
			if ( IsOpenBracket(c) )
				stackBrackets.push( brackets[c] );
			else
			{
				if ( c == cSeparator )
				{
					szVector.push_back( szString.substr( nLastPos, i - nLastPos ) );
					nLastPos = i + 1; // szString.find_first_not_of( cSeparator, i );
				}
			}
		}
		else if ( c == stackBrackets.top() )
			stackBrackets.pop();
	}
	if ( nLastPos + 1 <= int( i ) )
		szVector.push_back( szString.substr( nLastPos ) );
}
int NStr::FindCloseBracket( const std::string &szString, int nPos, const char cOpenBracket )
{
	return szString.find( brackets[cOpenBracket], nPos );
}
int NStr::FindMultipleCloseBracket( const std::string &szString, int nPos, const char cOpenBracket )
{
	std::stack<char> stackBrackets;
	stackBrackets.push( brackets[cOpenBracket] );
	for ( int i=nPos; i<szString.size(); ++i )
	{
		char c = szString[i];
		if ( IsOpenBracket(c) )
			stackBrackets.push( brackets[c] );
		else if ( c == stackBrackets.top() )
		{
			stackBrackets.pop();
			if ( stackBrackets.empty() )
				return i;
		}
	}
	return std::string::npos;
}
void NStr::TrimRight( std::string &szString, const char cTrim )
{
	int nPos = szString.find_last_not_of( cTrim );
	if ( nPos == std::string::npos )
	{
		if ( szString.find_first_of( cTrim ) == 0 )
			szString.clear();
	}
	else
		szString.erase( nPos + 1, std::string::npos );
}
void NStr::TrimRight( std::string &szString, const char *pszTrim )
{
	int nPos = szString.find_last_not_of( pszTrim );
	if ( nPos == std::string::npos )
	{
		if ( szString.find_first_of( pszTrim ) == 0 )
			szString.clear();
	}
	else
		szString.erase( nPos + 1, std::string::npos );
}
class CSymbolCheckFunctional
{
private:
  const char *pszSymbols;
public:
  explicit CSymbolCheckFunctional( const char *pszNewSymbols ) : pszSymbols( pszNewSymbols ) {  }
  bool operator()( const char cSymbol )
  {
    for ( const char *p = pszSymbols; *p != 0; ++p )
    {
      if ( *p == cSymbol )
        return true;
    }
    return false;
  }
};
void NStr::TrimInside( std::string &szString, const char *pszTrim )
{
  szString.erase( std::remove_if(szString.begin(), szString.end(), CSymbolCheckFunctional(pszTrim)), szString.end() );
}
void NStr::ToDotString( std::string *pDst, int nVal, const char cSeparator )
{
	char buff[32], buff2[32];
	buff[0] = buff2[0] = 0;
  int nOrder = static_cast<int>( log10( nVal ) );
  int nOrderVal = static_cast<int>( pow( 10, nOrder - (nOrder % 3) ) );
  while ( nOrderVal > 1 )
  {
    int nVal1 = nVal / nOrderVal;
		sprintf( buff2, "%d%c", nVal1, cSeparator );
		strcat( buff, buff2 );
    nVal -= nVal1 * nOrderVal;
    nOrderVal /= 1000;
  }
	strcat( buff, _itoa(nVal, buff2, 10) );
  *pDst = buff;
}
const char* __cdecl NStr::Format( const char *pszFormat, ... )
{
  static char buff[2048];
  va_list va;
  va_start( va, pszFormat );
  vsprintf( buff, pszFormat, va );
  va_end( va );
	return buff;
}
void __cdecl NStr::DebugTrace( const char *pszFormat, ... )
{
  static char buff[2048];
  va_list va;
  va_start( va, pszFormat );
  vsprintf( buff, pszFormat, va );
  va_end( va );
	OutputDebugString( buff );
	// [opt-diag] mirror traces into a file for offline analysis
	if ( FILE *pFile = fopen( "bk_trace.log", "a" ) )
	{
		fputs( buff, pFile );
		fclose( pFile );
	}
}
__forceinline char HalfByteToHexSymbol( const unsigned char chr )
{
	return chr >= 10 ? 'a' + ( chr - 10 ) : '0' + chr;
}
__forceinline unsigned char HexSymbolToHalfByte( const char chr )
{
	if ( chr >= 'a' )
		return chr - 'a' + 10;
	else if ( chr >= 'A' ) 
		return chr - 'A' + 10;
	else
		return chr - '0';
}
const char* NStr::BinToString( const void *pData, int nSize, char *pszBuffer )
{
	char *pszCurr = pszBuffer;
	for ( const unsigned char *it = (unsigned char*)pData; it != (unsigned char*)pData + nSize; ++it )
	{
		*pszCurr++ = HalfByteToHexSymbol( ((*it) >> 4) & 0x0f );
		*pszCurr++ = HalfByteToHexSymbol( (*it) & 0x0f );
	}
	return pszBuffer;
}
void* NStr::StringToBin( const char *pszData, void *pBuffer, int *pnSize )
{
	BYTE *pData = (BYTE*)pBuffer;
	for ( const char *it = pszData; *it != 0; it += 2 )
		*pData++ = ( HexSymbolToHalfByte( *it ) << 4 ) | HexSymbolToHalfByte( *(it +1) );
	if ( pnSize ) 
		*pnSize = int( pData - (BYTE*)pBuffer );
	return pBuffer;
}
bool NStr::CBracketCharSeparator::operator()( const char cSymbol )
{
	if ( IsOpenBracket(cSymbol) )
		stackBrackets.push( brackets[cSymbol] );
	else if ( stackBrackets.empty() )
		return cSymbol == cSeparator;
	else if ( cSymbol == stackBrackets.top() )
		stackBrackets.pop();
	return false;
}
bool NStr::IsDecNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits == 0 )
		return false;												// this is not a number at all => zero length digits
	if ( (nNumDigits > 1) && (szString[nFirstDigit] == '0') )
		return false;												// hex number
	for ( i=nFirstDigit; (i < szString.size()) && IsDecDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
bool NStr::IsOctNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits == 0 )
		return false;
	if ( szString[nFirstDigit] != '0' )
		return false;
	if ( nNumDigits < 2 )
		return false;

	for ( i=nFirstDigit; (i < szString.size()) && IsOctDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
bool NStr::IsHexNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits < 3 )
		return false;
	if ( (szString[nFirstDigit] != '0') || (szString[nFirstDigit + 1] != 'x') )
		return false;
	for ( i=nFirstDigit + 2; (i < szString.size()) && IsHexDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
int NStr::ToInt( const char *pszString )
{
	int nNumber = 0;
	sscanf( pszString, "%i", &nNumber );
	return nNumber;
}
float NStr::ToFloat( const char *pszString )
{
	float fNumber = 0;
	sscanf( pszString, "%f", &fNumber );
	return fNumber;
}
double NStr::ToDouble( const char *pszString )
{
	double fNumber = 0;
	sscanf( pszString, "%lf", &fNumber );
	return fNumber;
}
unsigned long NStr::ToULong( const char *pszString )
{
	unsigned long ulNumber = 0;
	sscanf( pszString, "%ul", &ulNumber );
	return ulNumber;
}
void NStr::SetCodePage( int _nCodePage )
{
	nCodePage = _nCodePage;
}
void NStr::ToAscii( std::string *pRes, const std::wstring &szSrc )
{
	ToAsciiImpl( pRes, szSrc, nCodePage );
}

void NStr::ToAscii( std::string *pRes, const TUnsignedShortString &szSrc )
{
	ToAsciiImpl( pRes, szSrc, nCodePage );
}

void NStr::ToUnicode( std::wstring *pRes, const std::string &szSrc )
{
	ToUnicodeImpl( pRes, szSrc, nCodePage );
}

void NStr::ToUnicode( TUnsignedShortString *pRes, const std::string &szSrc )
{
	ToUnicodeImpl( pRes, szSrc, nCodePage );
}
inline int MSVCMustDie_tolower( int a ) { return tolower(a); } 
inline int MSVCMustDie_toupper( int a ) { return toupper(a); }
void NStr::ToLower( std::string &szString ) 
{ 
	std::transform( szString.begin(), szString.end(), szString.begin(), [](int c){ return tolower(c); } ); 
}
void NStr::ToUpper( std::string &szString ) 
{ 
	std::transform( szString.begin(), szString.end(), szString.begin(), [](int c){ return toupper(c); } ); 
}
