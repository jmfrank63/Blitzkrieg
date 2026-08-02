#include "StdAfx.h"

#include "..\Misc\FileUtils.h"
#include "..\Platform\Debug.h"
#include "..\Platform\DynamicLibrary.h"
#include "..\Platform\Paths.h"

namespace NMain
{
typedef ISaveLoadSystem* (STDCALL *GETSLS_HOOK)();
typedef ISingleton* (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int, int );

static const char *ModuleSuffix()
{
#if defined(_WIN32) || defined(_WIN64)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}
static std::string ModuleFilePattern( const std::string &root )
{
    std::string path = root;
    if ( !path.empty() && path.back() != '/' && path.back() != '\\' ) path += '/';
#if defined(_WIN32) || defined(_WIN64)
    return path + "*.dll";
#elif defined(__APPLE__)
    return path + "lib*.dylib";
#else
    return path + "lib*.so";
#endif
}
static std::string StreamIOPath()
{
    std::string path = NPlatform::Paths::ModuleRoot();
    if ( !path.empty() && path.back() != '/' && path.back() != '\\' ) path += '/';
#if defined(_WIN32) || defined(_WIN64)
    return path + "StreamIO.dll";
#elif defined(__APPLE__)
    return path + "libStreamIO.dylib";
#else
    return path + "libStreamIO.so";
#endif
}

static void EnsureGlobalHooks()
{
    if ( GetSLS() != 0 && GetSingletonGlobal() != 0 && g_pfnGlobalGetTempRawBuffer != 0 ) return;
    static NPlatform::DynamicLibrary streamio;
    if ( !streamio.IsLoaded() && !streamio.Load( StreamIOPath().c_str() ) ) {
        NPlatform::DebugWriteFormat( "StreamIO hook load failed: %s\n", streamio.GetError() );
        return;
    }
    if ( GetSLS() == 0 ) if ( GETSLS_HOOK hook = reinterpret_cast<GETSLS_HOOK>( streamio.GetFunction( "GetSLS_Hook" ) ) ) g_pGlobalSaveLoadSystem = hook();
    if ( GetSingletonGlobal() == 0 ) if ( GETSINGLETONGLOBAL_HOOK hook = reinterpret_cast<GETSINGLETONGLOBAL_HOOK>( streamio.GetFunction( "GetSingletonGlobal_Hook" ) ) ) g_pGlobalSingleton = hook();
    if ( g_pfnGlobalGetTempRawBuffer == 0 ) g_pfnGlobalGetTempRawBuffer = reinterpret_cast<GETTEMPRAWBUFFER_HOOK>( streamio.GetFunction( "GetTempRawBuffer_Hook" ) );
}

struct SDllModule {
    NPlatform::DynamicLibrary *library = nullptr;
    const SModuleDescriptor *descriptor = nullptr;
    SDllModule() = default;
    SDllModule( const SDllModule & ) = delete;
    SDllModule &operator=( const SDllModule & ) = delete;
    SDllModule( SDllModule &&other ) noexcept : library( other.library ), descriptor( other.descriptor ) { other.library = nullptr; other.descriptor = nullptr; }
    SDllModule &operator=( SDllModule &&other ) noexcept { if ( this != &other ) { delete library; library = other.library; descriptor = other.descriptor; other.library = nullptr; other.descriptor = nullptr; } return *this; }
    ~SDllModule() { delete library; }
};
static std::list<SDllModule> modules;
static bool HasModuleType( int type ) { for ( const SDllModule &module : modules ) if ( module.descriptor && module.descriptor->nType == type ) return true; return false; }

const SModuleDescriptor* STDCALL GetModuleDesc( int type )
{
    for ( const SDllModule &module : modules ) if ( module.descriptor && module.descriptor->nType == type ) return module.descriptor;
    NPlatform::DebugWriteFormat( "can't find module of type 0x%.8x\n", type );
    return nullptr;
}
int STDCALL LoadAllModules( const char *root )
{
    const std::string directory = root && *root ? root : NPlatform::Paths::ModuleRoot();
    for ( NFile::CFileIterator iterator( ModuleFilePattern( directory ).c_str() ); !iterator.IsEnd(); ++iterator ) {
        NPlatform::DynamicLibrary *library = new NPlatform::DynamicLibrary( iterator.GetFilePath().c_str() );
        if ( !library->IsLoaded() ) { NPlatform::DebugWriteFormat( "Failed to load module %s: %s\n", iterator.GetFilePath().c_str(), library->GetError() ); delete library; continue; }
        typedef const SModuleDescriptor* (STDCALL *GetDescriptor)();
        GetDescriptor get_descriptor = reinterpret_cast<GetDescriptor>( library->GetFunction( "GetModuleDescriptor" ) );
        const SModuleDescriptor *descriptor = get_descriptor ? get_descriptor() : nullptr;
        if ( !descriptor || !descriptor->pFactory ) { NPlatform::DebugWriteFormat( "Module %s has no usable descriptor\n", iterator.GetFilePath().c_str() ); delete library; continue; }
        if ( HasModuleType( descriptor->nType ) ) { NPlatform::DebugWriteFormat( "Duplicate module type rejected: %s\n", descriptor->pszName ); delete library; continue; }
        SDllModule module; module.library = library; module.descriptor = descriptor; modules.push_back( std::move( module ) );
        NPlatform::DebugWriteFormat( "Loaded module %s version 0x%x\n", descriptor->pszName, descriptor->nVersion );
    }
    return static_cast<int>( modules.size() );
}
void STDCALL UnloadAllModules()
{
    if ( modules.empty() ) return;
    if ( GetSingletonGlobal() != 0 ) GetSingletonGlobal()->Done();
    while ( !modules.empty() ) modules.pop_back();
}
static int module_index = 0;
const SModuleDescriptor* GetFirstModuleDesc() { module_index = 0; return module_index < static_cast<int>( modules.size() ) ? modules.front().descriptor : nullptr; }
const SModuleDescriptor* GetNextModuleDesc() { ++module_index; if ( module_index < 0 || module_index >= static_cast<int>( modules.size() ) ) return nullptr; std::list<SDllModule>::const_iterator it = modules.begin(); std::advance( it, module_index ); return it->descriptor; }
const std::string GetModuleFileNameByDesc( const SModuleDescriptor *descriptor ) { for ( const SDllModule &module : modules ) if ( module.descriptor == descriptor && module.library ) return module.library->GetPath(); return {}; }
bool SetGameDirectory() { return true; }

class CModuleLoadAutoMagic {
public:
    CModuleLoadAutoMagic()
    {
        NMain::LoadAllModules( NPlatform::Paths::ModuleRoot().c_str() );
        EnsureGlobalHooks();
        if ( ISaveLoadSystem *save_load = GetSLS() ) for ( const SDllModule &module : modules ) if ( module.descriptor ) { if ( module.descriptor->pFactory ) save_load->AddFactory( module.descriptor->pFactory ); if ( module.descriptor->pChecker ) module.descriptor->pChecker->SetModuleFunctionalityLimits(); }
    }
    ~CModuleLoadAutoMagic() { NMain::UnloadAllModules(); }
};
static CModuleLoadAutoMagic moduleLoadAutoMagic;
}
