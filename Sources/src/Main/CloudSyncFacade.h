#pragma once

// C++ facade over the cloud sync library. Game code talks profiles, states
// and outcomes; Zig, JSON internals and rclone stay behind this header. The
// library is loaded on first use, so a build - or an installed game -
// without CloudSync still links and simply reports Available() == false.
//
// Every string returned here is facade-owned and valid until the next call
// on the same handle (module-wide strings until the next call at all); the
// caller copies what it wants to keep. No call here blocks on the network:
// jobs run on the library's worker thread and are observed through Poll.

namespace NCloudSync
{
	// Pinned against the library's ABI on both sides: the library asserts
	// these numbers at compile time, and the facade test asserts the enums.
	enum EState
	{
		STATE_IDLE = 0,
		STATE_STARTING = 1,
		STATE_PAIRING = 2,
		STATE_SYNCING = 3,
		STATE_DONE = 4,
		STATE_FAILED = 5,
		STATE_TESTING = 6,
	};

	enum EOutcome
	{
		OUTCOME_NONE = 0,
		OUTCOME_PAIRED = 1,
		OUTCOME_SYNCED = 2,
		OUTCOME_FAILED = 3,
		OUTCOME_CONNECTION_OK = 4,
		OUTCOME_BACKUPS_LISTED = 5,
		OUTCOME_RESTORE_STAGED = 6,
		OUTCOME_UNDO_DONE = 7,
	};

	enum EUndoAvailability
	{
		UNDO_NONE = 0,
		// A downloaded-but-unapplied restore exists and can be discarded.
		UNDO_CANCELLABLE = 1,
		// An applied restore can be reversed. The UI names these two
		// separately: discarding what has not happened is not the same act
		// as reversing what has.
		UNDO_REINSTATABLE = 2,
		// A job holds the operation slot; try again when it settles.
		UNDO_BUSY = 3,
	};

	enum ERestoreMode
	{
		// Every setting from the backup except the local display keys.
		RESTORE_MERGE = 0,
		// Verbatim; survivable but warn first.
		RESTORE_FULL = 1,
	};

	// True when the library is loadable and a usable rclone was discovered.
	bool Available();

	// One sync run for the profile: the first ever run pairs, every later
	// one syncs. bBackupConfig asks for a settings snapshot after a clean
	// finish (the Cloud.Config.Backup option; the caller owns option state).
	// Returns a handle for Poll/Outcome/Error/Release, or -1 with the reason
	// in LastError().
	int Begin( const char *pszProfile, bool bBackupConfig = false );
	EState Poll( int nHandle );
	EOutcome Outcome( int nHandle );
	const char *Error( int nHandle );
	void Cancel( int nHandle );
	void Release( int nHandle );
	// Stops the worker and the daemon; bounded, idempotent, safe mid-sync.
	void Shutdown();

	// Credentials (dialog surface). Load writes a JSON document the dialog
	// renders - the secret never comes back, only whether one is stored -
	// and Save merges an omitted secret rather than clearing it. Clearing
	// is the separate, deliberate call.
	bool CredentialsPresent();
	bool LoadCredentials( char *pszJsonOut, unsigned int nCap );
	bool SaveCredentials( const char *pszJson );
	bool ClearCredentialsSecret();
	// A pollable probe of the configured remote; on failure the handle's
	// Error() text begins with the classified outcome name.
	int TestConnection();

	// Discovery, for the settings dialog: { found, path, version, reason }.
	const char *DiscoveryStatus();
	bool RefreshDiscovery();

	// Backups and restore. Entries read back as JSON documents
	// { id, host, timestamp, size } the browser renders; a restore is
	// staged by a pollable download and applied at the next startup.
	int ListBackups( const char *pszProfile );
	bool BackupEntry( int nHandle, unsigned int nIndex, char *pszJsonOut, unsigned int nCap );
	int RestoreBackup( const char *pszProfile, const char *pszEntryID, ERestoreMode eMode );

	// Purely local - no daemon, no network, no credentials - and cheap when
	// nothing is staged, so startup calls it unconditionally before the
	// profile config is read. Works with the feature disabled and rclone
	// missing; only a missing library makes it a no-op. Returns 1 when a
	// stage was applied, 0 when nothing is staged, -1 on a hard error.
	int ApplyPendingRestore( const char *pszProfile );

	// Undo the restore state: cancel a pending stage, or stage the undo
	// snapshot back for the next startup. Pollable like a sync.
	int UndoRestore( const char *pszProfile );
	EUndoAvailability UndoAvailability( const char *pszProfile );

	// The most recent failure from any facade call, empty after a success.
	const char *LastError();
}
