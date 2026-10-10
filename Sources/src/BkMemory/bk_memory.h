/*
 * BkMemory: the one allocator of the process.
 *
 * Every module allocates and frees through these entries (directly, or via
 * the replacement operator new/delete), so a block may be freed by any module
 * and there is one set of leak records. The library is pure Zig; this header
 * is the whole contract.
 */
#ifndef BK_MEMORY_H
#define BK_MEMORY_H

#include <stddef.h>

#if defined(_WIN32)
#  if defined(BK_MEMORY_BUILD)
#    define BK_MEMORY_API __declspec(dllexport)
#  else
#    define BK_MEMORY_API __declspec(dllimport)
#  endif
#else
#  define BK_MEMORY_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Alignment is at least 16. alignment must be a power of two or the call
 * returns NULL. Size 0 returns a valid unique pointer. NULL means out of memory. */
BK_MEMORY_API void *bk_mem_alloc(size_t size);
BK_MEMORY_API void *bk_mem_alloc_aligned(size_t size, size_t alignment);

/* n * size overflow returns NULL. Memory is zeroed. */
BK_MEMORY_API void *bk_mem_calloc(size_t n, size_t size);

/* realloc(NULL, n) allocates; realloc(p, 0) frees and returns NULL, which
 * differs from glibc and MSVC and is documented on purpose. The _aligned form
 * never lowers the alignment of the block. */
BK_MEMORY_API void *bk_mem_realloc(void *p, size_t size);
BK_MEMORY_API void *bk_mem_realloc_aligned(void *p, size_t size, size_t alignment);

/* NULL is ignored. A pointer BkMemory did not hand out panics (bad header
 * magic). The sized form is what C++ sized/aligned delete maps to; size and
 * alignment are only cross-checked in Debug builds, alignment 0 = not given. */
BK_MEMORY_API void bk_mem_free(void *p);
BK_MEMORY_API void bk_mem_free_sized(void *p, size_t size, size_t alignment);

/* The requested size of a live block. */
BK_MEMORY_API size_t bk_mem_size(void *p);

/* Cheap counters for tests to take deltas against. */
BK_MEMORY_API size_t bk_mem_live_count(void);
BK_MEMORY_API size_t bk_mem_live_bytes(void);

/* Runs the final leak check now and returns the leak count. Afterwards a
 * free is a counted no-op and an alloc is a bug (panic).
 *
 * The same check runs at library detach, steered by the BK_MEM_REPORT
 * environment variable (the default is on in Debug builds, off otherwise):
 *   0    no report.
 *   1    report with stacks, exit code 3 when blocks leaked.
 *   log  the same report plus a line "bk_mem: N leaked block(s)" on stderr
 *        (also when N is 0), but the exit code is not changed. For tiers that
 *        must stay green while known leaks are fixed.
 *   count  as log, but the report is the summary line only, without the
 *        per-block stacks. For tiers whose leaks number in the hundreds of
 *        thousands, where symbolizing the stacks takes minutes.
 * A backend without leak records ignores all of them. */
BK_MEMORY_API size_t bk_mem_report(void);
BK_MEMORY_API size_t bk_mem_closed_free_count(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_MEMORY_H */
