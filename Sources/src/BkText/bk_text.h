/* bk_text C ABI (M004): the portable TrueType text engine behind a flat C interface.
 *
 * Shaping, measuring, wrapping and the R8 glyph atlas for UTF-16 text; the Game's renderer (S02)
 * and the tools consume it from here. Only fixed-width integers, float and pointers cross the
 * boundary: no wchar_t (2 bytes on Windows, 4 elsewhere), no C++ types, no exceptions.
 *
 * Ownership: every object is created and destroyed through this header, memory is allocated by
 * the library (through BkMemory) and the caller never frees anything it did not create. Output
 * buffers are the caller's; a function that does not fit its output returns BK_TEXT_BUFFER_TOO_SMALL
 * and writes the needed count to *out_count, so the call can be repeated with a big enough buffer.
 *
 * Threading: a context and everything made from it belong to one thread at a time.
 */
#ifndef BK_TEXT_H
#define BK_TEXT_H

#include <stdint.h>

#if defined(_WIN32)
#define BK_TEXT_API __declspec(dllexport)
#else
#define BK_TEXT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum BkTextResult {
	BK_TEXT_OK = 0,
	BK_TEXT_INVALID_ARGUMENT = 1,
	BK_TEXT_NOT_FOUND = 2, /* the fonts directory or a face file is missing */
	BK_TEXT_OUT_OF_MEMORY = 3,
	BK_TEXT_BUFFER_TOO_SMALL = 4,
	BK_TEXT_ATLAS_FULL = 5, /* one frame's glyphs do not fit the atlas page */
	BK_TEXT_FONT_ERROR = 6 /* a face is unreadable, fails its SHA256SUMS check or is not a font */
} BkTextResult;

typedef enum BkTextSizeClass {
	BK_TEXT_SIZE_TINY = 0,
	BK_TEXT_SIZE_SMALL = 1,
	BK_TEXT_SIZE_MEDIUM = 2,
	BK_TEXT_SIZE_LARGE = 3
} BkTextSizeClass;

typedef struct BkTextContext BkTextContext;
typedef struct BkTextFont BkTextFont;

typedef struct BkTextGlyph {
	/* Opaque atlas key (face and glyph id); hand it to bk_text_atlas_glyph. */
	uint32_t glyph_key;
	/* UTF-16 index in the shaped text where this glyph's cluster starts. */
	uint32_t cluster;
	/* Pixels. y follows HarfBuzz: positive is up. */
	float x_advance;
	float y_advance;
	float x_offset;
	float y_offset;
} BkTextGlyph;

typedef struct BkTextMetrics {
	/* Sum of the glyph advances, unrounded. */
	float advance;
	/* advance rounded up to whole pixels: the number wrap compares against the box. */
	float width;
	/* Pixels, descent positive below the baseline; from the language's primary face. */
	float ascent;
	float descent;
	/* Whole pixels between baselines. */
	float line_space;
} BkTextMetrics;

/* UTF-16 units [start, end) of the text form one line; a line broken at a space leaves the space out. */
typedef struct BkTextLine {
	uint32_t start;
	uint32_t end;
} BkTextLine;

typedef struct BkTextRect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
} BkTextRect;

typedef struct BkTextAtlasGlyph {
	/* The glyph's pixels in the atlas page; w = h = 0 for a glyph without ink (a space). */
	BkTextRect rect;
	/* Pixels from the pen to the bitmap's left edge, and from the baseline up to its top edge. */
	int32_t bearing_x;
	int32_t bearing_y;
	/* The atlas generation this rect belongs to. */
	uint32_t generation;
} BkTextAtlasGlyph;

typedef struct BkTextStats {
	uint32_t atlas_generation;
	uint32_t atlas_glyph_count;
	uint64_t atlas_used_area;
	uint32_t atlas_eviction_count;
	uint64_t shape_cache_hits;
	uint64_t shape_cache_misses;
	uint32_t shape_cache_entries;
	/* Distinct code points reported missing so far. */
	uint32_t missing_glyph_count;
} BkTextStats;

/* Loads and verifies the faces in fonts_dir_utf8 (the folder with SHA256SUMS) for a BCP-47 primary
 * language code such as "en", "ru", "ja", "zh", "hi"; the code picks the fallback order of the
 * faces and the shaping language. */
BK_TEXT_API BkTextResult bk_text_context_create(const char *fonts_dir_utf8, const char *lang_code, BkTextContext **out);
/* Destroys the context, its atlas and caches. Close its fonts first; null is accepted. */
BK_TEXT_API void bk_text_context_destroy(BkTextContext *ctx);

/* Called once per code point and font for a character no face can draw, with the line the Game's
 * BK_GLYPH_TRACE prints (no trailing newline): BK_GLYPH_TRACE: no glyph U+XXXX in font "name".
 * The callback runs inside the call that shaped the text. Without a callback the line goes to
 * stderr when the BK_GLYPH_TRACE environment variable is set. Pass null to remove it. */
BK_TEXT_API BkTextResult bk_text_context_set_missing_glyph_callback(
	BkTextContext *ctx, void (*fn)(void *user, uint32_t codepoint, uint32_t utf16_index, const char *trace_line), void *user);

/* A font is a size class and a pixel size of the context's face chain. */
BK_TEXT_API BkTextResult bk_text_font_open(BkTextContext *ctx, uint32_t size_class, float pixel_size, BkTextFont **out);
BK_TEXT_API void bk_text_font_close(BkTextFont *font);

/* Shapes text into at most cap glyphs in text order. BUFFER_TOO_SMALL sets *out_count to the
 * needed number; OK sets it to the number written. */
BK_TEXT_API BkTextResult bk_text_shape_utf16(
	BkTextFont *font, const uint16_t *text, uint32_t len, BkTextGlyph *out, uint32_t cap, uint32_t *out_count);

/* The advance sum of text, the same number shaping and wrapping use. */
BK_TEXT_API BkTextResult bk_text_measure_utf16(BkTextFont *font, const uint16_t *text, uint32_t len, BkTextMetrics *out);

/* Breaks text into lines of at most width pixels (kinsoku rules for CJK, as the Game does). */
BK_TEXT_API BkTextResult bk_text_wrap_utf16(
	BkTextFont *font, const uint16_t *text, uint32_t len, float width, BkTextLine *out, uint32_t cap, uint32_t *out_count);

/* The atlas page size and its generation. A generation change means the page was cleared to make
 * room: every rect cached from an older generation is invalid and glyphs must be asked for again. */
BK_TEXT_API BkTextResult bk_text_atlas_info(BkTextContext *ctx, uint32_t *w, uint32_t *h, uint32_t *generation);
/* The atlas entry of a glyph from bk_text_shape_utf16 at this font's pixel size, rasterised and
 * packed on first use. ATLAS_FULL when the frame's glyphs alone overflow the page. */
BK_TEXT_API BkTextResult bk_text_atlas_glyph(BkTextFont *font, uint32_t glyph_key, BkTextAtlasGlyph *out);
/* Glyphs used in the frame are not evicted; call at the start of every frame. */
BK_TEXT_API BkTextResult bk_text_atlas_begin_frame(BkTextContext *ctx, uint64_t frame);
/* The rects changed since the last successful call (after an eviction the whole page); a
 * successful call clears them. BUFFER_TOO_SMALL keeps them and sets *count to the needed number. */
BK_TEXT_API BkTextResult bk_text_atlas_take_dirty(BkTextContext *ctx, BkTextRect *out, uint32_t cap, uint32_t *count);
/* The page's R8 coverage bytes, pitch bytes per row. Valid until the next bk_text_atlas_glyph. */
BK_TEXT_API BkTextResult bk_text_atlas_pixels(BkTextContext *ctx, const uint8_t **pixels, uint32_t *pitch);

BK_TEXT_API BkTextResult bk_text_stats(BkTextContext *ctx, BkTextStats *out);

#ifdef __cplusplus
}
#endif

#endif /* BK_TEXT_H */
