#include "StdAfx.h"

#include "GraphicsEngineGpu.h"
#include "MeshManagerGpu.h"
#include "TextureGpu.h"
#include "FontGpu.h"
#include <chrono>
#include "TextWrap.h"
#include "BkTextBridge.h"
#include "..//Main//TextSystem.h"

#include "..//GFX//GFXObjectFactory.h"

#include <map>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <type_traits>

FontGpu::FontGpu() = default;

FontGpu::~FontGpu()
{
    CloseTtf();
    if ( texture_ ) texture_->Release();
}

void FontGpu::CloseTtf()
{
    // The context may be gone (graphics shut down, or recreated for another language) while a
    // font is still referenced; its BkTextFont died with it.
    if ( bk_font_ != nullptr && bk_serial_ == NBkTextBridge::Serial() && NBkTextBridge::Context() != nullptr )
        bk_text_font_close( bk_font_ );
    bk_font_ = nullptr;
}

void STDCALL FontGpu::SwapData( ISharedResource *resource )
{
    FontGpu *other = dynamic_cast<FontGpu *>( resource );
    if ( !other ) return;
    std::swap( format_, other->format_ );
    std::swap( bk_font_, other->bk_font_ );
    std::swap( bk_serial_, other->bk_serial_ );
}

namespace
{
    // The pixel size of each size class under ttf: the shipped bitmap fonts' heights, so layouts
    // built for them keep their proportions (exact line spacing is S03's job).
    float TtfPixelSize( BkTextSizeClass eClass )
    {
        switch ( eClass )
        {
            case BK_TEXT_SIZE_TINY: return 10.0f;
            case BK_TEXT_SIZE_SMALL: return 16.0f;
            case BK_TEXT_SIZE_LARGE: return 40.0f;
            default: return 20.0f;
        }
    }

    // "fonts\\tiny", "fonts/small": the class the name ends in; anything else is medium.
    BkTextSizeClass TtfSizeClass( const std::string &szName )
    {
        std::string sz = szName;
        for ( size_t i = 0; i < sz.size(); ++i ) sz[i] = char( std::tolower( (unsigned char)sz[i] ) );
        if ( sz.find( "tiny" ) != std::string::npos ) return BK_TEXT_SIZE_TINY;
        if ( sz.find( "small" ) != std::string::npos ) return BK_TEXT_SIZE_SMALL;
        if ( sz.find( "large" ) != std::string::npos ) return BK_TEXT_SIZE_LARGE;
        return BK_TEXT_SIZE_MEDIUM;
    }
}

// BK_DEBUG_LOG: one line per font load with the numbers a layout depends on, so the bitmap and
// the ttf font of a name can be compared from two runs' logs.
static void LogFontMetrics( const std::string &name, const char *engine, const SFontFormat &format )
{
    static const bool log = []{ const char *psz = getenv( "BK_DEBUG_LOG" ); return psz != 0 && *psz != 0 && *psz != '0'; }();
    if ( !log ) return;
    std::fprintf( stderr, "font %s %s: height %d ascent %d descent %d external %d line_space %d\n", engine, name.c_str(),
        format.metrics.nHeight, format.metrics.nAscent, format.metrics.nDescent, format.metrics.nExternalLeading, format.GetLineSpace() );
}

// The most the glyphs of a probe text rise above the baseline and sink below it. The probe holds
// the tall letters of Latin plus those of the context's script; a glyph the atlas cannot take is
// left out (the metrics then stay the face's own).
static void MeasureInkEnvelope( BkTextFont *font, float &up, float &down )
{
    static const uint16_t latin[] = { 'H', 'g', 'j', 'p', 'q', 'y', '|', '[', ']', '(', ')', '{', '}', 0xc0, 0xc5, 0xc9, 0xd6, 0xdc, 0xc7, 0xdf, 0 };
    // The Devanagari block, each code point after its own consonant, so a mark has a base to sit on.
    static const std::vector<uint16_t> devanagari = []{ std::vector<uint16_t> block; for ( uint16_t c = 0x901; c != 0x980; ++c ) { block.push_back( 0x915 ); block.push_back( c ); } block.push_back( 0 ); return block; }();
    static const uint16_t cjk[] = { 0x6f22, 0x5b57, 0x3042, 0x3068, 0x300c, 0x300d, 0xd840, 0xdc00, 0 };
    const std::string &language = NBkTextBridge::Language();
    const uint16_t *probes[2] = { latin, nullptr };
    if ( language == "hi" ) probes[1] = devanagari.data();
    else if ( language == "ja" || language == "zh" || language == "ko" ) probes[1] = cjk;
    for ( int probe = 0; probe != 2; ++probe )
    {
        if ( probes[probe] == nullptr ) continue;
        uint32_t length = 0;
        while ( probes[probe][length] ) ++length;
        std::vector<BkTextGlyph> glyphs( length * 2 + 8 );
        uint32_t count = 0;
        if ( bk_text_shape_utf16( font, probes[probe], length, glyphs.data(), uint32_t( glyphs.size() ), &count ) != BK_TEXT_OK ) continue;
        for ( uint32_t i = 0; i != count; ++i )
        {
            BkTextAtlasGlyph atlas = {};
            if ( bk_text_atlas_glyph( font, glyphs[i].glyph_key, &atlas ) != BK_TEXT_OK || atlas.rect.w == 0 || atlas.rect.h == 0 ) continue;
            const float top = glyphs[i].y_offset + float( atlas.bearing_y );
            const float bottom = float( atlas.rect.h ) - float( atlas.bearing_y ) - glyphs[i].y_offset;
            if ( top > up ) up = top;
            if ( bottom > down ) down = bottom;
        }
    }
}

bool FontGpu::OpenTtf()
{
    BkTextContext *context = NBkTextBridge::Context();
    if ( context == nullptr ) return false;
    CloseTtf();
    const BkTextSizeClass eClass = TtfSizeClass( name_ );
    BkTextFont *font = nullptr;
    if ( bk_text_font_open( context, eClass, TtfPixelSize( eClass ), &font ) != BK_TEXT_OK || font == nullptr ) return false;
    const uint16_t probe = 'H';
    BkTextMetrics metrics = {};
    if ( bk_text_measure_utf16( font, &probe, 1, &metrics ) != BK_TEXT_OK )
    {
        bk_text_font_close( font );
        return false;
    }
    bk_font_ = font;
    bk_serial_ = NBkTextBridge::Serial();
    // A synthetic bitmap-font description: nHeight + nExternalLeading is the line space, no
    // characters and no kerning (the bitmap path's tables are not read under ttf).
    format_ = SFontFormat();
    format_.szFaceName = "bk_text";
    const int nLineSpace = int( metrics.line_space );
    // GetHeight is exactly GetAscent + GetDescent (callers add the two); the line space never
    // drops below it.
    // The face's own ascent and descent, widened to the ink of tall glyphs of the language (marks,
    // accents, brackets, CJK): ink above the top of the line is cut by the text rectangle, and ink
    // below GetHeight overlaps the next line.
    float ink_up = metrics.ascent, ink_down = metrics.descent;
    MeasureInkEnvelope( font, ink_up, ink_down );
    format_.metrics.nAscent = int( ink_up + 0.5f );
    format_.metrics.nDescent = int( ink_down + 0.5f );
    if ( float( format_.metrics.nAscent ) < ink_up ) ++format_.metrics.nAscent;
    if ( float( format_.metrics.nDescent ) < ink_down ) ++format_.metrics.nDescent;
    format_.metrics.nHeight = format_.metrics.nAscent + format_.metrics.nDescent;
    format_.metrics.nExternalLeading = nLineSpace > format_.metrics.nHeight ? nLineSpace - format_.metrics.nHeight : 0;
    format_.metrics.nInternalLeading = 0;
    format_.metrics.nAveCharWidth = int( TtfPixelSize( eClass ) * 0.5f );
    format_.metrics.nMaxCharWidth = int( TtfPixelSize( eClass ) );
    format_.metrics.fSpaceWidth = TtfPixelSize( eClass ) * 0.25f;
    format_.metrics.cCharSet = 0;
    format_.metrics.wDefaultChar = 0;
    missing_reported_.clear();
    LogFontMetrics( name_, "ttf", format_ );
    return true;
}

bool STDCALL FontGpu::Load( bool bPreLoad )
{
    ISingleton *globals = GetSingletonGlobal();
    if ( !globals || name_.empty() ) return false;
    // BK_TEXT_ENGINE=ttf: the font is a bk_text font, there is no 1.tfd and no atlas texture.
    if ( NBkTextBridge::Context() != nullptr ) return OpenTtf();
    IDataStorage *storage = GetSingleton<IDataStorage>( globals );
    if ( !storage ) return false;

    std::string storage_name = name_;
    if ( storage_name.rfind( "fonts", 0 ) == 0 ) storage_name.replace( 0, 5, "Fonts" );
    else if ( storage_name.rfind( "ui", 0 ) == 0 ) storage_name.replace( 0, 2, "UI" );
    else if ( storage_name.rfind( "effects", 0 ) == 0 ) storage_name.replace( 0, 7, "Effects" );
    else if ( storage_name.rfind( "cursor", 0 ) == 0 ) storage_name.replace( 0, 6, "Cursor" );
    const std::string metrics_name = storage_name + "\\1.tfd";
    CPtr<IDataStream> stream = storage->OpenStream( metrics_name.c_str(), STREAM_ACCESS_READ );
    if ( !stream ) return false;
    CPtr<IStructureSaver> structure = CreateStructureSaver( stream, IStructureSaver::READ );
    if ( !structure ) return false;
    CSaverAccessor saver = structure;
    saver.Add( 1, &format_ );
    missing_reported_.clear();
    LogFontMetrics( name_, "bitmap", format_ );
    if ( bPreLoad ) return true;

    ITextureManager *textures = GetSingleton<ITextureManager>( globals );
    if ( !textures ) return false;
    // The lossless level by name, whatever the texture quality: the manager's probe takes the
    // quality's suffix first anywhere in the storage, so a language mod's font (which ships 1_h.dds
    // only) was drawn from the base game's 1_c or 1_l atlas, laid out for other characters.
    IGFXTexture *loaded = textures->GetTexture( (storage_name + "\\1_h.dds").c_str() );
    if ( !loaded ) return false;
    loaded->AddRef();
    // A font read again (FontManagerGpu::Reload) gives its old atlas back.
    if ( texture_ ) texture_->Release();
    texture_ = loaded;
    return true;
}

float FontGpu::TtfAdvance( const uint16_t *text, size_t length ) const
{
    if ( bk_font_ == nullptr || text == nullptr || length == 0 ) return 0;
    BkTextMetrics metrics = {};
    if ( bk_text_measure_utf16( bk_font_, text, uint32_t( length ), &metrics ) != BK_TEXT_OK ) return 0;
    return metrics.advance;
}

template <typename T>
float FontGpu::TextWidth( const T *text, int count ) const
{
    if ( !text || count <= 0 ) return 0;
    if ( bk_font_ != nullptr )
    {
        // The same contract as the bitmap sum: a line ends at \n or \r, narrow text is Latin-1.
        int used = 0;
        while ( used < count && text[used] && text[used] != static_cast<T>('\n') && text[used] != static_cast<T>('\r') ) ++used;
        // UTF-16 text is measured in place; only narrow text is widened, into a reused buffer.
        if ( sizeof( T ) == sizeof( uint16_t ) )
            return TtfAdvance( reinterpret_cast<const uint16_t *>( text ), used );
        static thread_local std::vector<uint16_t> units;
        units.resize( used );
        for ( int i = 0; i < used; ++i )
            units[i] = static_cast<uint16_t>( static_cast<typename std::make_unsigned<T>::type>( text[i] ) );
        return TtfAdvance( units.data(), units.size() );
    }
    float width = 0;
    T previous = 0;
    for ( int i = 0; i < count && text[i] && text[i] != static_cast<T>('\n') && text[i] != static_cast<T>('\r'); ++i )
    {
        const WORD current = static_cast<WORD>( text[i] );
        const SFontFormat::SCharDesc &character = format_.GetChar( current );
        width += character.fA + format_.GetKern( static_cast<WORD>( previous ), current ) + character.fB + character.fC;
        previous = text[i];
    }
    return width;
}

// CFont stores only its usage counter; the name and the reload come from the
// manager's shared map, which is serialized alongside.
int STDCALL FontGpu::operator&( IStructureSaver &ss )
{
    CSaverAccessor saver = &ss;
    saver.Add( 1, &shared_resource_last_usage_ );
    return 0;
}

int STDCALL FontGpu::GetTextWidth( const char *text, int count ) const { NTextPerf::Scope perf( NTextPerf::Measure ); return static_cast<int>( TextWidth( text, count ) ); }
// TextWidth is a template, so the UTF-16 string is measured as-is.
int STDCALL FontGpu::GetTextWidth( const WORD *text, int count ) const { NTextPerf::Scope perf( NTextPerf::Measure ); return static_cast<int>( TextWidth( text, count ) ); }
float FontGpu::TextWidthFloat( const WORD *text, int count ) const { NTextPerf::Scope perf( NTextPerf::Measure ); return TextWidth( text, count ); }

void FontGpu::ReportMissingGlyph( WORD character ) const
{
    static const bool trace = getenv( "BK_GLYPH_TRACE" ) != 0;
    if ( !trace || !missing_reported_.insert( character ).second ) return;
    std::fprintf( stderr, "BK_GLYPH_TRACE: no glyph U+%04X in font \"%s\"\n", static_cast<unsigned>( character ), name_.c_str() );
}

bool FontGpu::AppendGeometry( const wchar_t *text, float x, float y, float scale, DWORD color,
    std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices,
    float clip_top, float clip_bottom ) const
{
    if ( !text ) return true;
    size_t length = 0;
    while ( text[length] ) ++length;
    return AppendGeometry( text, length, x, y, scale, color, vertices, indices, clip_top, clip_bottom );
}

bool FontGpu::AppendGeometry( const wchar_t *text, size_t length, float x, float y, float scale, DWORD color,
    std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices,
    float clip_top, float clip_bottom ) const
{
    if ( !text || length == 0 || !*text || !texture_ ) return true;
    // Four vertices and six indices per glyph, asked for up front: growing the
    // vectors a quad at a time reallocated and copied several times for every
    // line of text drawn.
    size_t glyphs = 0;
    while ( glyphs < length && text[glyphs] && text[glyphs] != L'\n' && text[glyphs] != L'\r' ) ++glyphs;
    vertices.reserve( vertices.size() + glyphs * 4 );
    indices.reserve( indices.size() + glyphs * 6 );
    // The vertex shader now undoes the D3DCOLOR/RGBA channel order for every
    // vertex colour, so no per-glyph swizzle here.
    WORD previous = 0;
    const float glyph_height = format_.metrics.nHeight * scale;
    // GetChar hands back the default character's description for a missing one: compared by address,
    // so finding a missing glyph costs no lookup of its own.
    const SFontFormat::CCharacterMap::const_iterator fallback = format_.chars.find( format_.metrics.wDefaultChar );
    const SFontFormat::SCharDesc *const fallback_desc = fallback != format_.chars.end() ? &fallback->second : nullptr;
    const wchar_t *const text_end = text + length;
    for ( const wchar_t *cursor = text; cursor != text_end && *cursor && *cursor != L'\n' && *cursor != L'\r'; ++cursor )
    {
        const WORD current = static_cast<WORD>( *cursor );
        const SFontFormat::SCharDesc &character = format_.GetChar( current );
        if ( &character == fallback_desc && current != format_.metrics.wDefaultChar ) ReportMissingGlyph( current );
        x += (character.fA + format_.GetKern( previous, current )) * scale;
        const float glyph_width = character.fWidth * scale + 0.5f;
        // Cut the quad against the text rectangle and carry the texture
        // coordinates with it, exactly as ClipAARect does for the D3D path.
        // The pen still advances over a fully clipped glyph, so the visible
        // part of a half-shown line keeps its spacing.
        float top_fraction = 0.0f;
        float bottom_fraction = 1.0f;
        if ( glyph_height > 0.0f )
        {
            if ( y < clip_top ) top_fraction = ( clip_top - y ) / glyph_height;
            if ( y + glyph_height > clip_bottom ) bottom_fraction = ( clip_bottom - y ) / glyph_height;
        }
        if ( bottom_fraction > top_fraction )
        {
            const float top = y + top_fraction * glyph_height;
            const float bottom = y + bottom_fraction * glyph_height;
            const float v1 = character.y1 + top_fraction * ( character.y2 - character.y1 );
            const float v2 = character.y1 + bottom_fraction * ( character.y2 - character.y1 );
            const WORD base = static_cast<WORD>( vertices.size() );
            vertices.emplace_back(); vertices.back().Setup( x, bottom, 0, 1, color, 0xff000000, character.x1, v2 );
            vertices.emplace_back(); vertices.back().Setup( x, top, 0, 1, color, 0xff000000, character.x1, v1 );
            vertices.emplace_back(); vertices.back().Setup( x + glyph_width, bottom, 0, 1, color, 0xff000000, character.x2, v2 );
            vertices.emplace_back(); vertices.back().Setup( x + glyph_width, top, 0, 1, color, 0xff000000, character.x2, v1 );
            indices.push_back( base + 2 ); indices.push_back( base + 1 ); indices.push_back( base + 0 );
            indices.push_back( base + 1 ); indices.push_back( base + 2 ); indices.push_back( base + 3 );
        }
        x += (character.fB + character.fC) * scale;
        previous = current;
    }
    return true;
}

int FontGpu::AppendTtfLine( const uint16_t *text, size_t length, float x, float y, float scale, DWORD color,
    std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices, unsigned generation,
    float clip_top, float clip_bottom ) const
{
    BkTextContext *context = NBkTextBridge::Context();
    if ( bk_font_ == nullptr || context == nullptr || text == nullptr ) return kTtfOk;
    size_t used = 0;
    while ( used < length && text[used] && text[used] != '\n' && text[used] != '\r' ) ++used;
    if ( used == 0 ) return kTtfOk;
    uint32_t page_w = 0, page_h = 0, page_generation = 0;
    if ( bk_text_atlas_info( context, &page_w, &page_h, &page_generation ) != BK_TEXT_OK || page_w == 0 || page_h == 0 ) return kTtfOk;
    // Reused between calls: lines are drawn every frame.
    if ( atlas_rects_frame_ != NBkTextBridge::Frame() || atlas_rects_generation_ != page_generation )
    {
        if ( atlas_rects_.empty() ) atlas_rects_.assign( kAtlasRectSlots, AtlasRect() );
        ++atlas_rects_epoch_;
        atlas_rects_frame_ = NBkTextBridge::Frame();
        atlas_rects_generation_ = page_generation;
    }
    static thread_local std::vector<BkTextGlyph> glyphs;
    if ( glyphs.size() < used + 8 ) glyphs.resize( used + 8 );
    uint32_t count = 0;
    BkTextResult shaped = bk_text_shape_utf16( bk_font_, text, uint32_t( used ), glyphs.data(), uint32_t( glyphs.size() ), &count );
    if ( shaped == BK_TEXT_BUFFER_TOO_SMALL )
    {
        glyphs.resize( count );
        shaped = bk_text_shape_utf16( bk_font_, text, uint32_t( used ), glyphs.data(), uint32_t( glyphs.size() ), &count );
    }
    if ( shaped != BK_TEXT_OK ) return kTtfOk;
    const float baseline = y + float( format_.metrics.nAscent ) * scale;
    float pen = x;
    // Quads are written into room made once for the whole line: appending vertex by vertex costs
    // more than the quad's arithmetic, and lists draw hundreds of lines a frame.
    const size_t first_vertex = vertices.size();
    const size_t first_index = indices.size();
    vertices.resize( first_vertex + size_t( count ) * 4 );
    indices.resize( first_index + size_t( count ) * 6 );
    size_t quads = 0;
    // Every exit shrinks the buffers to the quads written.
    auto finish = [&]( int status ) { vertices.resize( first_vertex + quads * 4 ); indices.resize( first_index + quads * 6 ); return status; };
    for ( uint32_t i = 0; i < count; ++i )
    {
        const BkTextGlyph &glyph = glyphs[i];
        AtlasRect &slot = atlas_rects_[( glyph.glyph_key ^ ( glyph.glyph_key >> 10 ) ) % kAtlasRectSlots];
        if ( slot.epoch != atlas_rects_epoch_ || slot.key != glyph.glyph_key )
        {
            BkTextAtlasGlyph atlas;
            const BkTextResult found = bk_text_atlas_glyph( bk_font_, glyph.glyph_key, &atlas );
            if ( found == BK_TEXT_ATLAS_FULL ) return finish( kTtfAtlasFull );
            if ( found != BK_TEXT_OK ) { pen += glyph.x_advance * scale; continue; }
            // The page was cleared to make room for this glyph: the rects of the quads built so far
            // point at other glyphs now.
            if ( atlas.generation != generation ) return finish( kTtfRetry );
            const AtlasRect rect = { glyph.glyph_key, atlas_rects_epoch_, atlas.rect.x, atlas.rect.y, atlas.rect.w, atlas.rect.h, atlas.bearing_x, atlas.bearing_y };
            slot = rect;
        }
        const AtlasRect &atlas = slot;
        if ( atlas.w != 0 && atlas.h != 0 )
        {
            const float left = std::floor( pen + ( glyph.x_offset + float( atlas.bearing_x ) ) * scale + 0.5f );
            float top = std::floor( baseline - ( glyph.y_offset + float( atlas.bearing_y ) ) * scale + 0.5f );
            const float quad_w = float( atlas.w ) * scale;
            const float quad_h = float( atlas.h ) * scale;
            // Snapping to a whole pixel may move the ink half a pixel out of the line, which is
            // GetHeight * scale tall: pull it back in, so ink never leaves what GetHeight reports.
            const float line_height = float( format_.metrics.nHeight ) * scale;
            if ( quad_h <= line_height )
            {
                if ( top < y ) top = y;
                else if ( top + quad_h > y + line_height ) top = y + line_height - quad_h;
            }
            // Cut against the text rectangle and carry the texture coordinates, as the bitmap path does.
            float top_fraction = 0.0f;
            float bottom_fraction = 1.0f;
            if ( top < clip_top ) top_fraction = ( clip_top - top ) / quad_h;
            if ( top + quad_h > clip_bottom ) bottom_fraction = ( clip_bottom - top ) / quad_h;
            if ( bottom_fraction > top_fraction )
            {
                const float u1 = float( atlas.x ) / float( page_w );
                const float u2 = float( atlas.x + atlas.w ) / float( page_w );
                const float v_top = float( atlas.y ) / float( page_h );
                const float v_span = float( atlas.h ) / float( page_h );
                const float v1 = v_top + top_fraction * v_span;
                const float v2 = v_top + bottom_fraction * v_span;
                const float y1 = top + top_fraction * quad_h;
                const float y2 = top + bottom_fraction * quad_h;
                const WORD base = static_cast<WORD>( first_vertex + quads * 4 );
                SGFXLVertex *quad = vertices.data() + first_vertex + quads * 4;
                quad[0].Setup( left, y2, 0, 1, color, 0xff000000, u1, v2 );
                quad[1].Setup( left, y1, 0, 1, color, 0xff000000, u1, v1 );
                quad[2].Setup( left + quad_w, y2, 0, 1, color, 0xff000000, u2, v2 );
                quad[3].Setup( left + quad_w, y1, 0, 1, color, 0xff000000, u2, v1 );
                WORD *triangles = indices.data() + first_index + quads * 6;
                triangles[0] = base + 2; triangles[1] = base + 1; triangles[2] = base + 0;
                triangles[3] = base + 1; triangles[4] = base + 2; triangles[5] = base + 3;
                ++quads;
            }
        }
        pen += glyph.x_advance * scale;
    }
    return finish( kTtfOk );
}

class TextureManagerGpu final : public ITextureManager
{
public:
    OBJECT_COMPLETE_METHODS( TextureManagerGpu );
    bool STDCALL Init() override { return true; }
    void STDCALL SetSerialMode( ESharedDataSerialMode ) override {}
    void STDCALL SetShareMode( ESharedDataSharingMode ) override {}
    void STDCALL Clear( EClearMode mode, int, int ) override
    {
        if ( mode == CLEAR_ALL )
            textures_.clear();
    }
    // CTextureManager serializes its shared map, which is what gives every
    // stored texture pointer something to come back to. Without it the factory
    // wrote -1 for the type id and a loaded save handed back null, so
    // CTerrain::MovePatch dereferenced a null tileset.
    int STDCALL operator&( IStructureSaver &ss ) override
    {
        CSaverAccessor saver = &ss;
        saver.Add( 106, &textures_ );
        if ( saver.IsReading() )
        {
            for ( std::map<std::string, CObj<IGFXTexture> >::iterator it = textures_.begin(); it != textures_.end(); ++it )
            {
                if ( it->second == 0 ) continue;
                // The texture restores the resolved stream name itself. Only
                // fall back to the map key for a save written before that was
                // serialized, and resolve the suffix the same way GetTexture
                // does so the reload actually finds a file.
                const char *restored = it->second->GetSharedResourceName();
                if ( !restored || !*restored )
                    it->second->SetSharedResourceName( ResolveStreamName( it->first ) );
                it->second->Load( false );
            }
        }
        return 0;
    }
    // A lookup name has no extension; the file on disk carries a quality
    // suffix. Shared with the deserialization path above.
    std::string ResolveStreamName( const std::string &key ) const
    {
        ISingleton *globals = GetSingletonGlobal();
        IDataStorage *storage = globals ? GetSingleton<IDataStorage>( globals ) : 0;
        if ( !storage ) return std::string();
        const char *suffixes[] = { quality_suffix_.c_str(), "_h.dds", "_c.dds", "_l.dds", ".dds" };
        for ( const char *suffix : suffixes )
        {
            std::string candidate( key );
            if ( candidate.size() < 4 || candidate.substr( candidate.size() - 4 ) != ".dds" ) candidate += suffix;
            if ( storage->IsStreamExist( candidate.c_str() ) ) return candidate;

            std::string normalized = candidate;
            if ( normalized.rfind( "fonts", 0 ) == 0 ) normalized.replace( 0, 5, "Fonts" );
            else if ( normalized.rfind( "ui", 0 ) == 0 ) normalized.replace( 0, 2, "UI" );
            else if ( normalized.rfind( "effects", 0 ) == 0 ) normalized.replace( 0, 7, "Effects" );
            else if ( normalized.rfind( "particles", 0 ) == 0 ) normalized.replace( 0, 9, "Particles" );
            else if ( normalized.rfind( "sprites", 0 ) == 0 ) normalized.replace( 0, 7, "Sprites" );
            else if ( normalized.rfind( "cursor", 0 ) == 0 ) normalized.replace( 0, 6, "Cursor" );
            // Some UI XML uses lowercase "ui\intermissiontextures\...".
            // Canonicalize this segment even after the top-level "ui" -> "UI"
            // replacement above; the old else-if chain skipped this path.
            std::string lowered_for_search = normalized;
            for ( std::string::size_type i = 0; i < lowered_for_search.size(); ++i )
                lowered_for_search[i] = static_cast<char>( std::tolower( static_cast<unsigned char>( lowered_for_search[i] ) ) );
            const std::string::size_type intermission_pos = lowered_for_search.find( "intermissiontextures" );
            if ( intermission_pos != std::string::npos )
            {
                normalized.replace( intermission_pos, 20, "IntermissionTextures" );
            }
            if ( normalized != candidate && storage->IsStreamExist( normalized.c_str() ) ) return normalized;

            std::string lowered = normalized;
            const std::string::size_type slash = lowered.find_last_of( "\\/" );
            const std::string::size_type start = slash == std::string::npos ? 0 : slash + 1;
            for ( std::string::size_type i = start; i < lowered.size(); ++i )
                lowered[i] = static_cast<char>( std::tolower( static_cast<unsigned char>( lowered[i] ) ) );
            if ( lowered != normalized && storage->IsStreamExist( lowered.c_str() ) ) return lowered;
        }
        return std::string();
    }
    IGFXTexture * STDCALL GetTexture( const char *name ) override
    {
        if ( !name || !*name ) return nullptr;
        const std::string key( name );
        const bool bTraceUITextures = getenv( "BK_UI_TEX_TRACE" ) != 0;
        const auto cached = textures_.find( key );
        if ( cached != textures_.end() ) return cached->second;
        ISingleton *globals = GetSingletonGlobal();
        if ( !globals ) return nullptr;
        GraphicsEngineGpu *owner = dynamic_cast<GraphicsEngineGpu *>( GetSingleton<IGFX>( globals ) );
        IDataStorage *storage = GetSingleton<IDataStorage>( globals );
        if ( !owner || !storage ) return nullptr;
        const std::string stream_name = ResolveStreamName( key );
        if ( stream_name.empty() )
        {
            if ( bTraceUITextures )
            {
                const bool bIsUITexture = key.rfind( "ui\\", 0 ) == 0 || key.rfind( "UI\\", 0 ) == 0;
                if ( bIsUITexture )
                    std::fprintf( stderr, "BK_UI_TEX_TRACE: unresolved key=\"%s\"\n", key.c_str() );
            }
            return nullptr;
        }
        if ( bTraceUITextures )
        {
            const bool bIsUITexture = key.rfind( "ui\\", 0 ) == 0 || key.rfind( "UI\\", 0 ) == 0;
            if ( bIsUITexture )
                std::fprintf( stderr, "BK_UI_TEX_TRACE: key=\"%s\" -> stream=\"%s\"\n", key.c_str(), stream_name.c_str() );
        }
        CObj<IGFXTexture> texture = new TextureGpu( owner, 0, 0, 0, GFXPF_UNKNOWN, GFXD_STATIC );
        texture->SetSharedResourceName( stream_name );
        if ( !texture->Load() ) return nullptr;
        textures_[key] = texture;
        return texture;
    }
    const char * STDCALL GetTextureName( IGFXTexture *texture ) override
    {
        for ( const auto &entry : textures_ ) if ( entry.second == texture ) return entry.first.c_str();
        return "default";
    }
    void STDCALL SetQuality( ETextureQuality quality ) override
    {
        switch ( quality )
        {
            case TEXTURE_QUALITY_COMPRESSED: quality_suffix_ = "_c.dds"; break;
            case TEXTURE_QUALITY_LOW: quality_suffix_ = "_l.dds"; break;
            case TEXTURE_QUALITY_HIGH: quality_suffix_ = "_h.dds"; break;
            // ResolveStreamName tries this first and then walks the fallback
            // chain, so Ultra shows the _h originals until a _u pack exists.
            case TEXTURE_QUALITY_ULTRA: quality_suffix_ = "_u.dds"; break;
        }
    }

private:
    std::map<std::string, CObj<IGFXTexture> > textures_;
    std::string quality_suffix_ = "_h.dds";

};

class FontManagerGpu final : public IFontManager
{
public:
    OBJECT_COMPLETE_METHODS( FontManagerGpu );
    bool STDCALL Init() override { return true; }
    void STDCALL SetSerialMode( ESharedDataSerialMode ) override {}
    void STDCALL SetShareMode( ESharedDataSharingMode ) override {}
    // CFontManager drops every font on CLEAR_ALL, as the texture manager does.
    // A mod switch relies on it: with the cache kept, "fonts\medium" stayed
    // the base game's font for the whole session after a mod was loaded (or
    // started with), and only a save load, which rebuilds the map, brought in
    // the mod's own font. Fonts still held elsewhere stay alive through their
    // references until those screens are rebuilt.
    void STDCALL Clear( EClearMode mode, int, int ) override
    {
        if ( mode == CLEAR_ALL )
            fonts_.clear();
    }
    // See TextureManagerGpu::operator& - CFontManager serializes the same way.
    int STDCALL operator&( IStructureSaver &ss ) override
    {
        CSaverAccessor saver = &ss;
        saver.Add( 108, &fonts_ );
        if ( saver.IsReading() )
        {
            for ( std::map<std::string, CObj<IGFXFont> >::iterator it = fonts_.begin(); it != fonts_.end(); ++it )
            {
                if ( it->second == 0 ) continue;
                it->second->SetSharedResourceName( it->first );
                it->second->Load( false );
            }
        }
        return 0;
    }
    void STDCALL Reload() override
    {
        // A language switch changes which faces lead the fallback chain: the context is made
        // again, and every font closes its old handle first.
        if ( NBkTextBridge::LanguageChanged() )
        {
            for ( std::map<std::string, CObj<IGFXFont> >::iterator it = fonts_.begin(); it != fonts_.end(); ++it )
                if ( FontGpu *gpu_font = dynamic_cast<FontGpu *>( it->second.GetPtr() ) ) gpu_font->CloseTtf();
            NBkTextBridge::Recreate();
        }
        for ( std::map<std::string, CObj<IGFXFont> >::iterator it = fonts_.begin(); it != fonts_.end(); ++it )
            if ( it->second != 0 )
                it->second->Load( false );
    }
    IGFXFont * STDCALL GetFont( const char *name ) override
    {
        if ( !name || !*name ) return nullptr;
        const std::string key( name );
        const auto cached = fonts_.find( key );
        if ( cached != fonts_.end() ) return cached->second;
        CObj<IGFXFont> font = new FontGpu();
        font->SetSharedResourceName( key );
        if ( !font->Load() ) return nullptr;
        fonts_[key] = font;
        return font;
    }
private:
    std::map<std::string, CObj<IGFXFont> > fonts_;
};

// Adapts a FontGpu to what WrapTextLinesWith asks of a font.
struct FontGpuWrapMetrics
{
    const FontGpu *font;
    const SFontFormat *format;
    float Advance( WORD previous, WORD current ) const
    {
        const SFontFormat::SCharDesc &character = format->GetChar( current );
        return character.fA + format->GetKern( previous, current ) + character.fB + character.fC;
    }
    float RunWidth( const WORD *text, int count ) const { return font->TextWidthFloat( text, count ); }
};

void WrapTextLines( const FontGpu *font, const WORD *text, float scale, float wrap_width,
    std::vector<std::pair<size_t, size_t> > &lines )
{
    if ( font == 0 )
    {
        WrapTextLinesWith<FontGpuWrapMetrics>( 0, text, scale, wrap_width, lines );
        return;
    }
    if ( font->IsTtf() )
    {
        lines.clear();
        if ( !text ) return;
        size_t length = 0;
        while ( text[length] ) ++length;
        static thread_local std::vector<BkTextLine> wrapped;
        if ( wrapped.size() < length + 2 ) wrapped.resize( length + 2 );
        uint32_t count = 0;
        // GetWidth reports int( width * scale ) while bk_text fits a run by its rounded-up width,
        // so a text wrapped to its own GetWidth came out in two lines. Everything GetWidth calls
        // fitting must fit: the limit is the next whole pixel, rounded up in unscaled units.
        const float limit = wrap_width > 0.0f ? std::ceil( ( std::floor( wrap_width ) + 1.0f ) / scale ) : wrap_width / scale;
        BkTextResult result = bk_text_wrap_utf16( font->TtfFont(), text, uint32_t( length ), limit, wrapped.data(), uint32_t( wrapped.size() ), &count );
        if ( result == BK_TEXT_BUFFER_TOO_SMALL )
        {
            wrapped.resize( count );
            result = bk_text_wrap_utf16( font->TtfFont(), text, uint32_t( length ), limit, wrapped.data(), uint32_t( wrapped.size() ), &count );
        }
        if ( result != BK_TEXT_OK ) count = 0;
        for ( uint32_t i = 0; i < count; ++i ) lines.push_back( std::make_pair( size_t( wrapped[i].start ), size_t( wrapped[i].end ) ) );
        return;
    }
    const FontGpuWrapMetrics metrics = { font, &font->GetFormat() };
    WrapTextLinesWith( &metrics, text, scale, wrap_width, lines );
}

class TextGpu final : public IGFXText, public IGFXTextGpuFontProvider
{
public:
    OBJECT_COMPLETE_METHODS( TextGpu );
    int STDCALL operator&( IStructureSaver &ss ) override;
    void STDCALL SetFont( IGFXFont *font ) override { if ( font_.GetPtr() != font ) { font_ = font; wrap_valid_ = false; } }
    IGFXFont *Font() const override { return font_; }
    float Scale() const override { return scale_; }
    void STDCALL SetText( IText *text ) override { if ( text_.GetPtr() != text ) { text_ = text; wrap_valid_ = false; } }
    IText * STDCALL GetText() override { return text_; }
    // Only a real change invalidates: DrawText calls SetWidth with the same
    // rectangle every frame, and dropping the wrap there would defeat the cache.
    void STDCALL SetWidth( int width ) override { if ( width_ != width ) { width_ = width; wrap_valid_ = false; } }
    void STDCALL SetColor( DWORD color ) override { color_ = color; }
    DWORD Color() const override { return color_; }
    void STDCALL EnableRedLine( bool ) override {}
    void STDCALL SetRedLine( int ) override {}
    // The callers' way of saying "my text changed"; CGFXText drops its
    // pre-formatting here for the same reason.
    void STDCALL SetChanged() override { wrap_valid_ = false; }
    void STDCALL SetScale( float scale ) override { const float clamped = scale < 0.1f ? 0.1f : scale; if ( scale_ != clamped ) { scale_ = clamped; wrap_valid_ = false; } }
    float STDCALL GetScale() const override { return scale_; }
    int STDCALL GetNumLines() const override
    {
        if ( !text_ || !text_->GetString() ) return 0;
        // Count the wrapped lines, not just the explicit newlines: the list
        // sizes each row from this, so a wrapped entry needs the taller row.
        EnsureWrapped();
        return wrap_lines_.empty() ? 1 : static_cast<int>( wrap_lines_.size() );
    }
    const std::vector<std::pair<size_t, size_t> > &WrappedLines() const override
    {
        EnsureWrapped();
        return wrap_lines_;
    }
    int STDCALL GetLineSpace() const override { return font_ ? static_cast<int>( font_->GetLineSpace() * scale_ ) : 12; }
    int STDCALL GetWidth( int count = -1 ) const override
    {
        NTextPerf::Scope perf( NTextPerf::Measure );
        if ( !text_ || !text_->GetString() ) return 0;
        const WORD *value = text_->GetString();
        int length = 0;
        while ( value[length] && value[length] != L'\n' && value[length] != L'\r' ) ++length;
        if ( count >= 0 && count < length ) length = count;
        FontGpu *gpu_font = dynamic_cast_ptr<FontGpu *>( font_ );
        return gpu_font ? static_cast<int>( gpu_font->TextWidthFloat( value, length ) * scale_ ) : (font_ ? static_cast<int>( font_->GetTextWidth( value, length ) * scale_ ) : static_cast<int>( length * 8 * scale_ ));
    }

private:
    // GetNumLines is asked once per row while a list lays itself out, every
    // frame, and each call used to break the whole string again. Re-wrap only
    // when something the wrap depends on has actually changed.
    //
    // IText::IsChanged cannot be that signal: it is a one-shot flag that
    // CGFXText::PreFormat clears with ResetChanged and CInterfaceScreenBase
    // polls for tooltip refreshes, so consuming it here would both break those
    // and miss changes whenever two IGFXText share one IText. A shared IText
    // can also have its string replaced with no setter called on us at all.
    // The contents are the only key that cannot go stale, so keep the last
    // wrapped string and compare against it.
    void EnsureWrapped() const
    {
        NTextPerf::Scope perf( NTextPerf::Wrap );
        const WORD *value = text_ ? text_->GetString() : 0;
        size_t length = 0;
        if ( value ) while ( value[length] ) ++length;
        if ( wrap_valid_ && wrap_font_ == font_.GetPtr() && wrap_width_ == width_ && wrap_scale_ == scale_ &&
            wrap_text_.size() == length &&
            ( length == 0 || std::memcmp( &wrap_text_[0], value, length * sizeof( WORD ) ) == 0 ) )
            return;
        WrapTextLines( dynamic_cast_ptr<FontGpu *>( font_ ), value, scale_, static_cast<float>( width_ ), wrap_lines_ );
        if ( value ) wrap_text_.assign( value, value + length );
        else wrap_text_.clear();
        wrap_font_ = font_.GetPtr();
        wrap_width_ = width_;
        wrap_scale_ = scale_;
        wrap_valid_ = true;
    }
    // Owning, as CGFXText's are: a saved game restores the text object through
    // this class, and CWindowState hands its IText over without keeping a
    // reference of its own.
    CPtr<IText> text_;
    CPtr<IGFXFont> font_;
    DWORD color_ = 0xffffffff;
    float scale_ = 1.0f;
    int width_ = 0;
    mutable std::vector<std::pair<size_t, size_t> > wrap_lines_;
    mutable std::vector<WORD> wrap_text_;
    mutable IGFXFont *wrap_font_ = nullptr;
    mutable int wrap_width_ = 0;
    mutable float wrap_scale_ = 0.0f;
    mutable bool wrap_valid_ = false;
};

// Mirrors CGFXText::operator& chunk for chunk, so a saved game written by either
// backend restores in the other. Without it a restored window state came back
// with no text object at all, and CScene::Reposition walked into
// CUIScrollTextBox::RepositionText, which dereferences GetText() unguarded --
// the crash on loading a game.
int STDCALL TextGpu::operator&( IStructureSaver &ss )
{
    CSaverAccessor saver = &ss;
    float width = static_cast<float>( width_ );
    // The GPU text has no red line, but the chunks stay so the layout matches.
    bool red_line = false;
    float red_line_size = 0;
    saver.Add( 1, &text_ );
    saver.Add( 2, &width );
    saver.Add( 3, &color_ );
    saver.Add( 4, &font_ );
    saver.Add( 5, &red_line );
    saver.Add( 7, &red_line_size );
    if ( saver.IsReading() ) width_ = static_cast<int>( width );
    return 0;
}

class CGfxGpuObjectFactory : public CBasicObjectFactory
{
public:
    CGfxGpuObjectFactory()
    {
        REGISTER_CLASS( this, GFX_GFX, GraphicsEngineGpu );
        REGISTER_CLASS( this, GFX_TEXTURE_MANAGER, TextureManagerGpu );
        REGISTER_CLASS( this, GFX_MESH_MANAGER, MeshManagerGpu );
        REGISTER_CLASS( this, GFX_FONT_MANAGER, FontManagerGpu );
        REGISTER_CLASS( this, GFX_TEXTURE, TextureGpu );
        REGISTER_CLASS( this, GFX_FONT, FontGpu );
        REGISTER_CLASS( this, GFX_TEXT, TextGpu );
        REGISTER_CLASS( this, GFX_MESH, MeshGpu );
    }
};

class CGfxGpuModuleChecker : public IModuleChecker
{
public:
    int STDCALL CheckFunctionality() const override { return 0; }
    void STDCALL SetModuleFunctionalityLimits() const override {}
};

static CGfxGpuObjectFactory theGfxGpuObjectFactory;
static CGfxGpuModuleChecker theGfxGpuModuleChecker;
static SModuleDescriptor theModuleDescriptor( "Graphics (SDL GPU)", GFX_GFX, 0x0100, &theGfxGpuObjectFactory, &theGfxGpuModuleChecker );

extern "C" BK_EXPORT const SModuleDescriptor * STDCALL GetModuleDescriptor()
{
    return &theModuleDescriptor;
}
