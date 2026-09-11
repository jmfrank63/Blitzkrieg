// Runs this shared object's C++ static destructors when it is unloaded.
//
// A C++ static's destructor is registered with __cxa_atexit against the
// shared object's own __dso_handle, and it is gcc's crtbeginS.o - through
// __do_global_dtors_aux in .fini_array - that calls __cxa_finalize for that
// handle at dlclose. Zig links no crtbegin into a shared object, so nothing
// made that call: the destructors stayed queued in libc and ran at exit(),
// in code that had been unmapped with the library. gfxgpu-factory-test found
// it on Linux (SIGSEGV inside exit() at an address in no loaded image after
// dlclose of libGFXGPU.so); UnloadAllModules at game exit walks the same
// path. dyld does this itself on macOS, and Windows runs a DLL's atexit
// table in its DllMain detach, which is why neither showed it.
//
// Compiled into every shared object built here (build.zig,
// addSharedObjectFinalizer). Calling __cxa_finalize twice for one handle
// is harmless: it runs what is registered and removes it.
#if defined(__linux__)
extern "C" void *__dso_handle;
extern "C" int __cxa_finalize( void * );

__attribute__(( destructor )) static void FinalizeSharedObject()
{
    __cxa_finalize( &__dso_handle );
}
#endif
