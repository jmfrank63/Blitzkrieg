/*
 * Routes SDL's allocator to BkMemory.
 *
 * SDL_SetMemoryFunctions has to be the first SDL call of the process: a block
 * SDL allocated before it would be released through bk_mem_free and panic on
 * its header magic. Calling it again with the same functions is harmless, so
 * every entry point that may be the first SDL user of a module can call it.
 */
#ifndef BK_MEMORY_SDL_H
#define BK_MEMORY_SDL_H

#include <SDL3/SDL_stdinc.h>

#include "bk_memory.h"

static inline bool BkMemoryInstallSdlFunctions(void)
{
    return SDL_SetMemoryFunctions(bk_mem_alloc, bk_mem_calloc, bk_mem_realloc, bk_mem_free);
}

#endif /* BK_MEMORY_SDL_H */
