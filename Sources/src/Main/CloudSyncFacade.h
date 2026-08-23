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
		OUTCOME_CATALOGUE_READY = 8,
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
	// The per-field clear the generic schema needs: a backend can hold
	// several secrets, and the argument-free clear above wipes every
	// withheld field at once. Clearing an absent field succeeds.
	bool ClearCredentialsOption( const char *pszName );
	// The persisted pairing fingerprint. Returns its length; the string was
	// written only when that length is smaller than nCap (otherwise call
	// again with nCap = length + 1). -1 when none is stored.
	int CredentialsFingerprint( char *pszOut, unsigned int nCap );

	// The provider catalogue, for the generic credentials form.
	// EnsureCatalogue returns CATALOGUE_CACHED when the cache already
	// matches the discovered rclone (read it now), a pollable handle while
	// a fetch job fills it (OUTCOME_CATALOGUE_READY on done), or -1. The
	// two readers share the required-size contract: the return value is the
	// document length, written only when smaller than nCap.
	const int CATALOGUE_CACHED = -2;
	int EnsureCatalogue();
	int CatalogueProviders( char *pszJsonOut, unsigned int nCap );
	int CatalogueOptions( const char *pszBackend, char *pszJsonOut, unsigned int nCap );
	// The form model for one backend under one selected provider:
	// { backend, provider, basic:[fields], advanced:[fields] }, each field
	// carrying role/name/label/help/widget/kind/flags/placeholder and its
	// provider-filtered examples. Rebuild by calling again with the new
	// provider; typed values never cross — preserving them is the dialog's
	// job, by field name. Same required-size contract as the readers above.
	int CatalogueForm( const char *pszBackend, const char *pszProvider, char *pszJsonOut, unsigned int nCap );
	// { destinations: ["drive", "s3", ...] } — unhidden candidates sorted
	// alphabetically plus pszConfigured (the backend already saved, "" for
	// none), which stays offered whatever the filter thinks of it.
	int CatalogueDestinations( const char *pszConfigured, char *pszJsonOut, unsigned int nCap );
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
