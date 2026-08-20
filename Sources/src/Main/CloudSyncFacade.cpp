// The facade's whole job is translation: game vocabulary on one side, the
// CloudSync C ABI on the other, with the library loaded dynamically so its
// absence degrades to Available() == false instead of a link error.
//
// Deliberately C-runtime only (no STL): the facade sits on a module boundary
// and is also compiled into a small test binary whose link line must not
// pick a RuntimeLibrary fight. Buffers are fixed and bounded - profile names
// are printable ASCII by NProfile contract, and the JSON documents crossing
// here are small.

#include "CloudSyncFacade.h"
#include "../Platform/CloudSyncLoader.h"

#include <cstdio>
#include <cstring>

namespace
{
	// -- The raw ABI, resolved at runtime -----------------------------------

	typedef unsigned int ( *FnAvailable )();
	typedef int ( *FnDiscoveryStatus )( unsigned char *, unsigned int );
	typedef int ( *FnRefreshDiscovery )();
	typedef void ( *FnShutdown )();
	typedef const char *( *FnLastError )();
	typedef int ( *FnBegin )( const char * );
	typedef unsigned int ( *FnPoll )( int );
	typedef unsigned int ( *FnOutcome )( int );
	typedef const char *( *FnError )( int );
	typedef void ( *FnCancel )( int );
	typedef void ( *FnRelease )( int );
	typedef int ( *FnCredsLoad )( unsigned char *, unsigned int );
	typedef int ( *FnCredsSave )( const char * );
	typedef int ( *FnCredsClearSecret )();
	typedef unsigned int ( *FnCredsPresent )();
	typedef int ( *FnTestConnection )( const char * );
	typedef int ( *FnBackupList )( const char *, const char * );
	typedef int ( *FnBackupEntry )( int, unsigned int, unsigned char *, unsigned int );
	typedef int ( *FnBackupRestore )( const char *, const char *, const char *, unsigned int );
	typedef int ( *FnApplyPendingRestore )( const char * );
	typedef int ( *FnRestoreUndo )( const char *, const char * );
	typedef unsigned int ( *FnRestoreUndoAvailable )( const char * );

	struct SLibrary
	{
		bool bTried;
		bool bLoaded;
		FnAvailable pfnAvailable;
		FnDiscoveryStatus pfnDiscoveryStatus;
		FnRefreshDiscovery pfnRefreshDiscovery;
		FnShutdown pfnShutdown;
		FnLastError pfnLastError;
		FnBegin pfnBegin;
		FnPoll pfnPoll;
		FnOutcome pfnOutcome;
		FnError pfnError;
		FnCancel pfnCancel;
		FnRelease pfnRelease;
		FnCredsLoad pfnCredsLoad;
		FnCredsSave pfnCredsSave;
		FnCredsClearSecret pfnCredsClearSecret;
		FnCredsPresent pfnCredsPresent;
		FnTestConnection pfnTestConnection;
		FnBackupList pfnBackupList;
		FnBackupEntry pfnBackupEntry;
		FnBackupRestore pfnBackupRestore;
		FnApplyPendingRestore pfnApplyPendingRestore;
		FnRestoreUndo pfnRestoreUndo;
		FnRestoreUndoAvailable pfnRestoreUndoAvailable;
	};

	SLibrary s_library = {};
	char s_szLastError[512] = {};
	char s_szDiscovery[1024] = {};

	void SetLastError2( const char *pszText )
	{
		std::snprintf( s_szLastError, sizeof s_szLastError, "%s", pszText != 0 ? pszText : "" );
	}

	void *LoadSymbol( void *pModule, const char *pszName )
	{
		return NPlatform::CloudSyncLoadSymbol( pModule, pszName );
	}

	// Load once. Failure is remembered: a game installed without the library
	// must not pay a filesystem probe per call.
	SLibrary &Library()
	{
		if ( s_library.bTried )
			return s_library;
		s_library.bTried = true;

		void *pModule = NPlatform::CloudSyncLoadLibrary();
		if ( pModule == 0 )
			return s_library;

		s_library.pfnAvailable = reinterpret_cast<FnAvailable>( LoadSymbol( pModule, "bk_cloudsync_available" ) );
		s_library.pfnDiscoveryStatus = reinterpret_cast<FnDiscoveryStatus>( LoadSymbol( pModule, "bk_cloudsync_discovery_status" ) );
		s_library.pfnRefreshDiscovery = reinterpret_cast<FnRefreshDiscovery>( LoadSymbol( pModule, "bk_cloudsync_refresh_discovery" ) );
		s_library.pfnShutdown = reinterpret_cast<FnShutdown>( LoadSymbol( pModule, "bk_cloudsync_shutdown" ) );
		s_library.pfnLastError = reinterpret_cast<FnLastError>( LoadSymbol( pModule, "bk_cloudsync_last_error" ) );
		s_library.pfnBegin = reinterpret_cast<FnBegin>( LoadSymbol( pModule, "bk_cloudsync_begin" ) );
		s_library.pfnPoll = reinterpret_cast<FnPoll>( LoadSymbol( pModule, "bk_cloudsync_poll" ) );
		s_library.pfnOutcome = reinterpret_cast<FnOutcome>( LoadSymbol( pModule, "bk_cloudsync_outcome" ) );
		s_library.pfnError = reinterpret_cast<FnError>( LoadSymbol( pModule, "bk_cloudsync_error" ) );
		s_library.pfnCancel = reinterpret_cast<FnCancel>( LoadSymbol( pModule, "bk_cloudsync_cancel" ) );
		s_library.pfnRelease = reinterpret_cast<FnRelease>( LoadSymbol( pModule, "bk_cloudsync_release" ) );
		s_library.pfnCredsLoad = reinterpret_cast<FnCredsLoad>( LoadSymbol( pModule, "bk_cloudsync_creds_load" ) );
		s_library.pfnCredsSave = reinterpret_cast<FnCredsSave>( LoadSymbol( pModule, "bk_cloudsync_creds_save" ) );
		s_library.pfnCredsClearSecret = reinterpret_cast<FnCredsClearSecret>( LoadSymbol( pModule, "bk_cloudsync_creds_clear_secret" ) );
		s_library.pfnCredsPresent = reinterpret_cast<FnCredsPresent>( LoadSymbol( pModule, "bk_cloudsync_creds_present" ) );
		s_library.pfnTestConnection = reinterpret_cast<FnTestConnection>( LoadSymbol( pModule, "bk_cloudsync_test_connection" ) );
		s_library.pfnBackupList = reinterpret_cast<FnBackupList>( LoadSymbol( pModule, "bk_cloudsync_backup_list" ) );
		s_library.pfnBackupEntry = reinterpret_cast<FnBackupEntry>( LoadSymbol( pModule, "bk_cloudsync_backup_entry" ) );
		s_library.pfnBackupRestore = reinterpret_cast<FnBackupRestore>( LoadSymbol( pModule, "bk_cloudsync_backup_restore" ) );
		s_library.pfnApplyPendingRestore = reinterpret_cast<FnApplyPendingRestore>( LoadSymbol( pModule, "bk_cloudsync_apply_pending_restore" ) );
		s_library.pfnRestoreUndo = reinterpret_cast<FnRestoreUndo>( LoadSymbol( pModule, "bk_cloudsync_restore_undo" ) );
		s_library.pfnRestoreUndoAvailable = reinterpret_cast<FnRestoreUndoAvailable>( LoadSymbol( pModule, "bk_cloudsync_restore_undo_available" ) );

		s_library.bLoaded =
			s_library.pfnAvailable != 0 && s_library.pfnDiscoveryStatus != 0 &&
			s_library.pfnRefreshDiscovery != 0 && s_library.pfnShutdown != 0 &&
			s_library.pfnLastError != 0 && s_library.pfnBegin != 0 &&
			s_library.pfnPoll != 0 && s_library.pfnOutcome != 0 &&
			s_library.pfnError != 0 && s_library.pfnCancel != 0 &&
			s_library.pfnRelease != 0 && s_library.pfnCredsLoad != 0 &&
			s_library.pfnCredsSave != 0 && s_library.pfnCredsClearSecret != 0 &&
			s_library.pfnCredsPresent != 0 && s_library.pfnTestConnection != 0 &&
			s_library.pfnBackupList != 0 && s_library.pfnBackupEntry != 0 &&
			s_library.pfnBackupRestore != 0 && s_library.pfnApplyPendingRestore != 0 &&
			s_library.pfnRestoreUndo != 0 && s_library.pfnRestoreUndoAvailable != 0;
		return s_library;
	}

	// -- Job documents -------------------------------------------------------

	// Escape a string into a JSON literal: backslashes and quotes, which on
	// Windows is every path separator.
	void JsonEscape( const char *pszRaw, char *pszOut, unsigned int nCap )
	{
		unsigned int nAt = 0;
		for ( const char *p = pszRaw; *p != 0 && nAt + 2 < nCap; ++p )
		{
			if ( *p == '\\' || *p == '"' )
				pszOut[nAt++] = '\\';
			pszOut[nAt++] = *p;
		}
		pszOut[nAt] = 0;
	}

	void HostName( char *pszOut, unsigned int nCap )
	{
		NPlatform::CloudSyncHostName( pszOut, nCap );
	}

	// The remote identity for the pairing record, derived from the redacted
	// credentials document - endpoint and bucket for S3, url for WebDAV, no
	// secret material. Only self-consistency matters: the same credentials
	// must produce the same string on every run.
	void Fingerprint( char *pszOut, unsigned int nCap )
	{
		pszOut[0] = 0;
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return;
		unsigned char szDoc[4096];
		if ( library.pfnCredsLoad( szDoc, sizeof szDoc ) <= 0 )
			return;
		const char *pszDoc = reinterpret_cast<const char *>( szDoc );

		char szField[1024];
		szField[0] = 0;
		const char *pszNames[3] = { "\"endpoint\":\"", "\"bucket\":\"", "\"url\":\"" };
		unsigned int nAt = 0;
		for ( int i = 0; i < 3 && nAt + 2 < nCap; ++i )
		{
			const char *pszAt = std::strstr( pszDoc, pszNames[i] );
			if ( pszAt == 0 )
				continue;
			pszAt += std::strlen( pszNames[i] );
			const char *pszEnd = std::strchr( pszAt, '"' );
			if ( pszEnd == 0 )
				continue;
			const unsigned int nLength = static_cast<unsigned int>( pszEnd - pszAt );
			if ( nLength >= sizeof szField )
				continue;
			std::memcpy( szField, pszAt, nLength );
			szField[nLength] = 0;
			nAt += static_cast<unsigned int>( std::snprintf( pszOut + nAt, nCap - nAt, "%s%s", nAt != 0 ? "/" : "", szField ) );
		}
	}

	// -- Facade handles ------------------------------------------------------
	//
	// A thin indirection over the library handles, so the sync-then-pair
	// fallback can swap the underlying job without the caller noticing.

	struct SJob
	{
		bool bInUse;
		int nLibraryHandle;
		bool bSyncJob;					// eligible for the pair fallback
		bool bPairFallbackDone;
		bool bBackupConfig;
		char szProfile[128];
	};

	SJob s_jobs[8] = {};

	int BeginJob( const char *pszKind, const char *pszProfile, bool bBackupConfig )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
		{
			SetLastError2( "cloud sync library is not installed" );
			return -1;
		}

		char szProfileEscaped[256];
		JsonEscape( pszProfile, szProfileEscaped, sizeof szProfileEscaped );
		char szHost[128];
		HostName( szHost, sizeof szHost );
		char szHostEscaped[256];
		JsonEscape( szHost, szHostEscaped, sizeof szHostEscaped );
		char szFingerprint[1024];
		Fingerprint( szFingerprint, sizeof szFingerprint );
		char szFingerprintEscaped[2048];
		JsonEscape( szFingerprint, szFingerprintEscaped, sizeof szFingerprintEscaped );

		// Paths are the game's own conventions: the profile directory under
		// the working directory, the state root beside it.
		char szDoc[4096];
		std::snprintf( szDoc, sizeof szDoc,
			"{\"kind\":\"%s\",\"path1\":\"profiles/%s\",\"remote\":\"bkremote\","
			"\"profile\":\"%s\",\"game_dir\":\".\",\"profile_id\":\"%s\","
			"\"remote_fingerprint\":\"%s\",\"backup_config\":%s,\"host\":\"%s\"}",
			pszKind, szProfileEscaped, szProfileEscaped, szProfileEscaped,
			szFingerprintEscaped, bBackupConfig ? "true" : "false", szHostEscaped );

		return library.pfnBegin( szDoc );
	}

	SJob *JobAt( int nHandle )
	{
		if ( nHandle < 0 || nHandle >= int( sizeof s_jobs / sizeof s_jobs[0] ) )
			return 0;
		if ( !s_jobs[nHandle].bInUse )
			return 0;
		return &s_jobs[nHandle];
	}

	// Every handle this facade returns must be a facade slot: Poll, Error,
	// Cancel and Release all translate through the job table, so a raw
	// library handle handed to the caller could never be observed. The
	// probe-shaped jobs (test, list, restore, undo) go through here.
	int WrapLibraryHandle( int nLibraryHandle )
	{
		if ( nLibraryHandle < 0 )
			return nLibraryHandle;
		for ( int i = 0; i < int( sizeof s_jobs / sizeof s_jobs[0] ); ++i )
		{
			if ( !s_jobs[i].bInUse )
			{
				SJob &job = s_jobs[i];
				std::memset( &job, 0, sizeof job );
				job.bInUse = true;
				job.nLibraryHandle = nLibraryHandle;
				return i;
			}
		}
		// No slot free: a job nobody can observe must not keep running.
		SLibrary &library = Library();
		if ( library.bLoaded )
			library.pfnRelease( nLibraryHandle );
		SetLastError2( "all cloud sync handles are in use" );
		return -1;
	}
}

namespace NCloudSync
{
	bool Available()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return false;
		return library.pfnAvailable() != 0;
	}

	int Begin( const char *pszProfile, bool bBackupConfig )
	{
		int nSlot = -1;
		for ( int i = 0; i < int( sizeof s_jobs / sizeof s_jobs[0] ); ++i )
		{
			if ( !s_jobs[i].bInUse )
			{
				nSlot = i;
				break;
			}
		}
		if ( nSlot < 0 )
		{
			SetLastError2( "all cloud sync handles are in use" );
			return -1;
		}

		const int nLibraryHandle = BeginJob( "sync", pszProfile, bBackupConfig );
		if ( nLibraryHandle < 0 )
		{
			SLibrary &library = Library();
			if ( library.bLoaded )
				SetLastError2( library.pfnLastError() );
			return -1;
		}

		SJob &job = s_jobs[nSlot];
		job.bInUse = true;
		job.nLibraryHandle = nLibraryHandle;
		job.bSyncJob = true;
		job.bPairFallbackDone = false;
		job.bBackupConfig = bBackupConfig;
		std::snprintf( job.szProfile, sizeof job.szProfile, "%s", pszProfile );
		return nSlot;
	}

	EState Poll( int nHandle )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 || !library.bLoaded )
			return STATE_FAILED;

		unsigned int nState = library.pfnPoll( pJob->nLibraryHandle );

		// The first ever run has no pairing record and the sync refuses with
		// NotPaired; pairing is the designed bootstrap, so run it once,
		// transparently, on the same facade handle. Only the exact typed
		// refusal triggers this - a real failure must stay a failure.
		if ( nState == STATE_FAILED && pJob->bSyncJob && !pJob->bPairFallbackDone )
		{
			const char *pszError = library.pfnError( pJob->nLibraryHandle );
			if ( pszError != 0 && std::strcmp( pszError, "NotPaired" ) == 0 )
			{
				pJob->bPairFallbackDone = true;
				library.pfnRelease( pJob->nLibraryHandle );
				const int nPairHandle = BeginJob( "pair", pJob->szProfile, pJob->bBackupConfig );
				if ( nPairHandle >= 0 )
				{
					pJob->nLibraryHandle = nPairHandle;
					return STATE_STARTING;
				}
			}
		}
		return static_cast<EState>( nState );
	}

	EOutcome Outcome( int nHandle )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 || !library.bLoaded )
			return OUTCOME_FAILED;
		return static_cast<EOutcome>( library.pfnOutcome( pJob->nLibraryHandle ) );
	}

	const char *Error( int nHandle )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 || !library.bLoaded )
			return "no such cloud sync job";
		return library.pfnError( pJob->nLibraryHandle );
	}

	void Cancel( int nHandle )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 || !library.bLoaded )
			return;
		library.pfnCancel( pJob->nLibraryHandle );
	}

	void Release( int nHandle )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 )
			return;
		if ( library.bLoaded )
			library.pfnRelease( pJob->nLibraryHandle );
		std::memset( pJob, 0, sizeof *pJob );
	}

	void Shutdown()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return;
		library.pfnShutdown();
		std::memset( s_jobs, 0, sizeof s_jobs );
	}

	bool CredentialsPresent()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return false;
		return library.pfnCredsPresent() != 0;
	}

	bool LoadCredentials( char *pszJsonOut, unsigned int nCap )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded || pszJsonOut == 0 || nCap == 0 )
			return false;
		const int nLength = library.pfnCredsLoad( reinterpret_cast<unsigned char *>( pszJsonOut ), nCap );
		if ( nLength < 0 )
		{
			SetLastError2( library.pfnLastError() );
			return false;
		}
		return true;
	}

	bool SaveCredentials( const char *pszJson )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return false;
		if ( library.pfnCredsSave( pszJson ) != 0 )
		{
			SetLastError2( library.pfnLastError() );
			return false;
		}
		return true;
	}

	bool ClearCredentialsSecret()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return false;
		if ( library.pfnCredsClearSecret() != 0 )
		{
			SetLastError2( library.pfnLastError() );
			return false;
		}
		return true;
	}

	int TestConnection()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
		{
			SetLastError2( "cloud sync library is not installed" );
			return -1;
		}
		return WrapLibraryHandle( library.pfnTestConnection( "." ) );
	}

	const char *DiscoveryStatus()
	{
		s_szDiscovery[0] = 0;
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return s_szDiscovery;
		if ( library.pfnDiscoveryStatus( reinterpret_cast<unsigned char *>( s_szDiscovery ), sizeof s_szDiscovery ) < 0 )
			s_szDiscovery[0] = 0;
		return s_szDiscovery;
	}

	bool RefreshDiscovery()
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return false;
		return library.pfnRefreshDiscovery() == 0;
	}

	int ListBackups( const char *pszProfile )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
		{
			SetLastError2( "cloud sync library is not installed" );
			return -1;
		}
		return WrapLibraryHandle( library.pfnBackupList( ".", pszProfile ) );
	}

	bool BackupEntry( int nHandle, unsigned int nIndex, char *pszJsonOut, unsigned int nCap )
	{
		SJob *pJob = JobAt( nHandle );
		SLibrary &library = Library();
		if ( pJob == 0 || !library.bLoaded || pszJsonOut == 0 || nCap == 0 )
			return false;
		return library.pfnBackupEntry( pJob->nLibraryHandle, nIndex, reinterpret_cast<unsigned char *>( pszJsonOut ), nCap ) >= 0;
	}

	int RestoreBackup( const char *pszProfile, const char *pszEntryID, ERestoreMode eMode )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
		{
			SetLastError2( "cloud sync library is not installed" );
			return -1;
		}
		return WrapLibraryHandle( library.pfnBackupRestore( ".", pszProfile, pszEntryID, eMode == RESTORE_FULL ? 1u : 0u ) );
	}

	int ApplyPendingRestore( const char *pszProfile )
	{
		// Deliberately not gated on Available(): this is a local file
		// operation, and a restore already downloaded has to finish with
		// the feature off and rclone gone. Only a missing library makes it
		// a quiet no-op.
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return 0;
		const int nResult = library.pfnApplyPendingRestore( pszProfile );
		if ( nResult < 0 )
			SetLastError2( library.pfnLastError() );
		return nResult;
	}

	int UndoRestore( const char *pszProfile )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
		{
			SetLastError2( "cloud sync library is not installed" );
			return -1;
		}
		const int nHandle = library.pfnRestoreUndo( ".", pszProfile );
		if ( nHandle < 0 )
			SetLastError2( library.pfnLastError() );
		return WrapLibraryHandle( nHandle );
	}

	EUndoAvailability UndoAvailability( const char *pszProfile )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded )
			return UNDO_NONE;
		return static_cast<EUndoAvailability>( library.pfnRestoreUndoAvailable( pszProfile ) );
	}

	const char *LastError()
	{
		return s_szLastError;
	}
}
