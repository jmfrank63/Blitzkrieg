#pragma once
// BuildAnimations and GetActionFromName of Sources/src/editor/SpriteCompose.cpp
// without MFC: the one place sprite and infantry exports turn their frame
// lists into a packed picture and a .san animation format. The computation
// order and float expressions are MFC's line for line, because the .san bytes
// must equal what MFC wrote. What MFC reported through assertions and message
// boxes goes into SExportOutcome instead.
//
// Like image_export this needs the engine (the image processor), so it is
// built into the EditorBridge archive.

#include <string>
#include <unordered_map>
#include <vector>

#include "image_export.h"

namespace NResourceModel
{

namespace NCompose
{

// SAnimationDesc of SpriteCompose.h: one animation of a sprite or infantry
// project, its directions as lists of frame (file) indices.
struct SAnimationDesc
{
	struct SDirDesc
	{
		std::vector<short> frames;      // frame indices for this direction sequence
		CVec2 ptFrameShift = VNULL2;    // shift for all frames, which participates in this dir sequence
	};
	std::string szName;                 // animation name
	std::vector<SDirDesc> dirs;         // direction descriptions
	std::unordered_map<int, CVec2> frames;  // each frame unique shift
	int nFrameTime = 0;                 // general one frame show time
	CVec2 ptFrameShift = VNULL2;        // general one frame shift
	float fSpeed = 0.0f;                // translation speed (for animations with movement)
	bool bCycled = false;               // cycled or one-shot animation
	std::vector<int> usedFrames;

	void AddUsedFrame( int frame );
	int GetUsedFrameIndex( int frame ) const;
};

// The animation slot MFC's GetActionFromName gives a name (case ignored), one
// of the ANIMATION_* values. An unknown name is 0, as in MFC, and sets
// *pbKnown to false when it is given.
int GetActionFromName( const std::string &szAnimName, bool *pbKnown = 0 );

// Reads every file of szFileNames once (a name that repeats shares its frame),
// packs the pictures with the engine's ComposeImages (bProcessImages) and
// fills *pDst with one SSpriteAnimation per slot used by pSrc, whose frames
// index the used frames of each animation in ascending order. Each animation
// name is lower-cased in place, and its usedFrames are rewritten, as in MFC.
// dwMinAlpha > 0 also measures the depth of each frame's left and right edge.
//
// Null with outcome.szError naming the file and the reason when a picture
// cannot be read or the images cannot be packed. Warnings: an unknown
// animation name, and a frame with an empty rect ("may be a 4 dir animation").
CPtr<IImage> BuildAnimations( std::vector<SAnimationDesc> *pSrc, SSpriteAnimationFormat *pDst, const std::vector<std::string> &szFileNames,
                              bool bProcessImages, DWORD dwMinAlpha, SExportOutcome &outcome );

}

}
