// M004 S05: a mod that ships fonts/<size>/1.tfd keeps its bitmap font for that size while bk_text is
// the Game's text engine; the sizes it does not ship stay on bk_text.
//
// The harness is the GFXGPU module's own FontGpu over a fake IDataStorage that holds the base game's
// four 1.tfd and, in a second layer, whichever sizes the case gives a mod. The fake answers
// IsStreamInModLayer as the Game's storage does (a layer above the base provides the name), so what
// is checked is the decision FontGpu::Load makes per size: the bitmap path asks the storage for the
// size's 1.tfd, the bk_text path does not. The fake hands out no stream, so a bitmap Load ends false
// right after that ask and the font is not a ttf font.
//
// Each mismatch prints one line: case, size, expected and measured engine.
#include "StdAfx.h"
#include "GFX.H"
#include "../../Sources/src/GFXGPU/FontGpu.h"
#include "../../Sources/src/GFXGPU/BkTextBridge.h"
#include "../../Sources/src/StreamIO/Globals.h"
#include "../../Sources/src/BkMemory/bk_memory_sdl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

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
std::string Lower( const char *text )
{
    std::string out = text;
    for ( char &c : out ) c = char( std::tolower( (unsigned char)c ) );
    return out;
}

// Base layer and mod layer of the Game's storage, by lower-case name; opens are recorded.
struct FakeStorage : public IDataStorage
{
    std::set<std::string> base, mod, opened;
    int refs = 1;
    void STDCALL AddRef( int n = 1, int = 0x7fffffff ) override { refs += n; }
    void STDCALL Release( int n = 1, int = 0x7fffffff ) override { refs -= n; }
    bool STDCALL IsValid() const override { return true; }
    const bool STDCALL IsStreamExist( const char *name ) override { const std::string key = Lower( name ); return mod.count( key ) || base.count( key ); }
    bool STDCALL IsStreamInModLayer( const char *name ) override { return mod.count( Lower( name ) ) != 0; }
    IDataStream * STDCALL CreateStream( const char *, DWORD ) override { return nullptr; }
    IDataStream * STDCALL OpenStream( const char *name, DWORD ) override { opened.insert( Lower( name ) ); return nullptr; }
    bool STDCALL GetStreamStats( const char *, SStorageElementStats * ) override { return false; }
    bool STDCALL DestroyElement( const char * ) override { return false; }
    bool STDCALL RenameElement( const char *, const char * ) override { return false; }
    IStorageEnumerator * STDCALL CreateEnumerator() override { return nullptr; }
    const char * STDCALL GetName() const override { return "fake"; }
    bool STDCALL AddStorage( IDataStorage *, const char * ) override { return false; }
    bool STDCALL RemoveStorage( const char * ) override { return false; }
};
FakeStorage g_storage;

struct FakeSingleton : public ISingleton
{
    bool Register( int, IRefCount * ) override { return false; }
    bool UnRegister( int ) override { return false; }
    bool UnRegister( IRefCount * ) override { return false; }
    IRefCount *Get( int id ) override { return id == IDataStorage::tidTypeID ? &g_storage : nullptr; }
    int GetAllObjects( IRefCount ***, int * ) override { return 0; }
    void Done() override {}
};
FakeSingleton g_singleton;
}
ISingleton *g_pGlobalSingleton = &g_singleton;
ISaveLoadSystem *g_pGlobalSaveLoadSystem = nullptr;
void *( STDCALL *g_pfnGlobalGetTempRawBuffer )( int nAmount, int nBufferIndex ) = nullptr;

namespace {
int g_failures = 0;
int g_checks = 0;

// One size under one mod layer: ttf unless the mod ships it, and the 1.tfd is asked for only then.
void CheckSize( const char *label, const char *size, bool mod_ships )
{
    g_storage.opened.clear();
    FontGpu *font = new FontGpu();
    font->AddRef();
    font->SetSharedResourceName( std::string( "fonts\\" ) + size );
    font->Load( true );
    const bool ttf = font->IsTtf();
    const bool asked = g_storage.opened.count( std::string( "fonts\\" ) + size + "\\1.tfd" ) != 0;
    ++g_checks;
    if ( ttf == mod_ships )
    {
        ++g_failures;
        std::fprintf( stderr, "%s %s: expected %s, measured %s\n", label, size, mod_ships ? "bitmap" : "ttf", ttf ? "ttf" : "bitmap" );
    }
    ++g_checks;
    if ( asked != mod_ships )
    {
        ++g_failures;
        std::fprintf( stderr, "%s %s: 1.tfd %s opened\n", label, size, asked ? "was" : "was not" );
    }
    font->Release();
}

void RunCase( const char *label, const char *mod_size )
{
    g_storage.mod.clear();
    if ( mod_size != nullptr ) g_storage.mod.insert( std::string( "fonts\\" ) + mod_size + "\\1.tfd" );
    const char *const sizes[] = { "tiny", "small", "medium", "large" };
    for ( const char *size : sizes ) CheckSize( label, size, mod_size != nullptr && std::strcmp( mod_size, size ) == 0 );
}
}

int main()
{
    SetEnvironment( "BK_TEXT_ENGINE", "ttf" );
    if ( !NBkTextBridge::Create() || NBkTextBridge::Context() == nullptr )
    {
        std::fprintf( stderr, "the bk_text context was not created (BK_TEXT_FONTS_DIR=%s)\n", std::getenv( "BK_TEXT_FONTS_DIR" ) );
        return 3;
    }
    for ( const char *size : { "tiny", "small", "medium", "large" } )
        g_storage.base.insert( std::string( "fonts\\" ) + size + "\\1.tfd" );
    RunCase( "base only", nullptr );
    RunCase( "mod medium", "medium" );
    RunCase( "mod tiny", "tiny" );
    NBkTextBridge::Destroy();
    std::printf( "mod-font-fallback: %d checks, %d failures\n", g_checks, g_failures );
    return g_failures == 0 ? 0 : 1;
}
