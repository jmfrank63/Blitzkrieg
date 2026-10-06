#ifndef __EDITOR_BRIDGE_MAP_TOOLS_H__
#define __EDITOR_BRIDGE_MAP_TOOLS_H__

// The map-file work both bridges need: bridge.cpp owns the engine's minimap
// and map readers, resource_bridge.cpp's Mission export and BkResMissionMinimap
// call them (MinimapCreation.cpp's Create1Minimap and the .xml to .bzm half of
// CMissionFrame::ExportFrameData). Paths are host paths; every refusal names
// the path and why in szWhy.

#include <string>

namespace NMapTools
{

// MinimapCreation.cpp's Create1Minimap over the map files <szMapBase>.xml and
// <szMapBase>.bzm (either may be missing, not both): the newer one is read,
// and when <szPictureBase>_h.dds is newer than it nothing is done
// (bSkipped). Otherwise <szPictureBase>_c.dds, _l.dds and _h.dds are written
// at 512x512 through the engine's minimap code and read back by their headers.
bool CreateMissionMinimap( const std::string &szMapBase, const std::string &szPictureBase, bool &bSkipped, std::string &szWhy );

// CMissionFrame::ExportFrameData's bzm step: <szXmlPath> read, and written to
// <szBzmPath> as chunk 1 (the map) plus the SQuickLoadMapInfo chunk. The
// target's folder is created.
bool ConvertMapToBzm( const std::string &szXmlPath, const std::string &szBzmPath, std::string &szWhy );

}

#endif
