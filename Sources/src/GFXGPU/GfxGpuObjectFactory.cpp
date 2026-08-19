#include "StdAfx.h"

#include "GraphicsEngineGpu.h"
#include "MeshManagerGpu.h"
#include "TextureGpu.h"
#include "FontGpu.h"
#include "..//Main//TextSystem.h"

#include "..//GFX//GFXObjectFactory.h"

#include <map>
#include <cstring>
#include <cstdio>

FontGpu::FontGpu() = default;

FontGpu::~FontGpu()
{
    if ( texture_ ) texture_->Release();
}

void STDCALL FontGpu::SwapData( ISharedResource *resource )
{
    FontGpu *other = dynamic_cast<FontGpu *>( resource );
    if ( !other ) return;
    std::swap( format_, other->format_ );
}

bool STDCALL FontGpu::Load( bool bPreLoad )
{
    ISingleton *globals = GetSingletonGlobal();
    if ( !globals || name_.empty() ) return false;
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
    if ( bPreLoad ) return true;

    ITextureManager *textures = GetSingleton<ITextureManager>( globals );
    if ( !textures ) return false;
    texture_ = textures->GetTexture( (storage_name + "\\1").c_str() );
    if ( !texture_ ) return false;
    texture_->AddRef();
    return true;
}

template <typename T>
float FontGpu::TextWidth( const T *text, int count ) const
{
    if ( !text || count <= 0 ) return 0;
    float width = 0;
    T previous = 0;
    for ( int i = 0; i < count && text[i] && text[i] != static_cast<T>('\n'); ++i )
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

int STDCALL FontGpu::GetTextWidth( const char *text, int count ) const { return static_cast<int>( TextWidth( text, count ) ); }
// TextWidth is a template, so the UTF-16 string is measured as-is.
int STDCALL FontGpu::GetTextWidth( const WORD *text, int count ) const { return static_cast<int>( TextWidth( text, count ) ); }
float FontGpu::TextWidthFloat( const WORD *text, int count ) const { return TextWidth( text, count ); }

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
    while ( glyphs < length && text[glyphs] && text[glyphs] != L'\n' ) ++glyphs;
    vertices.reserve( vertices.size() + glyphs * 4 );
    indices.reserve( indices.size() + glyphs * 6 );
    // The vertex shader now undoes the D3DCOLOR/RGBA channel order for every
    // vertex colour, so no per-glyph swizzle here.
    WORD previous = 0;
    const float glyph_height = format_.metrics.nHeight * scale;
    const wchar_t *const text_end = text + length;
    for ( const wchar_t *cursor = text; cursor != text_end && *cursor && *cursor != L'\n'; ++cursor )
    {
        const WORD current = static_cast<WORD>( *cursor );
        const SFontFormat::SCharDesc &character = format_.GetChar( current );
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
    void STDCALL Clear( EClearMode, int, int ) override {}
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

void WrapTextLines( const FontGpu *font, const WORD *text, float scale, float wrap_width,
    std::vector<std::pair<size_t, size_t> > &lines )
{
    lines.clear();
    if ( !text ) return;
    size_t length = 0;
    while ( text[length] ) ++length;
    const bool measure = font != 0 && wrap_width > 0.0f;
    const SFontFormat *format = measure ? &font->GetFormat() : 0;
    size_t line_begin = 0;
    size_t last_space = static_cast<size_t>( -1 );
    // Running width of [line_begin, index]. Measuring the whole prefix again
    // for every character made the wrap quadratic, and a mission briefing pays
    // that on every frame it is on screen. The terms have to be added in
    // exactly the order FontGpu::TextWidth adds them: float addition is not
    // associative, and centered or right-aligned text derives its x from these
    // widths, so a single ulp of difference moves a line by a pixel.
    float run_width = 0.0f;
    WORD previous = 0;
    for ( size_t index = 0; index <= length; ++index )
    {
        // CRLF data would otherwise draw the carriage return as a missing glyph.
        if ( index == length || text[index] == L'\n' || text[index] == L'\r' )
        {
            lines.push_back( std::make_pair( line_begin, index ) );
            if ( index + 1 < length && text[index] == L'\r' && text[index + 1] == L'\n' ) ++index;
            line_begin = index + 1;
            last_space = static_cast<size_t>( -1 );
            run_width = 0.0f;
            previous = 0;
            continue;
        }
        if ( text[index] == L' ' ) last_space = index;
        if ( !measure ) continue;
        const WORD current = text[index];
        const SFontFormat::SCharDesc &character = format->GetChar( current );
        run_width += character.fA + format->GetKern( previous, current ) + character.fB + character.fC;
        previous = current;
        if ( index == line_begin ) continue;
        if ( run_width * scale <= wrap_width ) continue;
        const bool break_on_space = last_space != static_cast<size_t>( -1 ) && last_space > line_begin;
        const size_t break_at = break_on_space ? last_space : index;
        lines.push_back( std::make_pair( line_begin, break_at ) );
        line_begin = break_on_space ? break_at + 1 : break_at;
        last_space = static_cast<size_t>( -1 );
        // The word that moved down has to be measured again from the start of
        // the new line, since the kerning against 0 differs from the kerning it
        // had mid-line. Every character is measured at most twice this way, so
        // the wrap stays linear. Breaking on a space that is itself the
        // overflowing character leaves nothing on the new line yet, so the
        // kerning partner has to go back to 0 with it.
        const size_t carried = line_begin <= index ? index - line_begin + 1 : 0;
        run_width = font->TextWidthFloat( text + line_begin, static_cast<int>( carried ) );
        previous = carried != 0 ? text[index] : static_cast<WORD>( 0 );
    }
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
        if ( !text_ || !text_->GetString() ) return 0;
        const WORD *value = text_->GetString();
        int length = 0;
        while ( value[length] && value[length] != L'\n' ) ++length;
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
