#include "StdAfx.h"

#include "..\\Misc\\Win32Helper.h"
#include "..\\StreamIO\\StreamIOTypes.h"

typedef ISaveLoadSystem * (STDCALL *GETSLS_HOOK)();
typedef ISingleton * (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void * (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );

ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISingleton *g_pGlobalSingleton = 0;
void * (STDCALL *g_pfnGlobalGetTempRawBuffer)( int nAmount, int nBufferIndex ) = 0;

namespace
{
    class CGlobalsLoader
    {
    public:
        CGlobalsLoader()
        {
            NWin32Helper::CDLLHandle *streamio = new NWin32Helper::CDLLHandle( "streamio.dll" );
            if ( !streamio->IsLoaded() ) { delete streamio; return; }
            if ( GETSLS_HOOK hook = streamio->GetProcAddress( "GetSLS_Hook", (GETSLS_HOOK)0 ) ) g_pGlobalSaveLoadSystem = hook();
            if ( GETSINGLETONGLOBAL_HOOK hook = streamio->GetProcAddress( "GetSingletonGlobal_Hook", (GETSINGLETONGLOBAL_HOOK)0 ) ) g_pGlobalSingleton = hook();
            g_pfnGlobalGetTempRawBuffer = streamio->GetProcAddress( "GetTempRawBuffer_Hook", (GETTEMPRAWBUFFER_HOOK)0 );
            streamio_ = streamio;
        }

        ~CGlobalsLoader() { delete streamio_; }

    private:
        NWin32Helper::CDLLHandle *streamio_ = nullptr;
    };

    CGlobalsLoader theGlobalsLoader;
}
