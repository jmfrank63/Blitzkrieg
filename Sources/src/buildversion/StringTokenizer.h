#ifndef __STRINGTOKENIZER_H__
#define __STRINGTOKENIZER_H__
#pragma ONCE
template <class TChar>
class CStringTokenizer
{
	std::basic_string<TChar> szInputString;
	TChar cSeparator;
	int nFirstPos, nLastPos;
	int nLineNumber;
	int nLineStartPos;
	bool IsWhiteSpace( const TChar &chr ) const { return (chr == TChar(' ')) || (chr == TChar('\t')) || (chr == TChar(0xA)); }
	bool IsOpenBracket( const TChar &bracket ) const { return bracket == TChar('"'); }
	bool IsCloseBracket( const TChar &bracket ) const { return bracket == TChar('"'); }
	const TChar GetCloseBracket( const TChar &bracket ) const { return bracket; }
	void SkipWhiteSpaces()
	{
		while ( nFirstPos < szInputString.size() ) 
		{
			if ( !IsWhiteSpace(szInputString[nFirstPos]) )
				return;
			++nFirstPos;
		}
	}
	const bool IsSeparator( const TChar &chr, std::stack<TChar> &stackBrackets ) const
	{
		if ( !stackBrackets.empty() && (chr == stackBrackets.top()) ) 
			stackBrackets.pop();
		else if ( IsOpenBracket(chr) )
			stackBrackets.push( GetCloseBracket(chr) );
		if ( stackBrackets.empty() )
			return (chr == cSeparator) || (chr == TChar(0xD)) || (chr == TChar('='));
		return false;
	}
public:
	CStringTokenizer( const std::basic_string<TChar> &szString, const TChar &_cSeparator )
		: szInputString( szString ), cSeparator( _cSeparator )
	{
		nFirstPos = nLineNumber = nLineStartPos = 0;
		nLastPos = -1;
	}
	bool Next()
	{
		nFirstPos = nLastPos + 1;
		SkipWhiteSpaces();
		if ( nFirstPos >= szInputString.size() ) 
			return false;
		std::stack<TChar> stackBrackets;
		for ( int i = nFirstPos; i < szInputString.size(); ++i )
		{
			const TChar chr = szInputString[i];
			if ( IsSeparator(chr, stackBrackets) ) 
			{
				nLastPos = i;
				if ( chr == TChar(0xD) ) 
				{
					++nLineNumber;
					nLineStartPos = i + 2;
				}
				break;
			}
		}
		return nFirstPos == nLastPos ? Next() : true;
	}
	const std::string GetToken() const { return szInputString.substr(nFirstPos, nLastPos - nFirstPos); }
	const int GetLineNumber() const { return nLineNumber; }
	const int GetLinePos() const { return nFirstPos - nLineStartPos; }
};
#endif // __STRINGTOKENIZER_H__
