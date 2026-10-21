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

// Each operator new passes its own return address and frame record, so
// BkMemory can tell which module asked for the block (bk_mem_alloc_from).
#define BK_CALLER __builtin_return_address(0), __builtin_frame_address(0)

// The module registers before its other static constructors allocate and
// unregisters after its static destructors have freed (priority 101 is the
// first constructor and the last destructor). COFF has no priorities, and
// there a foreign DLL never reaches this module's operator new anyway.
#if defined(_WIN32)
#  define BK_FIRST_CONSTRUCTOR __attribute__((constructor))
#  define BK_LAST_DESTRUCTOR __attribute__((destructor))
#else
#  define BK_FIRST_CONSTRUCTOR __attribute__((constructor(101)))
#  define BK_LAST_DESTRUCTOR __attribute__((destructor(101)))
#endif

BK_FIRST_CONSTRUCTOR void RegisterModule()
{
	bk_mem_register_module(reinterpret_cast<const void *>(&RegisterModule));
}

BK_LAST_DESTRUCTOR void UnregisterModule()
{
	bk_mem_unregister_module(reinterpret_cast<const void *>(&UnregisterModule));
}

void *AllocOrThrow(std::size_t size, std::size_t alignment, const void *caller, const void *frame)
{
	// operator new(0) must return a unique non-null pointer; BkMemory does
	// that for size 0 as well, but 1 keeps the contract independent of it.
	if (size == 0)
		size = 1;
	for (;;)
	{
		void *p = bk_mem_alloc_from(size, alignment, caller, frame);
		if (p)
			return p;
		std::new_handler handler = std::get_new_handler();
		if (!handler)
			throw std::bad_alloc();
		handler();
	}
}

void *AllocNoThrow(std::size_t size, std::size_t alignment, const void *caller, const void *frame) noexcept
{
	if (size == 0)
		size = 1;
	for (;;)
	{
		void *p = bk_mem_alloc_from(size, alignment, caller, frame);
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

// On Apple platforms dyld replaces the system libc++'s operators with an
// image's own only if that image defines weak symbols (MH_WEAK_DEFINES). The
// Game has other weak symbols and so got the replacement, but the small
// platform-system-test did not: libc++.dylib's string code allocated with the
// system new, and the inline delete in System.cpp then freed that block
// through this file (bad header magic). Weak operators make every image that
// carries this file take part; all copies forward to the one BkMemory anyway.
// They are also kept when unused (`used`, which clang marks no_dead_strip on
// Mach-O). The linker had kept only the forms the image itself called (new and
// sized delete), while macOS 14's libc++, built without sized deallocation,
// frees with the plain delete: on macbook-air-frank its string code then freed
// a BkMemory block with the system free (malloc abort).
#if defined(__APPLE__)
#  define BK_REPLACEMENT __attribute__((weak, used))
#else
#  define BK_REPLACEMENT
#endif

// ---- new -------------------------------------------------------------

BK_REPLACEMENT void *operator new(std::size_t size) { return AllocOrThrow(size, 0, BK_CALLER); }
BK_REPLACEMENT void *operator new[](std::size_t size) { return AllocOrThrow(size, 0, BK_CALLER); }
BK_REPLACEMENT void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return AllocNoThrow(size, 0, BK_CALLER); }
BK_REPLACEMENT void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return AllocNoThrow(size, 0, BK_CALLER); }

BK_REPLACEMENT void *operator new(std::size_t size, std::align_val_t alignment)
{
	return AllocOrThrow(size, static_cast<std::size_t>(alignment), BK_CALLER);
}
BK_REPLACEMENT void *operator new[](std::size_t size, std::align_val_t alignment)
{
	return AllocOrThrow(size, static_cast<std::size_t>(alignment), BK_CALLER);
}
BK_REPLACEMENT void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return AllocNoThrow(size, static_cast<std::size_t>(alignment), BK_CALLER);
}
BK_REPLACEMENT void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return AllocNoThrow(size, static_cast<std::size_t>(alignment), BK_CALLER);
}

// ---- delete ----------------------------------------------------------

BK_REPLACEMENT void operator delete(void *p) noexcept { bk_mem_free(p); }
BK_REPLACEMENT void operator delete[](void *p) noexcept { bk_mem_free(p); }
BK_REPLACEMENT void operator delete(void *p, const std::nothrow_t &) noexcept { bk_mem_free(p); }
BK_REPLACEMENT void operator delete[](void *p, const std::nothrow_t &) noexcept { bk_mem_free(p); }

// The size a sized delete passes is only cross-checked against the block's
// header, and on Apple platforms it is not the platform's promise to keep:
// the system libc++ is built without sized deallocation, and the inline
// string code of a newer SDK computes a buffer's size by another rule than
// the older system libc++ that allocated it (the macOS 15.1 SDK on macOS 14.8:
// 25 bytes allocated, delete passed 24). The header is authoritative there.
#if defined(__APPLE__)
#  define BK_DELETE_SIZE(size) ((void)(size), std::size_t(0))
#else
#  define BK_DELETE_SIZE(size) (size)
#endif

BK_REPLACEMENT void operator delete(void *p, std::size_t size) noexcept { bk_mem_free_sized(p, BK_DELETE_SIZE(size), 0); }
BK_REPLACEMENT void operator delete[](void *p, std::size_t size) noexcept { bk_mem_free_sized(p, BK_DELETE_SIZE(size), 0); }

BK_REPLACEMENT void operator delete(void *p, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, 0, static_cast<std::size_t>(alignment));
}
BK_REPLACEMENT void operator delete[](void *p, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, 0, static_cast<std::size_t>(alignment));
}
BK_REPLACEMENT void operator delete(void *p, std::size_t size, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, BK_DELETE_SIZE(size), static_cast<std::size_t>(alignment));
}
BK_REPLACEMENT void operator delete[](void *p, std::size_t size, std::align_val_t alignment) noexcept
{
	bk_mem_free_sized(p, BK_DELETE_SIZE(size), static_cast<std::size_t>(alignment));
}
BK_REPLACEMENT void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { bk_mem_free(p); }
BK_REPLACEMENT void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { bk_mem_free(p); }
