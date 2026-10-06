/* Defines the symbol the MSVC compiler references for any code that uses
 * floating point. The MSVC CRT supplies it, but the Zig test tiers link Zig's
 * own libc, which does not, and linking the whole CRT as well duplicates its
 * TLS symbols. A program that does link the CRT never pulls its copy. */
int _fltused = 1;
