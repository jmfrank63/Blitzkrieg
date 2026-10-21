#pragma once

// The Game's one bk_text context (M004): opt-in with BK_TEXT_ENGINE=ttf, else the bitmap fonts
// stay as they are and Context() is null. The context is created with the graphics engine and
// destroyed after every font is closed, never when a font manager is cleared.

#include "bk_text.h"

#include <string>

namespace NBkTextBridge
{
	// True when BK_TEXT_ENGINE=ttf was asked for; read once.
	bool Requested();
	// Creates the context if it was requested and none exists. A failure is logged once with its
	// reason and leaves Context() null, so the caller keeps the bitmap path.
	bool Create();
	// Closes the context. Every BkTextFont must be closed before.
	void Destroy();
	BkTextContext *Context();
	// The code the context was created for ("en", "de", "hi", ...).
	const std::string &Language();
	// "Default" -> en, "lang-de" -> de, "lang-zh-hans" -> zh, "pt-br" -> pt; BK_TEXT_LANG wins.
	std::string LanguageCode( const char *pszGameLanguage );
}
