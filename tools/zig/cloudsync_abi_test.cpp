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

#include <cstdio>
#include <cstring>

extern "C" {
unsigned int bk_cloudsync_available(void);
int bk_cloudsync_discovery_status(unsigned char *json_out, unsigned int cap);
int bk_cloudsync_refresh_discovery(void);
void bk_cloudsync_shutdown(void);
const char *bk_cloudsync_last_error(void);
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

int main()
{
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

    return failures == 0 ? 0 : 1;
}
