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

// CGridFrame's GetOrigin2DPosition (SpriteCompose.cpp): where the grid origin
// vOrigin lies on screen relative to the sprite, from the camera's linear part.
CVec2 Origin2DPosition( const SGroundCamera &camera, const CVec2 &vOrigin );

// ComposeSingleObjectPack (BuildCompose.cpp:147-234): an object, fence or
// building picture packed as a sprite set with its locked tiles drawn in,
// written as <szName>.san with <szName>_c/_l/_h.dds, and its shadow (the
// shadow picture with its alpha multiplied by the inverse of the sprite's
// sharpened alpha, colour cleared) as <szName>s.san with <szName>s_c/_l/_h.dds.
// zeroPos is the zero cross in the picture, pass the passability grid and
// vLockedTilesCenter its origin on screen (Origin2DPosition). lowFormat is
// the frame's m_nLowFormat: ARGB1555 for an object or building, ARGB4444 for
// a fence. Both pictures are read and compared before anything is written, so
// a failed compose leaves nothing behind (MFC wrote the sprite half first).
bool ComposeSingleObjectPack( const SExportContext &context, const NImageExport::SGamma &gamma, EGFXPixelFormat lowFormat,
                              const std::string &szSprite, const std::string &szShadow, const std::string &szName,
                              const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vLockedTilesCenter, SExportOutcome &outcome );

// The noise ("g") picture of a damaged or destroyed building
// (CBuildingTreeRootItem::ComposeAnimations): the noise picture with its alpha
// multiplied by the inverse of the sprite's sharpened alpha, packed without
// locked tiles as <szName>.san with <szName>_c/_l/_h.dds. The colour is kept,
// unlike a shadow's. The two pictures are read and compared before anything is
// written; a size mismatch is MFC's message box and fails here with szError.
bool ComposeNoisePack( const SExportContext &context, const NImageExport::SGamma &gamma, EGFXPixelFormat lowFormat,
                       const std::string &szSprite, const std::string &szNoise, const std::string &szName,
                       const CVec2 &zeroPos, SExportOutcome &outcome );

// CFenceTreeRootItem::SaveShadowFile (FenceTreeItem.cpp): the shadow picture
// with its alpha multiplied by the inverse of the sprite's alpha sharpened at
// 100, colour cleared, saved as the TGA szTempShadow. A fence keeps its
// shadows as pictures because BuildAnimations takes file names. False with
// outcome.szError when either picture cannot be read, their sizes differ (MFC
// asserted) or the result cannot be written.
bool SaveShadowFile( const std::string &szSprite, const std::string &szShadow, const std::string &szTempShadow, SExportOutcome &outcome );

// CGridFrame::SaveIconFile: the picture cropped to the bounding box of its
// non-transparent pixels, scaled to fit 64 x 64 and centred on grey with
// zero alpha, saved as the TGA szName. A picture with no alpha at all fails.
bool SaveIconFile( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome );

}

}
