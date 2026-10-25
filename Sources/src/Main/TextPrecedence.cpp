#include "TextPrecedence.h"

namespace NTextPrecedence
{
	static bool IsSpace( char16_t c )
	{
		return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == 0x00a0 || c == 0x3000;
	}

	std::u16string Normalize( const std::u16string &szText )
	{
		std::u16string szResult;
		szResult.reserve( szText.size() );
		bool bSpace = false;
		for ( std::u16string::size_type i = 0; i < szText.size(); ++i )
		{
			char16_t c = szText[i];
			if ( c == 0xfeff )
				continue;
			if ( IsSpace( c ) )
			{
				bSpace = !szResult.empty();
				continue;
			}
			if ( bSpace )
				szResult += u' ';
			bSpace = false;
			// Upper to lower case in the alphabets the game's languages write with Latin and Cyrillic
			// letters: ASCII, Latin-1 (not the multiplication sign) and the Russian alphabet with Ukrainian.
			if ( ( c >= u'A' && c <= u'Z' ) || ( c >= 0xc0 && c <= 0xde && c != 0xd7 ) || ( c >= 0x410 && c <= 0x42f ) )
				c = char16_t( c + 0x20 );
			else if ( c >= 0x400 && c <= 0x40f )
				c = char16_t( c + 0x50 );
			szResult += c;
		}
		return szResult;
	}

	void CMemory::Add( const std::string &szKey, const std::u16string &szEnglish, const std::u16string &szTranslation )
	{
		const std::u16string szNormal = Normalize( szEnglish );
		if ( szNormal.empty() || Normalize( szTranslation ).empty() )
			return;
		STranslation translation = { szKey, szTranslation };
		entries[szNormal].push_back( translation );
	}

	bool CMemory::Find( const std::u16string &szEnglish, std::u16string *pTranslation ) const
	{
		const std::map<std::u16string, std::vector<STranslation>>::const_iterator pos = entries.find( Normalize( szEnglish ) );
		if ( pos == entries.end() )
			return false;
		// Votes per normalised translation, with the spelling of the key that sorts first.
		struct SVote { int nCount; std::string szKey; std::u16string szText; };
		std::map<std::u16string, SVote> votes;
		for ( std::vector<STranslation>::const_iterator it = pos->second.begin(); it != pos->second.end(); ++it )
		{
			SVote &vote = votes[Normalize( it->szText )];
			if ( vote.nCount == 0 || it->szKey < vote.szKey )
			{
				vote.szKey = it->szKey;
				vote.szText = it->szText;
			}
			++vote.nCount;
		}
		const SVote *pBest = 0;
		bool bTie = false;
		for ( std::map<std::u16string, SVote>::const_iterator it = votes.begin(); it != votes.end(); ++it )
		{
			if ( pBest == 0 || it->second.nCount > pBest->nCount )
			{
				pBest = &it->second;
				bTie = false;
			}
			else if ( it->second.nCount == pBest->nCount )
				bTie = true;
		}
		if ( pBest == 0 || bTie )
			return false;
		*pTranslation = pBest->szText;
		return true;
	}

	ESource Resolve( const std::string &szKey, const FReader &base, const FReader &mod, const FReader &language,
		const CMemory &memory, std::u16string *pText )
	{
		std::u16string szMod;
		if ( !mod( szKey, &szMod ) )
			return SOURCE_STORAGE;
		const std::u16string szModNormal = Normalize( szMod );
		std::u16string szBase, szLanguage;
		if ( base( szKey, &szBase ) && Normalize( szBase ) == szModNormal && language( szKey, &szLanguage ) )
		{
			*pText = szLanguage;
			return SOURCE_LANGUAGE;
		}
		if ( !szModNormal.empty() && memory.Find( szMod, pText ) )
			return SOURCE_MEMORY;
		*pText = szMod;
		return SOURCE_MOD;
	}
}
