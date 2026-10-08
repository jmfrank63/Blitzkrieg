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
#include "../Formats/fmtSprite.h"
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
// The same with the frame's own compressed format too: the tileset frame
// compresses _c.dds as DXT1 and _l.dds as ARGB0565 (TileSetFrm.cpp:60), and
// swaps in DXT5 and ARGB4444 for the crosset.
bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, EGFXPixelFormat compressedFormat, EGFXPixelFormat lowFormat, SExportOutcome &outcome );

// The mesh frame's SaveCompressedTexture (SpriteCompose.cpp:522): _c.dds and
// _l.dds take the format ChooseBestFormat gives the gamma-corrected picture
// (opaque: DXT1 and ARGB0565; only fully opaque or transparent: DXT1 and
// ARGB1555; otherwise DXT5 and ARGB4444), _h.dds is ARGB8888.
bool SaveCompressedTextureBestFormat( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// The animation format written as the engine's structure file szName (chunk
// 1), which is how MFC wrote every .san.
bool SaveAnimation( const SExportContext &context, SSpriteAnimationFormat &animations, const std::string &szName, SExportOutcome &outcome );

// CParentFrame::ConvertAndSaveImage for a mine or trench frame: the picture
// written as <szName>_c.dds (DXT5), _l.dds (ARGB1555) and _h.dds (ARGB8888)
// after the gamma correction. False with outcome.szError naming the source
// and the reason (missing, not an image or truncated, unsupported) when
// nothing could be written.
bool ConvertAndSaveImage( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );
// The same for a mesh frame, with SaveCompressedTextureBestFormat.
bool ConvertAndSaveImageBestFormat( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// ComposeSingleObject( sprite, shadow, szName, VNULL2 ) for the mine: the
// sprite picture packed into <szName>_c/_l/_h.dds with its <szName>.san
// animation, and the shadow modulated by the inverted sprite alpha into
// <szName>s_c/_l/_h.dds and <szName>s.san. Both pictures are checked before
// anything is written, so a failed compose leaves nothing behind.
bool ComposeSingleObject( const SExportContext &context, const std::string &szSprite, const std::string &szShadow, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome );

// SaveCompressedShadow, the shadow formats of the frame base class (no gamma):
// <szName>_c.dds (DXT5), _l.dds (ARGB4444) and _h.dds (ARGB8888).
bool SaveShadowTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, SExportOutcome &outcome );

// ComposeImageToTexture (BuildCompose.cpp:235): the picture padded to the next
// power of two on each side, the padding white with alpha 0, written as
// <szName>_c.dds, _l.dds and _h.dds after the gamma correction (bCorrect, with
// the frame's compressed and low formats: the medal frame DXT5 and ARGB1555,
// the chapter and campaign frames DXT3 and ARGB4444, the mission frame DXT3 and
// ARGB1555), or, without bCorrect, as _h.dds alone, which is what the editor's
// own preview of the picture wrote. False with outcome.szError naming the
// source and why when nothing could be written.
bool ComposeImageToTexture( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, EGFXPixelFormat compressedFormat, EGFXPixelFormat lowFormat, bool bCorrect, SExportOutcome &outcome );
// The same padding on a picture in memory, without writing: what the
// preview shows and the exporter packs.
CPtr<IImage> PadToPowerOfTwo( IImage *pSource );

// GetImageSize (BuildCompose.cpp:302): x1 and y1 the picture's size in pixels,
// x2 and y2 the used part of the padded texture, (size + 0.5) / padded size,
// which is what the stats' ImageRect holds. All zero with outcome.szError when
// the picture cannot be read.
CTRect<float> GetImageSize( const std::string &szImage, SExportOutcome &outcome );

// A packed sprite set (BuildSpritesPack of SpriteCompose.cpp) as the engine's
// structure file szName: chunk 1 the pack, chunk 127 its signature.
bool SaveSpritesPack( const SExportContext &context, SSpritesPack &pack, const std::string &szName, SExportOutcome &outcome );

// SaveImageAsTGA into the staging root: the picture written as szName (the
// unit's icon.tga). False with outcome.szError when it cannot be written;
// counts the file in outcome.nWritten when it is.
bool SaveTga( const SExportContext &context, IImage *pImage, const std::string &szName, SExportOutcome &outcome );

// SaveImageAsTGA into any folder (not the staging root): the temporary
// pictures an export hands to a later step that wants file names, such as the
// fence shadows ComposeFences feeds to BuildAnimations. The folder must exist.
bool SaveTgaFile( const std::string &szPath, IImage *pImage, SExportOutcome &outcome );

// MyCopyFile into the staging root: szSource copied to szName, replacing it.
// False with outcome.szError when the source is not there or the copy fails.
bool CopyFileInto( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome );

}

}
