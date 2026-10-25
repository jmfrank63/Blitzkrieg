// M004 S03: every measuring call of the GPU font and text objects against the geometry the draw
// path emits, on bk_text (BK_TEXT_ENGINE=ttf).
//
// The harness is the GFXGPU module's own sources (FontGpu, TextGpu, GraphicsEngineGpu) over a fake
// GPU API that records the vertices of every draw, so the numbers compared are the ones the Game
// draws. The reference is bk_text itself: the strings are shaped with bk_text_shape_utf16 and the
// atlas gives each glyph's bearing, so a drawn quad can be checked against where its glyph belongs.
//
// The contract, tolerance 0 unless a line says "within 0.5" (a quad is placed at floor(x + 0.5)):
//  1. FontGpu::GetTextWidth(char*) and (WORD*) == trunc(sum of the shaped x_advance), the pen end of
//     AppendTtfLine at scale 1; every drawn quad sits within 0.5 of its glyph's pen position.
//  2. TextGpu::GetWidth(count) == trunc(pen end of the first `count` characters drawn at the scale).
//  3. TextGpu::GetNumLines == the lines DrawText walks: the logical lines of the text, and a text
//     that fits its own measured width is one line (a list row is sized from GetNumLines).
//  4. TextGpu::GetLineSpace == the y step between the first quads of consecutive drawn lines.
//  5. GetNumLines * GetLineSpace == the drawn block height (list row heights).
//  6. GetHeight == GetAscent + GetDescent; the baseline is GetAscent below the top; the ink of tall
//     glyphs lies between the top of the line and GetHeight below it.
// A text wrapped to the width GetWidth gave for its first word breaks right after that word.
// FNT_FORMAT_SINGLE_LINE (SingleLine="1" in a layout) draws a text too wide for its box on one line,
// clipped to the box, aligned and trimmed as CGFXText::FillGeometryData does (RunSingleLine).
//
// Each mismatch prints one line: group/size/scale/string id, the check, measured and drawn numbers.
#include "StdAfx.h"
#include "GFX.H"
#include "../../Sources/src/GFXGPU/GraphicsEngineGpu.h"
#include "../../Sources/src/GFXGPU/FontGpu.h"
#include "../../Sources/src/GFXGPU/BkTextBridge.h"
#include "../../Sources/src/Main/TextSystem.h"
#include "../../Sources/src/StreamIO/Globals.h"
#include "../../Sources/src/BkMemory/bk_memory_sdl.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// The singleton registry is empty: GetGlobalVar answers its defaults, as in gfxgpu_factory_test.cpp.
namespace {
struct EmptySingleton : public ISingleton
{
    bool Register( int, IRefCount * ) override { return false; }
    bool UnRegister( int ) override { return false; }
    bool UnRegister( IRefCount * ) override { return false; }
    IRefCount *Get( int ) override { return nullptr; }
    int GetAllObjects( IRefCount ***, int * ) override { return 0; }
    void Done() override {}
};
EmptySingleton g_emptySingleton;
}
ISingleton *g_pGlobalSingleton = &g_emptySingleton;
ISaveLoadSystem *g_pGlobalSaveLoadSystem = nullptr;
void *( STDCALL *g_pfnGlobalGetTempRawBuffer )( int nAmount, int nBufferIndex ) = nullptr;

extern "C" const SModuleDescriptor * STDCALL GetModuleDescriptor();

namespace {

// The CRT spells setenv differently on Windows.
void SetEnvironment( const char *name, const char *value )
{
#ifdef _WIN32
    _putenv_s( name, value );
#else
    setenv( name, value, 1 );
#endif
}

// ---- the fake GPU API: records the vertices of every indexed temporary draw --------------------
struct Quad { float left, top, right, bottom; };
std::vector<SGFXLVertex> g_vertices;
SDL_Window *g_window = nullptr;
int g_texture_handles = 100;

GfxGpuResult fOk() { return GFXGPU_OK; }
GfxGpuResult fCreate( const GfxGpuCreateInfo *, GfxGpuRenderer **out ) { *out = reinterpret_cast<GfxGpuRenderer *>( 1 ); return GFXGPU_OK; }
void fDestroy( GfxGpuRenderer * ) {}
GfxGpuResult fBegin( GfxGpuRenderer * ) { return GFXGPU_OK; }
GfxGpuResult fEnd( GfxGpuRenderer * ) { return GFXGPU_OK; }
GfxGpuResult fPresent( GfxGpuRenderer * ) { return GFXGPU_OK; }
void fCancel( GfxGpuRenderer * ) {}
GfxGpuResult fClear( GfxGpuRenderer *, const GfxGpuClearInfo * ) { return GFXGPU_OK; }
GfxGpuResult fResize( GfxGpuRenderer *, uint32_t, uint32_t ) { return GFXGPU_OK; }
GfxGpuResult fViewport( GfxGpuRenderer *, const GfxGpuViewportInfo * ) { return GFXGPU_OK; }
GfxGpuResult fTransform( GfxGpuRenderer *, const GfxGpuMatrixInfo *, const GfxGpuMatrixInfo * ) { return GFXGPU_OK; }
GfxGpuResult fState( GfxGpuRenderer *, const GfxGpuStateInfo * ) { return GFXGPU_OK; }
GfxGpuResult fSetTexture( GfxGpuRenderer *, uint64_t ) { return GFXGPU_OK; }
GfxGpuResult fCreateTexture( GfxGpuRenderer *, const GfxGpuTextureCreateInfo *, GfxGpuHandle *out ) { *out = ++g_texture_handles; return GFXGPU_OK; }
GfxGpuResult fUploadTexture( GfxGpuRenderer *, GfxGpuHandle, const GfxGpuTextureUploadInfo * ) { return GFXGPU_OK; }
GfxGpuResult fUploadRegion( GfxGpuRenderer *, GfxGpuHandle, const GfxGpuTextureRegionUploadInfo * ) { return GFXGPU_OK; }
GfxGpuResult fDestroyTexture( GfxGpuRenderer *, GfxGpuHandle ) { return GFXGPU_OK; }
GfxGpuResult fCreateBuffer( GfxGpuRenderer *, const GfxGpuBufferCreateInfo *, GfxGpuHandle *out ) { *out = ++g_texture_handles; return GFXGPU_OK; }
GfxGpuResult fUploadBuffer( GfxGpuRenderer *, GfxGpuHandle, const GfxGpuBufferUploadInfo * ) { return GFXGPU_OK; }
GfxGpuResult fDestroyBuffer( GfxGpuRenderer *, GfxGpuHandle ) { return GFXGPU_OK; }
GfxGpuResult fDraw( GfxGpuRenderer *, uint32_t, uint32_t ) { return GFXGPU_OK; }
GfxGpuResult fDrawIndexed( GfxGpuRenderer *, uint64_t, uint32_t, uint32_t, uint32_t, int32_t ) { return GFXGPU_OK; }
GfxGpuResult fBindVertexBuffer( GfxGpuRenderer *, GfxGpuHandle ) { return GFXGPU_OK; }
GfxGpuResult fTarget( GfxGpuRenderer *, const GfxGpuRenderTargetCreateInfo *, GfxGpuHandle *out ) { *out = ++g_texture_handles; return GFXGPU_OK; }
GfxGpuResult fBindTarget( GfxGpuRenderer *, GfxGpuHandle ) { return GFXGPU_OK; }
GfxGpuResult fDrawTemporaryIndexed( GfxGpuRenderer *, const GfxGpuTemporaryIndexedGeometryInfo *info )
{
    if ( info->stride != sizeof( SGFXLVertex ) ) return GFXGPU_INVALID_ARGUMENT;
    const SGFXLVertex *first = static_cast<const SGFXLVertex *>( info->vertex_data );
    g_vertices.insert( g_vertices.end(), first, first + info->vertex_bytes / info->stride );
    return GFXGPU_OK;
}

std::vector<Quad> Quads()
{
    // AppendTtfLine emits [left,bottom] [left,top] [right,bottom] [right,top] per glyph.
    std::vector<Quad> quads;
    for ( size_t i = 0; i + 3 < g_vertices.size(); i += 4 )
        quads.push_back( Quad{ g_vertices[i].x, g_vertices[i + 1].y, g_vertices[i + 2].x, g_vertices[i].y } );
    return quads;
}

// ---- reporting ----------------------------------------------------------------------------------
int g_checks = 0, g_failures = 0;
std::map<std::string, int> g_failed_by_check;
struct Where { std::string group, string_id; int size = 0; float scale = 1.0f; };
const char *const kSizeNames[] = { "tiny", "small", "medium", "large" };

void Fail( const Where &where, const char *check, double measured, double drawn )
{
    ++g_failures;
    ++g_failed_by_check[check];
    std::printf( "FAIL %s/%s/x%.2f/%s: %s: measured %.3f drawn %.3f\n", where.group.c_str(), kSizeNames[where.size], where.scale, where.string_id.c_str(), check, measured, drawn );
}
bool Expect( const Where &where, const char *check, double measured, double drawn, double tolerance = 0.0 )
{
    ++g_checks;
    if ( std::fabs( measured - drawn ) <= tolerance ) return true;
    Fail( where, check, measured, drawn );
    return false;
}

// value must not be below / above bound; both numbers are printed on failure.
bool ExpectAtLeast( const Where &where, const char *check, double value, double bound )
{
    ++g_checks;
    if ( value >= bound ) return true;
    Fail( where, check, value, bound );
    return false;
}
bool ExpectAtMost( const Where &where, const char *check, double value, double bound )
{
    ++g_checks;
    if ( value <= bound ) return true;
    Fail( where, check, value, bound );
    return false;
}

// ---- reference shaping --------------------------------------------------------------------------
struct Shaped
{
    std::vector<BkTextGlyph> glyphs;
    float pen_end = 0;      // sequential sum of x_advance * scale, as AppendTtfLine accumulates it
    float advance_sum = 0;  // sequential sum of x_advance, scale 1
};
Shaped Shape( BkTextFont *font, const std::u16string &text, float scale )
{
    Shaped shaped;
    shaped.glyphs.resize( text.size() + 8 );
    uint32_t count = 0;
    BkTextResult result = bk_text_shape_utf16( font, reinterpret_cast<const uint16_t *>( text.data() ), uint32_t( text.size() ), shaped.glyphs.data(), uint32_t( shaped.glyphs.size() ), &count );
    if ( result == BK_TEXT_BUFFER_TOO_SMALL )
    {
        shaped.glyphs.resize( count );
        result = bk_text_shape_utf16( font, reinterpret_cast<const uint16_t *>( text.data() ), uint32_t( text.size() ), shaped.glyphs.data(), uint32_t( shaped.glyphs.size() ), &count );
    }
    shaped.glyphs.resize( result == BK_TEXT_OK ? count : 0 );
    for ( size_t i = 0; i != shaped.glyphs.size(); ++i )
    {
        shaped.pen_end += shaped.glyphs[i].x_advance * scale;
        shaped.advance_sum += shaped.glyphs[i].x_advance;
    }
    return shaped;
}

// The drawn quads of one line against the shaped glyphs: one quad per glyph with ink, each within
// 0.5 of the pen position plus bearing (floor(x + 0.5)), its top within 0.5 of the baseline
// (origin_y + GetAscent * scale) minus the glyph's rise, its size the atlas rect times the scale.
void CheckLineQuads( const Where &where, FontGpu *font, const Shaped &shaped, const std::vector<Quad> &quads, size_t first, float origin_x, float origin_y, float scale, size_t *used )
{
    const float baseline = origin_y + float( font->GetAscent() ) * scale;
    float pen = origin_x;
    size_t next = first;
    bool shape_ok = true;
    for ( size_t i = 0; i != shaped.glyphs.size(); ++i )
    {
        const BkTextGlyph &glyph = shaped.glyphs[i];
        BkTextAtlasGlyph atlas = {};
        if ( bk_text_atlas_glyph( font->TtfFont(), glyph.glyph_key, &atlas ) == BK_TEXT_OK && atlas.rect.w != 0 && atlas.rect.h != 0 )
        {
            if ( next >= quads.size() ) { shape_ok = false; break; }
            const Quad &quad = quads[next++];
            const float want_left = pen + ( glyph.x_offset + float( atlas.bearing_x ) ) * scale;
            const float want_top = baseline - ( glyph.y_offset + float( atlas.bearing_y ) ) * scale;
            Expect( where, "quad left within 0.5 of pen+bearing", quad.left, want_left, 0.5 );
            Expect( where, "quad top within 0.5 of baseline-rise", quad.top, want_top, 0.5 );
            Expect( where, "quad width == atlas width*scale", quad.right - quad.left, float( atlas.rect.w ) * scale, 0.001 );
            Expect( where, "quad height == atlas height*scale", quad.bottom - quad.top, float( atlas.rect.h ) * scale, 0.001 );
        }
        pen += glyph.x_advance * scale;
    }
    ++g_checks;
    if ( !shape_ok ) Fail( where, "quad count == glyphs with ink", double( quads.size() - first ), -1 );
    if ( used ) *used = next;
}

// ---- the objects under test ---------------------------------------------------------------------
class TestText final : public IText
{
public:
    explicit TestText( const std::u16string &text ) { Assign( text ); }
    void STDCALL AddRef( int n = 1, int = 0x7fffffff ) override { refs_ += n; }
    void STDCALL Release( int n = 1, int = 0x7fffffff ) override { refs_ -= n; if ( refs_ <= 0 ) delete this; }
    bool STDCALL IsValid() const override { return true; }
    void STDCALL SwapData( ISharedResource * ) override {}
    int STDCALL GetRefCounter() const override { return refs_; }
    const char * STDCALL GetSharedResourceName() const override { return ""; }
    void STDCALL SetSharedResourceName( const std::string & ) override {}
    bool STDCALL Load( const bool = false ) override { return true; }
    void STDCALL ClearInternalContainer() override {}
    const WORD * STDCALL GetString() const override { return units_.data(); }
    const int STDCALL GetLength() const override { return int( units_.size() ) - 1; }
    void STDCALL SetText( const WORD *text ) override { units_.clear(); while ( text && *text ) units_.push_back( *text++ ); units_.push_back( 0 ); }
    bool STDCALL IsChanged() const override { return false; }
    void STDCALL ResetChanged() override {}
private:
    ~TestText() = default;
    void Assign( const std::u16string &text ) { units_.assign( text.begin(), text.end() ); units_.push_back( 0 ); }
    int refs_ = 0;
    std::vector<WORD> units_;
};

struct Harness
{
    GraphicsEngineGpu *engine = nullptr;
    IFontManager *fonts = nullptr;
    IGFXFont *font = nullptr;
    FontGpu *gpu = nullptr;
    IGFXText *text = nullptr;

    // Draws `value` through TextGpu with DrawText into a rect 4000 wide-or-`width` and tall enough
    // not to clip; the pen starts at (kX, kY).
    static constexpr int kX = 10, kY = 20;
    // `lead` rows of headroom above the line inside the rect: DrawText cuts ink above rect.top, as
    // the D3D path does, which would hide a glyph that rises above the line's top.
    std::vector<Quad> DrawText( const std::u16string &value, float scale, int width, DWORD flags = FNT_FORMAT_LEFT, int lead = 0 )
    {
        TestText *source = new TestText( value );
        text->SetText( source );
        text->SetScale( scale );
        text->SetChanged();
        g_vertices.clear();
        RECT rect = { kX, kY - lead, kX + width, kY + 4000 };
        engine->DrawText( text, rect, lead, flags );
        return Quads();
    }
    std::vector<Quad> DrawString( const std::u16string &value )
    {
        std::vector<wchar_t> wide;
        for ( size_t i = 0; i < value.size(); ++i )
        {
            const uint32_t unit = value[i];
            if ( sizeof( wchar_t ) == 4 && unit >= 0xd800 && unit < 0xdc00 && i + 1 < value.size() )
                wide.push_back( wchar_t( 0x10000 + ( ( unit - 0xd800 ) << 10 ) + ( value[++i] - 0xdc00 ) ) );
            else
                wide.push_back( wchar_t( unit ) );
        }
        wide.push_back( 0 );
        engine->SetFont( font );
        g_vertices.clear();
        engine->DrawString( wide.data(), kX, kY, 0xffffffff );
        return Quads();
    }
};

struct Sample { const char *id; std::u16string text; };

std::u16string Repeat( const std::u16string &line, int count )
{
    std::u16string joined;
    for ( int i = 0; i != count; ++i ) { if ( i ) joined += u'\n'; joined += line; }
    return joined;
}
// Where the text first breaks, or npos.
size_t FirstSpace( const std::u16string &text ) { return text.find( u' ' ); }
bool SplitsSurrogate( const std::u16string &text, size_t count )
{
    return count > 0 && count < text.size() && text[count - 1] >= 0xd800 && text[count - 1] < 0xdc00;
}

// One string, one size, one scale: checks 1 to 5 and the wrap.
void RunSample( Harness &h, Where where, const Sample &sample, float scale )
{
    where.string_id = sample.id;
    where.scale = scale;
    BkTextFont *bk = h.gpu->TtfFont();
    const std::u16string &value = sample.text;
    const Shaped one = Shape( bk, value, scale );
    const Shaped plain = Shape( bk, value, 1.0f );

    // (1) FontGpu measures, at scale 1 where DrawString draws.
    if ( scale == 1.0f )
    {
        std::vector<WORD> units( value.begin(), value.end() );
        units.push_back( 0 );
        Expect( where, "GetTextWidth(WORD*) == trunc(sum advance)", h.font->GetTextWidth( units.data() ), int( plain.advance_sum ) );
        bool latin1 = true;
        std::string narrow;
        for ( size_t i = 0; i != value.size(); ++i ) { if ( value[i] > 0xff ) latin1 = false; narrow.push_back( char( value[i] ) ); }
        if ( latin1 ) Expect( where, "GetTextWidth(char*) == trunc(sum advance)", h.font->GetTextWidth( narrow.c_str() ), int( plain.advance_sum ) );
        const std::vector<Quad> drawn = h.DrawString( value );
        CheckLineQuads( where, h.gpu, plain, drawn, 0, float( Harness::kX ), float( Harness::kY ), 1.0f, nullptr );
        // DrawString and DrawText at scale 1 place the same glyphs at the same pixels.
        const std::vector<Quad> through_text = h.DrawText( value, 1.0f, 4000 );
        if ( Expect( where, "DrawString quad count == DrawText quad count", double( drawn.size() ), double( through_text.size() ) ) )
        {
            for ( size_t i = 0; i != drawn.size(); ++i )
            {
                if ( !Expect( where, "DrawString quad left == DrawText quad left", drawn[i].left, through_text[i].left ) ) break;
                if ( !Expect( where, "DrawString quad top == DrawText quad top", drawn[i].top, through_text[i].top ) ) break;
            }
        }
    }

    // (2) TextGpu::GetWidth(count) against the pen end of the first `count` characters drawn.
    {
        h.DrawText( value, scale, 4000 );
        for ( size_t count = 0; count <= value.size(); ++count )
        {
            if ( SplitsSurrogate( value, count ) ) continue;
            const Shaped prefix = Shape( bk, value.substr( 0, count ), scale );
            char check[64];
            std::snprintf( check, sizeof check, "GetWidth(%zu) == trunc(pen end)", count );
            Expect( where, check, h.text->GetWidth( int( count ) ), int( prefix.pen_end ) );
        }
        Expect( where, "GetWidth() == trunc(pen end)", h.text->GetWidth(), int( one.pen_end ) );
    }

    // (3)(4)(5) three copies of the line, one under the other.
    {
        const int kLines = 3;
        const std::vector<Quad> block = h.DrawText( Repeat( value, kLines ), scale, 4000 );
        Expect( where, "GetNumLines == logical lines", h.text->GetNumLines(), kLines );
        const size_t per_line = block.size() / kLines;
        if ( per_line != 0 && block.size() == per_line * kLines )
        {
            const int line_space = h.text->GetLineSpace();
            for ( int line = 1; line != kLines; ++line )
                Expect( where, "GetLineSpace == y step between drawn lines", line_space, block[line * per_line].top - block[( line - 1 ) * per_line].top );
            const float block_height = block[( kLines - 1 ) * per_line].top - block[0].top + float( line_space );
            Expect( where, "GetNumLines*GetLineSpace == drawn block height", h.text->GetNumLines() * line_space, block_height );
        }
        else
            Expect( where, "three drawn lines have equal quad counts", double( block.size() ), double( per_line * kLines ) );
    }

    // Wrap. A text wrapped to the width TextGpu::GetWidth reports for itself is one line.
    {
        h.DrawText( value, scale, 4000 );
        const int own_width = h.text->GetWidth();
        h.DrawText( value, scale, own_width > 0 ? own_width : 1 );
        if ( own_width > 0 && value.find( u'\n' ) == std::u16string::npos )
            Expect( where, "text wrapped to its own GetWidth is one line", h.text->GetNumLines(), 1 );
        // One space: the text wrapped to the width of its first word breaks right after that word.
        const size_t space = FirstSpace( value );
        if ( space != std::u16string::npos && space > 0 && value.find( u' ', space + 1 ) == std::u16string::npos && space + 1 < value.size() )
        {
            h.DrawText( value, scale, 4000 );
            const int word_width = h.text->GetWidth( int( space ) );
            h.DrawText( value, scale, word_width );
            IGFXTextGpuFontProvider *provider = dynamic_cast<IGFXTextGpuFontProvider *>( h.text );
            const std::vector<std::pair<size_t, size_t> > &lines = provider->WrappedLines();
            // The second word is not narrower than the first in general, so it may break again: at
            // least two lines, the first ending at the word.
            ExpectAtLeast( where, "wrapped to the first word's GetWidth: line count", double( lines.size() ), 2.0 );
            if ( !lines.empty() ) Expect( where, "wrapped to the first word's GetWidth: first line ends at the word", double( lines[0].second ), double( space ) );
        }
    }
}

// Newline shapes and the vertical metrics: independent of the language, run on Latin letters.
void RunFixed( Harness &h, Where where, const std::u16string &tall, float scale )
{
    where.scale = scale;
    struct NewlineCase { const char *id; std::u16string text; int lines; int ink_lines; };
    const NewlineCase cases[] = {
        { "nl-lf", u"A\nB", 2, 2 }, { "nl-crlf", u"A\r\nB", 2, 2 }, { "nl-empty", u"", 1, 0 },
        { "nl-trailing", u"A\n", 2, 1 }, { "nl-blank-row", u"A\n\nB", 3, 2 },
    };
    for ( size_t i = 0; i != sizeof cases / sizeof cases[0]; ++i )
    {
        where.string_id = cases[i].id;
        const std::vector<Quad> quads = h.DrawText( cases[i].text, scale, 4000 );
        Expect( where, "GetNumLines == logical lines", h.text->GetNumLines(), cases[i].lines );
        if ( cases[i].ink_lines == 2 && quads.size() == 2 )
        {
            const int rows = cases[i].lines - 1;
            Expect( where, "GetLineSpace * rows == y distance of the two drawn lines", h.text->GetLineSpace() * rows, quads[1].top - quads[0].top );
        }
        else
            Expect( where, "drawn glyph count == glyphs of the non-empty lines", double( quads.size() ), double( cases[i].ink_lines ) );
    }

    // (6) vertical metrics against a line of tall glyphs.
    where.string_id = "vmetrics";
    const std::vector<Quad> quads = h.DrawText( tall, scale, 4000, FNT_FORMAT_LEFT, 50 );
    float min_top = 1e30f, max_bottom = -1e30f;
    for ( size_t i = 0; i != quads.size(); ++i ) { min_top = (std::min)( min_top, quads[i].top ); max_bottom = (std::max)( max_bottom, quads[i].bottom ); }
    Expect( where, "GetHeight == GetAscent + GetDescent", h.font->GetHeight(), h.font->GetAscent() + h.font->GetDescent() );
    ExpectAtLeast( where, "GetLineSpace >= GetHeight*scale", h.text->GetLineSpace(), int( h.font->GetHeight() * scale ) );
    ExpectAtLeast( where, "ink top >= top of the line", min_top, float( Harness::kY ) );
    ExpectAtMost( where, "ink bottom <= top + GetHeight*scale", max_bottom, float( Harness::kY ) + float( h.font->GetHeight() ) * scale );
    if ( !quads.empty() && tall[0] == u'H' )
        Expect( where, "H bottom == top + GetAscent*scale (within 0.5)", quads[0].bottom, float( Harness::kY ) + float( h.font->GetAscent() ) * scale, 0.5 );
}

// FNT_FORMAT_SINGLE_LINE (a layout's SingleLine="1"), as CGFXText::FillGeometryData draws it: one
// line however wide the text, clipped to the rectangle. A left line starts where the unbounded line
// does and is cut at rect.right; a right line ends at rect.right and is cut at rect.left; centred
// text draws nothing. The line is CGFXText::PreFormatLine's: no spaces before the first word or
// after the last, and a line break only ends a word.
void RunSingleLine( Harness &h, Where where, float scale )
{
    where.scale = scale;
    where.string_id = "single-line";
    const std::u16string value = u"The quick brown fox jumps over the lazy dog";
    const std::vector<Quad> wide = h.DrawText( value, scale, 4000 );
    const int width = h.text->GetWidth( int( value.find( u" fox" ) ) );
    const float right_edge = float( Harness::kX + width );
    const int line_space = h.text->GetLineSpace();

    // Without the flag the same text in the same box wraps: the box is too narrow for it.
    const std::vector<Quad> wrapped = h.DrawText( value, scale, width );
    ExpectAtLeast( where, "the text wraps in the box without the flag: lines", h.text->GetNumLines(), 2 );
    float lowest = -1e30f;
    for ( size_t i = 0; i != wrapped.size(); ++i ) lowest = (std::max)( lowest, wrapped[i].top );
    ExpectAtLeast( where, "the text wraps in the box without the flag: drawn below the first line", lowest, float( Harness::kY + line_space ) );

    // Left: the unbounded line's quads left of rect.right, the last one cut there.
    const std::vector<Quad> left = h.DrawText( value, scale, width, FNT_FORMAT_LEFT | FNT_FORMAT_SINGLE_LINE );
    std::vector<Quad> want;
    for ( size_t i = 0; i != wide.size(); ++i )
        if ( wide[i].left < right_edge ) want.push_back( Quad{ wide[i].left, wide[i].top, (std::min)( wide[i].right, right_edge ), wide[i].bottom } );
    ExpectAtLeast( where, "single line left: drawn quads", double( left.size() ), 1.0 );
    if ( Expect( where, "single line left: quad count == unbounded quads left of rect.right", double( left.size() ), double( want.size() ) ) )
        for ( size_t i = 0; i != left.size(); ++i )
        {
            if ( !Expect( where, "single line left: quad left == unbounded quad left", left[i].left, want[i].left ) ) break;
            if ( !Expect( where, "single line left: quad top == unbounded quad top (one line)", left[i].top, want[i].top ) ) break;
            if ( !Expect( where, "single line left: quad right == unbounded quad right cut at rect.right", left[i].right, want[i].right ) ) break;
        }
    // Right: the unbounded line moved to end at rect.right (TextWidthFloat, as DrawText aligns), its
    // quads left of rect.left cut there. A quad is placed at floor(x + 0.5): within 1 of the move.
    const std::vector<Quad> right = h.DrawText( value, scale, width, FNT_FORMAT_RIGHT | FNT_FORMAT_SINGLE_LINE );
    const std::vector<WORD> units( value.begin(), value.end() );
    const float shift = right_edge - h.gpu->TextWidthFloat( units.data(), int( units.size() ) ) * scale - float( Harness::kX );
    want.clear();
    for ( size_t i = 0; i != wide.size(); ++i )
        if ( wide[i].right + shift > float( Harness::kX ) + 1.0f )
            want.push_back( Quad{ (std::max)( wide[i].left + shift, float( Harness::kX ) ), wide[i].top, wide[i].right + shift, wide[i].bottom } );
    ExpectAtLeast( where, "single line right: drawn quads", double( right.size() ), 1.0 );
    if ( Expect( where, "single line right: quad count == unbounded quads right of rect.left", double( right.size() ), double( want.size() ) ) )
        for ( size_t i = 0; i != right.size(); ++i )
        {
            if ( !Expect( where, "single line right: quad left == moved quad left, cut at rect.left (within 1)", right[i].left, want[i].left, 1.0 ) ) break;
            if ( !Expect( where, "single line right: quad top == unbounded quad top (one line)", right[i].top, want[i].top ) ) break;
            if ( !Expect( where, "single line right: quad right == moved quad right (within 1)", right[i].right, want[i].right, 1.0 ) ) break;
            if ( !ExpectAtLeast( where, "single line right: ink within rect.left", right[i].left, float( Harness::kX ) ) ) break;
        }

    // Centred and justified text: FillGeometryData refuses both in single-line mode.
    Expect( where, "single line centre draws nothing", double( h.DrawText( value, scale, width, FNT_FORMAT_CENTER | FNT_FORMAT_SINGLE_LINE ).size() ), 0.0 );
    Expect( where, "single line justify draws nothing", double( h.DrawText( value, scale, width, FNT_FORMAT_JUSTIFY | FNT_FORMAT_SINGLE_LINE ).size() ), 0.0 );

    // PreFormatLine's line: leading and trailing spaces dropped, a line break joins the words.
    struct LineCase { const char *id; std::u16string text, same_as; };
    const LineCase cases[] = { { "single-line-spaces", u"  Hello world  ", u"Hello world" }, { "single-line-break", u"Hello\nworld", u"Helloworld" }, { "single-line-crlf", u"Hello \r\nworld", u"Hello world" } };
    for ( size_t c = 0; c != sizeof cases / sizeof cases[0]; ++c )
    {
        where.string_id = cases[c].id;
        const std::vector<Quad> drawn = h.DrawText( cases[c].text, scale, 4000, FNT_FORMAT_LEFT | FNT_FORMAT_SINGLE_LINE );
        const std::vector<Quad> same = h.DrawText( cases[c].same_as, scale, 4000 );
        if ( Expect( where, "single line: quad count == the line's", double( drawn.size() ), double( same.size() ) ) )
            for ( size_t i = 0; i != drawn.size(); ++i )
                if ( !Expect( where, "single line: quad left == the line's", drawn[i].left, same[i].left ) || !Expect( where, "single line: quad top == the line's", drawn[i].top, same[i].top ) ) break;
    }
}

struct Group { const char *code; std::u16string tall; std::vector<Sample> samples; };

std::vector<Group> Groups()
{
    std::vector<Group> groups;
    Group en = { "en", u"H\u00c5\u00c9gjpq|[]", {} };
    en.samples = {
        { "en-word", u"Hello" }, { "en-sentence", u"Hello world" }, { "en-long", u"The quick brown fox jumps over the lazy dog" },
        { "en-kern", u"AVATAR To Ty" }, { "de-accents", u"\u00c4\u00d6\u00dc\u00df gro\u00dfe Stra\u00dfe" },
        { "ru", u"\u041f\u0440\u0438\u0432\u0435\u0442 \u043c\u0438\u0440" },
        { "emoji-pair", u"A\U0001F600B" }, { "cjk-ext-b-pair", u"A\U00020000B" }, { "missing-glyph", u"A\ue000B" },
    };
    Group ja = { "ja", u"\u6f22\u5b57\u3042\u3068\u300c\u300d\U00020000", {} };
    ja.samples = {
        { "ja-kinsoku", u"\u3053\u3093\u306b\u3061\u306f\u3001\u4e16\u754c\u3002\u300c\u7d42\u308f\u308a\u300d" },
        { "ja-cjk-ext-b", u"\u6f22\U00020000\u5b57" },
    };
    Group hi = { "hi", u"\u0915\u094d\u0937\u093f\u0930\u0940\u0902\u0916\u0917\u0941\u0942", {} };
    hi.samples = {
        { "hi-conjunct", u"\u0915\u094d\u0937\u0924\u094d\u0930\u093f\u092f" },
        { "hi-sentence", u"\u0928\u092e\u0938\u094d\u0924\u0947 \u0926\u0941\u0928\u093f\u092f\u093e" },
    };
    groups.push_back( en ); groups.push_back( ja ); groups.push_back( hi );
    return groups;
}

bool EnsureWindow()
{
    BkMemoryInstallSdlFunctions();
    SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "dummy" );
    if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) ) { std::fprintf( stderr, "SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError() ); return false; }
    g_window = SDL_CreateWindow( "text-measure-draw-test", 800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE );
    if ( g_window == nullptr ) std::fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
    return g_window != nullptr;
}

int Run()
{
    GfxGpuApi api{};
    api.abi_version = GFXGPU_ABI_VERSION;
    api.struct_size = sizeof( api );
    api.create = fCreate; api.destroy = fDestroy;
    api.begin_frame = fBegin; api.end_frame = fEnd; api.present = fPresent; api.cancel_frame = fCancel;
    api.clear = fClear; api.resize = fResize; api.set_viewport = fViewport; api.set_transform = fTransform;
    api.set_state = fState; api.set_texture = fSetTexture;
    api.create_texture = fCreateTexture; api.upload_texture = fUploadTexture; api.upload_texture_region = fUploadRegion; api.destroy_texture = fDestroyTexture;
    api.create_render_target = fTarget; api.bind_render_target = fBindTarget;
    api.create_buffer = fCreateBuffer; api.upload_buffer = fUploadBuffer; api.destroy_buffer = fDestroyBuffer;
    api.draw = fDraw; api.draw_indexed = fDrawIndexed; api.bind_vertex_buffer = fBindVertexBuffer;
    api.draw_temporary_indexed = fDrawTemporaryIndexed;
    (void)fOk;

    if ( !EnsureWindow() ) return 2;
    GraphicsEngineGpu engine( api );
    if ( !NBkTextBridge::Requested() ) { std::fprintf( stderr, "BK_TEXT_ENGINE=ttf was not set\n" ); return 3; }
    if ( !engine.Init( nullptr, GFXNativeWindow( g_window ) ) ) return 4;
    if ( NBkTextBridge::Context() == nullptr ) { std::fprintf( stderr, "the bk_text context was not created (BK_TEXT_FONTS_DIR=%s)\n", std::getenv( "BK_TEXT_FONTS_DIR" ) ); return 5; }
    if ( !engine.SetMode( 800, 600, 32, 0, GFXFS_WINDOWED ) ) return 6;
    if ( !engine.BeginScene() ) return 7;

    const SModuleDescriptor *descriptor = GetModuleDescriptor();
    const char *const font_names[] = { "fonts\\tiny", "fonts\\small", "fonts\\medium", "fonts\\large" };
    const float scales[] = { 1.0f, 1.25f };
    const std::vector<Group> groups = Groups();
    for ( size_t g = 0; g != groups.size(); ++g )
    {
        if ( g != 0 )
        {
            // Another language leads another fallback chain: every font closes before the context is made again.
            SetEnvironment( "BK_TEXT_LANG", groups[g].code );
            if ( !NBkTextBridge::Recreate() ) { std::fprintf( stderr, "bk_text context for %s was not created\n", groups[g].code ); return 8; }
        }
        for ( int size = 0; size != 4; ++size )
        {
            IFontManager *fonts = static_cast<IFontManager *>( descriptor->pFactory->CreateObject( GFX_FONT_MANAGER ) );
            fonts->AddRef();
            Harness h;
            h.engine = &engine;
            h.fonts = fonts;
            h.font = fonts->GetFont( font_names[size] );
            h.gpu = dynamic_cast<FontGpu *>( h.font );
            if ( h.gpu == nullptr || !h.gpu->IsTtf() ) { std::fprintf( stderr, "font %s did not open on bk_text\n", font_names[size] ); return 9; }
            h.text = static_cast<IGFXText *>( descriptor->pFactory->CreateObject( GFX_TEXT ) );
            h.text->AddRef();
            h.text->SetFont( h.font );
            Where where;
            where.group = groups[g].code;
            where.size = size;
            for ( size_t s = 0; s != 2; ++s )
            {
                for ( size_t i = 0; i != groups[g].samples.size(); ++i ) RunSample( h, where, groups[g].samples[i], scales[s] );
                RunFixed( h, where, groups[g].tall, scales[s] );
                if ( g == 0 ) RunSingleLine( h, where, scales[s] );
            }
            h.text->Release();
            engine.SetFont( nullptr );
            fonts->Release();
        }
    }
    engine.EndScene();
    engine.Done();
    return 0;
}

} // namespace

int main()
{
    SetEnvironment( "BK_TEXT_ENGINE", "ttf" );
    SetEnvironment( "BK_TEXT_LANG", "en" );
    const int status = Run();
    if ( g_window != nullptr ) SDL_DestroyWindow( g_window );
    g_window = nullptr;
    SDL_Quit();
    if ( g_failures != 0 )
        std::printf( "text-measure-draw: %d checks, %d failed\n", g_checks, g_failures );
    for ( std::map<std::string, int>::const_iterator it = g_failed_by_check.begin(); it != g_failed_by_check.end(); ++it )
        std::printf( "  %5d  %s\n", it->second, it->first.c_str() );
    std::fflush( stdout );
    if ( status != 0 ) { std::fprintf( stderr, "text-measure-draw: setup failed (%d)\n", status ); std::exit( status ); }
    std::exit( g_failures == 0 ? 0 : 1 );
}
