/* The exit-time leak report on ELF and Mach-O: a destructor of the BkMemory
 * shared library, run at process exit (or dlclose) after the importing
 * modules' own destructors, since BkMemory is loaded first and unloaded last.
 * It is C because Zig's debug backend cannot place a function in
 * .fini_array (ExportOptions.section is unimplemented there). Not built on
 * Windows, where DllMain does this. */
extern void bk_mem_fini_entry(void);

__attribute__((destructor)) static void bk_mem_fini(void) {
    bk_mem_fini_entry();
}
