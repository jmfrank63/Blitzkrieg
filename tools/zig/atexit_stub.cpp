// atexit for the Windows test executables that are entered at main (build.zig, addAtexitStub): no C runtime
// start-up or exit sequence runs in them, so nothing would ever call a registered handler. Without the stub
// the engine libraries' global destructors take atexit out of the static C runtime, whose members clash
// with the debug DLL runtime the engine objects use (duplicate _cexit, _wctype, ...).
extern "C" int atexit( void ( *)( void ) )
{
    return 0;
}
