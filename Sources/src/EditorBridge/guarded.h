#ifndef __EDITOR_BRIDGE_GUARDED_H__
#define __EDITOR_BRIDGE_GUARDED_H__

// Shared firewall between the bridge entry points and the engine: catches any
// C++ exception the engine throws so none crosses the C ABI, where it would
// unwind out of this module and into a Zig caller that has no landing pad for
// one. Lives here, in a header, so bridge.cpp (the map bridge) and
// resource_bridge.cpp (the resource bridge, S04) use the same wrapper rather
// than each defining its own copy and drifting - the research spells this out
// as the "do not duplicate" rule.
//
// The template takes an SEditorSession* (the shared base, from session.h)
// rather than the module-private BkEditorSession pointer: a resource bridge
// caller passes a BkResSession (which aliases BkEditorSession), the compiler
// upcasts it to the base pointer for the template body, and szMessage - the
// one field this wrapper touches - sits on the base. Both bridges inherit
// their own session shapes from SEditorSession, so a derived-pointer passed
// directly matches without a cast at the call site.

#include "session.h"

template<class F>
BkEditorStatus Guarded( SEditorSession *pSession, F body )
{
	if ( pSession == 0 )
		return BK_EDITOR_NO_SESSION;
	try
	{
		pSession->szMessage.clear();
		return body();
	}
	catch ( ... )
	{
		pSession->szMessage = "the engine threw";
		return BK_EDITOR_FAILED;
	}
}

#endif
