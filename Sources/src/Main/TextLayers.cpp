#include "StdAfx.h"

#include "TextLayers.h"
#include "TextPrecedence.h"

#include <cstdio>
#include <cstdlib>

namespace NTextLayers
{
	// Plain pointers with a reference each, not CPtr: a global CPtr would release at static
	// destruction, after the storage module that owns the object has unloaded.
	static IDataStorage *pModStorage = 0;
	static IDataStorage *pLanguageStorage = 0;
	static NTextPrecedence::CMemory *pMemory = 0;

	static void Forget()
	{
		delete pMemory;
		pMemory = 0;
	}

	static void Hold( IDataStorage **ppSlot, IDataStorage *pStorage )
	{
		if ( pStorage != 0 )
			pStorage->AddRef();
		if ( *ppSlot != 0 )
			( *ppSlot )->Release();
		*ppSlot = pStorage;
		Forget();
	}

	void SetMod( IDataStorage *pMod ) { Hold( &pModStorage, pMod ); }
	void SetLanguage( IDataStorage *pLanguage ) { Hold( &pLanguageStorage, pLanguage ); }
	void Clear()
	{
		SetMod( 0 );
		SetLanguage( 0 );
	}

	// A text file is UTF-16 with a byte order mark; anything else is not a text this rule reads.
	static bool ReadText( IDataStream *pStream, std::u16string *pText )
	{
		if ( pStream == 0 )
			return false;
		WORD wSignature = 0;
		const int nSize = pStream->GetSize() - 2;
		if ( nSize < 0 || ( nSize & 1 ) != 0 || pStream->Read( &wSignature, 2 ) != 2 || wSignature != 0xfeff )
			return false;
		pText->resize( nSize / 2 );
		return nSize == 0 || pStream->Read( &( ( *pText )[0] ), nSize ) == nSize;
	}

	static bool ReadFrom( IDataStorage *pStorage, const std::string &szKey, std::u16string *pText )
	{
		if ( pStorage == 0 )
			return false;
		CPtr<IDataStream> pStream = pStorage->OpenStream( szKey.c_str(), STREAM_ACCESS_READ );
		return ReadText( pStream, pText );
	}

	static bool ReadBase( const std::string &szKey, std::u16string *pText )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStreamBelow( szKey.c_str(), "MOD" );
		return ReadText( pStream, pText );
	}

	static bool Traced()
	{
		static const bool bTrace = getenv( "BK_TEXT_TRACE" ) != 0;
		return bTrace;
	}

	// Every text the language translates, with the base game's English of its key.
	static const NTextPrecedence::CMemory &Memory()
	{
		if ( pMemory != 0 )
			return *pMemory;
		pMemory = new NTextPrecedence::CMemory;
		int nTexts = 0;
		CPtr<IStorageEnumerator> pEnumerator = pLanguageStorage->CreateEnumerator();
		if ( pEnumerator != 0 )
		{
			pEnumerator->Reset( "*.*" );
			while ( pEnumerator->Next() )
			{
				const SStorageElementStats *pStats = pEnumerator->GetStats();
				if ( pStats == 0 || pStats->pszName == 0 )
					continue;
				const std::string szKey = pStats->pszName;
				if ( szKey.size() < 4 || NStr::CompareAsciiNoCase( szKey.substr( szKey.size() - 4 ).c_str(), ".txt" ) != 0 )
					continue;
				std::u16string szEnglish, szTranslation;
				if ( !ReadBase( szKey, &szEnglish ) || !ReadFrom( pLanguageStorage, szKey, &szTranslation ) )
					continue;
				pMemory->Add( szKey, szEnglish, szTranslation );
				++nTexts;
			}
		}
		NStr::DebugTrace( "text precedence: the language's translation memory holds %d texts\n", nTexts );
		return *pMemory;
	}

	bool Read( const char *pszName, std::u16string *pText )
	{
		if ( pModStorage == 0 || pLanguageStorage == 0 || pszName == 0 )
			return false;
		std::string szKey = pszName;
		NStr::ToLower( szKey );
		for ( std::string::size_type i = 0; i < szKey.size(); ++i )
		{
			if ( szKey[i] == '/' )
				szKey[i] = '\\';
		}
		// The mod is asked first, so a game without mod texts never builds the memory.
		std::u16string szMod;
		if ( !ReadFrom( pModStorage, szKey, &szMod ) )
			return false;
		const NTextPrecedence::FReader base = []( const std::string &szName, std::u16string *pOut ) { return ReadBase( szName, pOut ); };
		const NTextPrecedence::FReader mod = [&szMod]( const std::string &, std::u16string *pOut ) { *pOut = szMod; return true; };
		const NTextPrecedence::FReader language = []( const std::string &szName, std::u16string *pOut ) { return ReadFrom( pLanguageStorage, szName, pOut ); };
		const NTextPrecedence::ESource eSource = NTextPrecedence::Resolve( szKey, base, mod, language, Memory(), pText );
		if ( Traced() )
		{
			static const char *pszSources[] = { "storage", "language", "memory", "mod", "reworded" };
			fprintf( stderr, "BK_TEXT_TRACE: %s %s\n", pszSources[eSource], szKey.c_str() );
		}
		return eSource != NTextPrecedence::SOURCE_STORAGE;
	}
}
