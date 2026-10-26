// The replay core (Sources/src/Main/ReplayCore.h) over a C ABI, for
// tools/zig/replay_core_test.zig. The header is the engine's own; this file
// only wraps it, so the test exercises exactly what CCommandsHistory runs.
#include <cstring>
#include "../../Sources/src/Main/ReplayCore.h"

namespace
{
	struct SHashes
	{
		std::vector<NReplay::SStateHash> hashes;
		NReplay::CStateHashCheck check;
	};
	// Copies text into a caller's buffer; false when it does not fit.
	bool CopyOut( const std::string &szText, char *pBuffer, const int nCapacity )
	{
		if ( pBuffer == 0 || nCapacity <= 0 || int( szText.size() ) >= nCapacity )
			return false;
		std::memcpy( pBuffer, szText.c_str(), szText.size() + 1 );
		return true;
	}
}

extern "C"
{
	int bk_replay_state_hash_period() { return NReplay::STATE_HASH_PERIOD; }
	bool bk_replay_is_state_hash_segment( int nSegment ) { return NReplay::IsStateHashSegment( nSegment ); }
	bool bk_replay_format_time( int nSegment, char *pBuffer, int nCapacity ) { return CopyOut( NReplay::FormatSegmentTime( nSegment ), pBuffer, nCapacity ); }
	bool bk_replay_format_random_state( unsigned int nState, char *pBuffer, int nCapacity ) { return CopyOut( NReplay::FormatRandomState( nState ), pBuffer, nCapacity ); }
	bool bk_replay_parse_random_state( const char *pszText, unsigned int *pnState ) { return NReplay::ParseRandomState( pszText, pnState ); }

	void *bk_replay_hashes_create() { return new SHashes(); }
	void bk_replay_hashes_destroy( void *pHandle ) { delete static_cast<SHashes*>( pHandle ); }
	void bk_replay_hashes_record( void *pHandle, int nSegment, unsigned int nHash ) { NReplay::RecordStateHash( &static_cast<SHashes*>( pHandle )->hashes, nSegment, nHash ); }
	int bk_replay_hashes_count( void *pHandle ) { return int( static_cast<SHashes*>( pHandle )->hashes.size() ); }
	int bk_replay_hashes_segment( void *pHandle, int nIndex ) { return static_cast<SHashes*>( pHandle )->hashes[nIndex].nSegment; }
	unsigned int bk_replay_hashes_hash( void *pHandle, int nIndex ) { return static_cast<SHashes*>( pHandle )->hashes[nIndex].nHash; }
	bool bk_replay_hashes_encode( void *pHandle, char *pBuffer, int nCapacity ) { return CopyOut( NReplay::EncodeStateHashes( static_cast<SHashes*>( pHandle )->hashes ), pBuffer, nCapacity ); }
	bool bk_replay_hashes_decode( void *pHandle, const char *pszText ) { return NReplay::DecodeStateHashes( pszText, &static_cast<SHashes*>( pHandle )->hashes ); }
	// The playback check, armed with what the list holds now.
	void bk_replay_check_arm( void *pHandle ) { SHashes *p = static_cast<SHashes*>( pHandle ); p->check.SetRecorded( p->hashes ); }
	int bk_replay_check( void *pHandle, int nSegment, unsigned int nHash ) { return int( static_cast<SHashes*>( pHandle )->check.Check( nSegment, nHash ) ); }
	int bk_replay_check_first_mismatch( void *pHandle ) { return static_cast<SHashes*>( pHandle )->check.GetFirstMismatch(); }
	int bk_replay_check_matched( void *pHandle ) { return static_cast<SHashes*>( pHandle )->check.GetMatched(); }
}
