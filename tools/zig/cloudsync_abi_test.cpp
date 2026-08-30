// C++ smoke consumer for the CloudSync C ABI.
//
// This is the only proof that the exports are callable from C++ at all: the
// facade in Sources/src/Main does not exist until P06-M01, so until then every
// export is reached from here and nowhere else. A packet that adds an export
// adds a case below in the same commit (see the amendment rule in the plan's
// EXECUTION.md) — an export with no consumer is how a feature ships unwired.
//
// The declarations are written out by hand rather than included from a header
// because there is no cloudsync_c.h yet; when one arrives this file should
// include it instead so the two cannot drift.

// C runtime only, on purpose: MSVC's STL objects (locale, iostreams,
// <thread>) pick a RuntimeLibrary and start a mismatch fight with this
// executable's mixed link line. The game's own C++ has its own runtime
// configuration; this consumer proves the ABI, so it sticks to what every
// configuration shares.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#endif

extern "C" {
unsigned int bk_cloudsync_available(void);
int bk_cloudsync_discovery_status(unsigned char *json_out, unsigned int cap);
int bk_cloudsync_refresh_discovery(void);
void bk_cloudsync_shutdown(void);
const char *bk_cloudsync_last_error(void);
// Sync jobs. States: 0 idle, 1 starting, 2 pairing, 3 syncing, 4 done,
// 5 failed. Outcomes: 0 none, 1 paired, 2 synced, 3 failed. Both pinned by
// comptime asserts on the Zig side.
int bk_cloudsync_begin(const char *job_json);
unsigned int bk_cloudsync_poll(int handle);
unsigned int bk_cloudsync_outcome(int handle);
const char *bk_cloudsync_error(int handle);
// The full redacted failure detail (error line plus support log tail),
// required-size contract: the return is the length (0 when nothing
// failed or the last job succeeded), written only below cap; -1 on an
// unknown handle.
int bk_cloudsync_error_detail(int handle, unsigned char *out, unsigned int cap);
// The interactive config machine as a job: poll like any other handle,
// state 7 (awaiting_input) means config_question holds a field question or
// a consent card (role "consent"; its url is a credential — never log it),
// config_answer resumes a field question. Completion runs the connection
// test.
int bk_cloudsync_config_begin(const char *game_dir);
int bk_cloudsync_config_question(int handle, unsigned char *out, unsigned int cap);
int bk_cloudsync_config_answer(int handle, const char *result);
void bk_cloudsync_cancel(int handle);
void bk_cloudsync_release(int handle);
// Credentials, over profiles/cloud.credentials relative to the working
// directory. creds_load withholds the secret and reports has_secret.
int bk_cloudsync_creds_load(unsigned char *json_out, unsigned int cap);
int bk_cloudsync_creds_save(const char *json);
int bk_cloudsync_creds_clear_secret(void);
unsigned int bk_cloudsync_creds_present(void);
// A pollable probe of the configured remote; on failure the handle's error
// text begins with the classified outcome name.
int bk_cloudsync_test_connection(const char *game_dir);
// Backup listing: a pollable fetch (outcome 5 = backups_listed), then one
// JSON entry per index; -1 past the end.
int bk_cloudsync_backup_list(const char *game_dir, const char *profile);
int bk_cloudsync_backup_entry(int handle, unsigned int index, unsigned char *json_out, unsigned int cap);
// Staged restore: a pollable download (outcome 6 = restore_staged; mode 0
// merge, 1 full), applied later by the purely local apply step (1 applied,
// 0 nothing staged, -1 hard error).
int bk_cloudsync_backup_restore(const char *game_dir, const char *profile, const char *entry_id, unsigned int mode);
int bk_cloudsync_apply_pending_restore(const char *profile);
// Undo: a pollable local job (outcome 7 = undo_done); availability is 0
// nothing, 1 cancellable stage, 2 reinstatable, 3 busy.
int bk_cloudsync_restore_undo(const char *game_dir, const char *profile);
unsigned int bk_cloudsync_restore_undo_available(const char *profile);
// Catalogue: ensure returns -2 when the cache already matches the
// discovered rclone (read it now), a pollable handle when a fetch job
// started (outcome 8 = catalogue_ready), -1 on failure. The two readers
// use the required-size buffer contract: the return value is always the
// document length excluding the NUL; the document was written only when
// that length is smaller than cap, otherwise the buffer is untouched and
// the caller retries with cap = length + 1.
int bk_cloudsync_catalogue_ensure(const char *game_dir);
int bk_cloudsync_catalogue_providers(const char *game_dir, unsigned char *json_out, unsigned int cap);
int bk_cloudsync_catalogue_options(const char *game_dir, const char *backend, unsigned char *json_out, unsigned int cap);
// The persisted pairing fingerprint (required-size contract; plain text,
// not JSON), and the per-field clear the generic schema needs — the no-arg
// clear_secret above wipes every withheld field at once.
int bk_cloudsync_creds_fingerprint(unsigned char *out, unsigned int cap);
// The saved document's backend id, same required-size contract; -1 when
// no credentials are saved.
int bk_cloudsync_creds_backend(unsigned char *out, unsigned int cap);
int bk_cloudsync_creds_clear_option(const char *name);
// The form model for one backend under one selected provider — basic and
// advanced field lists with widget, flags, placeholder and filtered
// examples. Required-size contract like the other catalogue readers. The
// provider crosses the boundary because a build-by-backend-name export
// cannot express provider filtering; the current option map deliberately
// does not, so freshly typed secrets never ride along on a rebuild.
int bk_cloudsync_catalogue_form(const char *game_dir, const char *backend, const char *provider, unsigned char *json_out, unsigned int cap);
// The offered destination list: unhidden candidates sorted alphabetically,
// plus the configured backend whatever the filter thinks of it. Same
// required-size contract.
int bk_cloudsync_catalogue_destinations(const char *game_dir, const char *configured, unsigned char *json_out, unsigned int cap);
}

static int failures = 0;

static void check(bool condition, const char *what)
{
    if (condition) return;
    // Only failures print. A silent success keeps the build output readable
    // and keeps this exe consistent with the zig test binaries, which fail
    // their step outright if they write anything to stderr.
    std::fprintf(stderr, "cloudsync-abi-test: FAILED %s\n", what);
    failures += 1;
}

// Crude, deliberately: the point is to prove the bytes crossing the ABI are
// the JSON document the settings screen will parse, not to ship a parser.
static bool contains(const char *haystack, const char *needle)
{
    return std::strstr(haystack, needle) != nullptr;
}

// -- C-runtime file helpers ---------------------------------------------------
//
// Paths are built with '/' separators throughout; every Win32 A-function and
// CRT call below accepts them.

static void sleep_ms(unsigned int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000u);
#endif
}

static void make_dir(const char *at)
{
#ifdef _WIN32
    _mkdir(at);
#else
    mkdir(at, 0755);
#endif
}

// mkdir -p over forward-slash paths.
static void make_dirs(const char *at)
{
    char partial[1024];
    size_t length = std::strlen(at);
    if (length >= sizeof partial) return;
    for (size_t i = 0; i <= length; ++i)
    {
        if (at[i] == '/' || at[i] == '\0')
        {
            std::memcpy(partial, at, i);
            partial[i] = '\0';
            if (i > 0 && partial[i - 1] != ':') make_dir(partial);
        }
    }
}

static void write_file(const char *at, const char *content)
{
    char parent[1024];
    std::snprintf(parent, sizeof parent, "%s", at);
    if (char *slash = std::strrchr(parent, '/'))
    {
        *slash = '\0';
        make_dirs(parent);
    }
    if (std::FILE *out = std::fopen(at, "wb"))
    {
        std::fwrite(content, 1, std::strlen(content), out);
        std::fclose(out);
    }
}

// Read a small file into `out`; empty string when missing.
static void read_file(const char *at, char *out, size_t cap)
{
    out[0] = '\0';
    std::FILE *in = std::fopen(at, "rb");
    if (in == nullptr) return;
    const size_t got = std::fread(out, 1, cap - 1, in);
    out[got] = '\0';
    std::fclose(in);
}

static bool file_has(const char *at, const char *expected)
{
    char content[4096];
    read_file(at, content, sizeof content);
    return std::strcmp(content, expected) == 0;
}

static bool file_exists(const char *at)
{
    std::FILE *in = std::fopen(at, "rb");
    if (in == nullptr) return false;
    std::fclose(in);
    return true;
}

// Visit every entry name directly under `dir`; returns false when the
// directory cannot be opened. `visit` gets the bare name.
template <typename Visit>
static bool for_each_entry(const char *dir, Visit visit)
{
#ifdef _WIN32
    char pattern[1024];
    std::snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA found;
    HANDLE cursor = FindFirstFileA(pattern, &found);
    if (cursor == INVALID_HANDLE_VALUE) return false;
    do
    {
        if (std::strcmp(found.cFileName, ".") != 0 && std::strcmp(found.cFileName, "..") != 0)
            visit(found.cFileName);
    } while (FindNextFileA(cursor, &found));
    FindClose(cursor);
    return true;
#else
    DIR *cursor = opendir(dir);
    if (cursor == nullptr) return false;
    while (dirent *entry = readdir(cursor))
    {
        if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0)
            visit(entry->d_name);
    }
    closedir(cursor);
    return true;
#endif
}

// Whether any run directory under `trash_root` holds `name` — the run id is
// the worker's to know, so the consumer searches rather than predicts.
static bool trash_holds(const char *trash_root, const char *name)
{
    bool held = false;
    for_each_entry(trash_root, [&](const char *run) {
        char candidate[1024];
        std::snprintf(candidate, sizeof candidate, "%s/%s/%s", trash_root, run, name);
        if (file_exists(candidate)) held = true;
    });
    return held;
}

static bool has_conflict_file(const char *dir)
{
    bool conflicted = false;
    for_each_entry(dir, [&](const char *entry) {
        if (std::strstr(entry, ".conflict") != nullptr) conflicted = true;
    });
    return conflicted;
}

// rm -rf, for the fixture tree only.
static void remove_tree(const char *dir)
{
    for_each_entry(dir, [&](const char *entry) {
        char child[1024];
        std::snprintf(child, sizeof child, "%s/%s", dir, entry);
        if (std::remove(child) != 0)
        {
            remove_tree(child);
#ifdef _WIN32
            _rmdir(child);
#else
            rmdir(child);
#endif
        }
    });
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}

// A JSON string literal from a path: backslashes and quotes escaped, which on
// Windows is every separator.
static void json_escape(const char *raw, char *out, size_t cap)
{
    size_t at = 0;
    for (const char *c = raw; *c != '\0' && at + 2 < cap; ++c)
    {
        if (*c == '\\' || *c == '"') out[at++] = '\\';
        out[at++] = *c;
    }
    out[at] = '\0';
}

static void job_json(const char *kind, const char *path1, const char *game_dir, char *out, size_t cap)
{
    char path1_escaped[1024];
    char game_escaped[1024];
    json_escape(path1, path1_escaped, sizeof path1_escaped);
    json_escape(game_dir, game_escaped, sizeof game_escaped);
    std::snprintf(out, cap,
                  "{\"kind\":\"%s\",\"path1\":\"%s\",\"remote\":\"bkremote\","
                  "\"profile\":\"hero\",\"game_dir\":\"%s\","
                  "\"profile_id\":\"hero-id\",\"remote_fingerprint\":\"alias:cloud\"}",
                  kind, path1_escaped, game_escaped);
}

// Poll a handle to rest at frame-ish cadence. Returns the final state.
static unsigned int poll_to_rest(int handle, unsigned int budget_ms)
{
    unsigned int waited = 0;
    for (;;)
    {
        const unsigned int state = bk_cloudsync_poll(handle);
        if (state == 4u || state == 5u) return state;
        if (waited >= budget_ms) return state;
        sleep_ms(50);
        waited += 50;
    }
}

// Poll until the pending config question contains `needle` — polling the
// question text, not just state 7, because an accepted answer leaves the
// state at awaiting_input while the previous question is still coming
// down.
static bool poll_for_question(int handle, const char *needle, unsigned int budget_ms)
{
    static unsigned char buffer[8192];
    unsigned int waited = 0;
    for (;;)
    {
        std::memset(buffer, 0, sizeof buffer);
        if (bk_cloudsync_config_question(handle, buffer, sizeof buffer) > 0 &&
            contains(reinterpret_cast<const char *>(buffer), needle))
            return true;
        if (bk_cloudsync_poll(handle) == 5u) return false;
        if (waited >= budget_ms) return false;
        sleep_ms(50);
        waited += 50;
    }
}

// -- the bundled binary -------------------------------------------------------
//
// The game ships rclone beside its own executable. The settings screen never
// calls discovery itself — it calls `bk_cloudsync_available` — so the
// out-of-the-box claim can only be proven from here, and the layout that
// proves it is this executable's own directory: the neighbour relationship a
// shipped install has, reproduced by staging a binary next to the consumer
// and emptying `PATH`.

#ifdef _WIN32
static const char *const rclone_exe_name = "rclone.exe";
#else
static const char *const rclone_exe_name = "rclone";
#endif

// The directory holding this executable, spelled the way discovery spells it:
// the image path resolved through realpath, because that is what
// `std.process.executableDirPath` does before taking the dirname.
static bool executable_dir(char *out, size_t cap)
{
    char raw[1024];
#ifdef _WIN32
    const DWORD length = GetModuleFileNameA(nullptr, raw, sizeof raw);
    if (length == 0 || length >= sizeof raw) return false;
#elif defined(__APPLE__)
    char image[1024];
    unsigned int image_size = sizeof image;
    if (_NSGetExecutablePath(image, &image_size) != 0) return false;
    if (realpath(image, raw) == nullptr) return false;
#else
    const ssize_t length = readlink("/proc/self/exe", raw, sizeof raw - 1);
    if (length <= 0) return false;
    raw[length] = '\0';
#endif
    for (char *c = raw; *c != '\0'; ++c)
        if (*c == '\\') *c = '/';
    char *slash = std::strrchr(raw, '/');
    if (slash == nullptr) return false;
    *slash = '\0';
    if (std::strlen(raw) >= cap) return false;
    std::snprintf(out, cap, "%s", raw);
    return true;
}

static void set_path(const char *value)
{
#ifdef _WIN32
    _putenv_s("PATH", value);
#else
    setenv("PATH", value, 1);
#endif
}

// A shell script is a perfectly good rclone for discovery and the version
// gate: both do nothing but run `<binary> version` and read the first line.
// It is also the only way to pin a *chosen* version, which is what lets the
// assertions below name the binary that answered instead of comparing paths.
// POSIX only — a script is not an executable image on Windows.
#ifndef _WIN32
static void write_version_stub(const char *at, const char *version)
{
    char body[256];
    std::snprintf(body, sizeof body,
                  "#!/bin/sh\necho \"rclone %s\"\necho \"- os/version: test\"\n", version);
    write_file(at, body);
    chmod(at, 0755);
}
#endif

// Put a real rclone at `at`: a symlink on POSIX, which discovery follows on
// purpose, and a copy on Windows, where a link needs a privilege this process
// has no business requiring.
static bool stage_real_rclone(const char *real, const char *at)
{
#ifdef _WIN32
    return CopyFileA(real, at, TRUE) != 0;
#else
    return symlink(real, at) == 0;
#endif
}

// The out-of-the-box contract, through the export the settings screen reads:
// a binary staged beside this executable is found and gated with `PATH`
// emptied, an override still beats it, and nothing is left behind afterwards.
static void bundled_out_of_box()
{
    char exe_dir[1024];
    if (!executable_dir(exe_dir, sizeof exe_dir)) return;
    char staged[1200];
    std::snprintf(staged, sizeof staged, "%s/%s", exe_dir, rclone_exe_name);
    // Never clobber a neighbour this test did not create: a genuinely
    // packaged layout, or the leftovers of a run that died mid-test, are not
    // this function's to overwrite or delete.
    if (file_exists(staged)) return;

    char restore_path[8192];
    const char *inherited = std::getenv("PATH");
    std::snprintf(restore_path, sizeof restore_path, "%s", inherited != nullptr ? inherited : "");

    unsigned char status[1024];

#ifndef _WIN32
    // 9.75.3 is a version no rclone has ever printed, which is the point: it
    // can only have come from the file staged a line above, so `found` here
    // cannot be some other rclone the machine happens to carry.
    write_version_stub(staged, "v9.75.3");
    set_path("");
    check(bk_cloudsync_refresh_discovery() == 0, "discovery runs with PATH emptied");
    check(bk_cloudsync_available() == 1u, "the bundled rclone is available with an empty PATH");
    std::memset(status, 0, sizeof status);
    check(bk_cloudsync_discovery_status(status, sizeof status) > 0, "status with an empty PATH");
    const char *bundled_json = reinterpret_cast<const char *>(status);
    check(contains(bundled_json, "\"found\":true"), "and the status agrees it was found");
    check(contains(bundled_json, "\"version\":\"9.75.3\""),
          "the binary beside the executable is the one that answered");
    check(contains(bundled_json, "\"reason\":null"), "a usable bundled copy carries no rejection");

    // The version gate applies to our own copy: a staged binary older than the
    // minimum has to be refused in the settings screen, naming what it found,
    // rather than being discovered at the first sync.
    write_version_stub(staged, "v1.65.2");
    check(bk_cloudsync_refresh_discovery() == 0, "discovery re-runs over the older staged copy");
    check(bk_cloudsync_available() == 0u, "a bundled rclone below the minimum is not available");
    std::memset(status, 0, sizeof status);
    check(bk_cloudsync_discovery_status(status, sizeof status) > 0, "status for the older staged copy");
    const char *old_json = reinterpret_cast<const char *>(status);
    check(contains(old_json, "\"found\":false"), "an old bundled copy is not a working one");
    check(contains(old_json, "\"reason\":\"too_old\""), "and the rejection is typed, not free text");
    check(contains(old_json, "\"version\":\"1.65.2\""), "naming the version it rejected");

    // A player pointing at their own build keeps beating ours. The override
    // travels the way the credentials dialog sends it — `rclone_path` through
    // creds_save — with the bundled copy still in place and still too old, so
    // an override that failed to win would be visible as `too_old`.
    {
        const char *temp_root = std::getenv("TMPDIR");
        if (temp_root == nullptr) temp_root = "/tmp";
        char base[1024];
        std::snprintf(base, sizeof base, "%s/bk-bundled-%u", temp_root,
                      static_cast<unsigned>(std::rand() & 0xffffff));
        make_dirs(base);
        char player[1200];
        std::snprintf(player, sizeof player, "%s/player/%s", base, rclone_exe_name);
        write_version_stub(player, "v9.66.1");

        char old_cwd[1024];
        check(getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
        check(chdir(base) == 0, "chdir into the override fixture");

        char escaped[1200];
        json_escape(player, escaped, sizeof escaped);
        char doc[2048];
        std::snprintf(doc, sizeof doc,
                      "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                      "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                      "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\","
                      "\"secret\":\"unused-here\"},\"rclone_path\":\"%s\"}",
                      escaped);
        check(bk_cloudsync_creds_save(doc) == 0, "saving the player's own rclone succeeds");
        check(bk_cloudsync_available() == 1u, "the override rescues an unusable bundled copy");
        std::memset(status, 0, sizeof status);
        check(bk_cloudsync_discovery_status(status, sizeof status) > 0, "status after the override");
        check(contains(reinterpret_cast<const char *>(status), "\"version\":\"9.66.1\""),
              "the override is the binary that answered, not the bundled copy");

        // Clearing the override hands the search back to the bundled copy,
        // which is still the old one — proof the override was what changed the
        // answer, and that removing it restores the earlier verdict.
        check(bk_cloudsync_creds_save(
                  "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                  "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                  "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\"},"
                  "\"rclone_path\":null}") == 0,
              "clearing the override succeeds");
        check(bk_cloudsync_available() == 0u, "and the old bundled copy is refused again");

        check(chdir(old_cwd) == 0, "chdir back out of the override fixture");
        remove_tree(base);
    }

    std::remove(staged);
#endif

    // The same claim about the file the build actually ships, when there is
    // one to stage: no script, no chosen version, just a real rclone beside
    // this executable and nothing on PATH. Without one this is a silent no-op,
    // exactly like the Zig live cases.
    if (const char *real = std::getenv("BK_TEST_RCLONE"))
    {
        if (real[0] != '\0' && stage_real_rclone(real, staged))
        {
            set_path("");
            check(bk_cloudsync_refresh_discovery() == 0, "discovery runs over the real staged binary");
            check(bk_cloudsync_available() == 1u,
                  "a real bundled rclone is available with an empty PATH");
            std::memset(status, 0, sizeof status);
            check(bk_cloudsync_discovery_status(status, sizeof status) > 0,
                  "status for the real staged binary");
            const char *real_json = reinterpret_cast<const char *>(status);
            check(contains(real_json, "\"found\":true"), "the real staged binary is found");
            check(contains(real_json, "\"reason\":null"), "and passes the version gate");
            check(!contains(real_json, "\"version\":null"), "reporting the version it read");
            std::remove(staged);
        }
    }

    // Leave the machine as it was found: PATH back, nothing staged, and the
    // cache holding this machine's real verdict rather than the fixture's.
    set_path(restore_path);
    check(!file_exists(staged), "the staged binary is gone again");
    check(bk_cloudsync_refresh_discovery() == 0, "discovery recovers once PATH is back");
}

// The credentials exports, driven end to end against a fixture working
// directory: the secret never comes back, omission preserves it, clearing is
// deliberate. Runs everywhere — no daemon involved.
static void creds_contract()
{
    // The exports resolve profiles/cloud.credentials against the working
    // directory, exactly as the game itself resolves profile paths; chdir
    // into a fixture so nothing touches a real profile.
    const char *temp_root = std::getenv("TEMP");
    if (temp_root == nullptr) temp_root = std::getenv("TMPDIR");
    if (temp_root == nullptr) temp_root = "/tmp";
    char base[1024];
    std::snprintf(base, sizeof base, "%s/bk-creds-%u", temp_root,
                  static_cast<unsigned>(std::rand() & 0xffffff));
    for (char *c = base; *c != '\0'; ++c)
        if (*c == '\\') *c = '/';
    make_dirs(base);
    char old_cwd[1024];
#ifdef _WIN32
    check(_getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(_chdir(base) == 0, "chdir into the credentials fixture");
#else
    check(getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(chdir(base) == 0, "chdir into the credentials fixture");
#endif

    check(bk_cloudsync_creds_present() == 0u, "no credentials before the first save");
    unsigned char out[2048];
    check(bk_cloudsync_creds_load(out, sizeof out) == -1, "load without a file fails readably");
    check(bk_cloudsync_last_error()[0] != '\0', "and names the reason");

    static const char *secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    char doc[2048];
    std::snprintf(doc, sizeof doc,
                  "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                  "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                  "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\","
                  "\"secret\":\"%s\"},\"rclone_path\":null}",
                  secret);
    check(bk_cloudsync_creds_save(doc) == 0, "the first save succeeds");
    check(bk_cloudsync_creds_present() == 1u, "credentials are present after saving");

    const int loaded = bk_cloudsync_creds_load(out, sizeof out);
    check(loaded > 0, "load returns the document");
    const char *json = reinterpret_cast<const char *>(out);
    check(!contains(json, secret), "the secret never crosses the ABI outward");
    check(contains(json, "\"has_secret\":true"), "but its presence is reported");
    check(contains(json, "\"endpoint\":\"http://127.0.0.1:9000\""), "the editable fields all return");

    // The dialog's edit: endpoint changed, no secret field at all — the
    // stored secret must survive on disk.
    check(bk_cloudsync_creds_save(
              "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
              "\"endpoint\":\"http://192.168.0.5:9000\",\"bucket\":\"bk\","
              "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\"},"
              "\"rclone_path\":null}") == 0,
          "an endpoint-only save succeeds");
    char on_disk[4096];
    read_file("profiles/cloud.credentials", on_disk, sizeof on_disk);
    check(std::strstr(on_disk, secret) != nullptr, "omission preserved the stored secret");
    check(std::strstr(on_disk, "192.168.0.5") != nullptr, "and the endpoint edit landed");

    check(bk_cloudsync_creds_clear_secret() == 0, "clearing the secret succeeds");
    read_file("profiles/cloud.credentials", on_disk, sizeof on_disk);
    check(std::strstr(on_disk, secret) == nullptr, "the deliberate clear removed it");
    check(bk_cloudsync_creds_load(out, sizeof out) > 0, "load still works after clearing");
    check(contains(reinterpret_cast<const char *>(out), "\"has_secret\":false"),
          "and reports the secret gone");

    // With a real rclone: saving a valid rclone_path must flip discovery to
    // the override without any restart — the dialog's re-discover promise.
    if (const char *rclone = std::getenv("BK_TEST_RCLONE"))
    {
        if (rclone[0] != '\0')
        {
            char escaped[1024];
            json_escape(rclone, escaped, sizeof escaped);
            // A complete credential this time — the secret was cleared above,
            // and a missing secret classifies as auth_failed before any dial;
            // the probe below is about the network class.
            std::snprintf(doc, sizeof doc,
                          "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                          "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                          "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\","
                          "\"secret\":\"%s\"},"
                          "\"rclone_path\":\"%s\"}",
                          secret, escaped);
            check(bk_cloudsync_creds_save(doc) == 0, "saving an rclone override succeeds");
            check(bk_cloudsync_available() == 1u, "the override is discovered with no restart");
            unsigned char status[1024];
            std::memset(status, 0, sizeof status);
            check(bk_cloudsync_discovery_status(status, sizeof status) > 0, "status after the override");
            check(contains(reinterpret_cast<const char *>(status), "\"found\":true"),
                  "and the status agrees");

            // The saved endpoint points at a dead port, which is exactly
            // what the connection probe must say — classified, pollable,
            // and never blocking this thread.
            const int probe = bk_cloudsync_test_connection(base);
            check(probe >= 0, "test_connection hands out a handle");
            if (probe >= 0)
            {
                const unsigned int probed = poll_to_rest(probe, 90000);
                check(probed == 5u, "the dead endpoint probe reports failed");
                const char *why = bk_cloudsync_error(probe);
                if (std::strstr(why, "remote_unreachable") != why)
                    std::fprintf(stderr, "cloudsync-abi-test: probe said: %s\n", why);
                check(std::strstr(why, "remote_unreachable") == why,
                      "the classified outcome leads the error text");
                bk_cloudsync_release(probe);
            }
        }
    }

    bk_cloudsync_shutdown();
#ifdef _WIN32
    check(_chdir(old_cwd) == 0, "chdir back out of the credentials fixture");
#else
    check(chdir(old_cwd) == 0, "chdir back out of the credentials fixture");
#endif
    remove_tree(base);
}

// The generic-schema credential exports, catalogue-free: the persisted
// fingerprint crosses the boundary (the continuity the pairing record
// depends on), one named secret clears while another survives, and nothing
// follows a backend change.
static void generic_creds_contract()
{
    const char *temp_root = std::getenv("TEMP");
    if (temp_root == nullptr) temp_root = std::getenv("TMPDIR");
    if (temp_root == nullptr) temp_root = "/tmp";
    char base[1024];
    std::snprintf(base, sizeof base, "%s/bk-gcreds-%u", temp_root,
                  static_cast<unsigned>(std::rand() & 0xffffff));
    for (char *c = base; *c != '\0'; ++c)
        if (*c == '\\') *c = '/';
    make_dirs(base);
    char old_cwd[1024];
#ifdef _WIN32
    check(_getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(_chdir(base) == 0, "chdir into the generic credentials fixture");
#else
    check(getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(chdir(base) == 0, "chdir into the generic credentials fixture");
#endif

    unsigned char out[2048];
    check(bk_cloudsync_creds_fingerprint(out, sizeof out) == -1,
          "the fingerprint of no credentials fails readably");
    check(bk_cloudsync_creds_backend(out, sizeof out) == -1,
          "the backend of no credentials fails readably");

    // A legacy two-arm document, exactly what an existing install holds.
    static const char *secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    char doc[2048];
    std::snprintf(doc, sizeof doc,
                  "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                  "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                  "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\","
                  "\"secret\":\"%s\"},\"rclone_path\":null}",
                  secret);
    check(bk_cloudsync_creds_save(doc) == 0, "the legacy save migrates");

    {
        unsigned char backend[64];
        const int length = bk_cloudsync_creds_backend(backend, sizeof backend);
        check(length == 2 && std::strcmp(reinterpret_cast<const char *>(backend), "s3") == 0,
              "the backend of a saved document is its rclone id");
        unsigned char tiny[2];
        check(bk_cloudsync_creds_backend(tiny, sizeof tiny) == 2,
              "a buffer too small reports the required size without writing");
    }

    // The continuity claim itself: the exported fingerprint is byte-equal
    // to what the facade's old scraper derived from this document — the
    // string every existing pairing record holds.
    std::memset(out, 0, sizeof out);
    const int print_len = bk_cloudsync_creds_fingerprint(out, sizeof out);
    check(print_len > 0 && print_len < static_cast<int>(sizeof out),
          "the fingerprint crosses the boundary");
    check(std::strcmp(reinterpret_cast<const char *>(out), "http://127.0.0.1:9000/bk") == 0,
          "a migrated document fingerprints as the old scrape did");

    // Required-size contract: a too-small buffer reports the length and
    // writes nothing.
    unsigned char tiny[4];
    tiny[0] = 0x7f;
    check(bk_cloudsync_creds_fingerprint(tiny, sizeof tiny) == print_len,
          "a too-small buffer reports the required length");
    check(tiny[0] == 0x7f, "and leaves the buffer untouched");

    // Per-field clearing: one named secret goes, its sibling survives.
    char on_disk[8192];
    check(bk_cloudsync_creds_clear_option("secret_access_key") == 0,
          "clearing one named field succeeds");
    read_file("profiles/cloud.credentials", on_disk, sizeof on_disk);
    check(std::strstr(on_disk, secret) == nullptr, "the named secret is gone");
    check(std::strstr(on_disk, "AKIAIOSFODNN7EXAMPLE") != nullptr,
          "the sibling secret survives a per-field clear");
    check(bk_cloudsync_creds_clear_option("secret_access_key") == 0,
          "clearing an already-clear field is idempotent");
    // A secret-only change is not a new remote.
    std::memset(out, 0, sizeof out);
    check(bk_cloudsync_creds_fingerprint(out, sizeof out) == print_len,
          "a cleared secret leaves the fingerprint length alone");
    check(std::strcmp(reinterpret_cast<const char *>(out), "http://127.0.0.1:9000/bk") == 0,
          "and the fingerprint itself");

    // Nothing crosses a backend change: a generic save naming a secret the
    // old backend also stores must start from nothing.
    check(bk_cloudsync_creds_save(doc) == 0, "restoring the s3 credential succeeds");
    check(bk_cloudsync_creds_save(
              "{\"backend\":\"sftp\",\"remote_root\":\"saves\","
              "\"options\":{\"host\":\"sftp.example.net\",\"user\":\"player\"},"
              "\"secret_options\":[\"pass\"],\"password_options\":[\"pass\"],"
              "\"rclone_path\":null}") == 0,
          "a generic save for another backend succeeds");
    read_file("profiles/cloud.credentials", on_disk, sizeof on_disk);
    check(std::strstr(on_disk, secret) == nullptr, "the old backend's secret did not follow");
    check(std::strstr(on_disk, "secret_access_key") == nullptr, "nor its field");
    std::memset(out, 0, sizeof out);
    check(bk_cloudsync_creds_fingerprint(out, sizeof out) > 0, "the new backend fingerprints");
    check(std::strstr(reinterpret_cast<const char *>(out), "sftp:saves#") ==
              reinterpret_cast<const char *>(out),
          "as the generic derivation, not a scrape");

    bk_cloudsync_shutdown();
#ifdef _WIN32
    check(_chdir(old_cwd) == 0, "chdir back out of the generic credentials fixture");
#else
    check(chdir(old_cwd) == 0, "chdir back out of the generic credentials fixture");
#endif
    remove_tree(base);
}

// The catalogue exports over a synthetic cache: enumeration, the
// required-size contract, ensure's cached short-circuit, and the
// vendor-change cleanup on the save path. A live rclone extends this with a
// real fetch job below.
static void catalogue_contract()
{
    bk_cloudsync_shutdown(); // the worker binds one game dir per process

    const char *temp_root = std::getenv("TEMP");
    if (temp_root == nullptr) temp_root = std::getenv("TMPDIR");
    if (temp_root == nullptr) temp_root = "/tmp";
    char base[1024];
    std::snprintf(base, sizeof base, "%s/bk-cat-%u", temp_root,
                  static_cast<unsigned>(std::rand() & 0xffffff));
    for (char *c = base; *c != '\0'; ++c)
        if (*c == '\\') *c = '/';
    make_dirs(base);
    char cloudsync_dir[1200];
    std::snprintf(cloudsync_dir, sizeof cloudsync_dir, "%s/cloudsync", base);
    make_dirs(cloudsync_dir);
    char old_cwd[1024];
#ifdef _WIN32
    check(_getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(_chdir(base) == 0, "chdir into the catalogue fixture");
#else
    check(getcwd(old_cwd, sizeof old_cwd) != nullptr, "remember the working directory");
    check(chdir(base) == 0, "chdir into the catalogue fixture");
#endif

    // Discovery pinned to a version stub through the credentials override,
    // so ensure has a running version to compare the cache stamp against.
    // The stub is a shell script, so this pin is POSIX-only; the local
    // reads below need no discovery and run everywhere.
    char creds_doc[4096];
#ifndef _WIN32
    char stub_dir[1200];
    std::snprintf(stub_dir, sizeof stub_dir, "%s/bin", base);
    make_dirs(stub_dir);
    char stub[1400];
    std::snprintf(stub, sizeof stub, "%s/%s", stub_dir, rclone_exe_name);
    write_version_stub(stub, "v9.66.1");
    char stub_escaped[1400];
    json_escape(stub, stub_escaped, sizeof stub_escaped);
    std::snprintf(creds_doc, sizeof creds_doc,
                  "{\"backend\":\"s3\",\"remote_root\":\"bk\","
                  "\"options\":{\"provider\":\"VendorA\",\"endpoint\":\"http://127.0.0.1:9000\","
                  "\"special\":\"special-value\",\"closed\":\"va-only\",\"kept\":\"kept-value\"},"
                  "\"secret_options\":[],\"password_options\":[],"
                  "\"rclone_path\":\"%s\"}",
                  stub_escaped);
    check(bk_cloudsync_creds_save(creds_doc) == 0, "the fixture credentials save");
    check(bk_cloudsync_refresh_discovery() == 0, "discovery re-runs over the stub");
    check(bk_cloudsync_available() == 1u, "and the stub is the running rclone");
#else
    std::snprintf(creds_doc, sizeof creds_doc,
                  "{\"backend\":\"s3\",\"remote_root\":\"bk\","
                  "\"options\":{\"provider\":\"VendorA\",\"endpoint\":\"http://127.0.0.1:9000\","
                  "\"special\":\"special-value\",\"closed\":\"va-only\",\"kept\":\"kept-value\"},"
                  "\"secret_options\":[],\"password_options\":[],"
                  "\"rclone_path\":null}");
    check(bk_cloudsync_creds_save(creds_doc) == 0, "the fixture credentials save");
#endif

    // A synthetic catalogue, stamped as the stub's version: one backend the
    // credentials use, one that only proves enumeration.
    static const char *cache_doc =
        "{\"rclone_version\":\"v9.66.1\",\"providers\":["
        "{\"Name\":\"s3\",\"Description\":\"synthetic s3\",\"Prefix\":\"s3\",\"Options\":["
        "{\"Name\":\"provider\",\"Help\":\"vendor\",\"Type\":\"string\",\"Examples\":["
        "{\"Value\":\"VendorA\",\"Help\":\"\"},{\"Value\":\"VendorB\",\"Help\":\"\"}]},"
        "{\"Name\":\"endpoint\",\"Help\":\"where\",\"Type\":\"string\"},"
        "{\"Name\":\"special\",\"Help\":\"va only\",\"Type\":\"string\",\"Provider\":\"VendorA\"},"
        "{\"Name\":\"closed\",\"Help\":\"closed list\",\"Type\":\"string\",\"Exclusive\":true,\"Examples\":["
        "{\"Value\":\"va-only\",\"Help\":\"\",\"Provider\":\"VendorA\"},"
        "{\"Value\":\"vb-only\",\"Help\":\"\",\"Provider\":\"VendorB\"}]},"
        "{\"Name\":\"kept\",\"Help\":\"editable\",\"Type\":\"string\"},"
        "{\"Name\":\"sec\",\"Help\":\"secret\",\"Type\":\"string\",\"Sensitive\":true,\"IsPassword\":false}"
        "]},"
        "{\"Name\":\"aux\",\"Description\":\"only enumerated\",\"Prefix\":\"aux\",\"Options\":[]},"
        "{\"Name\":\"alias\",\"Description\":\"a wrapper\",\"Prefix\":\"alias\",\"Options\":[]},"
        "{\"Name\":\"veiled\",\"Description\":\"rclone hides it\",\"Prefix\":\"veiled\",\"Hide\":true,\"Options\":[]}"
        "]}";
    {
        std::FILE *f = std::fopen("cloudsync/providers.json", "wb");
        check(f != nullptr, "the synthetic cache writes");
        if (f != nullptr)
        {
            std::fwrite(cache_doc, 1, std::strlen(cache_doc), f);
            std::fclose(f);
        }
    }

    // Enumeration, and the required-size contract on its buffer.
    unsigned char json[16384];
    std::memset(json, 0, sizeof json);
    const int providers_len = bk_cloudsync_catalogue_providers(".", json, sizeof json);
    check(providers_len > 0 && providers_len < static_cast<int>(sizeof json),
          "the provider list crosses the boundary");
    const char *providers_json = reinterpret_cast<const char *>(json);
    check(contains(providers_json, "\"name\":\"s3\""), "the list names the synthetic backend");
    check(contains(providers_json, "\"name\":\"aux\""), "and the enumeration-only one");
    check(contains(providers_json, "\"description\":\"only enumerated\""), "with descriptions");
    unsigned char tiny[8];
    tiny[0] = 0x7f;
    check(bk_cloudsync_catalogue_providers(".", tiny, sizeof tiny) == providers_len,
          "a too-small providers buffer reports the required length");
    check(tiny[0] == 0x7f, "and leaves the buffer untouched");

    // One backend's options, flags and examples included.
    std::memset(json, 0, sizeof json);
    const int options_len = bk_cloudsync_catalogue_options(".", "s3", json, sizeof json);
    check(options_len > 0, "the option list crosses the boundary");
    const char *options_json = reinterpret_cast<const char *>(json);
    check(contains(options_json, "\"name\":\"special\""), "options are enumerated");
    check(contains(options_json, "\"provider\":\"VendorA\""), "with their vendor expressions");
    check(contains(options_json, "\"exclusive\":true"), "their closed-list flag");
    check(contains(options_json, "\"secret\":true"), "and their secret classification");
    check(contains(options_json, "\"value\":\"vb-only\""), "examples ride along");
    check(bk_cloudsync_catalogue_options(".", "no-such-backend", json, sizeof json) == -1,
          "an unknown backend fails readably");
    check(bk_cloudsync_last_error()[0] != '\0', "and names the reason");

    // The cache matches the running stub, so ensure is a local no-op.
    // Needs the POSIX-only version pin above.
#ifndef _WIN32
    check(bk_cloudsync_catalogue_ensure(".") == -2,
          "a matching cache reports cached without a job");
#endif

    // The destination list is the candidate filter's answer: wrappers and
    // rclone-hidden backends out, alphabetical, and a configured backend
    // kept whatever the filter thinks of it.
    std::memset(json, 0, sizeof json);
    const int dests = bk_cloudsync_catalogue_destinations(".", "", json, sizeof json);
    check(dests > 0, "the destination list crosses the boundary");
    const char *dests_json = reinterpret_cast<const char *>(json);
    check(contains(dests_json, "\"aux\"") && contains(dests_json, "\"s3\""),
          "candidates are offered");
    check(!contains(dests_json, "\"alias\""), "a wrapper is not offered");
    check(!contains(dests_json, "\"veiled\""), "an rclone-hidden backend is not offered");
    std::memset(json, 0, sizeof json);
    check(bk_cloudsync_catalogue_destinations(".", "alias", json, sizeof json) > 0,
          "the configured-wrapper list crosses");
    check(contains(reinterpret_cast<const char *>(json), "\"alias\""),
          "a configured backend stays offered whatever the filter thinks");

    // The form model crosses the boundary with the provider argument
    // honoured — a boundary that dropped it would pass every Zig test and
    // still render the unfiltered form.
    std::memset(json, 0, sizeof json);
    const int form_a = bk_cloudsync_catalogue_form(".", "s3", "VendorA", json, sizeof json);
    check(form_a > 0 && form_a < static_cast<int>(sizeof json), "a form crosses the boundary");
    const char *form_a_json = reinterpret_cast<const char *>(json);
    check(contains(form_a_json, "\"name\":\"special\""),
          "the vendor's own option is in its form");
    check(contains(form_a_json, "\"value\":\"va-only\""),
          "with the vendor's example");
    check(!contains(form_a_json, "\"value\":\"vb-only\""),
          "and not the other vendor's");
    check(contains(form_a_json, "\"widget\":\"masked\""),
          "the masked marking survives the boundary");
    check(contains(form_a_json, "\"widget\":\"droplist_closed\""),
          "so does the closed droplist");
    check(contains(form_a_json, "\"role\":\"remote_root\""),
          "and the remote-root field is present");

    std::memset(json, 0, sizeof json);
    check(bk_cloudsync_catalogue_form(".", "s3", "VendorB", json, sizeof json) > 0,
          "the rebuild for the other vendor crosses too");
    const char *form_b_json = reinterpret_cast<const char *>(json);
    check(!contains(form_b_json, "\"name\":\"special\""),
          "an option the new vendor never declares is not offered");
    check(contains(form_b_json, "\"value\":\"vb-only\""),
          "its example set follows the vendor");

    tiny[0] = 0x7f;
    check(bk_cloudsync_catalogue_form(".", "s3", "VendorA", tiny, sizeof tiny) == form_a,
          "a too-small form buffer reports the required length");
    check(tiny[0] == 0x7f, "and leaves the buffer untouched");
    check(bk_cloudsync_catalogue_form(".", "no-such-backend", "", json, sizeof json) == -1,
          "a form for an unknown backend fails readably");

    // The vendor-change cleanup on the save path: same backend, new vendor.
    // The resubmitted values are the point — the cleanup must judge the
    // merged submission, not only what was previously stored.
    check(bk_cloudsync_creds_save(
              "{\"backend\":\"s3\",\"remote_root\":\"bk\","
              "\"options\":{\"provider\":\"VendorB\",\"endpoint\":\"http://127.0.0.1:9000\","
              "\"special\":\"special-value\",\"closed\":\"va-only\",\"kept\":\"kept-value\"},"
              "\"secret_options\":[],\"password_options\":[],"
              "\"rclone_path\":null}") == 0,
          "the vendor-change save succeeds");
    char on_disk[8192];
    read_file("profiles/cloud.credentials", on_disk, sizeof on_disk);
    check(std::strstr(on_disk, "\"provider\":\"VendorB\"") != nullptr, "the vendor change landed");
    check(std::strstr(on_disk, "special-value") == nullptr,
          "an option the new vendor never declares is dropped");
    check(std::strstr(on_disk, "va-only") == nullptr,
          "a closed value the new vendor never offers is cleared");
    check(std::strstr(on_disk, "kept-value") != nullptr, "an editable value survives");

    // With a live rclone: a stale stamp becomes a real fetch job and the
    // enumeration turns into rclone's own catalogue.
    if (const char *real = std::getenv("BK_TEST_RCLONE"))
    {
        if (real[0] != '\0')
        {
            char real_escaped[1200];
            json_escape(real, real_escaped, sizeof real_escaped);
            std::snprintf(creds_doc, sizeof creds_doc,
                          "{\"backend\":\"s3\",\"remote_root\":\"bk\","
                          "\"options\":{\"provider\":\"VendorB\",\"endpoint\":\"http://127.0.0.1:9000\"},"
                          "\"secret_options\":[],\"password_options\":[],"
                          "\"rclone_path\":\"%s\"}",
                          real_escaped);
            check(bk_cloudsync_creds_save(creds_doc) == 0, "pointing discovery at the real rclone");
            check(bk_cloudsync_refresh_discovery() == 0, "discovery re-runs over it");
            const int fetch = bk_cloudsync_catalogue_ensure(".");
            check(fetch >= 0, "a stale stamp becomes a fetch job");
            if (fetch >= 0)
            {
                const unsigned int rested = poll_to_rest(fetch, 90000);
                check(rested == 4u, "the fetch reaches done");
                check(bk_cloudsync_outcome(fetch) == 8u, "with the catalogue_ready outcome");
                // A succeeded job has no failure detail: length 0, not -1.
                unsigned char detail[64] = { 1 };
                check(bk_cloudsync_error_detail(fetch, detail, sizeof detail) == 0,
                      "a succeeded job reports empty failure detail");
                check(detail[0] == '\0', "and writes the empty string");
                bk_cloudsync_release(fetch);
            }
            std::memset(json, 0, sizeof json);
            check(bk_cloudsync_catalogue_providers(".", json, sizeof json) > 0,
                  "the fetched catalogue enumerates");
            check(contains(reinterpret_cast<const char *>(json), "\"name\":\"drive\""),
                  "and is rclone's own list");
            check(bk_cloudsync_catalogue_ensure(".") == -2,
                  "the fresh cache reports cached");

            // Four real backends build across the boundary, and the same
            // vendor-rebuild assertion the Zig tests make holds through
            // the ABI: AWS is offered regions Wasabi never had.
            static const char *backends[4] = { "s3", "webdav", "sftp", "drive" };
            static unsigned char big_json[262144];
            for (int i = 0; i < 4; ++i)
            {
                const int written = bk_cloudsync_catalogue_form(
                    ".", backends[i], "", big_json, sizeof big_json);
                check(written > 0 && written < static_cast<int>(sizeof big_json),
                      "a real backend's form builds across the ABI");
            }
            std::memset(big_json, 0, sizeof big_json);
            check(bk_cloudsync_catalogue_form(".", "s3", "AWS", big_json, sizeof big_json) > 0,
                  "the AWS form builds");
            check(contains(reinterpret_cast<const char *>(big_json), "\"value\":\"us-east-2\""),
                  "AWS is offered its own regions");
            std::memset(big_json, 0, sizeof big_json);
            check(bk_cloudsync_catalogue_form(".", "s3", "Wasabi", big_json, sizeof big_json) > 0,
                  "the Wasabi form builds");
            check(!contains(reinterpret_cast<const char *>(big_json), "\"value\":\"us-east-2\""),
                  "Wasabi is not offered AWS regions");

            // The interactive config machine through the ABI against the
            // real rclone: drive with a client_id of our own asks
            // config_is_local first, the paste-flow answer leads to the
            // token question, and cancel settles the job. The browser
            // dance itself is the evidence run's business — it needs a
            // consent page to land on.
            std::snprintf(creds_doc, sizeof creds_doc,
                          "{\"backend\":\"drive\",\"remote_root\":\"\","
                          "\"options\":{\"client_id\":\"abi-test-client\"},"
                          "\"secret_options\":[],\"password_options\":[],"
                          "\"rclone_path\":\"%s\"}",
                          real_escaped);
            check(bk_cloudsync_creds_save(creds_doc) == 0,
                  "drive credentials for the config walk");
            const int cfg = bk_cloudsync_config_begin(".");
            check(cfg >= 0, "the config machine begins as a job");
            if (cfg >= 0)
            {
                const bool bFirst = poll_for_question(cfg, "config_is_local", 30000);
                if (!bFirst)
                    std::fprintf(stderr, "cloudsync-abi-test: config walk state=%u error: %s\n",
                                 bk_cloudsync_poll(cfg), bk_cloudsync_error(cfg));
                check(bFirst, "the machine's own first question arrives");
                check(bk_cloudsync_config_answer(cfg, "false") == 0,
                      "the answer is accepted");
                check(poll_for_question(cfg, "config_token", 30000),
                      "the paste-flow token question follows");
                bk_cloudsync_cancel(cfg);
                check(poll_to_rest(cfg, 30000) == 5u, "cancel settles the machine");
                check(contains(bk_cloudsync_error(cfg), "Cancelled"), "as cancelled");

                // The flow is bound to its handle like the backup listing:
                // a settled-but-unreleased handle must not read a newer
                // job's consent URL, nor answer a question it never asked.
                const int cfg2 = bk_cloudsync_config_begin(".");
                check(cfg2 >= 0, "a second config job begins");
                if (cfg2 >= 0)
                {
                    unsigned char stale[512];
                    check(bk_cloudsync_config_question(cfg, stale, sizeof stale) == -1,
                          "the old handle cannot read the new flow's question");
                    check(bk_cloudsync_config_answer(cfg, "true") == -1,
                          "nor answer it");
                    check(poll_for_question(cfg2, "config_is_local", 30000),
                          "while the new handle walks its own flow");
                    bk_cloudsync_cancel(cfg2);
                    check(poll_to_rest(cfg2, 30000) == 5u, "and cancels cleanly");
                    bk_cloudsync_release(cfg2);
                }
                bk_cloudsync_release(cfg);
            }
        }
    }

    bk_cloudsync_shutdown();
#ifdef _WIN32
    check(_chdir(old_cwd) == 0, "chdir back out of the catalogue fixture");
#else
    check(chdir(old_cwd) == 0, "chdir back out of the catalogue fixture");
#endif
    remove_tree(base);
}

// The handle contract needs no daemon: invalid handles answer failed with a
// readable error instead of crashing or inventing a second error channel.
static void sync_handle_contract()
{
    check(bk_cloudsync_begin("this is not json") == -1, "begin refuses a malformed document");
    check(bk_cloudsync_last_error()[0] != '\0', "and leaves a readable error");
    check(bk_cloudsync_begin("{\"kind\":\"neither\",\"path1\":\"\",\"remote\":\"\",\"profile\":\"\",\"game_dir\":\"\",\"profile_id\":\"\",\"remote_fingerprint\":\"\"}") == -1,
          "begin refuses an unknown job kind");

    check(bk_cloudsync_poll(-1) == 5u, "poll of an invalid handle reports failed");
    check(bk_cloudsync_poll(9999) == 5u, "poll of an out-of-range handle reports failed");
    check(bk_cloudsync_outcome(-1) == 3u, "outcome of an invalid handle reports failed");
    check(bk_cloudsync_error(-1)[0] != '\0', "error of an invalid handle is a readable string");
    unsigned char detail[16];
    check(bk_cloudsync_error_detail(-1, detail, sizeof detail) == -1,
          "failure detail of an invalid handle is -1");
    check(bk_cloudsync_error_detail(9999, detail, sizeof detail) == -1,
          "failure detail of an out-of-range handle is -1");
    unsigned char question[16];
    check(bk_cloudsync_config_question(-1, question, sizeof question) == -1,
          "config question of an invalid handle is -1");
    check(bk_cloudsync_config_answer(-1, "x") == -1,
          "config answer to an invalid handle is -1");
    bk_cloudsync_cancel(-1);  // must not crash
    bk_cloudsync_release(-1); // must not crash
    bk_cloudsync_release(9999);
}

// The full cycle: pair against an empty remote, diverge both sides, converge
// through a steady sync — entirely through the exports, with the conflict
// file and both trash entries asserted from C++. Needs a real rclone, so it
// runs only when BK_TEST_RCLONE points at one; without it this function is a
// silent no-op exactly like the Zig live tests.
static void sync_full_cycle()
{
    const char *rclone = std::getenv("BK_TEST_RCLONE");
    if (rclone == nullptr || rclone[0] == '\0') return;

    // Discovery searches the game directory and PATH; the pinned binary's
    // directory is prepended to PATH so the ABI finds exactly it.
    {
        char rclone_dir[1024];
        std::snprintf(rclone_dir, sizeof rclone_dir, "%s", rclone);
        for (char *c = rclone_dir; *c != '\0'; ++c)
            if (*c == '\\') *c = '/';
        if (char *slash = std::strrchr(rclone_dir, '/')) *slash = '\0';

        static char path_value[8192];
#ifdef _WIN32
        const char sep = ';';
#else
        const char sep = ':';
#endif
        const char *old_path = std::getenv("PATH");
        std::snprintf(path_value, sizeof path_value, "%s%c%s", rclone_dir, sep,
                      old_path != nullptr ? old_path : "");
#ifdef _WIN32
        _putenv_s("PATH", path_value);
#else
        setenv("PATH", path_value, 1);
#endif
    }
    check(bk_cloudsync_refresh_discovery() == 0, "discovery refresh with the pinned rclone on PATH");
    check(bk_cloudsync_available() == 1u, "the pinned rclone is discovered");

    const char *temp_root = std::getenv("TEMP");
    if (temp_root == nullptr) temp_root = std::getenv("TMPDIR");
    if (temp_root == nullptr) temp_root = "/tmp";
    char base[1024];
    std::snprintf(base, sizeof base, "%s/bk-abi-%u", temp_root,
                  static_cast<unsigned>(std::rand() & 0xffffff));
    for (char *c = base; *c != '\0'; ++c)
        if (*c == '\\') *c = '/';

    char game[1024], cloud[1024], profile[1024], at[1280], at2[1280];
    std::snprintf(game, sizeof game, "%s/game", base);
    std::snprintf(cloud, sizeof cloud, "%s/cloud", base);
    std::snprintf(profile, sizeof profile, "%s/p1", base);
    make_dirs(cloud);
    make_dirs(profile);

    // The named remote the daemon will find, exactly as the credentials
    // packet arranges it in production.
    char conf[2048];
    std::snprintf(conf, sizeof conf, "[bkremote]\ntype = alias\nremote = %s\n", cloud);
    std::snprintf(at, sizeof at, "%s/cloudsync/rclone.conf", game);
    write_file(at, conf);

    // Enough files that the diverge step's two deletes stay inside the 50%
    // ratio the sentinel arithmetic promises.
    std::snprintf(at, sizeof at, "%s/quick.sav", profile);
    write_file(at, "v1");
    std::snprintf(at, sizeof at, "%s/f-local.sav", profile);
    write_file(at, "keep-l");
    std::snprintf(at, sizeof at, "%s/f-remote.sav", profile);
    write_file(at, "keep-r");
    std::snprintf(at, sizeof at, "%s/pad.sav", profile);
    write_file(at, "pad");
    std::snprintf(at, sizeof at, "%s/config.cfg", profile);
    write_file(at, "GFX.Mode = 4k");

    // Pair against the completely empty remote.
    char doc[4096];
    job_json("pair", profile, game, doc, sizeof doc);
    const int pair_handle = bk_cloudsync_begin(doc);
    check(pair_handle >= 0, "begin(pair) hands out a handle");
    if (pair_handle < 0)
    {
        std::fprintf(stderr, "cloudsync-abi-test: begin error: %s\n", bk_cloudsync_last_error());
        return;
    }
    const unsigned int paired = poll_to_rest(pair_handle, 120000);
    if (paired != 4u)
        std::fprintf(stderr, "cloudsync-abi-test: pair error: %s\n", bk_cloudsync_error(pair_handle));
    check(paired == 4u, "pairing reaches done");
    check(bk_cloudsync_outcome(pair_handle) == 1u, "pairing reports the paired outcome");
    check(bk_cloudsync_error(pair_handle)[0] == '\0', "a clean pairing has an empty error");
    std::snprintf(at, sizeof at, "%s/profiles/hero/quick.sav", cloud);
    check(file_has(at, "v1"), "the save reached the remote");
    std::snprintf(at, sizeof at, "%s/profiles/hero/.bkprofile", cloud);
    check(file_exists(at), "the sentinel was delivered up");
    std::snprintf(at, sizeof at, "%s/profiles/hero/config.cfg", cloud);
    check(!file_exists(at), "config.cfg stayed home");
    bk_cloudsync_release(pair_handle);
    check(bk_cloudsync_poll(pair_handle) == 5u, "a released handle reports failed");

    // Diverge: a true conflict on quick.sav (remote first, local newer), one
    // delete on each side.
    std::snprintf(at, sizeof at, "%s/profiles/hero/quick.sav", cloud);
    write_file(at, "v2-remote");
    sleep_ms(2000);
    std::snprintf(at, sizeof at, "%s/quick.sav", profile);
    write_file(at, "v2-local");
    std::snprintf(at, sizeof at, "%s/f-local.sav", profile);
    std::remove(at);
    std::snprintf(at, sizeof at, "%s/profiles/hero/f-remote.sav", cloud);
    std::remove(at);

    job_json("sync", profile, game, doc, sizeof doc);
    const int sync_handle = bk_cloudsync_begin(doc);
    check(sync_handle >= 0, "begin(sync) hands out a handle");
    if (sync_handle < 0) return;
    const unsigned int synced = poll_to_rest(sync_handle, 120000);
    if (synced != 4u)
        std::fprintf(stderr, "cloudsync-abi-test: sync error: %s\n", bk_cloudsync_error(sync_handle));
    check(synced == 4u, "the steady sync reaches done");
    check(bk_cloudsync_outcome(sync_handle) == 2u, "the steady sync reports the synced outcome");
    bk_cloudsync_release(sync_handle);

    // Converged: the newer side of the conflict won on both sides, the loser
    // is preserved as a conflict file, and each side's delete left the other
    // side's copy in the correct run-scoped trash.
    std::snprintf(at, sizeof at, "%s/quick.sav", profile);
    check(file_has(at, "v2-local"), "the newer conflict side survives locally");
    std::snprintf(at, sizeof at, "%s/profiles/hero/quick.sav", cloud);
    check(file_has(at, "v2-local"), "and on the remote");
    std::snprintf(at, sizeof at, "%s/profiles/hero", cloud);
    check(has_conflict_file(profile) || has_conflict_file(at),
          "the conflict loser is preserved as a conflict file");
    std::snprintf(at, sizeof at, "%s/trash/hero", cloud);
    check(trash_holds(at, "f-local.sav"),
          "the locally deleted file's remote copy is in the remote trash");
    std::snprintf(at2, sizeof at2, "%s/.cloudsync-trash", profile);
    check(trash_holds(at2, "f-remote.sav"),
          "the remotely deleted file's local copy is in the local trash");

    // The backup listing, through the same handle machinery: two snapshots
    // planted for one host, fetched and read back entry by entry.
    std::snprintf(at, sizeof at, "%s/config-backups/hero/RigA/20260801T090000Z-0a0a0a0a.cfg", cloud);
    write_file(at, "older-snapshot");
    std::snprintf(at, sizeof at, "%s/config-backups/hero/RigA/20260821T090000Z-0b0b0b0b.cfg", cloud);
    write_file(at, "newer-snapshot");
    const int listing = bk_cloudsync_backup_list(game, "hero");
    check(listing >= 0, "backup_list hands out a handle");
    if (listing >= 0)
    {
        const unsigned int listed = poll_to_rest(listing, 60000);
        if (listed != 4u)
            std::fprintf(stderr, "cloudsync-abi-test: listing error: %s\n", bk_cloudsync_error(listing));
        check(listed == 4u, "the listing fetch reaches done");
        check(bk_cloudsync_outcome(listing) == 5u, "with the backups_listed outcome");

        unsigned char entry[1024];
        check(bk_cloudsync_backup_entry(listing, 0, entry, sizeof entry) > 0, "entry 0 reads");
        const char *entry_json = reinterpret_cast<const char *>(entry);
        check(contains(entry_json, "\"host\":\"RigA\""), "the entry names its host");
        check(contains(entry_json, "20260821T090000Z"), "and the newest snapshot comes first");
        check(bk_cloudsync_backup_entry(listing, 1, entry, sizeof entry) > 0, "entry 1 reads");
        check(bk_cloudsync_backup_entry(listing, 99, entry, sizeof entry) == -1,
              "past the end is -1, which is how a caller counts");

        // Restore the newest snapshot: extract its id from the entry, stage
        // it over the network, then apply it purely locally.
        char entry_id[256];
        entry_id[0] = '\0';
        check(bk_cloudsync_backup_entry(listing, 0, entry, sizeof entry) > 0, "entry 0 re-reads");
        if (const char *id_at = std::strstr(reinterpret_cast<const char *>(entry), "\"id\":\""))
        {
            const char *value = id_at + 6;
            const char *end = std::strchr(value, '"');
            if (end != nullptr && static_cast<size_t>(end - value) < sizeof entry_id)
            {
                std::memcpy(entry_id, value, static_cast<size_t>(end - value));
                entry_id[end - value] = '\0';
            }
        }
        check(entry_id[0] != '\0', "the entry id parses out of the JSON");

        // The listing is bound to its handle: a newer listing claims the
        // worker's single list, the old handle refuses instead of serving
        // the new profile's entries, and the new handle refuses until its
        // own fetch has actually delivered.
        const int second = bk_cloudsync_backup_list(game, "hero");
        check(second >= 0, "a second listing hands out a handle");
        if (second >= 0)
        {
            check(bk_cloudsync_backup_entry(listing, 0, entry, sizeof entry) == -1,
                  "the old handle refuses once a newer listing exists");
            check(bk_cloudsync_backup_entry(second, 0, entry, sizeof entry) == -1,
                  "the new handle refuses before its fetch completes");
            check(poll_to_rest(second, 60000) == 4u, "the second fetch reaches done");
            check(bk_cloudsync_backup_entry(second, 0, entry, sizeof entry) > 0,
                  "and then serves its entries");
            bk_cloudsync_release(second);
            check(bk_cloudsync_backup_entry(second, 0, entry, sizeof entry) == -1,
                  "a released listing handle serves nothing");
        }
        bk_cloudsync_release(listing);

        const int staging = bk_cloudsync_backup_restore(game, "hero", entry_id, 0);
        check(staging >= 0, "backup_restore hands out a handle");
        if (staging >= 0)
        {
            const unsigned int staged = poll_to_rest(staging, 60000);
            if (staged != 4u)
                std::fprintf(stderr, "cloudsync-abi-test: staging error: %s\n", bk_cloudsync_error(staging));
            check(staged == 4u, "the staging download reaches done");
            check(bk_cloudsync_outcome(staging) == 6u, "with the restore_staged outcome");
            bk_cloudsync_release(staging);

            // The apply step resolves profiles/<name> against the working
            // directory, exactly as the game does at startup.
            char before_cwd[1024];
#ifdef _WIN32
            check(_getcwd(before_cwd, sizeof before_cwd) != nullptr, "remember cwd for apply");
            check(_chdir(game) == 0, "chdir into the game dir for apply");
#else
            check(getcwd(before_cwd, sizeof before_cwd) != nullptr, "remember cwd for apply");
            check(chdir(game) == 0, "chdir into the game dir for apply");
#endif
            write_file("profiles/hero/config.cfg", "local-config");
            check(bk_cloudsync_apply_pending_restore("hero") == 1, "the staged restore applies");
            char restored[4096];
            read_file("profiles/hero/config.cfg", restored, sizeof restored);
            check(std::strcmp(restored, "newer-snapshot") == 0, "the applied config is the snapshot");
            // The pre-restore config is recoverable from the undo snapshot.
            char undo_pointer[256];
            read_file("profiles/hero/.cloudsync-trash/config/LATEST_UNDO", undo_pointer, sizeof undo_pointer);
            check(undo_pointer[0] != '\0', "LATEST_UNDO names the undo snapshot");
            char undo_path[1024];
            std::snprintf(undo_path, sizeof undo_path, "profiles/hero/.cloudsync-trash/config/%s.cfg", undo_pointer);
            char undo_content[4096];
            read_file(undo_path, undo_content, sizeof undo_content);
            check(std::strcmp(undo_content, "local-config") == 0, "the undo snapshot holds the original");
            check(bk_cloudsync_apply_pending_restore("hero") == 0, "a second apply reports nothing staged");

            // Undo, end to end: available as a reinstate, staged as a job,
            // applied at the "next startup", and the original comes back.
            check(bk_cloudsync_restore_undo_available("hero") == 2u,
                  "an applied restore reports reinstatable");
            const int undoing = bk_cloudsync_restore_undo(game, "hero");
            check(undoing >= 0, "restore_undo hands out a handle");
            if (undoing >= 0)
            {
                const unsigned int undone = poll_to_rest(undoing, 30000);
                check(undone == 4u, "the undo job reaches done");
                check(bk_cloudsync_outcome(undoing) == 6u + 1u, "with the undo_done outcome");
                bk_cloudsync_release(undoing);
                check(bk_cloudsync_restore_undo_available("hero") == 1u,
                      "the staged reinstate reports cancellable");
                check(bk_cloudsync_apply_pending_restore("hero") == 1, "the reinstate applies");
                read_file("profiles/hero/config.cfg", restored, sizeof restored);
                check(std::strcmp(restored, "local-config") == 0, "and the original config is back");
            }
#ifdef _WIN32
            check(_chdir(before_cwd) == 0, "chdir back after apply");
#else
            check(chdir(before_cwd) == 0, "chdir back after apply");
#endif
        }
    }

    // Shutdown with everything released stops worker and daemon; the fixture
    // tree can then be removed, which doubles as proof nothing keeps a handle
    // open into it.
    std::snprintf(at, sizeof at, "%s/cloudsync/rclone.conf", game);
    bk_cloudsync_shutdown();
    remove_tree(base);
    check(!file_exists(at), "the fixture tree is removable after shutdown");
}

int main()
{
    // The fixture directories are keyed by rand(); unseeded, every run picks
    // the same names and the second run inherits the first one's leftovers.
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    unsigned char raw[1024];
    std::memset(raw, 0, sizeof raw);
    const char *json = reinterpret_cast<const char *>(raw);

    const int written = bk_cloudsync_discovery_status(raw, static_cast<unsigned int>(sizeof raw));
    check(written > 0, "discovery_status returns a positive length");
    if (written <= 0)
    {
        std::fprintf(stderr, "cloudsync-abi-test: last error: %s\n", bk_cloudsync_last_error());
        return 1;
    }
    check(static_cast<size_t>(written) == std::strlen(json), "discovery_status NUL-terminates and returns the length");

    // Every field the settings screen needs, present on every path.
    check(contains(json, "\"found\":"), "status carries found");
    check(contains(json, "\"path\":"), "status carries path");
    check(contains(json, "\"version\":"), "status carries version");
    check(contains(json, "\"reason\":"), "status carries reason");

    // available and discovery_status read one cache, so they cannot disagree.
    const unsigned int available = bk_cloudsync_available();
    check(available == 0u || available == 1u, "available is a boolean");
    const bool found = contains(json, "\"found\":true");
    check(found == (available == 1u), "available agrees with the status document");

    // The reason is the typed rejection, not free text, and exists exactly
    // when rclone is unusable.
    const bool typed_reason = contains(json, "\"reason\":\"not_found\"") ||
                              contains(json, "\"reason\":\"too_old\"") ||
                              contains(json, "\"reason\":\"not_executable\"");
    check(found ? contains(json, "\"reason\":null") : typed_reason,
          "reason is null when ready and one of the three typed rejections otherwise");
    if (found) check(!contains(json, "\"path\":null"), "a ready status names the binary it chose");

    // -1 is the only failure value, and it leaves a readable last error.
    unsigned char tiny[8];
    std::memset(tiny, 0xcd, sizeof tiny);
    check(bk_cloudsync_discovery_status(tiny, 4) == -1, "a buffer that cannot hold the document fails");
    check(bk_cloudsync_last_error()[0] != '\0', "a failure leaves a last error");

    // Refresh replaces the cache and reports success; on one machine with one
    // PATH it must land on the same answer.
    check(bk_cloudsync_refresh_discovery() == 0, "refresh succeeds");
    check(bk_cloudsync_last_error()[0] == '\0', "success clears the last error");

    unsigned char again[1024];
    std::memset(again, 0, sizeof again);
    const int rewritten = bk_cloudsync_discovery_status(again, static_cast<unsigned int>(sizeof again));
    check(rewritten == written, "a refresh over an unchanged machine produces the same document");
    check(std::memcmp(raw, again, static_cast<size_t>(written)) == 0, "and the same bytes");
    check(bk_cloudsync_available() == available, "and the same availability");

    // Shutdown drops the cache; the next call rediscovers rather than
    // returning a stale or empty answer.
    bk_cloudsync_shutdown();
    unsigned char after[1024];
    std::memset(after, 0, sizeof after);
    const int post = bk_cloudsync_discovery_status(after, static_cast<unsigned int>(sizeof after));
    check(post == written, "discovery restarts after shutdown");
    check(bk_cloudsync_available() == available, "and reaches the same verdict");
    bk_cloudsync_shutdown();

    bundled_out_of_box();
    creds_contract();
    generic_creds_contract();
    catalogue_contract();
    sync_handle_contract();
    sync_full_cycle();

    bk_cloudsync_shutdown();
    bk_cloudsync_shutdown(); // twice, deliberately: idempotence is contract

    return failures == 0 ? 0 : 1;
}
