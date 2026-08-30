# Cloud Provider Row Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Cloud tab's Provider row the cloud-sync switch and backend selector, put Config… and Backups… inside the Cloud box, and run a sync only when the saved credentials name the chosen backend.

**Architecture:** `Cloud.Provider` holds `Off` or an rclone backend id and replaces `Cloud.Enabled`; the credentials document keeps the backend's configuration. The three lifecycle gates in `GameMain.cpp` require the row and the document to agree and publish an `unconfigured` indicator state otherwise. The settings screen fills the Provider row from the cloud facade's destination list through a per-row override on `COptionsListWrapper`, rebuilds the Cloud division whenever the row changes, and the credentials dialog takes its backend from the row instead of its own chooser.

**Tech Stack:** C++17 legacy game modules (`Sources/src/Game`, `Main`, `GameTT`, `StreamIOZig`); the Zig 0.16 CloudSync library with its C ABI (`Sources/src/CloudSync`); UI layout XML under `Data/UI`; UTF-16LE text files under `Data/Textes`; `zig build` from the repository root; the `BK_AUTO_UI` headless harness for game-side verification.

**Spec:** `docs/superpowers/specs/2026-08-30-cloud-provider-row-design.md`

## Global Constraints

- Build and run only from the repository root: `zig build install-game -Dtarget=aarch64-macos --release=fast`; the game runs from `zig-out/game/macos/arm64/release` (`./Game -windowed -profile=<name>`). Data edits need the same install step to be staged.
- Zig/ABI suites that must stay green: `zig build test-cloudsync-abi test-cloudsync-facade test-cloudsync-creds test-cloudsync-catalogue test-cloudsync-form test-cloudsync-worker test-cloudsync-engine` (creds 19, catalogue 20, form 9, worker 17, engine 20 before this plan).
- Text files under `Data/Textes` are UTF-16LE with a BOM and no trailing newline. Write them with this zsh helper, defined once per shell: `bk_text() { { printf '\xff\xfe'; printf '%s' "$2" | iconv -f UTF-8 -t UTF-16LE; } > "$1"; }` — usage `bk_text path/file.txt 'Text'`.
- Exact copy: indicator `Cloud: storage not set up - Settings > Cloud > Config...`; button `Config...`; row `Back up settings after sync`; Provider tooltip `Off, or the storage service to sync this profile with. Set it up under Config...` (three ASCII dots, matching `Storage...` and `Backups...`).
- Option values: the literal `Off` (this spelling) plus rclone backend ids exactly as the facade's destination list returns them (`s3`, `webdav`, `google cloud storage`). The Provider row's list must always contain the row's current value and the saved credentials' backend (spec: the entry-0 trap).
- No provider names in source beyond the fallback strings this plan removes.
- New C ABI export ordinal: `@34` in both `CloudSync.def` and `CloudSync.x64.def`.
- Commit messages: `area: lowercase summary`, body explaining why, ending with `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- The three loopback services of P04-M01 (`/tmp/bk-p04`, credentials in `/tmp/bk-p04/credentials.env`, MinIO on 19100, WebDAV 19200, SFTP 19300) are the test remotes for the game-side verification; `lsof -nP -iTCP:19100 -iTCP:19200 -iTCP:19300 -sTCP:LISTEN` shows whether they are up, `open /tmp/bk-p04/<minio|webdav|sftp>.command` restarts one.

---

## File structure

| File | Responsibility in this plan |
|---|---|
| `Sources/src/CloudSync/cloudsync.zig` | new export `bk_cloudsync_creds_backend` |
| `Sources/src/CloudSync/CloudSync.def`, `CloudSync.x64.def` | export tables, ordinal 34 |
| `tools/zig/cloudsync_abi_test.cpp` | ABI consumer checks for the export (absent/present) |
| `Sources/src/Main/CloudSyncFacade.h`, `.cpp` | `NCloudSync::CredentialsBackend` |
| `tools/zig/cloudsync_facade_test.cpp` | facade checks (absent/present) |
| `Data/Configs/defconf.cfg` | `Cloud.Enabled` removed; `Cloud.Provider` values/default |
| `Data/Textes/Options/Cloud.*.txt` | option names and tooltips |
| `Data/Textes/UI/CloudSync/unconfigured.txt` | the new indicator line |
| `Data/Textes/UI/CloudCredentials/open_button.txt` | `Config...` |
| `Sources/src/StreamIOZig/options_bridge.cpp`, `legacy_bridge.cpp` | `GetCloudProvider` fallback fill |
| `Sources/src/GameTT/OptionEntryWrapper.h`, `.cpp` | per-row drop-value override; generated-text list |
| `Sources/src/Game/GameMain.cpp` | gates, raw scanner, unconfigured publish, recheck |
| `Sources/src/GameTT/MainMenu.cpp` | `unconfigured` outcome key |
| `Sources/src/GameTT/CloudJson.h` (new) | `SJsonValue`, `JsonParse`, `ReadSizedDocument` shared by two screens |
| `Sources/src/GameTT/InterfaceCloudCredentials.cpp` | uses the header; backend from the row; chooser becomes a label |
| `Sources/src/GameTT/InterfaceOptionsSettings.h`, `.cpp` | Cloud division: filtered rows, Provider values, catalogue fetch, rebuild, buttons, recheck |
| `Data/UI/OptionsSettings.xml` | the two buttons inside the Cloud box |
| `docs/superpowers/evidence/cloud-sync/provider-row.md` (new), `p04-m01-backends.md`, `docs/superpowers/plans/2026-08-21-cloud-provider-coverage/NEXT.md` | evidence and status |

---

### Task 1: `CredentialsBackend` across the ABI and the facade

**Files:**
- Modify: `Sources/src/CloudSync/cloudsync.zig` (after `bk_cloudsync_creds_fingerprint`, which ends near line 631)
- Modify: `Sources/src/CloudSync/CloudSync.def` (after the `bk_cloudsync_config_answer` line), `Sources/src/CloudSync/CloudSync.x64.def` (same)
- Modify: `tools/zig/cloudsync_abi_test.cpp` (declaration block near line 98; the generic-credentials fixture near line 705)
- Modify: `Sources/src/Main/CloudSyncFacade.h:117` (after `CredentialsFingerprint`), `Sources/src/Main/CloudSyncFacade.cpp` (typedef near line 46, struct member near 84, `LoadSymbol` near 145, loaded check near 169, function after `CredentialsFingerprint` near 560)
- Test: `tools/zig/cloudsync_facade_test.cpp` (`run_absent` near line 70, `run_present` near line 100 and the long-root block near 145)

**Interfaces:**
- Produces: `pub export fn bk_cloudsync_creds_backend(out: [*]u8, cap: u32) callconv(.c) i32` — the saved document's `backend` under the `writeSized` required-size contract (returns the length; writes only when `length < cap`); `-1` with `bk_cloudsync_last_error` set when no credentials are saved.
- Produces: `int NCloudSync::CredentialsBackend( char *pszOut, unsigned int nCap )` — same contract; `-1` when the library is absent or nothing is stored. Used by Tasks 2 and 4.

- [ ] **Step 1: Write the failing ABI consumer checks**

In `tools/zig/cloudsync_abi_test.cpp`, next to the existing declaration `int bk_cloudsync_creds_fingerprint(unsigned char *out, unsigned int cap);` add:

```cpp
// The saved document's backend id, same required-size contract; -1 when
// no credentials are saved.
int bk_cloudsync_creds_backend(unsigned char *out, unsigned int cap);
```

In the generic-credentials fixture (the block that begins with `chdir into the generic credentials fixture`), directly after
`check(bk_cloudsync_creds_fingerprint(out, sizeof out) == -1, "the fingerprint of no credentials fails readably");` add:

```cpp
    check(bk_cloudsync_creds_backend(out, sizeof out) == -1,
          "the backend of no credentials fails readably");
```

and directly after that fixture's `check(bk_cloudsync_creds_save(doc) == 0, ...)` line add:

```cpp
    {
        unsigned char backend[64];
        const int length = bk_cloudsync_creds_backend(backend, sizeof backend);
        check(length == 2 && std::strcmp(reinterpret_cast<const char *>(backend), "s3") == 0,
              "the backend of a saved document is its rclone id");
        unsigned char tiny[2];
        check(bk_cloudsync_creds_backend(tiny, sizeof tiny) == 2,
              "a buffer too small reports the required size without writing");
    }
```

- [ ] **Step 2: Run the ABI suite to see it fail**

Run: `zig build test-cloudsync-abi`
Expected: the consumer fails to link — undefined symbol `bk_cloudsync_creds_backend`.

- [ ] **Step 3: Add the export and the two `.def` entries**

In `Sources/src/CloudSync/cloudsync.zig`, after the closing brace of `bk_cloudsync_creds_fingerprint`:

```zig
/// The saved document's backend id — `s3`, `webdav`, `google cloud
/// storage` — under the `writeSized` contract, or -1 when no credentials
/// are saved. The settings screen's Provider row and the lifecycle gates
/// compare this with `Cloud.Provider`: a sync runs only when the two agree,
/// so a row changed casually (an arrow key steps it) can never route a sync
/// at a service whose configuration was never saved.
pub export fn bk_cloudsync_creds_backend(out: [*]u8, cap: u32) callconv(.c) i32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse {
        setError("cloud sync: no credentials are saved");
        return -1;
    };
    defer loaded.deinit();
    clearError();
    return writeSized(out, cap, loaded.creds.backend);
}
```

`Sources/src/CloudSync/CloudSync.def`, after the `bk_cloudsync_config_answer` line:

```
    bk_cloudsync_creds_backend = _bk_cloudsync_creds_backend @34
```

`Sources/src/CloudSync/CloudSync.x64.def`, same place:

```
    bk_cloudsync_creds_backend @34
```

- [ ] **Step 4: Run the ABI suite to see it pass**

Run: `zig build test-cloudsync-abi`
Expected: builds and runs with no `FAILED` lines.

- [ ] **Step 5: Write the failing facade checks**

In `tools/zig/cloudsync_facade_test.cpp`, `run_absent()`, after `check( !NCloudSync::CredentialsPresent(), "absent: no credentials" );`:

```cpp
	char szBackend[64];
	check( NCloudSync::CredentialsBackend( szBackend, sizeof szBackend ) == -1, "absent: no backend" );
```

In `run_present()`, after `check( !NCloudSync::CredentialsPresent(), "present: fresh directory has no credentials" );`:

```cpp
	char szBackend[64];
	check( NCloudSync::CredentialsBackend( szBackend, sizeof szBackend ) == -1, "present: fresh directory has no backend" );
```

In the long-root block, directly after `check( NCloudSync::SaveCredentials( szDoc ), "present: the long-root save succeeds" );`:

```cpp
		const int nBackend = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
		check( nBackend == 2 && std::strcmp( szBackend, "s3" ) == 0, "present: the saved backend reads back as its id" );
```

- [ ] **Step 6: Run the facade suite to see it fail**

Run: `zig build test-cloudsync-facade`
Expected: compile error — `CredentialsBackend` is not a member of `NCloudSync`.

- [ ] **Step 7: Implement the facade call**

`Sources/src/Main/CloudSyncFacade.h`, after the `CredentialsFingerprint` declaration:

```cpp
	// The saved credentials' backend id (required-size contract, plain
	// text, -1 when none is stored). The settings screen's Provider row and
	// the sync gates compare it with Cloud.Provider; they must agree before
	// a sync runs.
	int CredentialsBackend( char *pszOut, unsigned int nCap );
```

`Sources/src/Main/CloudSyncFacade.cpp`:

after `typedef int ( *FnCredsFingerprint )( unsigned char *, unsigned int );`:
```cpp
	typedef int ( *FnCredsBackend )( unsigned char *, unsigned int );
```
after the struct member `FnCredsFingerprint pfnCredsFingerprint;`:
```cpp
		FnCredsBackend pfnCredsBackend;
```
after the `s_library.pfnCredsFingerprint = ...` load line:
```cpp
		s_library.pfnCredsBackend = reinterpret_cast<FnCredsBackend>( LoadSymbol( pModule, "bk_cloudsync_creds_backend" ) );
```
in the `bLoaded` conjunction, change `s_library.pfnCredsFingerprint != 0 && s_library.pfnCredsClearOption != 0 &&` to:
```cpp
			s_library.pfnCredsFingerprint != 0 && s_library.pfnCredsBackend != 0 && s_library.pfnCredsClearOption != 0 &&
```
after the `CredentialsFingerprint` function body:
```cpp
	int CredentialsBackend( char *pszOut, unsigned int nCap )
	{
		SLibrary &library = Library();
		if ( !library.bLoaded || pszOut == 0 || nCap == 0 )
			return -1;
		const int nLength = library.pfnCredsBackend( reinterpret_cast<unsigned char *>( pszOut ), nCap );
		if ( nLength < 0 )
			SetLastError2( library.pfnLastError() );
		return nLength;
	}
```

- [ ] **Step 8: Run both suites to see them pass**

Run: `zig build test-cloudsync-abi test-cloudsync-facade test-cloudsync-creds`
Expected: no `FAILED` lines; the creds suite still reports 19 tests.

- [ ] **Step 9: Commit**

```bash
git add Sources/src/CloudSync/cloudsync.zig Sources/src/CloudSync/CloudSync.def Sources/src/CloudSync/CloudSync.x64.def tools/zig/cloudsync_abi_test.cpp Sources/src/Main/CloudSyncFacade.h Sources/src/Main/CloudSyncFacade.cpp tools/zig/cloudsync_facade_test.cpp
git commit -m "cloudsync: expose the saved credentials' backend

The Provider row is about to name the backend in the option system while
the credentials document keeps its configuration; a sync must run only
when the two agree. bk_cloudsync_creds_backend and the facade's
CredentialsBackend read the document's backend under the required-size
contract the fingerprint already uses.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: The Provider row is the switch — option, texts, fallback fills, gates, indicator

**Files:**
- Modify: `Data/Configs/defconf.cfg:232-257`
- Delete: `Data/Textes/Options/Cloud.Enabled.name.txt`, `Data/Textes/Options/Cloud.Enabled.tooltip.txt`
- Modify: `Data/Textes/Options/Cloud.Provider.tooltip.txt`, `Data/Textes/Options/Cloud.Config.Backup.name.txt`
- Create: `Data/Textes/UI/CloudSync/unconfigured.txt`
- Modify: `Sources/src/StreamIOZig/options_bridge.cpp:385`, `Sources/src/StreamIOZig/legacy_bridge.cpp:1230`
- Modify: `Sources/src/GameTT/OptionEntryWrapper.cpp:27-31`
- Modify: `Sources/src/Game/GameMain.cpp:248-289` (helpers), `:429-451` (startup gate), `:944-950` (recheck, beside the `SkipToOffline` block), `:992-997` (post-save gate), `:1362-1368` (exit gate)
- Modify: `Sources/src/GameTT/MainMenu.cpp:260`

**Interfaces:**
- Consumes: `NCloudSync::CredentialsBackend` (Task 1).
- Produces: option `Cloud.Provider` with value `Off` or a backend id; `ActionFill` name `GetCloudProvider`; global var `CloudSync.Recheck` (set to 1 by the settings screen in Task 4, consumed here); indicator error text `unconfigured`; `GameMain.cpp` statics `CloudSyncOptionValue`, `CloudOptionValue`, `CloudProviderSelected`, `CloudCredentialsMatch`, `PublishCloudUnconfigured`.

There are no unit tests for these paths; the release build plus the harness traces in Step 8 are the verification.

- [ ] **Step 1: Option declarations**

In `Data/Configs/defconf.cfg` delete the whole `Cloud.Enabled` item (the `<item ... Order="15" ...>` block ending with `<KeyName>Cloud.Enabled</KeyName>` and `</item>`). Replace the `Cloud.Provider` item with:

```xml
			<item EditorType="3" Flags="1" Order="16" Type="8" InstantApply="1">
				<Var>Off</Var>
				<Action/>
				<ActionFill>GetCloudProvider</ActionFill>
				<Default Type="8">
					<Var>Off</Var>
				</Default>
				<KeyName>Cloud.Provider</KeyName>
			</item>
```

Replace the comment above the Cloud items (`<!-- Cloud profile sync. Orders start at 15 ...` through `... cannot reach them. -->`) with:

```xml
			<!-- Cloud profile sync. Orders start at 16 so the division is
			     encountered after every existing one and the tab lands fifth;
			     Flags 1 is OPTION_FLAG_GENERIC_OPTION only, keeping the whole
			     division out of the in-mission screen. Cloud.Provider is the
			     switch: Off, or the rclone backend id the profile syncs with -
			     the settings screen fills its list from the cloud facade, and
			     GetCloudProvider here is only the fallback that keeps the
			     stored value selectable. Everything defaults off: cloud sync
			     must cost a fresh install nothing until a player turns it on.
			     No free-text options here - endpoints and credentials live in
			     cloud.credentials. -->
```

- [ ] **Step 2: Texts**

```zsh
bk_text() { { printf '\xff\xfe'; printf '%s' "$2" | iconv -f UTF-8 -t UTF-16LE; } > "$1"; }
git rm -q Data/Textes/Options/Cloud.Enabled.name.txt Data/Textes/Options/Cloud.Enabled.tooltip.txt
bk_text Data/Textes/Options/Cloud.Provider.tooltip.txt 'Off, or the storage service to sync this profile with. Set it up under Config...'
bk_text Data/Textes/Options/Cloud.Config.Backup.name.txt 'Back up settings after sync'
bk_text Data/Textes/UI/CloudSync/unconfigured.txt 'Cloud: storage not set up - Settings > Cloud > Config...'
file Data/Textes/UI/CloudSync/unconfigured.txt   # expect: Unicode text, UTF-16, little-endian
```

- [ ] **Step 3: Fallback fills in both option bridges**

`Sources/src/StreamIOZig/options_bridge.cpp` — replace the line
`else if (fill && std::strcmp(fill, "GetCloudProvider") == 0) { values[0] = "Off"; values[1] = "S3"; values[2] = "WebDAV"; count = 3; }` with:

```cpp
        else if (fill && std::strcmp(fill, "GetCloudProvider") == 0) {
            // The settings screen supplies the real list (Off plus the
            // catalogue's destinations) through COptionsListWrapper's
            // override; this fallback only keeps the stored value selectable,
            // because COptionSelection resolves an absent value to entry 0
            // and an OK press would then turn cloud sync off.
            values[0] = "Off"; count = 1;
            unsigned short cur_type = VT_EMPTY; const char *cur = api.value(state_, name.c_str(), &cur_type);
            if (cur && *cur && !EqualAsciiIgnoreCase(cur, "Off")) { values[1] = cur; count = 2; }
        }
```

`Sources/src/StreamIOZig/legacy_bridge.cpp` — replace
`else if (fill && std::string(fill) == "GetCloudProvider") { values[0] = "Off"; values[1] = "S3"; values[2] = "WebDAV"; count = 3; }` with:

```cpp
        else if (fill && std::string(fill) == "GetCloudProvider") {
            values[0] = "Off"; count = 1;
            unsigned short cur_type = VT_EMPTY; const char *cur = bk_options_value(options_, name.c_str(), &cur_type);
            if (cur && *cur && !EqualAsciiIgnoreCase(cur, "Off")) { values[1] = cur; count = 2; }
        }
```

- [ ] **Step 4: Suppress the missing-text console noise for the row's values**

`Sources/src/GameTT/OptionEntryWrapper.cpp`, `IsGeneratedSelectionText`:

```cpp
static bool IsGeneratedSelectionText( const char *pszOptionName )
{
	return (NStr::CompareAsciiNoCase( pszOptionName, "GFX.Mode" ) == 0) ||
		     (NStr::CompareAsciiNoCase( pszOptionName, "GFX.Monitor" ) == 0) ||
		     (NStr::CompareAsciiNoCase( pszOptionName, "Cloud.Provider" ) == 0);		// rclone ids, shown as they are
}
```

- [ ] **Step 5: GameMain helpers**

Replace `CloudSyncOptionOn` (the live-option reader) and `CloudOptionIsOn` (the raw scanner) in `Sources/src/Game/GameMain.cpp` with this block; the raw scanner's body is unchanged except that it returns the value:

```cpp
// A Cloud.* option's value through the live option system - available once
// the config has been read, unlike the raw-scan path the startup window
// needs. Empty when unset.
static std::string CloudSyncOptionValue( const char *pszKey )
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( pszKey, &var ) )
		return std::string();
	return std::string( (const char*)bstr_t( var ) );
}
static bool CloudSyncOptionOn( const char *pszKey )
{
	return CloudSyncOptionValue( pszKey ) == "ON";
}

// A Cloud.* option's value in the profile's config - by a minimal scan of
// the raw XML, because this is asked before the option system has
// initialised (the whole point of the startup window is that the config has
// not been read yet). Inside an item the live <Var> comes first and the
// <Default> block - with its own inner <Var> - sits between it and
// <KeyName>, so the scan must take the FIRST <Var> after the enclosing
// item's start; the nearest one before the key is always the default.
// Anything missing or malformed is empty.
static std::string CloudOptionValue( const std::string &szConfigPath, const char *pszKey )
{
	std::ifstream file( szConfigPath, std::ios::binary );
	if ( !file )
		return std::string();
	std::string szContent( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );

	const std::string szNeedle = std::string( "<KeyName>" ) + pszKey + "</KeyName>";
	const std::string::size_type nKeyAt = szContent.find( szNeedle );
	if ( nKeyAt == std::string::npos )
		return std::string();
	const std::string::size_type nItemAt = szContent.rfind( "<item", nKeyAt );
	if ( nItemAt == std::string::npos )
		return std::string();
	const std::string::size_type nVarAt = szContent.find( "<Var>", nItemAt );
	if ( nVarAt == std::string::npos || nVarAt > nKeyAt )
		return std::string();
	const std::string::size_type nVarEnd = szContent.find( "</Var>", nVarAt );
	if ( nVarEnd == std::string::npos || nVarEnd > nKeyAt )
		return std::string();
	return szContent.substr( nVarAt + 5, nVarEnd - nVarAt - 5 );
}
static bool CloudOptionIsOn( const std::string &szConfigPath, const char *pszKey )
{
	return CloudOptionValue( szConfigPath, pszKey ) == "ON";
}

// Cloud.Provider is the switch. "Off" - and the pre-row "ON"/"OFF" values a
// profile written before it may still carry - mean cloud sync is off;
// anything else is the rclone backend id the profile syncs with.
static bool CloudProviderSelected( const std::string &szProvider )
{
	return !szProvider.empty() &&
		NStr::CompareAsciiNoCase( szProvider.c_str(), "Off" ) != 0 &&
		NStr::CompareAsciiNoCase( szProvider.c_str(), "On" ) != 0;
}
// The saved credentials must name the chosen backend. The row is changed
// casually (an arrow key steps it), the document only by a deliberate save
// in Config..., and a sync must never run against a service whose setup was
// never saved. Loads the library, never probes for rclone.
static bool CloudCredentialsMatch( const std::string &szProvider )
{
	char szBackend[256];
	const int nLength = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	return nLength > 0 && nLength < (int)sizeof szBackend && szProvider == szBackend;
}
// The indicator's "chosen but not set up" line. No job exists to publish
// from, so the state is written directly; the menu maps the error text to
// textes\ui\cloudsync\unconfigured.
static void PublishCloudUnconfigured()
{
	SetGlobalVar( "CloudSync.State", (int)NCloudSync::STATE_FAILED );
	SetGlobalVar( "CloudSync.Outcome", (int)NCloudSync::OUTCOME_FAILED );
	SetGlobalVar( "CloudSync.Error", "unconfigured" );
	NStr::DebugTrace( "cloud sync: provider chosen but not set up\n" );
}
```

- [ ] **Step 6: The three gates and the recheck**

Startup gate — replace the block from `const std::string szConfigPath = "profiles/" + szProfile + "/config.cfg";` through the closing brace after `startup sync refused` with:

```cpp
		const std::string szConfigPath = "profiles/" + szProfile + "/config.cfg";
		const std::string szProvider = CloudOptionValue( szConfigPath, "Cloud.Provider" );
		if ( CloudProviderSelected( szProvider ) )
		{
			if ( !CloudCredentialsMatch( szProvider ) )
				PublishCloudUnconfigured();
			else if ( CloudOptionIsOn( szConfigPath, "Cloud.Sync.OnStartup" ) && NCloudSync::Available() )
			{
				// Begin only enqueues - the daemon spawn (reaping any orphan
				// from a crashed run first, the P00-M03 identity-checked path)
				// and the run itself happen on the library's worker, and the
				// main loop polls. A slow link can never stall the first frame.
				g_nCloudStartupSync = NCloudSync::Begin( szProfile.c_str(),
					CloudOptionIsOn( szConfigPath, "Cloud.Config.Backup" ) );
				if ( g_nCloudStartupSync >= 0 )
					NStr::DebugTrace( "cloud sync: startup sync begun for \"%s\"\n", szProfile.c_str() );
				else
					NStr::DebugTrace( "cloud sync: startup sync refused: %s\n", NCloudSync::LastError() );
			}
		}
```

Also change the comment above it: `Cloud.Sync.OnStartup lives in that same unread config` stays; append one sentence: `Cloud.Provider is read the same way, and a chosen provider whose credentials are missing or name another backend publishes the unconfigured indicator instead of syncing.`

Recheck — directly after the `SkipToOffline` block (`RemoveGlobalVar( "CloudSync.SkipToOffline" ); ... }`), add:

```cpp
			if ( GetGlobalVar( "CloudSync.Recheck", 0 ) )
			{
				// The settings screen closed: re-evaluate "chosen but not set
				// up" for the indicator. Only while no run holds the handle - a
				// run's own settle owns the state until it lands.
				RemoveGlobalVar( "CloudSync.Recheck" );
				if ( g_nCloudStartupSync < 0 )
				{
					const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
					if ( CloudProviderSelected( szProvider ) && !CloudCredentialsMatch( szProvider ) )
						PublishCloudUnconfigured();
					else if ( std::string( GetGlobalVar( "CloudSync.Error", "" ) ) == "unconfigured" )
					{
						SetGlobalVar( "CloudSync.State", (int)NCloudSync::STATE_IDLE );
						SetGlobalVar( "CloudSync.Error", "" );
					}
				}
			}
```

Post-save gate — replace `if ( CloudSyncOptionOn( "Cloud.Enabled" ) && CloudSyncOptionOn( "Cloud.Sync.OnSave" ) &&` with:

```cpp
					const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
					if ( CloudProviderSelected( szProvider ) && CloudCredentialsMatch( szProvider ) && CloudSyncOptionOn( "Cloud.Sync.OnSave" ) &&
```

Exit gate — replace `const bool bExitSyncWanted = CloudSyncOptionOn( "Cloud.Enabled" ) &&` with:

```cpp
			const std::string szProvider = CloudSyncOptionValue( "Cloud.Provider" );
			const bool bExitSyncWanted = CloudProviderSelected( szProvider ) && CloudCredentialsMatch( szProvider ) &&
```

and the sentence `Honours Cloud.Sync.OnExit` in its comment becomes `Honours Cloud.Sync.OnExit under a chosen and set-up provider`.

- [ ] **Step 7: The indicator key**

`Sources/src/GameTT/MainMenu.cpp`, `CloudFailureTextKey`: change the table's first entry so it reads

```cpp
	static const char *pszOutcomes[] = { "unconfigured", "needs_resync", "too_many_deletes", "name_too_long",
```

and extend the comment above the function with: `"unconfigured" is not a run outcome - the main loop publishes it when a provider is chosen but the saved credentials do not name it.`

- [ ] **Step 8: Build and verify headless**

```zsh
zig build install-game -Dtarget=aarch64-macos --release=fast
cd zig-out/game/macos/arm64/release
```

(a) Fresh profile, provider chosen, no credentials for it. `profiles/cloud.credentials` on this machine holds the backend last saved by P04 (read it: `grep -o '"backend":"[^"]*"' profiles/cloud.credentials`); pick a *different* id below:

```zsh
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=webdav,130:set=Cloud.Sync.OnStartup=ON,160:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
grep -A1 -B7 'Cloud.Provider' profiles/P05/config.cfg | grep '<Var>' | head -1     # expect <Var>webdav</Var>
BK_AUTO_UI="200:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
```
Expected on the second launch: `cloud sync: provider chosen but not set up` and no `startup sync begun`.

(b) Provider equals the saved backend:

```zsh
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=s3,130:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
BK_AUTO_UI="600:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
```
Expected: `cloud sync: startup sync begun for "P05"` and later `sync finished (paired)` (MinIO must be up; substitute the saved backend's id if it is not `s3`).

(c) Provider Off:

```zsh
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=Off,130:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
BK_AUTO_UI="200:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'cloud sync'
```
Expected: no cloud sync line at all on the second launch.

Restore `Cloud.Provider` on P05 to whatever the next task needs (Task 4 starts from `Off`). Run `zig build test-cloudsync-abi test-cloudsync-facade` once more — unchanged.

- [ ] **Step 9: Commit**

```bash
git add Data/Configs/defconf.cfg Data/Textes/Options Data/Textes/UI/CloudSync/unconfigured.txt Sources/src/StreamIOZig/options_bridge.cpp Sources/src/StreamIOZig/legacy_bridge.cpp Sources/src/GameTT/OptionEntryWrapper.cpp Sources/src/Game/GameMain.cpp Sources/src/GameTT/MainMenu.cpp
git commit -m "settings: the Provider row is the cloud sync switch

Cloud.Enabled goes; Cloud.Provider holds Off or the rclone backend id and
gates every sync, together with the saved credentials naming that same
backend. A chosen provider without matching credentials publishes the
unconfigured indicator instead of syncing, at startup and on the settings
screen's recheck. The legacy store's 12-character truncation that forced
the row to ON/OFF is no longer compiled.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: Share the cloud JSON helpers

**Files:**
- Create: `Sources/src/GameTT/CloudJson.h`
- Modify: `Sources/src/GameTT/InterfaceCloudCredentials.cpp:112-292` (the block from the comment `// ---- a small JSON document model` through the end of `JsonParse`) and `:305-330` (`ReadSizedDocument` with its comment)

**Interfaces:**
- Produces: header-only `struct SJsonValue` (unchanged shape: `eType`, `bValue`, `szValue`, `keys`, `children`, `Get`, `Str`, `Bool`), `inline bool JsonParse( const std::string &szDoc, SJsonValue *pOut )`, `template <typename TCall> inline bool ReadSizedDocument( TCall call, std::string *pszOut )`. Consumed by Task 4.

Pure move; no behaviour change.

- [ ] **Step 1: Create the header**

`Sources/src/GameTT/CloudJson.h`:

```cpp
#ifndef __CLOUDJSON_H__
#define __CLOUDJSON_H__
#pragma ONCE
// A small JSON document model for the documents the cloud facade produces
// (the form, the destination list, the credentials), shared by the
// credentials dialog and the settings screen. Objects, arrays, strings,
// numbers, booleans, null; unknown escapes and malformed bytes degrade
// rather than fail. Header-only: the GameTT project list is a .vcxproj the
// build reads, and a header needs no entry there.
#include <string>
#include <vector>

// (paste struct SJsonValue and the static functions JsonSkipSpace,
//  JsonParseString, JsonParseValue, JsonParse here verbatim from
//  InterfaceCloudCredentials.cpp, changing each `static` to `inline`)

// Read a facade document that follows the required-size contract: the
// return value is the length, written only when it fit; otherwise retry
// with the reported size.
template <typename TCall>
inline bool ReadSizedDocument( TCall call, std::string *pszOut )
{
	std::vector<char> buffer( 16384 );
	int nLength = call( &buffer[0], (unsigned int)buffer.size() );
	if ( nLength < 0 )
		return false;
	if ( nLength >= (int)buffer.size() )
	{
		buffer.resize( (size_t)nLength + 1 );
		nLength = call( &buffer[0], (unsigned int)buffer.size() );
		if ( nLength < 0 || nLength >= (int)buffer.size() )
			return false;
	}
	pszOut->assign( &buffer[0], (size_t)nLength );
	return true;
}
#endif // __CLOUDJSON_H__
```

The parenthesised note is an instruction to the implementer, not content of the file: the moved bodies are the exact lines of `InterfaceCloudCredentials.cpp` between `struct SJsonValue` and the closing brace of `JsonParse`, with `static bool`/`static void` becoming `inline bool`/`inline void`.

- [ ] **Step 2: Remove the originals and include the header**

In `InterfaceCloudCredentials.cpp` delete the moved block and the original `ReadSizedDocument` (with its comment), and add `#include "CloudJson.h"` after `#include "../Main/CloudSyncFacade.h"`.

- [ ] **Step 3: Build and prove the dialog unchanged**

```zsh
zig build install-game -Dtarget=aarch64-macos --release=fast
cd zig-out/game/macos/arm64/release
BK_AUTO_UI="40:var=notransition=1,48:cmd=0x100e0104,200:shot,210:cancel,230:exit" ./Game -windowed -profile=P05 2>&1 | grep -i 'BK_AUTO_UI\|cloud'
```
Expected: the dialog opens (the `cloud credentials:` trace lines appear as before) and `autoshot_200_<W>x<H>.rgba` shows the form. Convert a capture with this stdlib-only script, saved as `/tmp/bk-p04/rgba2png.py` (`python3 rgba2png.py in.rgba W H out.png`; if the result is upside down, reverse the row order):

```python
import sys, zlib, struct
src, w, h, dst = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
raw = open(src, 'rb').read()
rows = b''.join(b'\x00' + raw[y * w * 4:(y + 1) * w * 4] for y in range(h))
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
open(dst, 'wb').write(png)
```

- [ ] **Step 4: Commit**

```bash
git add Sources/src/GameTT/CloudJson.h Sources/src/GameTT/InterfaceCloudCredentials.cpp
git commit -m "settings: share the cloud JSON helpers

The settings screen is about to read the facade's destination list for
the Provider row; the parser and the sized-document reader move out of
the credentials dialog into a header both screens include.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: The Cloud tab follows the Provider row

**Files:**
- Modify: `Sources/src/GameTT/OptionEntryWrapper.h` (before `class COptionsListWrapper`, and its members/constructors), `Sources/src/GameTT/OptionEntryWrapper.cpp` (constructors near line 302-316; `InitList`'s `EOET_CLICK_SWITCHES` case near line 374-387)
- Modify: `Sources/src/GameTT/InterfaceOptionsSettings.h`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp` (`Create` loop near line 180-200; `OnChangeDivision` near 236-260; `StepLocal` near 79-95; `Close` near 316-327; `Done` near 105)

**Interfaces:**
- Consumes: `NCloudSync::CredentialsBackend`, `NCloudSync::EnsureCatalogue`, `NCloudSync::CATALOGUE_CACHED`, `NCloudSync::CatalogueDestinations`, `NCloudSync::Poll/Release/STATE_DONE/STATE_FAILED` (facade); `SJsonValue`, `JsonParse`, `ReadSizedDocument` (Task 3); global var `CloudSync.Recheck` (Task 2).
- Produces: `typedef std::map< std::string, std::vector<SOptionDropListValue> > OptionDropOverrides;` and the constructor `COptionsListWrapper( IUIListControl *, OptionDescs &, const int, const OptionDropOverrides &, IOptionSystem * = 0, const bool = false )`; settings-screen members/methods `cloudDescs`, `szCloudProvider`, `cloudDestinations`, `nCatalogueHandle`, `IsCloudProviderOff`, `ReadCloudProvider`, `BuildCloudList`, `BeginCloudCatalogue`, `LoadCloudDestinations`, `RefreshCloudButtons`.

- [ ] **Step 1: The per-row override on the list wrapper**

`OptionEntryWrapper.h` — add `#include <map>` after `#pragma ONCE`; before `class COptionsListWrapper` add:

```cpp
// Values a screen supplies for a droplist row in place of the option
// system's fill - for rows whose list the option bridge cannot build itself.
// The cloud Provider row is the case: its values come from the cloud facade,
// which the streamio library does not link. Keyed by option name.
typedef std::map< std::string, std::vector<SOptionDropListValue> > OptionDropOverrides;
```

Inside `COptionsListWrapper`, after `bool bDisableChange;` add `OptionDropOverrides dropOverrides;`, and after the second constructor declaration add:

```cpp
	COptionsListWrapper( IUIListControl * _pList, OptionDescs & optionDescs, const int _nIDToStartFrom, const OptionDropOverrides &overrides, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
```

`OptionEntryWrapper.cpp` — after the existing two-argument-list constructor add:

```cpp
COptionsListWrapper::COptionsListWrapper( IUIListControl * _pList, OptionDescs & _optionDescs, const int _nIDToStartFrom, const OptionDropOverrides &overrides, IOptionSystem * pSystem, const bool _bDisableChange )
: pList( _pList ), optionsDescs( _optionDescs ), nIDToStartFrom( _nIDToStartFrom ), bDisableChange( _bDisableChange ), pSetOptionSystem( pSystem ), dropOverrides( overrides )
{
	InitList( false );
}
```

In `InitList`, `case EOET_CLICK_SWITCHES:`, replace
`const std::vector<SOptionDropListValue>& values = pSystem->GetDropValues( pDesc->szName );` with:

```cpp
				const OptionDropOverrides::const_iterator itOverride = dropOverrides.find( pDesc->szName );
				const std::vector<SOptionDropListValue>& values = itOverride != dropOverrides.end() ? itOverride->second : pSystem->GetDropValues( pDesc->szName );
```

- [ ] **Step 2: Settings screen header**

`InterfaceOptionsSettings.h` — replace `class COptionsListWrapper;` with

```cpp
#include "../StreamIO/OptionSystem.h"
#include "OptionEntryWrapper.h"
```

and inside the class, after `CPtr<IInputSlider> pWheelScroll;`, add:

```cpp
	OptionDescs cloudDescs;								// every Cloud.* descriptor, kept to rebuild the tab's list
	std::string szCloudProvider;						// Cloud.Provider as last built; a change rebuilds the list
	std::vector<std::string> cloudDestinations;	// the catalogue's destination list, empty until fetched
	int nCatalogueHandle;									// the catalogue fetch job, -1 when idle
	static bool IsCloudProviderOff( const std::string &szValue );
	std::string ReadCloudProvider() const;
	void BuildCloudList();
	void BeginCloudCatalogue();
	void LoadCloudDestinations();
	void RefreshCloudButtons();
```

and extend the constructor's initialiser list with `, nCatalogueHandle( -1 )` after `nCloudDivision( -1 )`.

- [ ] **Step 3: Settings screen source — helpers**

Add after the existing includes: `#include "../Main/CloudSyncFacade.h"`, `#include "CloudJson.h"`, `#include <algorithm>`. Then add these methods (anywhere after `Done()`):

```cpp
// Cloud.Provider is the switch: "Off" - and the pre-row "ON"/"OFF" values a
// profile written before it may still carry - mean cloud sync is off.
bool CInterfaceOptionsSettings::IsCloudProviderOff( const std::string &szValue )
{
	return szValue.empty() ||
		NStr::CompareAsciiNoCase( szValue.c_str(), "Off" ) == 0 ||
		NStr::CompareAsciiNoCase( szValue.c_str(), "On" ) == 0;
}
std::string CInterfaceOptionsSettings::ReadCloudProvider() const
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( "Cloud.Provider", &var ) )
		return std::string();
	return std::string( (const char*)bstr_t( var ) );
}
// The Cloud tab's list. Provider first; the timing rows and the settings
// backup only when a provider is chosen - with Provider Off they would be
// four switches that do nothing. Rebuilt whenever the Provider value changes
// and when the catalogue fetch lands, so the row's list is always the full
// destination list the facade offers.
void CInterfaceOptionsSettings::BuildCloudList()
{
	if ( nCloudDivision < 0 || pUIScreen == 0 )
		return;
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( _E_LIST_BEGIN + nCloudDivision ) );
	if ( pList == 0 )
		return;
	const bool bOff = IsCloudProviderOff( szCloudProvider );

	OptionDescs descs;
	for ( OptionDescs::const_iterator it = cloudDescs.begin(); it != cloudDescs.end(); ++it )
	{
		// Cloud.Enabled is gone from defconf; a profile config written before
		// the Provider row may still carry it, and it must not come back as a
		// stray row.
		if ( NStr::CompareAsciiNoCase( it->szName.c_str(), "Cloud.Enabled" ) == 0 )
			continue;
		const bool bProviderRow = NStr::CompareAsciiNoCase( it->szName.c_str(), "Cloud.Provider" ) == 0;
		if ( bProviderRow || !bOff )
			descs.push_back( *it );
	}

	// The Provider row's list: Off, then the destinations sorted by id, and
	// always the row's own value - COptionSelection resolves an absent value
	// to entry 0, which would turn cloud sync off on the next OK. The saved
	// credentials' backend rides along for the same reason.
	OptionDropOverrides overrides;
	std::vector<SOptionDropListValue> &values = overrides["Cloud.Provider"];
	SOptionDropListValue off;
	off.szProgName = "Off";
	values.push_back( off );
	std::vector<std::string> names = cloudDestinations;
	if ( !bOff )
		names.push_back( szCloudProvider );
	char szBackend[256];
	const int nBackend = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	if ( nBackend > 0 && nBackend < (int)sizeof szBackend )
		names.push_back( szBackend );
	std::sort( names.begin(), names.end() );
	for ( size_t i = 0; i < names.size(); ++i )
	{
		if ( IsCloudProviderOff( names[i] ) || ( i > 0 && names[i] == names[i - 1] ) )
			continue;
		SOptionDropListValue value;
		value.szProgName = names[i];
		values.push_back( value );
	}

	while ( pList->GetNumberOfItems() )
		pList->RemoveItem( 0 );
	optionsLists[nCloudDivision] = new COptionsListWrapper( pList, descs, 100, overrides );
	if ( pList->GetNumberOfItems() > 0 )
		pList->SetSelectionItem( 0 );
	RefreshCloudButtons();
}
// The catalogue fetch, on opening the Cloud tab: a deliberate player action,
// the same rule under which the credentials dialog spawns the daemon. A
// cached list rebuilds at once; a fetch is polled in StepLocal. No catalogue
// is not an error here - the row offers Off and the values it must keep, and
// the full list arrives when the fetch does.
void CInterfaceOptionsSettings::BeginCloudCatalogue()
{
	if ( nCatalogueHandle >= 0 || !cloudDestinations.empty() )
		return;
	const int nResult = NCloudSync::EnsureCatalogue();
	if ( nResult == NCloudSync::CATALOGUE_CACHED )
		LoadCloudDestinations();
	else if ( nResult >= 0 )
		nCatalogueHandle = nResult;
}
// The facade's filtered destination list, with the saved backend riding
// along even when the running rclone's catalogue lacks it.
void CInterfaceOptionsSettings::LoadCloudDestinations()
{
	char szBackend[256];
	const int nBackend = NCloudSync::CredentialsBackend( szBackend, sizeof szBackend );
	const char *pszConfigured = ( nBackend > 0 && nBackend < (int)sizeof szBackend ) ? szBackend : "";
	std::string szListDoc;
	if ( !ReadSizedDocument( [pszConfigured]( char *psz, unsigned int nCap ) { return NCloudSync::CatalogueDestinations( pszConfigured, psz, nCap ); }, &szListDoc ) )
		return;
	SJsonValue list;
	if ( !JsonParse( szListDoc, &list ) )
		return;
	cloudDestinations.clear();
	if ( const SJsonValue *pNames = list.Get( "destinations" ) )
		for ( size_t i = 0; i < pNames->children.size(); ++i )
			if ( pNames->children[i].eType == SJsonValue::T_STRING )
				cloudDestinations.push_back( pNames->children[i].szValue );
	BuildCloudList();
}
// The cloud screens belong to the Cloud tab, and only to a chosen provider:
// Config... sets up the service the row names, Backups... browses what that
// service holds.
void CInterfaceOptionsSettings::RefreshCloudButtons()
{
	const bool bShow = nActive == nCloudDivision && nCloudDivision >= 0 && !IsCloudProviderOff( szCloudProvider );
	const int nShow = bShow ? UI_SW_SHOW_DONT_MOVE_UP : UI_SW_HIDE;
	if ( IUIElement *pCredentials = pUIScreen->GetChildByID( E_BUTTON_CLOUD_CREDENTIALS ) )
		pCredentials->ShowWindow( nShow );
	if ( IUIElement *pBackups = pUIScreen->GetChildByID( E_BUTTON_CLOUD_BACKUPS ) )
		pBackups->ShowWindow( nShow );
}
```

- [ ] **Step 4: Wire the screen's lifecycle**

In `Create()`'s division loop replace

```cpp
		optionsLists.push_back( new COptionsListWrapper( pList, sections[szSection], 100 ) );
```
with
```cpp
		if ( szSection == "Cloud" )
		{
			nCloudDivision = nMaxDivision;
			cloudDescs = sections[szSection];
			szCloudProvider = ReadCloudProvider();
			optionsLists.push_back( CPtr<COptionsListWrapper>() );
			BuildCloudList();
		}
		else
			optionsLists.push_back( new COptionsListWrapper( pList, sections[szSection], 100 ) );
```
and delete the later `if ( szSection == "Cloud" ) nCloudDivision = nMaxDivision;` two lines.

In `OnChangeDivision`, replace the block from `// The cloud screen buttons belong to the Cloud tab alone.` to the end of the function with:

```cpp
	RefreshCloudButtons();
	if ( nActive == nCloudDivision )
		BeginCloudCatalogue();
}
```

In `StepLocal`, after `const bool bResult = CInterfaceInterMission::StepLocal( bAppActive );` add:

```cpp
	// A Provider change - a row click, or the left/right keys - reshapes the
	// Cloud tab: the timing rows and the two buttons follow the value. Read
	// per frame rather than per message so every path that commits the
	// instant-apply row is caught, including clicks the list handles itself.
	if ( nCloudDivision >= 0 && nActive == nCloudDivision )
	{
		const std::string szProvider = ReadCloudProvider();
		if ( szProvider != szCloudProvider )
		{
			szCloudProvider = szProvider;
			BuildCloudList();
		}
	}
	if ( nCatalogueHandle >= 0 )
	{
		const NCloudSync::EState eState = NCloudSync::Poll( nCatalogueHandle );
		if ( eState == NCloudSync::STATE_DONE || eState == NCloudSync::STATE_FAILED )
		{
			NCloudSync::Release( nCatalogueHandle );
			nCatalogueHandle = -1;
			if ( eState == NCloudSync::STATE_DONE )
				LoadCloudDestinations();
		}
	}
```

In `Close()`, as the first statement:

```cpp
	// Whatever changed on the Cloud tab, the menu indicator re-evaluates
	// "chosen but not set up" when it gets the screen back.
	SetGlobalVar( "CloudSync.Recheck", 1 );
```

In `Done()`, before `CInterfaceInterMission::Done();`:

```cpp
	if ( nCatalogueHandle >= 0 )
	{
		NCloudSync::Release( nCatalogueHandle );
		nCatalogueHandle = -1;
	}
```

- [ ] **Step 5: Build and verify headless**

```zsh
zig build install-game -Dtarget=aarch64-macos --release=fast
cd zig-out/game/macos/arm64/release
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=Off,130:exit" ./Game -windowed -profile=P05 >/dev/null 2>&1
BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,240:shot,250:key=RIGHT,330:shot,340:key=RIGHT,420:shot,430:key=LEFT,510:shot,520:ok,560:exit" ./Game -windowed -profile=P05 2>&1 | grep 'BK_AUTO_UI\|cloud'
```
Expected captures (convert with `rgba2png.py`): frame 240 — Cloud tab, one row `Provider  Off`, no buttons; 330 — Provider shows the first id alphabetically (`azureblob` when the catalogue is cached) plus the four rows below and Config.../Backups... visible; 420 — the next id; 510 — back to the first. After exit, `profiles/P05/config.cfg` holds that first id under `Cloud.Provider`.

Row click path: repeat with `250:click=<x>x<y>` on the Provider row's value column instead of `key=RIGHT`, where `x,y` = window pixels of UI point (900, 210) scaled by the window size the shot's file name reports (`autoshot_240_<W>x<H>.rgba`: `x = 900*W/1024`, `y = 210*H/768`). Expected: the same step as a right-arrow.

Entry-0 trap: with `Cloud.Provider=sftp` in `profiles/P05/config.cfg` and the catalogue cache moved aside (`mv cloudsync/providers.json /tmp/bk-p04/providers.json.bak`), run
`BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,200:ok,240:exit"` and confirm `Cloud.Provider` is still `sftp` afterwards; the fetch that ran may have recreated `cloudsync/providers.json` — leave it.

Stale key: confirm no `Cloud.Enabled` row on Johannes's own profile: `BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,240:shot,260:cancel,280:exit" ./Game -windowed` (default profile) — the capture shows Provider first, no "Cloud sync" row.

- [ ] **Step 6: Commit**

```bash
git add Sources/src/GameTT/OptionEntryWrapper.h Sources/src/GameTT/OptionEntryWrapper.cpp Sources/src/GameTT/InterfaceOptionsSettings.h Sources/src/GameTT/InterfaceOptionsSettings.cpp
git commit -m "settings: the Cloud tab follows the Provider row

The row lists Off and every destination the catalogue offers - fetched on
opening the tab, polled like the dialog does - and always keeps its own
value and the saved backend, since an absent value resolves to entry 0
and an OK press would turn cloud sync off. The timing rows and the two
cloud buttons exist only under a chosen provider; the list is rebuilt when
the row changes, and closing the screen asks the menu to recheck.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: Config... and Backups... inside the Cloud box

**Files:**
- Modify: `Data/UI/OptionsSettings.xml:355-356` (element 10013 `WindowPos`), `:380-381` (element 10014 `WindowPos`), and the comment on line 354
- Modify: `Data/Textes/UI/CloudCredentials/open_button.txt`

**Interfaces:**
- Consumes: `RefreshCloudButtons` (Task 4) decides visibility; this task only moves and relabels.

- [ ] **Step 1: Geometry and label**

In `OptionsSettings.xml` set element 10013's `<WindowPos x="529" y="569"/>` and element 10014's `<WindowPos x="745" y="569"/>` (sizes stay 212×66). Replace the comment on element 10013 with:

```xml
			<!--button: cloud credentials dialog (Config...), inside the Cloud list box's lower band beside the backup browser (10014); shown by code only while the Cloud tab is active and a provider is chosen. The list is 462 wide at x=527 with a 28-wide scrollbar and ends at y=641; the two buttons sit 4 apart, right-aligned to the scrollbar-->
```

and element 10014's comment with `<!--button: cloud backup browser (Backups...), the right of the pair-->`.

```zsh
bk_text() { { printf '\xff\xfe'; printf '%s' "$2" | iconv -f UTF-8 -t UTF-16LE; } > "$1"; }
bk_text Data/Textes/UI/CloudCredentials/open_button.txt 'Config...'
```

- [ ] **Step 2: Build, capture, and prove the clicks land**

```zsh
zig build install-game -Dtarget=aarch64-macos --release=fast
cd zig-out/game/macos/arm64/release
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=s3,130:exit" ./Game -windowed -profile=P05 >/dev/null 2>&1
BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,240:shot,260:exit" ./Game -windowed -profile=P05 2>&1 | grep BK_AUTO_UI
```
The capture shows both buttons inside the metal box at its lower right, clear of the five rows, labelled `Config...` and `Backups...`.

Click test — the Config... button's centre is UI point (635, 602); scale to window pixels as in Task 4:
```zsh
BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,240:click=<x>x<y>,320:shot,340:cancel,360:cancel,380:exit" ./Game -windowed -profile=P05 2>&1 | grep 'BK_AUTO_UI\|cloud credentials'
```
Expected: the credentials dialog's trace lines appear after frame 240 and the frame-320 capture shows the dialog.

If the click never reaches the button (no dialog; the list swallowed it), apply the fallback: move the two `<item ... ElementID="10013">` / `10014` blocks into list element 1004's `<Children>` (positions become relative to the list: `x="2" y="439"` and `x="218" y="439"`), and in `InterfaceOptionsSettings.cpp` look them up through the list — in `RefreshCloudButtons` and `CycleNavButton`'s helper, replace `pUIScreen->GetChildByID( E_BUTTON_CLOUD_CREDENTIALS )` with `CloudButton( E_BUTTON_CLOUD_CREDENTIALS )` where

```cpp
IUIElement *CInterfaceOptionsSettings::CloudButton( int nID )
{
	if ( IUIElement *pDirect = pUIScreen->GetChildByID( nID ) )
		return pDirect;
	if ( nCloudDivision < 0 )
		return 0;
	IUIElement *pList = pUIScreen->GetChildByID( _E_LIST_BEGIN + nCloudDivision );
	return pList ? pList->GetChildByID( nID ) : 0;
}
```
(declared in the header next to `RefreshCloudButtons`), and re-run the click test.

- [ ] **Step 3: Commit**

```bash
git add Data/UI/OptionsSettings.xml Data/Textes/UI/CloudCredentials/open_button.txt Sources/src/GameTT/InterfaceOptionsSettings.h Sources/src/GameTT/InterfaceOptionsSettings.cpp
git commit -m "settings: Config and Backups live inside the Cloud box

The two cloud screens sit with the settings they belong to, at the lower
right of the Cloud list, instead of in the left column's spare tab slot;
Storage... becomes Config... now that the row names the service.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
(drop the two `InterfaceOptionsSettings` paths from `git add` when the fallback was not needed.)

---

### Task 6: The credentials dialog sets up the row's backend

**Files:**
- Modify: `Sources/src/GameTT/InterfaceCloudCredentials.cpp` — the misc helpers near `TextOrFallback` (line ~300), `BeginCatalogue` (~405), `ShowCatalogueMissing` (~427), `OnCatalogueReady` (~443-476), `LayoutRows`' chooser label (~699-706), `ProcessMessage` `case E_BUTTON_BACKEND:` (~1014-1035), `StartInterface` (~1252-1285)

**Interfaces:**
- Consumes: option `Cloud.Provider` (Task 2) through `IOptionSystem`.
- Produces: nothing new; `szBackend` is now fixed to the row for the dialog's lifetime.

- [ ] **Step 1: Read the row**

Next to `TextOrFallback` add:

```cpp
// The Provider row's value: the backend this dialog sets up. The row is the
// selector now; the dialog no longer offers one of its own. Empty when the
// row is Off (the button that opens this dialog is hidden then, so only a
// harness opening it directly arrives here) or holds a pre-row ON/OFF.
static std::string ProviderRowValue()
{
	variant_t var;
	if ( !GetSingleton<IOptionSystem>()->Get( "Cloud.Provider", &var ) )
		return std::string();
	const std::string szValue( (const char*)bstr_t( var ) );
	if ( NStr::CompareAsciiNoCase( szValue.c_str(), "Off" ) == 0 || NStr::CompareAsciiNoCase( szValue.c_str(), "On" ) == 0 )
		return std::string();
	return szValue;
}
```

- [ ] **Step 2: Fix the backend at open**

In `StartInterface`, directly after `LoadStored();` add:

```cpp
	szBackend = ProviderRowValue();
	if ( szBackend.empty() )
		szBackend = szStoredBackend;
```

In `OnCatalogueReady` replace `szBackend = szStoredBackend.empty() ? destinations[0] : szStoredBackend;` with:

```cpp
	// The backend is the Provider row's, fixed at open; the destination list
	// is kept only so RebuildForm can tell a backend this catalogue lacks
	// (the empty, cannot-save form) from a missing catalogue.
	if ( szBackend.empty() )
		szBackend = szStoredBackend.empty() ? destinations[0] : szStoredBackend;
```

- [ ] **Step 3: The chooser becomes a label, and stays the retry**

In `LayoutRows`, inside `if ( bCatalogueReady ) { if ( IUIElement *pChooser = ... ) { ... SetWindowText(...); } }` add after the `SetWindowText` call:

```cpp
			pChooser->EnableWindow( false );		// a label now: the row chose
```

In `ShowCatalogueMissing`, inside its `if ( IUIElement *pChooser = ... )` block, add before `SetWindowText`:

```cpp
		pChooser->EnableWindow( true );			// the retry
```

In `ProcessMessage`, `case E_BUTTON_BACKEND:` — keep the `if ( !bCatalogueReady ) { BeginCatalogue(); return true; }` branch and replace the rest of the case (the `if ( !destinations.empty() ) { ... }` walk) with `return true;`.

Update the enum comment on `E_BUTTON_BACKEND` to `// "Service: <id>" label; the retry while there is no catalogue`.

- [ ] **Step 4: Build and verify**

```zsh
zig build install-game -Dtarget=aarch64-macos --release=fast
cd zig-out/game/macos/arm64/release
# saved backend is s3 (P04); the row names webdav -> empty webdav form, nothing from s3 leaks
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=webdav,130:exit" ./Game -windowed -profile=P05 >/dev/null 2>&1
BK_AUTO_UI="40:var=notransition=1,48:cmd=0x100e0104,240:shot,260:cancel,280:exit" ./Game -windowed -profile=P05 2>&1 | grep 'BK_AUTO_UI\|cloud credentials'
# the row names the saved backend -> prefilled form
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=s3,130:exit" ./Game -windowed -profile=P05 >/dev/null 2>&1
BK_AUTO_UI="40:var=notransition=1,48:cmd=0x100e0104,240:shot,260:cancel,280:exit" ./Game -windowed -profile=P05 2>&1 | grep 'BK_AUTO_UI\|cloud credentials'
```
Expected: first capture — `Service: webdav`, greyed, the webdav rows (`Server URL`, `Vendor`, `User`, `Password`, `Folder on the service`) empty; second — `Service: s3`, the stored endpoint/region/root prefilled and secrets masked. In both, pressing the Service label (`msg=10020` at frame 250) changes nothing.

- [ ] **Step 5: Commit**

```bash
git add Sources/src/GameTT/InterfaceCloudCredentials.cpp
git commit -m "settings: the credentials dialog sets up the row's backend

The Provider row chooses; the dialog opens on that backend, prefills only
when it is the saved one, and its former chooser is a label - still the
retry while no catalogue is cached.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: End-to-end evidence and docs

**Files:**
- Create: `docs/superpowers/evidence/cloud-sync/provider-row.md`, `docs/superpowers/evidence/cloud-sync/provider-row/` (PNG captures)
- Modify: `docs/superpowers/evidence/cloud-sync/p04-m01-backends.md` (the `Commands (abbreviated)` section and the FingerprintChanged finding), `docs/superpowers/plans/2026-08-21-cloud-provider-coverage/NEXT.md` (the status paragraph)

**Interfaces:**
- Consumes: everything above, the P04 services, `rgba2png.py` from Task 3.

- [ ] **Step 1: The full cycle through the row**

All three P04 services up. Saved credentials on this machine are the P04 ones (their backend from `grep -o '"backend":"[^"]*"' profiles/cloud.credentials`; the commands below assume `s3`).

```zsh
cd zig-out/game/macos/arm64/release
rm -rf profiles/P06; mkdir -p profiles/P06/saves; cp profiles/P04/saves/*.sav profiles/P06/saves/ 2>/dev/null
# 1. Off: the menu shows no indicator, no cloud trace
BK_AUTO_UI="40:var=notransition=1,200:shot,220:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
# 2. choose s3 through the tab (arrow keys), then open Config..., Test, OK
BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,150:key=RIGHT,230:shot,240:msg=10013,400:msg=10021,700:shot,720:ok,800:ok,840:exit" ./Game -windowed -profile=P06 2>&1 | grep 'BK_AUTO_UI\|cloud'
# 3. sync on startup ON, relaunch: the pairing runs
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Sync.OnStartup=ON,130:exit" ./Game -windowed -profile=P06 >/dev/null 2>&1
BK_AUTO_UI="900:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
/Users/johannes/Projects/src/Blitzkrieg/zig-out/game/macos/arm64/release/rclone --config /tmp/bk-p04/rc.conf lsl minio:bk-saves/profiles/P06 --max-depth 3
```
Step 2 assumes `s3` is the first id right of `Off` that the row reaches with one key press only if the catalogue lists nothing before it — it does not (`azureblob` sorts first), so use as many `key=RIGHT` entries (8 frames apart) as the alphabetical position of the saved backend needs; count them from `rclone config providers | python3 -c 'import json,sys; print(sorted(p["Name"] for p in json.load(sys.stdin)))'` minus the eleven wrapper backends. Expected: frame-230 capture shows `Provider s3` with the four rows and the two buttons; frame-700 capture shows `Connection OK`; the relaunch prints `startup sync begun for "P06"` then `sync finished (paired)`; the listing shows the saves and `.bkprofile`.

- [ ] **Step 2: The unconfigured indicator**

```zsh
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=webdav,130:exit" ./Game -windowed -profile=P06 >/dev/null 2>&1
BK_AUTO_UI="200:shot,220:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
```
Expected: `cloud sync: provider chosen but not set up`; the capture shows `Cloud: storage not set up - Settings > Cloud > Config...` at the lower left of the main menu. Then set it back to `s3` through the settings screen and confirm the line clears on return: `BK_AUTO_UI="40:var=notransition=1,60:settings,120:msg=10011,150:key=RIGHT,...,300:ok,360:shot,380:exit"` (the same key count as Step 1) — the frame-360 capture has no indicator.

- [ ] **Step 3: Write the evidence**

`docs/superpowers/evidence/cloud-sync/provider-row.md`, in the style of `p04-m01-backends.md`: the build and profile, one table of the captures (`off-tab.png`, `s3-tab.png`, `test-ok.png`, `unconfigured.png`, `cleared.png`, converted PNGs in `provider-row/`), the trace lines quoted verbatim, the entry-0 check from Task 4 restated with its result, and a `Human approval` section left `Pending`.

In `p04-m01-backends.md`: under `## Commands (abbreviated)` add the sentence `The chooser walk (msg=10020 ×N) is historical: since the Provider row the backend is chosen on the Cloud tab (msg=10011, then key=RIGHT per step) and the dialog opens on it - see provider-row.md.`; in the FingerprintChanged finding replace `Recorded, not fixed` and the sentence `A player who switches services today is stuck ... own the confirmation flow.` with `**Fixed in 129dcc166**: the credentials save retires pairing records naming another fingerprint, and the next sync takes the NotPaired → pair bootstrap.`

In `NEXT.md`, add after the `P04-M01 is machine-complete` paragraph: `**Provider row (2026-08-30).** Cloud.Enabled is gone; Cloud.Provider is Off or the backend id and gates every sync together with the saved credentials naming it; the Cloud tab lists the catalogue's destinations in the row, shows the timing rows and Config.../Backups... inside its box only under a chosen provider; the dialog opens on the row's backend. Spec docs/superpowers/specs/2026-08-30-cloud-provider-row-design.md, plan docs/superpowers/plans/2026-08-30-cloud-provider-row.md, evidence docs/superpowers/evidence/cloud-sync/provider-row.md.`

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/evidence/cloud-sync/provider-row.md docs/superpowers/evidence/cloud-sync/provider-row docs/superpowers/evidence/cloud-sync/p04-m01-backends.md docs/superpowers/plans/2026-08-21-cloud-provider-coverage/NEXT.md
git commit -m "docs: cloud provider row evidence

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
