#pragma once
// GetActionFromName of Sources/src/editor/SpriteCompose.cpp: the animation
// slot of SSpriteAnimationFormat::animations a sprite or infantry animation
// name goes to. On its own, without compose.h's image export, so bk-sprite
// (tools/zig/bk_sprite.cpp) maps names with the same table as the exporter.

#include <string>

namespace NResourceModel
{

namespace NCompose
{

// The animation slot MFC's GetActionFromName gives a name (case ignored), one
// of the ANIMATION_* values. An unknown name is 0, as in MFC, and sets
// *pbKnown to false when it is given.
int GetActionFromName( const std::string &szAnimName, bool *pbKnown = 0 );

}

}
