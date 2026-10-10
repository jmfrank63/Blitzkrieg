// Proves each hooked C library allocates through BkMemory. For zlib, libpng,
// Lua and SDL it takes bk_mem_live_count()/live_bytes() before, during (after
// the library allocated, before it released) and after, and fails naming the
// library when the delta is not positive during and zero after. A library that
// still uses the CRT shows a zero delta, so the failure line says which one.
#include "bk_memory.h"
#include "bk_memory_sdl.h"

#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
#include "zlib.h"
#include "png.h"
#include "lua.h"
}

namespace
{
int g_failures = 0;

struct Sample
{
	size_t count;
	size_t bytes;
};

Sample Take()
{
	return Sample{ bk_mem_live_count(), bk_mem_live_bytes() };
}

// during must be above before in both counters and after must be back at before.
void Verdict(const char *library, const char *what, const Sample &before, const Sample &during, const Sample &after)
{
	std::printf("bk_mem_c_sources: %-6s %-20s before %zu/%zu  during %zu/%zu  after %zu/%zu\n", library, what,
		before.count, before.bytes, during.count, during.bytes, after.count, after.bytes);
	const bool grew = during.count > before.count && during.bytes > before.bytes;
	const bool released = after.count == before.count && after.bytes == before.bytes;
	if (grew && released)
		return;
	++g_failures;
	std::fprintf(stderr, "FAIL: %s (%s): %s\n", library, what,
		!grew ? "no BkMemory allocation seen, the library still uses the CRT" : "blocks were not returned to BkMemory");
}

// Fixed storage: the png stream must not allocate, or the counters move.
unsigned char g_png[16384];
size_t g_pngSize = 0;
size_t g_pngRead = 0;

void PngWrite(png_structp, png_bytep data, png_size_t length)
{
	if (g_pngSize + length > sizeof(g_png))
		return;
	std::memcpy(g_png + g_pngSize, data, length);
	g_pngSize += length;
}

void PngFlush(png_structp) {}

void PngReadFn(png_structp, png_bytep data, png_size_t length)
{
	if (g_pngRead + length > g_pngSize)
		return;
	std::memcpy(data, g_png + g_pngRead, length);
	g_pngRead += length;
}

void TestZlib()
{
	static unsigned char input[4096];
	static unsigned char packed[8192];
	static unsigned char output[4096];
	for (size_t i = 0; i < sizeof(input); ++i)
		input[i] = static_cast<unsigned char>((i * 7) % 61);

	z_stream deflater;
	std::memset(&deflater, 0, sizeof(deflater));
	const Sample beforeDeflate = Take();
	if (deflateInit(&deflater, Z_DEFAULT_COMPRESSION) != Z_OK)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: zlib: deflateInit\n");
		return;
	}
	const Sample duringDeflate = Take();
	deflater.next_in = input;
	deflater.avail_in = sizeof(input);
	deflater.next_out = packed;
	deflater.avail_out = sizeof(packed);
	if (deflate(&deflater, Z_FINISH) != Z_STREAM_END)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: zlib: deflate\n");
	}
	const uLong packedSize = deflater.total_out;
	deflateEnd(&deflater);
	Verdict("zlib", "deflate", beforeDeflate, duringDeflate, Take());

	z_stream inflater;
	std::memset(&inflater, 0, sizeof(inflater));
	const Sample beforeInflate = Take();
	if (inflateInit(&inflater) != Z_OK)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: zlib: inflateInit\n");
		return;
	}
	const Sample duringInflate = Take();
	inflater.next_in = packed;
	inflater.avail_in = static_cast<uInt>(packedSize);
	inflater.next_out = output;
	inflater.avail_out = sizeof(output);
	if (inflate(&inflater, Z_FINISH) != Z_STREAM_END || std::memcmp(input, output, sizeof(input)) != 0)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: zlib: inflate round trip\n");
	}
	inflateEnd(&inflater);
	Verdict("zlib", "inflate", beforeInflate, duringInflate, Take());
}

void TestPng()
{
	png_byte pixels[4 * 4 * 4];
	for (size_t i = 0; i < sizeof(pixels); ++i)
		pixels[i] = static_cast<png_byte>(i * 3);
	png_bytep rows[4];
	for (int y = 0; y < 4; ++y)
		rows[y] = pixels + y * 16;

	g_pngSize = 0;
	const Sample beforeWrite = Take();
	png_structp write = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop writeInfo = write ? png_create_info_struct(write) : nullptr;
	if (!write || !writeInfo)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: libpng: png_create_write_struct\n");
		return;
	}
	if (setjmp(png_jmpbuf(write)))
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: libpng: write error\n");
		png_destroy_write_struct(&write, &writeInfo);
		return;
	}
	png_set_write_fn(write, nullptr, PngWrite, PngFlush);
	png_set_IHDR(write, writeInfo, 4, 4, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);
	png_write_info(write, writeInfo);
	png_write_image(write, rows);
	png_write_end(write, writeInfo);
	const Sample duringWrite = Take();
	png_destroy_write_struct(&write, &writeInfo);
	Verdict("libpng", "write", beforeWrite, duringWrite, Take());

	g_pngRead = 0;
	const Sample beforeRead = Take();
	png_structp read = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop readInfo = read ? png_create_info_struct(read) : nullptr;
	if (!read || !readInfo)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: libpng: png_create_read_struct\n");
		return;
	}
	if (setjmp(png_jmpbuf(read)))
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: libpng: read error\n");
		png_destroy_read_struct(&read, &readInfo, nullptr);
		return;
	}
	png_set_read_fn(read, nullptr, PngReadFn);
	png_read_info(read, readInfo);
	png_byte back[4 * 4 * 4];
	png_bytep backRows[4];
	for (int y = 0; y < 4; ++y)
		backRows[y] = back + y * 16;
	png_read_image(read, backRows);
	const bool same = std::memcmp(back, pixels, sizeof(pixels)) == 0;
	const Sample duringRead = Take();
	png_destroy_read_struct(&read, &readInfo, nullptr);
	if (!same)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: libpng: image round trip\n");
	}
	Verdict("libpng", "read", beforeRead, duringRead, Take());
}

void TestLua()
{
	const Sample before = Take();
	lua_State *state = lua_open(0);
	if (!state)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: Lua: lua_open\n");
		return;
	}
	const Sample during = Take();
	lua_close(state);
	Verdict("Lua", "lua_open/lua_close", before, during, Take());
}

void TestSdl()
{
	BkMemoryInstallSdlFunctions();
	SDL_malloc_func m = nullptr;
	SDL_calloc_func c = nullptr;
	SDL_realloc_func r = nullptr;
	SDL_free_func f = nullptr;
	SDL_GetMemoryFunctions(&m, &c, &r, &f);
	if (m != bk_mem_alloc || c != bk_mem_calloc || r != bk_mem_realloc || f != bk_mem_free)
	{
		++g_failures;
		std::fprintf(stderr, "FAIL: SDL: SDL_GetMemoryFunctions are not the bk_mem_* entries\n");
	}
	const Sample before = Take();
	void *block = SDL_malloc(256);
	const Sample during = Take();
	SDL_free(block);
	Verdict("SDL", "SDL_malloc/SDL_free", before, during, Take());
}
} // namespace

int main()
{
	// SDL first: nothing else may allocate through SDL before its hooks are in.
	TestSdl();
	TestZlib();
	TestPng();
	TestLua();
	if (g_failures != 0)
	{
		std::fprintf(stderr, "bk_memory_c_sources_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("bk_memory_c_sources_test: ok\n");
	return 0;
}
