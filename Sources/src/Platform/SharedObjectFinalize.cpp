// Runs this shared object's C++ static destructors when it is unloaded.
//
// A C++ static's destructor is registered with __cxa_atexit against the
// shared object's own __dso_handle, and it is gcc's crtbeginS.o - through
// __do_global_dtors_aux in .fini_array - that calls __cxa_finalize for that
// handle at dlclose. Zig links no crtbegin into a shared object, so nothing
// made that call: the destructors stayed queued in libc and ran at exit(),
// in code that had been unmapped with the library. gfxgpu-factory-test found
// it on Linux (SIGSEGV inside exit(), resolved to CGfxGpuObjectFactory's
// destructor in the already unloaded libGFXGPU.so); UnloadAllModules at game
// exit walks the same path. dyld does this itself on macOS, and Windows runs
// a DLL's atexit table in its DllMain detach, which is why neither showed it.
//
// Compiled into every C++ shared object built here (build.zig,
// addSharedObjectFinalizer). Calling __cxa_finalize twice for one handle is
// harmless: it runs what is registered and removes it. No headers: this is
// added to modules with very different include setups, and the three libc
// entry points it needs are declared here.
#if defined(__linux__)
extern "C" void *__dso_handle;
extern "C" int __cxa_finalize( void * );
extern "C" char *getenv( const char * );
extern "C" long write( int, const void *, unsigned long );

namespace
{
    // BK_SO_FINALIZE_TRACE=1 reports each finalization with the handle it used:
    // what to compare against the handle the static registrations passed
    // (objdump around the __cxa_atexit calls) when unload misbehaves.
    void TraceHandle( const void *handle )
    {
        char line[64] = "shared object finalize: handle 0x";
        unsigned long length = 0;
        while ( line[length] != 0 ) ++length;
        const unsigned long value = reinterpret_cast<unsigned long>( handle );
        for ( int shift = 60; shift >= 0; shift -= 4 )
            line[length++] = "0123456789abcdef"[( value >> shift ) & 0xf];
        line[length++] = '\n';
        write( 2, line, length );
    }
}

__attribute__(( used, destructor )) static void FinalizeSharedObject()
{
    if ( getenv( "BK_SO_FINALIZE_TRACE" ) != nullptr )
        TraceHandle( &__dso_handle );
    __cxa_finalize( &__dso_handle );
}
#endif
