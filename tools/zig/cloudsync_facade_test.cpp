// Facade smoke test: NCloudSync over the real dynamically-loaded library.
//
// Two modes, chosen by argv[1] and driven as two build-graph run steps:
//   absent  - run from a directory without CloudSync; every entry point must
//             degrade (Available() false, jobs refuse, apply no-ops) rather
//             than crash or fail to link.
//   present - run from the directory holding the freshly built library; the
//             whole surface must be callable, including a Begin whose job
//             fails cleanly on a machine with no credentials configured.
//
// The enum pinning is compile-time: the facade's values must match the ABI
// numbers the library asserts on its own side.

#include "../../Sources/src/Main/CloudSyncFacade.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

static_assert( NCloudSync::STATE_IDLE == 0, "state pin" );
static_assert( NCloudSync::STATE_STARTING == 1, "state pin" );
static_assert( NCloudSync::STATE_PAIRING == 2, "state pin" );
static_assert( NCloudSync::STATE_SYNCING == 3, "state pin" );
static_assert( NCloudSync::STATE_DONE == 4, "state pin" );
static_assert( NCloudSync::STATE_FAILED == 5, "state pin" );
static_assert( NCloudSync::STATE_TESTING == 6, "state pin" );
static_assert( NCloudSync::OUTCOME_NONE == 0, "outcome pin" );
static_assert( NCloudSync::OUTCOME_PAIRED == 1, "outcome pin" );
static_assert( NCloudSync::OUTCOME_SYNCED == 2, "outcome pin" );
static_assert( NCloudSync::OUTCOME_FAILED == 3, "outcome pin" );
static_assert( NCloudSync::OUTCOME_CONNECTION_OK == 4, "outcome pin" );
static_assert( NCloudSync::OUTCOME_BACKUPS_LISTED == 5, "outcome pin" );
static_assert( NCloudSync::OUTCOME_RESTORE_STAGED == 6, "outcome pin" );
static_assert( NCloudSync::OUTCOME_UNDO_DONE == 7, "outcome pin" );
static_assert( NCloudSync::UNDO_NONE == 0, "undo pin" );
static_assert( NCloudSync::UNDO_CANCELLABLE == 1, "undo pin" );
static_assert( NCloudSync::UNDO_REINSTATABLE == 2, "undo pin" );
static_assert( NCloudSync::UNDO_BUSY == 3, "undo pin" );
static_assert( NCloudSync::RESTORE_MERGE == 0, "mode pin" );
static_assert( NCloudSync::RESTORE_FULL == 1, "mode pin" );

static int failures = 0;

static void check( bool bCondition, const char *pszWhat )
{
	if ( bCondition )
		return;
	std::fprintf( stderr, "cloudsync-facade-test: FAILED %s\n", pszWhat );
	failures += 1;
}

static void sleep_ms( unsigned int nMs )
{
#ifdef _WIN32
	Sleep( nMs );
#else
	usleep( nMs * 1000u );
#endif
}

static void run_absent()
{
	check( !NCloudSync::Available(), "absent: Available is false" );
	check( NCloudSync::Begin( "hero" ) == -1, "absent: Begin refuses" );
	check( NCloudSync::LastError()[0] != 0, "absent: and names the reason" );
	check( NCloudSync::Poll( -1 ) == NCloudSync::STATE_FAILED, "absent: Poll of nothing is failed" );
	check( NCloudSync::Outcome( 0 ) == NCloudSync::OUTCOME_FAILED, "absent: Outcome of nothing is failed" );
	check( NCloudSync::Error( 0 )[0] != 0, "absent: Error of nothing is readable" );
	// A restore already downloaded cannot exist without the library that
	// downloaded it, so the quiet no-op is the honest answer.
	check( NCloudSync::ApplyPendingRestore( "hero" ) == 0, "absent: ApplyPendingRestore no-ops" );
	check( !NCloudSync::CredentialsPresent(), "absent: no credentials" );
	check( NCloudSync::DiscoveryStatus()[0] == 0, "absent: discovery is empty" );
	check( NCloudSync::UndoAvailability( "hero" ) == NCloudSync::UNDO_NONE, "absent: nothing to undo" );
	check( NCloudSync::TestConnection() == -1, "absent: the probe refuses" );
	check( NCloudSync::ListBackups( "hero" ) == -1, "absent: the listing refuses" );
	NCloudSync::Cancel( 0 );
	NCloudSync::Release( 0 );
	NCloudSync::Shutdown();
	NCloudSync::Shutdown();
}

static void run_present()
{
	// The library is here; whether rclone is depends on the machine, so
	// Available() has no asserted value - only that the call answers.
	const bool bAvailable = NCloudSync::Available();
	(void)bAvailable;

	// Discovery answers a document either way.
	const char *pszStatus = NCloudSync::DiscoveryStatus();
	check( pszStatus != 0, "present: discovery status answers" );
	check( std::strstr( pszStatus, "\"found\":" ) != 0, "present: and carries found" );

	// Local surfaces work without any credentials or daemon.
	check( !NCloudSync::CredentialsPresent(), "present: fresh directory has no credentials" );
	check( NCloudSync::ApplyPendingRestore( "hero" ) == 0, "present: nothing staged applies as 0" );
	check( NCloudSync::UndoAvailability( "hero" ) == NCloudSync::UNDO_NONE, "present: nothing to undo" );

	// A job on a machine with no credentials fails cleanly through the full
	// handle surface: begin, poll to rest, a readable error, release.
	const int nHandle = NCloudSync::Begin( "hero" );
	check( nHandle >= 0, "present: Begin hands out a handle" );
	if ( nHandle >= 0 )
	{
		NCloudSync::EState eState = NCloudSync::STATE_IDLE;
		for ( int i = 0; i < 1200; ++i )
		{
			eState = NCloudSync::Poll( nHandle );
			if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
				break;
			sleep_ms( 50 );
		}
		check( eState == NCloudSync::STATE_FAILED, "present: the credential-less job fails rather than hangs" );
		check( NCloudSync::Error( nHandle )[0] != 0, "present: with a readable error" );
		NCloudSync::Release( nHandle );
		check( NCloudSync::Poll( nHandle ) == NCloudSync::STATE_FAILED, "present: a released handle reports failed" );
	}

	NCloudSync::Shutdown();
	NCloudSync::Shutdown();
}

int main()
{
	// The build runner invokes this with -fentry=main, which skips the CRT's
	// argv setup entirely - so the mode travels by environment variable.
	const char *pszMode = std::getenv( "BK_FACADE_MODE" );
	if ( pszMode != 0 && std::strcmp( pszMode, "present" ) == 0 )
		run_present();
	else
		run_absent();
	return failures == 0 ? 0 : 1;
}
