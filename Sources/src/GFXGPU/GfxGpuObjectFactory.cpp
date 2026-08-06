#include "StdAfx.h"

#include "GraphicsEngineGpu.h"
#include "MeshManagerGpu.h"
#include "TextureGpu.h"
#include "FontGpu.h"
#include "..//Main//TextSystem.h"

#include "..//GFX//GFXObjectFactory.h"

#include <map>
#include <cstring>

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

    const std::string metrics_name = name_ + "\\1.tfd";
    CPtr<IDataStream> stream = storage->OpenStream( metrics_name.c_str(), STREAM_ACCESS_READ );
    if ( !stream ) return false;
    CPtr<IStructureSaver> structure = CreateStructureSaver( stream, IStructureSaver::READ );
    if ( !structure ) return false;
    CSaverAccessor saver = structure;
    saver.Add( 1, &format_ );
    if ( bPreLoad ) return true;

    ITextureManager *textures = GetSingleton<ITextureManager>( globals );
    if ( !textures ) return false;
    texture_ = textures->GetTexture( (name_ + "\\1").c_str() );
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

int STDCALL FontGpu::GetTextWidth( const char *text, int count ) const { return static_cast<int>( TextWidth( text, count ) ); }
int STDCALL FontGpu::GetTextWidth( const WORD *text, int count ) const { return static_cast<int>( TextWidth( reinterpret_cast<const wchar_t *>( text ), count ) ); }
float FontGpu::TextWidthFloat( const WORD *text, int count ) const { return TextWidth( reinterpret_cast<const wchar_t *>( text ), count ); }

bool FontGpu::AppendGeometry( const wchar_t *text, float x, float y, float scale, DWORD color,
    std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
    if ( !text || !*text || !texture_ ) return true;
    // Legacy D3D9 consumes DWORD colors as BGRA memory; SDL_GPU's normalized
    // byte vertex format consumes RGBA memory.
    color = (color & 0xff00ff00u) | ((color & 0x000000ffu) << 16) | ((color & 0x00ff0000u) >> 16);
    WORD previous = 0;
    for ( const wchar_t *cursor = text; *cursor && *cursor != L'\n'; ++cursor )
    {
        const WORD current = static_cast<WORD>( *cursor );
        const SFontFormat::SCharDesc &character = format_.GetChar( current );
        x += (character.fA + format_.GetKern( previous, current )) * scale;
        const WORD base = static_cast<WORD>( vertices.size() );
        vertices.emplace_back(); vertices.back().Setup( x, y + format_.metrics.nHeight * scale, 0, 1, color, 0xff000000, character.x1, character.y2 );
        vertices.emplace_back(); vertices.back().Setup( x, y, 0, 1, color, 0xff000000, character.x1, character.y1 );
        const float glyph_width = character.fWidth * scale + 0.5f;
        vertices.emplace_back(); vertices.back().Setup( x + glyph_width, y + format_.metrics.nHeight * scale, 0, 1, color, 0xff000000, character.x2, character.y2 );
        vertices.emplace_back(); vertices.back().Setup( x + glyph_width, y, 0, 1, color, 0xff000000, character.x2, character.y1 );
        indices.push_back( base + 2 ); indices.push_back( base + 1 ); indices.push_back( base + 0 );
        indices.push_back( base + 1 ); indices.push_back( base + 2 ); indices.push_back( base + 3 );
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
        {
            for ( auto &entry : textures_ ) if ( entry.second ) entry.second->Release();
            textures_.clear();
        }
    }
    IGFXTexture * STDCALL GetTexture( const char *name ) override
    {
        if ( !name || !*name ) return nullptr;
        const std::string key( name );
        const auto cached = textures_.find( key );
        if ( cached != textures_.end() ) return cached->second;
        ISingleton *globals = GetSingletonGlobal();
        if ( !globals ) return nullptr;
        GraphicsEngineGpu *owner = dynamic_cast<GraphicsEngineGpu *>( GetSingleton<IGFX>( globals ) );
        IDataStorage *storage = GetSingleton<IDataStorage>( globals );
        if ( !owner || !storage ) return nullptr;
        const char *suffixes[] = { quality_suffix_.c_str(), "_h.dds", "_c.dds", "_l.dds", ".dds" };
        std::string stream_name;
        for ( const char *suffix : suffixes )
        {
            std::string candidate( key );
            if ( candidate.size() < 4 || candidate.substr( candidate.size() - 4 ) != ".dds" ) candidate += suffix;
            if ( storage->IsStreamExist( candidate.c_str() ) ) { stream_name = candidate; break; }
        }
        if ( stream_name.empty() ) return nullptr;
        TextureGpu *texture = new TextureGpu( owner, 0, 0, 0, GFXPF_UNKNOWN, GFXD_STATIC );
        texture->AddRef();
        texture->SetSharedResourceName( stream_name );
        if ( !texture->Load() ) { texture->Release(); return nullptr; }
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
        }
    }

private:
    std::map<std::string, IGFXTexture *> textures_;
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
    IGFXFont * STDCALL GetFont( const char *name ) override
    {
        if ( !name || !*name ) return nullptr;
        const std::string key( name );
        const auto cached = fonts_.find( key );
        if ( cached != fonts_.end() ) return cached->second;
        FontGpu *font = new FontGpu();
        font->AddRef();
        font->SetSharedResourceName( key );
        if ( !font->Load() ) { font->Release(); return nullptr; }
        fonts_[key] = font;
        return font;
    }
private:
    std::map<std::string, FontGpu *> fonts_;
};

class TextGpu final : public IGFXText, public IGFXTextGpuFontProvider
{
public:
    OBJECT_COMPLETE_METHODS( TextGpu );
    void STDCALL SetFont( IGFXFont *font ) override { font_ = font; }
    IGFXFont *Font() const override { return font_; }
    float Scale() const override { return scale_; }
    void STDCALL SetText( IText *text ) override { text_ = text; }
    IText * STDCALL GetText() override { return text_; }
    void STDCALL SetWidth( int ) override {}
    void STDCALL SetColor( DWORD color ) override { color_ = color; }
    DWORD Color() const override { return color_; }
    void STDCALL EnableRedLine( bool ) override {}
    void STDCALL SetRedLine( int ) override {}
    void STDCALL SetChanged() override {}
    void STDCALL SetScale( float scale ) override { scale_ = scale < 0.1f ? 0.1f : scale; }
    float STDCALL GetScale() const override { return scale_; }
    int STDCALL GetNumLines() const override
    {
        if ( !text_ || !text_->GetString() ) return 0;
        int lines = 1;
        for ( const wchar_t *it = reinterpret_cast<const wchar_t *>( text_->GetString() ); *it; ++it ) if ( *it == L'\n' ) ++lines;
        return lines;
    }
    int STDCALL GetLineSpace() const override { return font_ ? static_cast<int>( font_->GetLineSpace() * scale_ ) : 12; }
    int STDCALL GetWidth( int count = -1 ) const override
    {
        if ( !text_ || !text_->GetString() ) return 0;
        const wchar_t *value = reinterpret_cast<const wchar_t *>( text_->GetString() );
        int length = 0;
        while ( value[length] && value[length] != L'\n' ) ++length;
        if ( count >= 0 && count < length ) length = count;
        FontGpu *gpu_font = dynamic_cast<FontGpu *>( font_ );
        return gpu_font ? static_cast<int>( gpu_font->TextWidthFloat( reinterpret_cast<const WORD *>( value ), length ) * scale_ ) : (font_ ? static_cast<int>( font_->GetTextWidth( reinterpret_cast<const WORD *>( value ), length ) * scale_ ) : static_cast<int>( length * 8 * scale_ ));
    }

private:
    IText *text_ = nullptr;
    IGFXFont *font_ = nullptr;
    DWORD color_ = 0xffffffff;
    float scale_ = 1.0f;
};

class CGfxGpuObjectFactory : public CBasicObjectFactory
{
public:
    CGfxGpuObjectFactory()
    {
        REGISTER_CLASS( this, GFX_GFX, GraphicsEngineGpu );
        REGISTER_CLASS( this, GFX_TEXTURE_MANAGER, TextureManagerGpu );
        REGISTER_CLASS( this, GFX_MESH_MANAGER, MeshManagerGpu );
        REGISTER_CLASS( this, GFX_FONT_MANAGER, FontManagerGpu );
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
