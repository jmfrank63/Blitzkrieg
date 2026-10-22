#ifndef BLITZKRIEG_FONT_GPU_H
#define BLITZKRIEG_FONT_GPU_H

#include "..//GFX//GFX.H"
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>
#if defined( _MSC_VER )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "..//Formats//fmtFont.h"
#include "..//GFX//GFXHelper.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

// bk_text's font handle (Sources/src/BkText/bk_text.h), opaque here.
typedef struct BkTextFont BkTextFont;

class FontGpu final : public IGFXFont
{
public:
    FontGpu();
    // Recreated by the object factory when a saved game is loaded; the manager's
    // shared map then supplies the name and calls Load().
    static IRefCount * STDCALL CreateNewClassInstanceInternal() { return new FontGpu(); }
    int STDCALL operator&( IStructureSaver &ss ) override;
    // S04/T08a: BK_REF_TRACE=<substring> names every reference transition
    // with the holder's module+offset (the same instrumentation as the
    // CInputBinder override; FontGpu's glyph storage was the Game's largest
    // remaining family at 564 blocks).
    static void TraceWhere( void *pCaller, char *szOut, int nOut )
    {
#if defined( _MSC_VER )
        HMODULE hMod = 0;
        if ( pCaller != 0 && GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>( pCaller ), &hMod ) != 0 )
        {
            char szMod[MAX_PATH] = "";
            GetModuleFileNameA( hMod, szMod, MAX_PATH );
            const char *pszName = szMod;
            for ( const char *p = szMod; *p != 0; ++p )
                if ( *p == '\\' || *p == '/' )
                    pszName = p + 1;
            _snprintf( szOut, nOut, "%s+0x%llx", pszName, (unsigned long long)pCaller - reinterpret_cast<unsigned long long>( hMod ) );
            return;
        }
#endif
        snprintf( szOut, nOut, "addr=%p", pCaller );
    }
    // The immediate caller, per compiler (S04/T08a's guarded pattern).
    static void *TraceCaller()
    {
#if defined( _MSC_VER )
        return _ReturnAddress();
#elif defined( __GNUC__ )
        return __builtin_return_address( 0 );
#else
        return 0;
#endif
    }
    void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) override
    {
        (void)nMask;
        static const bool bTrace = []() { const char *psz = getenv( "BK_REF_TRACE" ); return psz != 0 && *psz != 0; }();
        ref_count_ += nRef;
        if ( bTrace )
        {
            char szWhere[320];
            TraceWhere( TraceCaller(), szWhere, sizeof( szWhere ) );
            fprintf( stderr, "font-ref: refs=%d Add caller=%s\n", ref_count_, szWhere );
        }
    }
    void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) override
    {
        (void)nMask;
        static const bool bTrace = []() { const char *psz = getenv( "BK_REF_TRACE" ); return psz != 0 && *psz != 0; }();
        ref_count_ -= nRef;
        if ( bTrace )
        {
            char szWhere[320];
            TraceWhere( TraceCaller(), szWhere, sizeof( szWhere ) );
            fprintf( stderr, "font-ref: refs=%d Rel caller=%s\n", ref_count_, szWhere );
        }
        if ( ref_count_ == 0 ) delete this;
    }
    bool STDCALL IsValid() const override { return ref_count_ >= 0 && ( texture_ != nullptr || bk_font_ != nullptr ); }
    void STDCALL SwapData( ISharedResource *resource ) override;
    int STDCALL GetRefCounter() const override { return ref_count_; }
    const char * STDCALL GetSharedResourceName() const override { return name_.c_str(); }
    void STDCALL SetSharedResourceName( const std::string &name ) override { name_ = name; }
    bool STDCALL Load( bool bPreLoad = false ) override;
    void STDCALL ClearInternalContainer() override {}
    int STDCALL GetHeight() const override { return format_.metrics.nHeight; }
    int STDCALL GetLineSpace() const override { return format_.GetLineSpace(); }
    int STDCALL GetAscent() const override { return format_.metrics.nAscent; }
    int STDCALL GetDescent() const override { return format_.metrics.nDescent; }
    const SFontFormat & STDCALL GetFormat() const override { return format_; }
    int STDCALL GetTextWidth( const char *text, int count = 2000000000 ) const override;
    int STDCALL GetTextWidth( const WORD *text, int count = 2000000000 ) const override;
    float TextWidthFloat( const WORD *text, int count = 2000000000 ) const;

    IGFXTexture *Texture() const { return texture_; }

    // BK_TEXT_ENGINE=ttf (M004): the font is a bk_text font and draws from the glyph atlas, not
    // from a bitmap texture. format_ then holds only synthetic metrics.
    bool IsTtf() const { return bk_font_ != nullptr; }
    BkTextFont *TtfFont() const { return bk_font_; }
    // Closes the bk_text font if its context is still the one it was opened in.
    void CloseTtf();
    // Appends the quads of one line of UTF-16 text (at most up to its first \n or \r), shaped by
    // bk_text and placed from the atlas. Returns kTtfOk, kTtfRetry when the atlas was cleared
    // while the line was built (every quad of the batch is stale: build it again with the new
    // generation), or kTtfAtlasFull when the glyphs do not fit the page (the line draws nothing).
    enum { kTtfOk = 0, kTtfRetry = 1, kTtfAtlasFull = 2 };
    int AppendTtfLine( const uint16_t *text, size_t length, float x, float y, float scale, DWORD color,
        std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices, unsigned generation,
        float clip_top = -1e30f, float clip_bottom = 1e30f ) const;
    // clip_top/clip_bottom bound the glyph quads the way CTextClipVisitor does
    // in the D3D path: a line that only partly fits its rectangle is cut, not
    // spilled over whatever sits below the text box.
    bool AppendGeometry( const wchar_t *text, float x, float y, float scale, DWORD color,
        std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices,
        float clip_top = -1e30f, float clip_bottom = 1e30f ) const;
    // Same, over the [text, text + length) slice of a larger buffer. DrawText
    // draws every wrapped line out of one widened copy of the string, and
    // cutting each line out of it with substr() allocated once per line, per
    // frame.
    bool AppendGeometry( const wchar_t *text, size_t length, float x, float y, float scale, DWORD color,
        std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices,
        float clip_top = -1e30f, float clip_bottom = 1e30f ) const;

private:
    ~FontGpu();
    template <typename T> float TextWidth( const T *text, int count ) const;
    bool OpenTtf();
    float TtfAdvance( const uint16_t *text, size_t length ) const;
    // BK_GLYPH_TRACE: names a character this font has no glyph for, once per font load. The font
    // draws its default character instead, an empty box, and says nothing; a frame measure of solid
    // boxes misses that, so the l10n tiers read this line.
    void ReportMissingGlyph( WORD character ) const;
    int ref_count_ = 0;
    std::string name_;
    SFontFormat format_{};
    IGFXTexture *texture_ = nullptr;
    BkTextFont *bk_font_ = nullptr;
    unsigned bk_serial_ = 0;
    // Atlas rects handed out in the current frame, by glyph key. Looking a glyph up marks it used for
    // the frame, so one lookup per glyph per frame is enough; dropped on a new frame or generation.
    // Same fields as BkTextAtlasGlyph, which this header keeps opaque; the page generation is the cache's own.
    // Direct-mapped by glyph key: a collision only costs a lookup. A slot is live when its epoch is the current one.
    struct AtlasRect { uint32_t key, epoch, x, y, w, h; int32_t bearing_x, bearing_y; };
    static const unsigned kAtlasRectSlots = 1024;
    mutable std::vector<AtlasRect> atlas_rects_;
    mutable uint32_t atlas_rects_epoch_ = 1;
    mutable uint64_t atlas_rects_frame_ = ~uint64_t( 0 );
    mutable uint32_t atlas_rects_generation_ = 0;
    mutable std::unordered_set<WORD> missing_reported_;
    int shared_resource_last_usage_ = 0;
};

// Shared greedy line breaking, so the row height reported by
// TextGpu::GetNumLines and the lines actually emitted by
// GraphicsEngineGpu::DrawText can never disagree. Returns [begin,end)
// ranges over the UTF-16 string; carriage returns are skipped.
void WrapTextLines( const FontGpu *font, const WORD *text, float scale, float wrap_width,
    std::vector<std::pair<size_t, size_t> > &lines );

// BK_PERF=1 text timing (M004 S03): steady-clock time and call counts of the measuring, wrapping
// and drawing work, on the bitmap and the bk_text path alike, printed once a second beside the
// BK_PERF frame line. Only the outermost scope on a thread adds its time (a draw that wraps and
// measures is all draw), so the three buckets never overlap; nested scopes still count calls.
// Without BK_PERF the scope is one static bool test.
namespace NTextPerf
{
    enum Bucket { Measure, Draw, Wrap, BucketCount };
    bool Enabled();
    void Add( Bucket bucket, double seconds, bool outermost );
    class Scope
    {
    public:
        explicit Scope( Bucket bucket );
        ~Scope();
    private:
        Bucket bucket_;
        bool active_;
        bool outermost_ = false;
        long long start_ = 0;
    };
    // Returns "measure X ms draw Y ms wrap Z ms calls N" for the work since the last call and
    // resets it; the milliseconds are totals over the interval.
    std::string TakeInterval();
}

class IGFXTextGpuFontProvider
{
public:
    virtual ~IGFXTextGpuFontProvider() = default;
    virtual IGFXFont *Font() const = 0;
    virtual DWORD Color() const = 0;
    virtual float Scale() const = 0;
    // The line breaking the text object has already done for GetNumLines.
    // DrawText reuses it instead of re-wrapping the whole string every frame;
    // the two agreed by construction anyway, since both called WrapTextLines
    // with the same font, scale and width.
    virtual const std::vector<std::pair<size_t, size_t> > &WrappedLines() const = 0;
};

#endif
