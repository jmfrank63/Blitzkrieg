#pragma once
// The graphics half of the S06 exporters (mine, trench, squad): what MFC's
// CParentFrame::ConvertAndSaveImage, SaveCompressedTexture/-Shadow and
// ComposeSingleObject (Sources/src/editor/ParentFrame.cpp, SpriteCompose.cpp,
// BuildCompose.cpp) did with the source art.
//
// The pixels go through the engine's own IImageProcessor, the code MFC called:
// the TGA/PNG/BMP readers, ComposeImages, the gamma correction and Compress
// (NDxt for the DXT formats, the raw packers for ARGB). So _h.dds, the
// uncompressed copy, is byte-identical to what MFC wrote, and the DXT files
// differ from MFC's S3TC encoder only within the measured dxt-tolerance.json
// gate. Like the stats half this needs the engine, so it is built into the
// EditorBridge archive, not the engine-free model tests.
//
// Every output name is relative to the staging root, with backslashes, as
// StatsFileName gives it, and nothing is written anywhere else.

#include <string>

#include "../Anim/Animation.h"
#include "../Formats/fmtAnimation.h"
#include "../Image/Image.h"
#include "exporter.h"

namespace NResourceModel
{

namespace NImageExport
{

// The Brightness/Contrast/Gamma of the project's gamma.cfg, found as
// CParentFrame::ReadConfigFile found it: in the project folder, else in each
// parent folder up to the root. Without a config file all three are 0, which
// leaves the picture alone (MFC's batch export stopped instead).
struct SGamma
{
	float fBrightness = 0.0f;
	float fContrast = 0.0f;
	float fGamma = 0.0f;
};
SGamma ReadGammaConfig( const std::string &szProjectDirectory );

// A picture the engine can read, from a path with slashes or backslashes, or
// null with outcome.szError naming the path and why: missing, or present and
// not a readable TGA/PNG/BMP.
CPtr<IImage> LoadPicture( const std::string &szSource, SExportOutcome &outcome );

// SaveCompressedTexture: the picture after the gamma correction as
// <szName>_c.dds (DXT5), _l.dds (lowFormat) and _h.dds (ARGB8888). Mine and
// trench frames use ARGB1555 for the low format, sprites and infantry
// ARGB4444; the overload without lowFormat is the ARGB1555 one.
bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, EGFXPixelFormat lowFormat, SExportOutcome &outcome );
bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// The animation format written as the engine's structure file szName (chunk
// 1), which is how MFC wrote every .san.
bool SaveAnimation( const SExportContext &context, SSpriteAnimationFormat &animations, const std::string &szName, SExportOutcome &outcome );

// CParentFrame::ConvertAndSaveImage for a mine or trench frame: the picture
// written as <szName>_c.dds (DXT5), _l.dds (ARGB1555) and _h.dds (ARGB8888)
// after the gamma correction. False with outcome.szError naming the source
// and the reason (missing, not an image or truncated, unsupported) when
// nothing could be written.
bool ConvertAndSaveImage( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// ComposeSingleObject( sprite, shadow, szName, VNULL2 ) for the mine: the
// sprite picture packed into <szName>_c/_l/_h.dds with its <szName>.san
// animation, and the shadow modulated by the inverted sprite alpha into
// <szName>s_c/_l/_h.dds and <szName>s.san. Both pictures are checked before
// anything is written, so a failed compose leaves nothing behind.
bool ComposeSingleObject( const SExportContext &context, const std::string &szSprite, const std::string &szShadow, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// SaveImageAsTGA into the staging root: the picture written as szName (the
// unit's icon.tga). False with outcome.szError when it cannot be written;
// counts the file in outcome.nWritten when it is.
bool SaveTga( const SExportContext &context, IImage *pImage, const std::string &szName, SExportOutcome &outcome );

// MyCopyFile into the staging root: szSource copied to szName, replacing it.
// False with outcome.szError when the source is not there or the copy fails.
bool CopyFileInto( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome );

}

}
