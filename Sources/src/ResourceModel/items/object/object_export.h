#pragma once
// What the Object editor keeps beside its tree and the exporter and the
// bridge's import share: CObjectFrame's own_data (sprite_pos, krest_pos,
// TransLines; SaveFrameOwnData) and the grid half of desc (SaveRPGStats:
// passability, origin, visibility, VisOrigin) of the project element. No item
// owns these elements, so they are read from and written to the document, in
// the shape CDataTreeXML gives them. Engine-free: the grids are the
// STileGrid of grid_projection.h, the positions plain floats.

#include <string>
#include <vector>

#include "../../grid_projection.h"
#include "../../project.h"

namespace NResourceModel
{

struct SObjectFrameData
{
	// MFC's CObjectFrame constructor: both crosses start at 16 world cells.
	SVec3 vSpritePos;
	SVec3 vZeroPos;
	std::vector<STransLine> transLines;
	STileGrid passability;      // sizeX, sizeY and data; the min tile is not stored
	SVec2 vOrigin;
	STileGrid visibility;
	SVec2 vVisOrigin;

	SObjectFrameData();
};

// The project's own_data and desc grids. An absent chunk or field keeps the
// frame's default, as CTreeAccessor::Add leaves a member the file lacks. False
// with szError when a grid's rows do not match its size.
bool ReadObjectFrameData( const Project &project, SObjectFrameData &data, std::string &szError );

// Writes data into the project element's own_data and desc, creating the
// chunks when the project has none and leaving every other field as it was.
void WriteObjectFrameData( Project &project, const SObjectFrameData &data );

}
