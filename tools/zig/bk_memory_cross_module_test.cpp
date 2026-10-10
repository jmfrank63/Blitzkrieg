// Cross-module proof of BkMemory: two DLLs and this exe each carry their own
// operator new/delete and all forward to the one allocator instance. Blocks
// are allocated in one module and freed in another through every new/delete
// form and every bk_mem_* entry. With `plant-leak` one block is leaked on
// purpose and the exit code must come from BkMemory's detach report; with
// `report` the explicit bk_mem_report path is exercised instead.
#include "bk_memory.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#if defined(_WIN32)
#include <windows.h>
using LibraryHandle = HMODULE;
static LibraryHandle open_library(const char *path) { return LoadLibraryA(path); }
static void *lookup(LibraryHandle handle, const char *name) { return reinterpret_cast<void *>(GetProcAddress(handle, name)); }
static void close_library(LibraryHandle handle) { FreeLibrary(handle); }
#else
#include <dlfcn.h>
using LibraryHandle = void *;
static LibraryHandle open_library(const char *path) { return dlopen(path, RTLD_NOW); }
static void *lookup(LibraryHandle handle, const char *name) { return dlsym(handle, name); }
static void close_library(LibraryHandle handle) { dlclose(handle); }
#endif

namespace
{
constexpr std::size_t kSize = 32;
constexpr std::size_t kAlign = 64;
int g_failures = 0;

void Check(bool ok, const char *what, int a = 0, int b = 0)
{
	if (ok)
		return;
	++g_failures;
	std::fprintf(stderr, "FAIL: %s (%d, %d)\n", what, a, b);
}

using NewFn = void *(*)(int, std::size_t);
using DeleteFn = int (*)(int, void *, std::size_t);
using MemAllocFn = void *(*)(int, std::size_t);
using MemFreeFn = int (*)(int, void *, std::size_t);
using ReleaseFn = int *(*)();
using AdoptFn = void (*)(int *);
using VoidFn = void (*)();

struct Consumer
{
	LibraryHandle lib;
	NewFn new_form;
	DeleteFn delete_form;
	MemAllocFn mem_alloc;
	MemFreeFn mem_free;
	ReleaseFn release_static;
	AdoptFn adopt;
	VoidFn plant_leak;
};

bool Load(Consumer &c, const char *id, const char *path)
{
	c.lib = open_library(path);
	if (!c.lib)
		return false;
	char symbol[96];
	auto get = [&](const char *fn) {
		std::snprintf(symbol, sizeof(symbol), "bk_consumer_%s_%s", id, fn);
		return lookup(c.lib, symbol);
	};
	c.new_form = reinterpret_cast<NewFn>(get("new_form"));
	c.delete_form = reinterpret_cast<DeleteFn>(get("delete_form"));
	c.mem_alloc = reinterpret_cast<MemAllocFn>(get("mem_alloc"));
	c.mem_free = reinterpret_cast<MemFreeFn>(get("mem_free"));
	c.release_static = reinterpret_cast<ReleaseFn>(get("release_static"));
	c.adopt = reinterpret_cast<AdoptFn>(get("adopt"));
	c.plant_leak = reinterpret_cast<VoidFn>(get("plant_leak"));
	return c.new_form && c.delete_form && c.mem_alloc && c.mem_free && c.release_static && c.adopt && c.plant_leak;
}

// The exe is a module too: its own operator new/delete forms.
void *ExeNew(int form, std::size_t size)
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

int ExeDelete(int form, void *p, std::size_t size)
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

void *ExeMemAlloc(int kind, std::size_t size)
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

int ExeMemFree(int kind, void *p, std::size_t size)
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

// A module as the test sees it: three of them (exe, A, B) so every ordered
// pair is covered.
struct Module
{
	const char *name;
	NewFn new_form;
	DeleteFn delete_form;
	MemAllocFn mem_alloc;
	MemFreeFn mem_free;
};

void CheckLive(std::size_t expected_count, const char *what, int a, int b)
{
	Check(bk_mem_live_count() == expected_count, what, a, b);
}

void NewDeleteMatrix(const Module &from, const Module &to)
{
	for (int nf = 0; nf < 8; ++nf)
	{
		const bool aligned = nf >= 4;
		for (int k = 0; k < 6; ++k)
		{
			const int df = aligned ? 6 + k : k;
			const std::size_t base = bk_mem_live_count();
			void *p = from.new_form(nf, kSize);
			Check(p != nullptr, "operator new returned null", nf, df);
			if (!p)
				continue;
			CheckLive(base + 1, "live count after new", nf, df);
			Check(bk_mem_size(p) == kSize, "bk_mem_size after new", nf, df);
			if (aligned)
				Check(reinterpret_cast<std::uintptr_t>(p) % kAlign == 0, "aligned new misaligned", nf, df);
			std::memset(p, 0xA5, kSize);
			Check(to.delete_form(df, p, kSize) == 1, "delete form unknown", nf, df);
			CheckLive(base, "live count after delete", nf, df);
		}
	}
	std::printf("new/delete forms %s -> %s ok\n", from.name, to.name);
}

void MemMatrix(const Module &from, const Module &to)
{
	for (int ak = 0; ak < 5; ++ak)
	{
		const bool aligned = ak == 1 || ak == 4;
		for (int fk = 0; fk < 4; ++fk)
		{
			if (fk == 2 && !aligned)
				continue; // the 64-byte alignment cross-check only fits aligned blocks
			const std::size_t base = bk_mem_live_count();
			void *p = from.mem_alloc(ak, kSize);
			Check(p != nullptr, "bk_mem alloc returned null", ak, fk);
			if (!p)
				continue;
			CheckLive(base + 1, "live count after bk_mem alloc", ak, fk);
			Check(bk_mem_size(p) == kSize, "bk_mem_size", ak, fk);
			if (ak == 2)
			{
				const unsigned char *bytes = static_cast<const unsigned char *>(p);
				bool zero = true;
				for (std::size_t i = 0; i < kSize; ++i)
					zero = zero && bytes[i] == 0;
				Check(zero, "bk_mem_calloc not zeroed", ak, fk);
			}
			if (aligned)
				Check(reinterpret_cast<std::uintptr_t>(p) % kAlign == 0, "aligned bk_mem misaligned", ak, fk);
			Check(to.mem_free(fk, p, kSize) == 1, "bk_mem free", ak, fk);
			CheckLive(base, "live count after bk_mem free", ak, fk);
		}
	}
	std::printf("bk_mem entries %s -> %s ok\n", from.name, to.name);
}

// Memory made by `new` is also valid for the C entries and the reverse: one
// header layer, so a mixed free is not an error.
void MixedPaths(const Module &from, const Module &to)
{
	const std::size_t base = bk_mem_live_count();
	void *p = from.new_form(0, kSize);
	Check(to.mem_free(0, p, kSize) == 1, "bk_mem_free of operator new block");
	CheckLive(base, "live count after mixed free", 0, 0);
	void *q = from.mem_alloc(0, kSize);
	Check(to.delete_form(0, q, kSize) == 1, "operator delete of bk_mem block");
	CheckLive(base, "live count after mixed delete", 0, 0);
}

void ReallocAcrossModules(const Module &a, const Module &b)
{
	const std::size_t base = bk_mem_live_count();
	void *p = a.mem_alloc(0, 16);
	std::memset(p, 0x11, 16);
	void *grown = bk_mem_realloc(p, 4096);
	Check(grown != nullptr, "realloc grow");
	if (grown)
	{
		const unsigned char *bytes = static_cast<const unsigned char *>(grown);
		Check(bytes[0] == 0x11 && bytes[15] == 0x11, "realloc keeps content");
		Check(bk_mem_size(grown) == 4096, "realloc size");
		Check(b.mem_free(0, grown, 4096) == 1, "free realloc'ed block");
	}
	CheckLive(base, "live count after realloc cycle", 0, 0);
}
} // namespace

int main(int argc, char **argv)
{
	if (argc < 3)
		return 10;
	const bool plant_leak = argc > 3 && std::strcmp(argv[3], "plant-leak") == 0;
	const bool explicit_report = argc > 3 && std::strcmp(argv[3], "report") == 0;

	Consumer a = {};
	Consumer b = {};
	if (!Load(a, "a", argv[1]) || !Load(b, "b", argv[2]))
		return 11;
	const Module exe = {"exe", ExeNew, ExeDelete, ExeMemAlloc, ExeMemFree};
	const Module ma = {"A", a.new_form, a.delete_form, a.mem_alloc, a.mem_free};
	const Module mb = {"B", b.new_form, b.delete_form, b.mem_alloc, b.mem_free};
	const Module *modules[] = {&exe, &ma, &mb};

	for (const Module *from : modules)
		for (const Module *to : modules)
		{
			NewDeleteMatrix(*from, *to);
			MemMatrix(*from, *to);
			MixedPaths(*from, *to);
		}
	ReallocAcrossModules(ma, mb);
	ReallocAcrossModules(mb, ma);

	// Static-init and teardown order: A's constructor allocated *owned with
	// new; B adopts the block and frees it from its static destructor when the
	// DLL is unloaded, together with the block B's own constructor allocated.
	int *handed = a.release_static();
	Check(handed != nullptr && *handed == 42, "static constructor block");
	b.adopt(handed);
	const std::size_t before_unload = bk_mem_live_count();
	close_library(b.lib);
	Check(bk_mem_live_count() == before_unload - 2, "static destructor freed both blocks",
	      static_cast<int>(before_unload), static_cast<int>(bk_mem_live_count()));
	std::printf("static init/teardown order ok\n");

	if (g_failures != 0)
	{
		std::fprintf(stderr, "bk_memory cross-module test: %d failure(s)\n", g_failures);
		return 1;
	}

	if (plant_leak)
	{
		// The exit code of this run must come from BkMemory's detach report.
		a.plant_leak();
		std::printf("leak planted\n");
		std::fflush(stdout);
		return 0;
	}
	if (explicit_report)
	{
		const std::size_t leaks = bk_mem_report();
		if (leaks != 0 || bk_mem_report() != 0)
		{
			std::fprintf(stderr, "bk_mem_report returned %zu leaks\n", leaks);
			return 2;
		}
		bk_mem_free(nullptr);
		std::printf("bk_mem_report clean\n");
		std::fflush(stdout);
		return 0;
	}
	std::printf("bk_memory cross-module test ok\n");
	std::fflush(stdout);
	return 0;
}
