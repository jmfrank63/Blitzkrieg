#ifndef __REPLAY_CORE_H__
#define __REPLAY_CORE_H__
#pragma ONCE
// What a replay keeps besides the commands, so that playback can start from
// the recording's own state and prove it stays there
// (docs/superpowers/specs/2026-10-26-mission-replay-design.md, slice 1): the
// random generator's state at the mission start, written as eight hex
// digits, and a hash of the AI's state every STATE_HASH_PERIOD segments.
// CCommandsHistory stores both in the replay and in a save's history chunk;
// the rules for writing, reading and comparing them live here, free of the
// engine, so tools/zig/replay_core_test.zig can test them.
#include <cstdio>
#include <string>
#include <vector>

namespace NReplay
{
	// One second of game time: a segment is AI_SEGMENT_DURATION, 50 ms.
	const int STATE_HASH_PERIOD = 20;

	struct SStateHash
	{
		int nSegment;
		unsigned int nHash;
	};

	// The hash is taken after the AI has run nSegment segments, every whole
	// second of game time, not at the start: the start already has its own
	// check (StartMapCheckSum).
	inline bool IsStateHashSegment( const int nSegment )
	{
		return nSegment > 0 && nSegment % STATE_HASH_PERIOD == 0;
	}

	// "mm:ss" of game time after nSegment segments, for the out-of-sync report.
	inline std::string FormatSegmentTime( const int nSegment )
	{
		const int nSeconds = nSegment / STATE_HASH_PERIOD;
		char buffer[32];
		std::snprintf( buffer, sizeof( buffer ), "%02d:%02d", nSeconds / 60, nSeconds % 60 );
		return buffer;
	}

	// StartRandomState: the generator's whole state, eight lower-case hex
	// digits. A string rather than an int so a replay without it (every replay
	// written before this format) is told apart from a state that happens to be
	// zero: such a replay plays from the zero seed, as before.
	inline std::string FormatRandomState( const unsigned int nState )
	{
		char buffer[16];
		std::snprintf( buffer, sizeof( buffer ), "%08x", nState );
		return buffer;
	}
	// The value of one hex digit, or -1.
	inline int HexDigit( const char c )
	{
		if ( c >= '0' && c <= '9' )
			return c - '0';
		if ( c >= 'a' && c <= 'f' )
			return c - 'a' + 10;
		if ( c >= 'A' && c <= 'F' )
			return c - 'A' + 10;
		return -1;
	}
	inline bool ParseRandomState( const std::string &szText, unsigned int *pnState )
	{
		if ( szText.size() != 8 )
			return false;
		unsigned int nState = 0;
		for ( size_t i = 0; i < szText.size(); ++i )
		{
			const int nDigit = HexDigit( szText[i] );
			if ( nDigit < 0 )
				return false;
			nState = ( nState << 4 ) | unsigned( nDigit );
		}
		*pnState = nState;
		return true;
	}

	// Adds the hash taken after nSegment segments. A save brings the hashes
	// back as they were when it was written, and the mission goes on from the
	// save's segment, so a hash at or after nSegment belongs to a timeline the
	// load left: it is dropped before the new one goes in.
	inline void RecordStateHash( std::vector<SStateHash> *pHashes, const int nSegment, const unsigned int nHash )
	{
		while ( !pHashes->empty() && pHashes->back().nSegment >= nSegment )
			pHashes->pop_back();
		SStateHash hash;
		hash.nSegment = nSegment;
		hash.nHash = nHash;
		pHashes->push_back( hash );
	}

	// StateHashes in a replay: "<segments since the previous entry>:<hash in
	// hex>" separated by spaces, the first counted from segment 0. With one
	// entry a second the gaps are all 20, so an hour is about 3,600 short
	// entries in one attribute rather than as many XML elements.
	inline std::string EncodeStateHashes( const std::vector<SStateHash> &hashes )
	{
		std::string szResult;
		int nPrevious = 0;
		for ( size_t i = 0; i < hashes.size(); ++i )
		{
			char buffer[32];
			std::snprintf( buffer, sizeof( buffer ), "%s%d:%x", i == 0 ? "" : " ", hashes[i].nSegment - nPrevious, hashes[i].nHash );
			szResult += buffer;
			nPrevious = hashes[i].nSegment;
		}
		return szResult;
	}
	// False for text that is not that form, or whose segments do not grow;
	// pHashes is then left empty, so a damaged list checks nothing rather than
	// reporting a false desync.
	inline bool DecodeStateHashes( const std::string &szText, std::vector<SStateHash> *pHashes )
	{
		pHashes->clear();
		int nSegment = 0;
		size_t nPos = 0;
		while ( nPos < szText.size() )
		{
			if ( szText[nPos] == ' ' )
			{
				++nPos;
				continue;
			}
			// The gap: decimal, 1 to 9 digits, then ':'.
			int nDelta = 0;
			int nDigits = 0;
			while ( nPos < szText.size() && szText[nPos] >= '0' && szText[nPos] <= '9' && nDigits < 9 )
			{
				nDelta = nDelta * 10 + ( szText[nPos] - '0' );
				++nPos;
				++nDigits;
			}
			bool bValid = nDigits > 0 && nDelta > 0 && nPos < szText.size() && szText[nPos] == ':';
			// The hash: 1 to 8 hex digits, then a space or the end.
			unsigned int nHash = 0;
			nDigits = 0;
			if ( bValid )
			{
				++nPos;
				while ( nPos < szText.size() && szText[nPos] != ' ' )
				{
					const int nDigit = HexDigit( szText[nPos] );
					if ( nDigit < 0 || nDigits == 8 )
					{
						bValid = false;
						break;
					}
					nHash = ( nHash << 4 ) | unsigned( nDigit );
					++nPos;
					++nDigits;
				}
				bValid = bValid && nDigits > 0;
			}
			if ( !bValid )
			{
				pHashes->clear();
				return false;
			}
			nSegment += nDelta;
			SStateHash hash;
			hash.nSegment = nSegment;
			hash.nHash = nHash;
			pHashes->push_back( hash );
		}
		return true;
	}

	// Playback's comparison: each hash the AI computes against the one the
	// recording took at the same segment. Only the first difference is
	// reported; once the world has left the recording every later hash
	// differs too.
	class CStateHashCheck
	{
		std::vector<SStateHash> recorded;
		int nFirstMismatch;
		int nMatched;
	public:
		enum EResult
		{
			NOT_RECORDED,		// the recording has no hash at this segment (it ended earlier, or is an old replay)
			MATCH,
			FIRST_MISMATCH,
			MISMATCH,
		};
		CStateHashCheck() : nFirstMismatch( -1 ), nMatched( 0 ) { }

		void SetRecorded( const std::vector<SStateHash> &hashes )
		{
			recorded = hashes;
			nFirstMismatch = -1;
			nMatched = 0;
		}
		bool HasRecorded() const { return !recorded.empty(); }
		int GetFirstMismatch() const { return nFirstMismatch; }
		int GetMatched() const { return nMatched; }

		EResult Check( const int nSegment, const unsigned int nHash )
		{
			size_t nLow = 0;
			size_t nHigh = recorded.size();
			while ( nLow < nHigh )
			{
				const size_t nMid = ( nLow + nHigh ) / 2;
				if ( recorded[nMid].nSegment < nSegment )
					nLow = nMid + 1;
				else
					nHigh = nMid;
			}
			if ( nLow == recorded.size() || recorded[nLow].nSegment != nSegment )
				return NOT_RECORDED;
			if ( recorded[nLow].nHash == nHash )
			{
				++nMatched;
				return MATCH;
			}
			if ( nFirstMismatch >= 0 )
				return MISMATCH;
			nFirstMismatch = nSegment;
			return FIRST_MISMATCH;
		}
	};
}
#endif // __REPLAY_CORE_H__
