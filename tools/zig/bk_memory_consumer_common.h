// Shared body of the two BkMemory test consumer DLLs. Each consumer defines
// BK_CONSUMER_ID (a, b) before including this header, so the two DLLs export
// the same API under different names and each carries its own copy of
// operator new/delete (linkBkMemory) and its own static object.
#ifndef BK_CONSUMER_ID
#error "define BK_CONSUMER_ID before including bk_memory_consumer_common.h"
#endif

#include "bk_memory.h"

#include <cstddef>
#include <new>

#if defined(_WIN32)
#define BK_CONSUMER_EXPORT extern "C" __declspec(dllexport)
#else
#define BK_CONSUMER_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define BK_CAT_(x, y) x##y
#define BK_CAT(x, y) BK_CAT_(x, y)
#define BK_NAME(n) BK_CAT(BK_CAT(BK_CAT(bk_consumer_, BK_CONSUMER_ID), _), n)

namespace
{
constexpr std::size_t kAlign = 64;

// A static object whose constructor allocates with operator new. The test hands
// the first consumer's block to the second one, whose destructor frees it: that
// proves static-init and teardown order across the DLLs and the allocator.
struct StaticObject
{
	int *owned;
	int *adopted;
	StaticObject() : owned(new int(42)), adopted(nullptr) {}
	~StaticObject()
	{
		delete owned;
		delete adopted;
	}
};

StaticObject g_static;
} // namespace

// Form 0..3 plain new, new[], nothrow new, nothrow new[]; 4..7 the aligned
// (64) counterparts. Returns NULL for an unknown form or on out of memory.
BK_CONSUMER_EXPORT void *BK_NAME(new_form)(int form, std::size_t size)
{
	const std::align_val_t al = static_cast<std::align_val_t>(kAlign);
	switch (form)
	{
	case 0: return ::operator new(size);
	case 1: return ::operator new[](size);
	case 2: return ::operator new(size, std::nothrow);
	case 3: return ::operator new[](size, std::nothrow);
	case 4: return ::operator new(size, al);
	case 5: return ::operator new[](size, al);
	case 6: return ::operator new(size, al, std::nothrow);
	case 7: return ::operator new[](size, al, std::nothrow);
	}
	return nullptr;
}

// Form 0..5 pair with new forms 0..3, 6..11 with new forms 4..7. Returns 0 for
// an unknown form.
BK_CONSUMER_EXPORT int BK_NAME(delete_form)(int form, void *p, std::size_t size)
{
	const std::align_val_t al = static_cast<std::align_val_t>(kAlign);
	switch (form)
	{
	case 0: ::operator delete(p); break;
	case 1: ::operator delete[](p); break;
	case 2: ::operator delete(p, std::nothrow); break;
	case 3: ::operator delete[](p, std::nothrow); break;
	case 4: ::operator delete(p, size); break;
	case 5: ::operator delete[](p, size); break;
	case 6: ::operator delete(p, al); break;
	case 7: ::operator delete[](p, al); break;
	case 8: ::operator delete(p, size, al); break;
	case 9: ::operator delete[](p, size, al); break;
	case 10: ::operator delete(p, al, std::nothrow); break;
	case 11: ::operator delete[](p, al, std::nothrow); break;
	default: return 0;
	}
	return 1;
}

// Kind 0 bk_mem_alloc, 1 bk_mem_alloc_aligned(64), 2 bk_mem_calloc(1, n),
// 3 bk_mem_realloc(NULL, n), 4 bk_mem_realloc_aligned(NULL, n, 64).
BK_CONSUMER_EXPORT void *BK_NAME(mem_alloc)(int kind, std::size_t size)
{
	switch (kind)
	{
	case 0: return bk_mem_alloc(size);
	case 1: return bk_mem_alloc_aligned(size, kAlign);
	case 2: return bk_mem_calloc(1, size);
	case 3: return bk_mem_realloc(nullptr, size);
	case 4: return bk_mem_realloc_aligned(nullptr, size, kAlign);
	}
	return nullptr;
}

// Kind 0 bk_mem_free, 1 bk_mem_free_sized(size, 0), 2 bk_mem_free_sized(size,
// 64; aligned blocks only), 3 bk_mem_realloc(p, 0).
BK_CONSUMER_EXPORT int BK_NAME(mem_free)(int kind, void *p, std::size_t size)
{
	switch (kind)
	{
	case 0: bk_mem_free(p); return 1;
	case 1: bk_mem_free_sized(p, size, 0); return 1;
	case 2: bk_mem_free_sized(p, size, kAlign); return 1;
	case 3: return bk_mem_realloc(p, 0) == nullptr;
	}
	return 0;
}

// Gives away the block the static constructor allocated; the static
// destructor then no longer frees it.
BK_CONSUMER_EXPORT int *BK_NAME(release_static)(void)
{
	int *p = g_static.owned;
	g_static.owned = nullptr;
	return p;
}

// The static destructor frees the adopted block, which another module made.
BK_CONSUMER_EXPORT void BK_NAME(adopt)(int *p)
{
	delete g_static.adopted;
	g_static.adopted = p;
}

BK_CONSUMER_EXPORT void BK_NAME(plant_leak)(void)
{
	int *leaked = new int(7);
	*static_cast<volatile int *>(leaked) = 8;
}
