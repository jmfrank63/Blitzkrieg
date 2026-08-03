#include "StdAfx.h"

#include "..//Platform//DynamicLibrary.h"
#include "..//StreamIO//StreamIOTypes.h"

typedef ISaveLoadSystem * (STDCALL *GETSLS_HOOK)();
typedef ISingleton * (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void * (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );

ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISingleton *g_pGlobalSingleton = 0;
void * (STDCALL *g_pfnGlobalGetTempRawBuffer)( int nAmount, int nBufferIndex ) = 0;

namespace
{
    static const char *StreamIOName()
    {
#if defined(_WIN32) || defined(_WIN64)
        return "StreamIO.dll";
#elif defined(__APPLE__)
        return "libStreamIO.dylib";
#else
        return "libStreamIO.so";
#endif
    }
    class CGlobalsLoader
    {
    public:
        CGlobalsLoader()
        {
            NPlatform::DynamicLibrary *streamio = new NPlatform::DynamicLibrary( StreamIOName() );
            if ( !streamio->IsLoaded() ) { delete streamio; return; }
            if ( GETSLS_HOOK hook = reinterpret_cast<GETSLS_HOOK>( streamio->GetFunction( "GetSLS_Hook" ) ) ) g_pGlobalSaveLoadSystem = hook();
            if ( GETSINGLETONGLOBAL_HOOK hook = reinterpret_cast<GETSINGLETONGLOBAL_HOOK>( streamio->GetFunction( "GetSingletonGlobal_Hook" ) ) ) g_pGlobalSingleton = hook();
            g_pfnGlobalGetTempRawBuffer = reinterpret_cast<GETTEMPRAWBUFFER_HOOK>( streamio->GetFunction( "GetTempRawBuffer_Hook" ) );
            streamio_ = streamio;
        }

        ~CGlobalsLoader() { delete streamio_; }

    private:
        NPlatform::DynamicLibrary *streamio_ = nullptr;
    };

    CGlobalsLoader theGlobalsLoader;
}
