// Replacement operator new/delete: every form routes to BkMemory.
//
// This file is compiled into every module (exe and each DLL) instead of
// being a library of its own. That is deliberate: all copies forward to the
// one BkMemory instance, so it does not matter which module's operator new
// the loader binds a call to, and a block allocated in one module may be
// freed by any other. Only bk_memory.h is included; the legacy
// Misc/MemorySystem.h must not be pulled in here.
#include "bk_memory.h"

#include <cstddef>
#include <new>

namespace {

void *AllocOrThrow(std::size_t size, std::size_t alignment)
{
	// operator new(0) must return a unique non-null pointer; BkMemory does
	// that for size 0 as well, but 1 keeps the contract independent of it.
	if (size == 0)
		size = 1;
	for (;;)
	{
		void *p = alignment ? bk_mem_alloc_aligned(size, alignment) : bk_mem_alloc(size);
		if (p)
			return p;
		std::new_handler handler = std::get_new_handler();
		if (!handler)
			throw std::bad_alloc();
		handler();
	}
}

void *AllocNoThrow(std::size_t size, std::size_t alignment) noexcept
{
	if (size == 0)
		size = 1;
	for (;;)
	{
		void *p = alignment ? bk_mem_alloc_aligned(size, alignment) : bk_mem_alloc(size);
		if (p)
			return p;
		std::new_handler handler = std::get_new_handler();
		if (!handler)
			return nullptr;
		try
		{
			handler();
		}
		catch (...)
		{
			return nullptr;
		}
	}
}

} // namespace

// ---- new -------------------------------------------------------------

void *operator new(std::size_t size) { return AllocOrThrow(size, 0); }
void *operator new[](std::size_t size) { return AllocOrThrow(size, 0); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return AllocNoThrow(size, 0); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return AllocNoThrow(size, 0); }

void *operator new(std::size_t size, std::align_val_t alignment)
{
	return AllocOrThrow(size, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t size, std::align_val_t alignment)
{
	return AllocOrThrow(size, static_cast<std::size_t>(alignment));
}
void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return AllocNoThrow(size, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return AllocNoThrow(size, static_cast<std::size_t>(alignment));
}

// ---- delete ----------------------------------------------------------

void operator delete(void *p) noexcept { bk_mem_free(p); }
void operator delete[](void *p) noexcept { bk_mem_free(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { bk_mem_free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { bk_mem_free(p); }

void operator delete(void *p, std::size_t size) noexcept { bk_mem_free_sized(p, size, 0); }
void operator delete[](void *p, std::size_t size) noexcept { bk_mem_free_sized(p, size, 0); }

void operator delete(void *p, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, 0, static_cast<std::size_t>(alignment));
}
void operator delete[](void *p, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, 0, static_cast<std::size_t>(alignment));
}
void operator delete(void *p, std::size_t size, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, size, static_cast<std::size_t>(alignment));
}
void operator delete[](void *p, std::size_t size, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, size, static_cast<std::size_t>(alignment));
}
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { bk_mem_free(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { bk_mem_free(p); }
