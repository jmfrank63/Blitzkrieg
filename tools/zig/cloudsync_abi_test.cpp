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
void bk_cloudsync_cancel(int handle);
void bk_cloudsync_release(int handle);
// Credentials, over profiles/cloud.credentials relative to the working
// directory. creds_load withholds the secret and reports has_secret.
int bk_cloudsync_creds_load(unsigned char *json_out, unsigned int cap);
int bk_cloudsync_creds_save(const char *json);
int bk_cloudsync_creds_clear_secret(void);
unsigned int bk_cloudsync_creds_present(void);
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
            std::snprintf(doc, sizeof doc,
                          "{\"protocol\":\"s3\",\"s3\":{\"s3_provider\":\"Minio\","
                          "\"endpoint\":\"http://127.0.0.1:9000\",\"bucket\":\"bk\","
                          "\"region\":\"us-east-1\",\"access_key\":\"AKIAIOSFODNN7EXAMPLE\"},"
                          "\"rclone_path\":\"%s\"}",
                          escaped);
            check(bk_cloudsync_creds_save(doc) == 0, "saving an rclone override succeeds");
            check(bk_cloudsync_available() == 1u, "the override is discovered with no restart");
            unsigned char status[1024];
            std::memset(status, 0, sizeof status);
            check(bk_cloudsync_discovery_status(status, sizeof status) > 0, "status after the override");
            check(contains(reinterpret_cast<const char *>(status), "\"found\":true"),
                  "and the status agrees");
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

    creds_contract();
    sync_handle_contract();
    sync_full_cycle();

    bk_cloudsync_shutdown();
    bk_cloudsync_shutdown(); // twice, deliberately: idempotence is contract

    return failures == 0 ? 0 : 1;
}
