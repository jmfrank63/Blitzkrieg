// stb_truetype with internal linkage (STBTT_STATIC), exactly as FontAtlas.cpp
// includes it, so the copies inside dcimgui and the editors never clash. The
// non-static bk_stbtt_* wrappers are the only way Zig reaches it.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

#include <stddef.h>

size_t bk_stbtt_info_size(void)
{
	return sizeof(stbtt_fontinfo);
}

int bk_stbtt_init_font(void *info, const unsigned char *data, int offset)
{
	return stbtt_InitFont((stbtt_fontinfo *)info, data, offset);
}

int bk_stbtt_find_glyph_index(const void *info, int codepoint)
{
	return stbtt_FindGlyphIndex((const stbtt_fontinfo *)info, codepoint);
}

void bk_stbtt_get_font_vmetrics(const void *info, int *ascent, int *descent, int *line_gap)
{
	stbtt_GetFontVMetrics((const stbtt_fontinfo *)info, ascent, descent, line_gap);
}

float bk_stbtt_scale_for_mapping_em_to_pixels(const void *info, float pixels)
{
	return stbtt_ScaleForMappingEmToPixels((const stbtt_fontinfo *)info, pixels);
}

void bk_stbtt_get_glyph_hmetrics(const void *info, int glyph, int *advance, int *lsb)
{
	stbtt_GetGlyphHMetrics((const stbtt_fontinfo *)info, glyph, advance, lsb);
}

void bk_stbtt_get_glyph_bitmap_box_subpixel(const void *info, int glyph, float sx, float sy, float shift_x, float shift_y, int *x0, int *y0, int *x1, int *y1)
{
	stbtt_GetGlyphBitmapBoxSubpixel((const stbtt_fontinfo *)info, glyph, sx, sy, shift_x, shift_y, x0, y0, x1, y1);
}

void bk_stbtt_make_glyph_bitmap_subpixel(const void *info, unsigned char *output, int out_w, int out_h, int out_stride, float sx, float sy, float shift_x, float shift_y, int glyph)
{
	stbtt_MakeGlyphBitmapSubpixel((const stbtt_fontinfo *)info, output, out_w, out_h, out_stride, sx, sy, shift_x, shift_y, glyph);
}
