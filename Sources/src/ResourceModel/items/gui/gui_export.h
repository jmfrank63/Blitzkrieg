#pragma once
// The GUI exporter: the open screen written as a <base> document where the game finds it
// for a -mod. The registration is the "gui" row of exporter.cpp.
//
// Where the game reads it: CUIScreen opens its layout through OpenLayoutStream
// (UIScreen.cpp), which first tries ui\ModStyles\<MOD.Folder>\<name>.xml and then plain
// ui\<name>.xml through IDataStorage. The mod's data folder is a storage layer over Data,
// so data/ui/<Screen>.xml of the mod replaces Data/UI/<Screen>.xml for that screen. This
// exporter writes that plain overlay (<staging>/ui/<Screen>.xml): it applies whichever mod
// is loaded, where the ModStyles route would need the folder name baked into the path.
// The screen name is the opened file's name without its extension.

#include "../../exporter.h"

namespace NResourceModel
{

bool ExportGui( const Project &project, const SExportContext &context, SExportOutcome &outcome );

}
