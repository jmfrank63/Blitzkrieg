#pragma once

// The Game's one bk_text context (M004): opt-in with BK_TEXT_ENGINE=ttf, else the bitmap fonts
// stay as they are and Context() is null. The context is created with the graphics engine and
// destroyed after every font is closed, never when a font manager is cleared.

#include "bk_text.h"

#include <string>

class GraphicsEngineGpu;
class IGFXTexture;

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
	std::string Language();
	// Counts contexts created; a font closes its BkTextFont only while its serial is current, so a
	// font outliving Destroy() never touches a dead context.
	unsigned Serial();
	// True when the game's language now maps to another code than the context was made for.
	bool LanguageChanged();
	// Destroys and creates the context again (for the language that is current now). Every
	// BkTextFont must be closed before.
	bool Recreate();
	// Called when a frame begins: tells the atlas which glyphs the new frame may evict.
	// The atlas frame bk_text last began: its glyph rects stay valid for the rest of the frame
	// unless the atlas generation changes.
	uint64_t Frame();
	void BeginFrame();
	// The R8 page texture, created on first use at the context's page size (and again when that
	// changes). Null if the context or the texture is missing.
	IGFXTexture *Atlas( GraphicsEngineGpu *pEngine );
	// Sends what the atlas changed since the last call to the page texture as one bounding-box
	// region (the whole page after an eviction). False when an upload failed.
	bool FlushAtlas( GraphicsEngineGpu *pEngine );
	// The atlas generation now, to detect an eviction in the middle of a batch.
	unsigned AtlasGeneration();
	// Logged under BK_DEBUG_LOG, once per generation: the page was too small for the frame's glyphs.
	void ReportAtlasFull();
	// "Default" -> en, "lang-de" -> de, "lang-zh-hans" -> zh, "pt-br" -> pt; BK_TEXT_LANG wins.
	std::string LanguageCode( const char *pszGameLanguage );
}
