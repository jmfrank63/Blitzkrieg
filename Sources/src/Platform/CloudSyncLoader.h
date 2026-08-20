#pragma once

// The cloud sync facade's one platform-specific corner: loading the
// CloudSync module and asking the OS two small questions. Path-scoped under
// Platform/ per the runtime platform audit; the facade itself stays free of
// native includes.

namespace NPlatform
{
	// The CloudSync shared library, loaded once per process, or null when it
	// is not installed. The handle is never unloaded: the worker thread
	// inside it must outlive every caller.
	void *CloudSyncLoadLibrary();
	void *CloudSyncLoadSymbol( void *pModule, const char *pszName );

	// This machine's name, for the per-host backup key. Falls back to
	// "host" rather than failing.
	void CloudSyncHostName( char *pszOut, unsigned int nCap );
}
