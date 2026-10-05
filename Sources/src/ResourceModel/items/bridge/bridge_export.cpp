// The bridge exporter: CBridgeFrame::ExportFrameData (Sources/src/editor/
// BridgeFrm.cpp:652-1037), FillRPGStats (1038), SaveSegmentInformation (2416)
// and AddSpriteAndShadow (1286). The stats are SBridgeRPGStats written as
// tree.Add( "RPG", &stats ): per damage stage (whole, damaged, destroyed) the
// spans of its begin, center and end parts, each span a slab and a back and a
// front girder segment, the segments with the passability and visibility grids
// of the first span of each part kind, the defences, and the fire, smoke and
// directed-explosion points. The graphics are one sprite set per stage, 1, 2 and 3
// (.san with _c/_l/_h.dds), and its shadows 1s, 2s and 3s, plus icon.tga from
// the slab of the first begin span.
//
// MFC computed what needs the editor view from the scene: the marks' world
// positions through IScene::GetPos2/GetPos3 and each sprite's zero cross from
// the sprite object. The port uses the camera the host hands in (the engine
// scene's, or the default editor camera) and the same expressions: the sprite
// was composed from one picture without pre-processing, which puts its top-left
// corner at its position (14, 16 world cells) and the frame shift at half the
// picture, so ComputeSpriteNewZeroPos reduces to the zero cross on screen minus
// the sprite's position plus half the picture. MFC's frame kept the marks and
// girder offsets in own_data (Begin, End, Front, Back; the centre mark is a
// constant) and the fire and smoke points in the project's "RPG" chunk; see
// bridge_export.h.
//
// MFC's quirks that change the files are kept: the back and front girder index
// of a span that has no such girder is that of the previous span of its kind;
// the grids come from the first span of the whole stage only, the others
// copying them; and the pass pack of the slab is drawn at the world origin cast
// to int. Where MFC wrote through a null (a girder index of -1 in the copy of
// the grids) the port skips the segment.
//
// What MFC reported with a message box is a refusal that names the file or the
// item: a part without a picture where one is needed (the slab always), a
// picture that cannot be read, a sprite and shadow of different sizes, a span
// without its SpanIndex, a stage without a begin, center or end span. A refused
// export writes nothing: the pictures are all read and compared before the
// stats are written, which MFC did not do.
//
// MFC had no up-to-date check for a bridge (FindMaximalSourceTime is the base
// class's, which is never older than an export), so every export writes.
#include "StdAfx.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "bridge_export.h"
#include "bridge.h"
#include "../stats_export.h"
#include "../../compose.h"
#include "../../image_export.h"
#include "../../mfc_value.h"
#include "../tree_item_types.h"
#include "../stats_item.h"
#include "../../../Main/RPGStats.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

typedef SBridgeRPGStats::SSegmentRPGStats TSegment;

const char kBridgeAddDir[] = "bridges\\";
// BridgeFrm.cpp:27: the shift of the zero cross.
const float kZeroShift = 15.4f;
// BridgeFrm.cpp:34: where the centre spans stand.
const SVec3 kCenterKrest{ 596.657f, 742.038f, 0 };

const int kSpanTypes[3] = { ETIT_BRIDGE_BEGIN_SPANS_ITEM, ETIT_BRIDGE_CENTER_SPANS_ITEM, ETIT_BRIDGE_END_SPANS_ITEM };
const char *const kSpanNames[3] = { "Begin spans", "Center spans", "End spans" };
const char *const kStageNames[3] = { "Whole", "Damaged", "Destroyed" };

const char kImportedDir[] = "imported\\";
const char kFirePointName[] = "Fire point";
const char kSmokePointName[] = "Smoke point";

int DefenceIndex( const std::string &szName )
{
	if ( szName == "Left" )
		return RPG_LEFT;
	if ( szName == "Right" )
		return RPG_RIGHT;
	if ( szName == "Top" )
		return RPG_TOP;
	if ( szName == "Bottom" )
		return RPG_BOTTOM;
	if ( szName == "Front" )
		return RPG_FRONT;
	if ( szName == "Back" )
		return RPG_BACK;
	return 0;
}

struct SClass { DWORD dwClass; const char *pszName; };
// The four passability flags of the basic properties, in the order of their values.
const SClass kClasses[4] = { { AI_CLASS_HUMAN, "infantry" }, { AI_CLASS_WHEEL, "wheels" }, { AI_CLASS_HALFTRACK, "halftracks" }, { AI_CLASS_TRACK, "tracks" } };

std::vector<const NResourceXml::Node *> Items( const NResourceXml::Node *pList )
{
	std::vector<const NResourceXml::Node *> out;
	if ( pList == nullptr )
		return out;
	for ( const auto &child : pList->children )
		if ( child.kind == NResourceXml::Node::Element && child.name == "item" )
			out.push_back( &child );
	return out;
}

float AttrFloat( const NResourceXml::Node &node, const char *pszName, float fDefault )
{
	const std::string *pValue = FindAttr( node, pszName );
	return pValue != nullptr ? float( std::strtod( pValue->c_str(), nullptr ) ) : fDefault;
}

std::string BodyText( const NResourceXml::Node &node )
{
	for ( const auto &child : node.children )
		if ( child.kind == NResourceXml::Node::Text || child.kind == NResourceXml::Node::CData )
			return child.text;
	return std::string();
}

SVec3 ReadVec3( const NResourceXml::Node &parent, const char *pszName, const SVec3 &vDefault )
{
	SVec3 v = vDefault;
	if ( const NResourceXml::Node *pNode = NResourceXml::FindChild( parent, pszName ) )
	{
		v.x = AttrFloat( *pNode, "x", v.x );
		v.y = AttrFloat( *pNode, "y", v.y );
		v.z = AttrFloat( *pNode, "z", v.z );
	}
	return v;
}

CVec3 ReadPos3( const NResourceXml::Node &item, const char *pszName )
{
	const SVec3 v = ReadVec3( item, pszName, SVec3{} );
	return CVec3( v.x, v.y, v.z );
}

CVec2 ReadPos2( const NResourceXml::Node &item, const char *pszName )
{
	const SVec3 v = ReadVec3( item, pszName, SVec3{} );
	return CVec2( v.x, v.y );
}

NResourceXml::Node NewElement( const std::string &szName )
{
	NResourceXml::Node node;
	node.kind = NResourceXml::Node::Element;
	node.name = szName;
	return node;
}

NResourceXml::Node &ChildOrNew( NResourceXml::Node &parent, const std::string &szName )
{
	for ( auto &child : parent.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == szName )
			return child;
	parent.children.push_back( NewElement( szName ) );
	return parent.children.back();
}

NResourceXml::Node Vec2Node( const char *pszName, const CVec2 &v )
{
	NResourceXml::Node node = NewElement( pszName );
	SetAttr( node, "x", MfcFloat( v.x ) );
	SetAttr( node, "y", MfcFloat( v.y ) );
	return node;
}

NResourceXml::Node Vec3Node( const char *pszName, const CVec3 &v )
{
	NResourceXml::Node node = Vec2Node( pszName, CVec2( v.x, v.y ) );
	SetAttr( node, "z", MfcFloat( v.z ) );
	return node;
}

const CTreeItem *NthChild( const CTreeItem &container, std::size_t nIndex )
{
	const auto &children = container.GetChildren();
	return nIndex < children.size() ? children[nIndex].get() : nullptr;
}

// The frame's own data (LoadFrameOwnData): the span marks and the girders'
// offsets from the span centre, from the constructor's values where the file has none.
struct SFrame
{
	SVec3 vBegin, vEnd;
	float fFront = 0, fBack = 0;

	SFrame()
	{
		vBegin = vEnd = SVec3{ 16 * fWorldCellSize - 300, 16 * fWorldCellSize, 0 };
	}
};

SFrame ReadFrame( const NResourceXml::Node &projectElement )
{
	SFrame frame;
	if ( const NResourceXml::Node *pOwn = NResourceXml::FindChild( projectElement, "own_data" ) )
	{
		frame.vBegin = ReadVec3( *pOwn, "Begin", frame.vBegin );
		frame.vEnd = ReadVec3( *pOwn, "End", frame.vEnd );
		frame.fFront = AttrFloat( *pOwn, "Front", 0 );
		frame.fBack = AttrFloat( *pOwn, "Back", 0 );
	}
	return frame;
}

// The span's mark moved on screen by the zero cross's shift and back to the
// world, which is where ExportFrameData took vPapa.
SVec3 PapaOf( const GridProjection &projection, const SVec3 &vMark )
{
	SVec2 v2 = projection.Pos3To2( vMark );
	v2.x += kZeroShift;
	v2.y += kZeroShift;
	return projection.Pos2To3( v2 );
}

const SVec3 &MarkOf( const SFrame &frame, int nKind )
{
	return nKind == 0 ? frame.vBegin : nKind == 1 ? kCenterKrest : frame.vEnd;
}

// SaveSegmentInformation's grids of one span: the passability grid of its
// locked tiles (low nibble) and unlocked tiles (high nibble) and the
// visibility grid of its transparences, each with its origin from the span's
// mark. The unlocked tiles' extent is returned for the span's length and
// width. An empty unlocked list leaves that extent at 0, which MFC then merged
// into the grid's bounds: a grid with no unlocked tiles reaches tile 0.
void SaveSegmentInformation( TSegment &segment, const CBridgePartsItem &parts, const SVec3 &vPapa, const GridProjection &projection,
                             int &nUMinX, int &nUMaxX, int &nUMinY, int &nUMaxY )
{
	const CListOfTiles &locked = parts.lockedTiles;
	const CListOfTiles &unlocked = parts.unLockedTiles;
	if ( locked.empty() && unlocked.empty() )
	{
		segment.passability.SetSizes( 0, 0 );
		segment.vOrigin = CVec2( 0, 0 );
	}
	else
	{
		const SAITile &first = !locked.empty() ? locked.front() : unlocked.front();
		int nTileMinX = first.nTileX, nTileMaxX = first.nTileX;
		int nTileMinY = first.nTileY, nTileMaxY = first.nTileY;
		for ( const SAITile &tile : locked )
		{
			nTileMinX = std::min( nTileMinX, tile.nTileX );
			nTileMaxX = std::max( nTileMaxX, tile.nTileX );
			nTileMinY = std::min( nTileMinY, tile.nTileY );
			nTileMaxY = std::max( nTileMaxY, tile.nTileY );
		}

		nUMinX = nUMaxX = nUMinY = nUMaxY = 0;
		if ( !unlocked.empty() )
		{
			nUMinX = nUMaxX = unlocked.front().nTileX;
			nUMinY = nUMaxY = unlocked.front().nTileY;
		}
		for ( const SAITile &tile : unlocked )
		{
			nUMinX = std::min( nUMinX, tile.nTileX );
			nUMaxX = std::max( nUMaxX, tile.nTileX );
			nUMinY = std::min( nUMinY, tile.nTileY );
			nUMaxY = std::max( nUMaxY, tile.nTileY );
		}
		nTileMinX = std::min( nTileMinX, nUMinX );
		nTileMaxX = std::max( nTileMaxX, nUMaxX );
		nTileMinY = std::min( nTileMinY, nUMinY );
		nTileMaxY = std::max( nTileMaxY, nUMaxY );

		const int nWidth = nTileMaxX - nTileMinX + 1;
		segment.passability.SetSizes( nWidth, nTileMaxY - nTileMinY + 1 );
		BYTE *pBuf = segment.passability.GetBuffer();
		for ( int y = 0; y < nTileMaxY - nTileMinY + 1; ++y )
			for ( int x = 0; x < nWidth; ++x )
			{
				BYTE nVal = 0;
				for ( const SAITile &tile : locked )
					if ( x == tile.nTileX - nTileMinX && y == tile.nTileY - nTileMinY )
					{
						nVal = BYTE( tile.nVal );
						break;
					}
				for ( const SAITile &tile : unlocked )
					if ( x == tile.nTileX - nTileMinX && y == tile.nTileY - nTileMinY )
					{
						nVal = BYTE( tile.nVal << 4 );
						break;
					}
				pBuf[x + y * nWidth] = nVal;
			}
		const SVec3 origin = projection.OriginOfGrid( vPapa, nTileMinX, nTileMinY );
		segment.vOrigin = CVec2( origin.x, origin.y );
	}

	const CListOfTiles &transparences = parts.transeparences;
	if ( transparences.empty() )
	{
		segment.visibility.SetSizes( 0, 0 );
		segment.vVisOrigin = CVec2( 0, 0 );
		return;
	}
	int nTileMinX = transparences.front().nTileX, nTileMaxX = nTileMinX;
	int nTileMinY = transparences.front().nTileY, nTileMaxY = nTileMinY;
	for ( const SAITile &tile : transparences )
	{
		nTileMinX = std::min( nTileMinX, tile.nTileX );
		nTileMaxX = std::max( nTileMaxX, tile.nTileX );
		nTileMinY = std::min( nTileMinY, tile.nTileY );
		nTileMaxY = std::max( nTileMaxY, tile.nTileY );
	}
	const int nWidth = nTileMaxX - nTileMinX + 1;
	segment.visibility.SetSizes( nWidth, nTileMaxY - nTileMinY + 1 );
	BYTE *pBuf = segment.visibility.GetBuffer();
	for ( int y = 0; y < nTileMaxY - nTileMinY + 1; ++y )
		for ( int x = 0; x < nWidth; ++x )
		{
			BYTE nVal = 0;
			for ( const SAITile &tile : transparences )
				if ( x == tile.nTileX - nTileMinX && y == tile.nTileY - nTileMinY )
				{
					nVal = BYTE( tile.nVal );
					break;
				}
			pBuf[x + y * nWidth] = nVal;
		}
	const SVec3 origin = projection.OriginOfGrid( vPapa, nTileMinX, nTileMinY );
	segment.vVisOrigin = CVec2( origin.x, origin.y );
}

// MakeFullPath( project folder, name ) of one picture, with the case of the
// folders on disk (the projects were authored on Windows).
std::string SourceFile( const SExportContext &context, const std::string &szName )
{
	std::string szRel = szName;
	std::replace( szRel.begin(), szRel.end(), '/', '\\' );
	std::string szDir = ProjectDirectory( context );
	std::replace( szDir.begin(), szDir.end(), '/', '\\' );
	const std::string szFull = IsRelatedPath( szRel ) ? MakeFullPath( szDir, szRel ) : szRel;
	return FoldedFile( szFull ).string();
}

// AddSpriteAndShadow: the picture and its shadow (the picture's name with an s
// before the extension), read and compared, and where the zero cross lies in
// it. A stats-only export has no picture to read and adds an empty entry.
bool AddSpriteAndShadow( std::vector<NCompose::SPackPicture> &pictures, const SExportContext &context, const GridProjection &projection,
                         const std::string &szName, const SVec3 &vZero, SExportOutcome &outcome )
{
	NCompose::SPackPicture picture;
	if ( !context.bStatsOnly )
	{
		const std::string szSprite = SourceFile( context, szName );
		picture.pSprite = NImageExport::LoadPicture( szSprite, outcome );
		if ( picture.pSprite == 0 )
			return false;
		const std::string::size_type nDot = szSprite.rfind( '.' );
		const std::string szShadow = FoldedFile( szSprite.substr( 0, nDot ) + "s.tga" ).string();
		picture.pShadow = NImageExport::LoadPicture( szShadow, outcome );
		if ( picture.pShadow == 0 )
			return false;
		const int nWidth = picture.pSprite->GetSizeX(), nHeight = picture.pSprite->GetSizeY();
		if ( nWidth != picture.pShadow->GetSizeX() || nHeight != picture.pShadow->GetSizeY() )
		{
			outcome.szError = "The size of sprite does not equal the size of shadow: " + szSprite + " is " + std::to_string( nWidth ) + "x" + std::to_string( nHeight ) +
			                  ", " + szShadow + " is " + std::to_string( picture.pShadow->GetSizeX() ) + "x" + std::to_string( picture.pShadow->GetSizeY() );
			return false;
		}

		// ComputeSpriteNewZeroPos of the sprite LoadSpriteItem built: texel 0.5 of
		// the picture at the sprite's position, the frame shift at half the picture.
		const SVec2 vZero2 = projection.Pos3To2( vZero );
		const SVec2 vSprite2 = projection.Pos3To2( SVec3{ 14 * fWorldCellSize, 16 * fWorldCellSize, 0 } );
		const float fMapX = ( 0.0f + 0.5f ) / float( nWidth ), fMapY = ( 0.0f + 0.5f ) / float( nHeight );
		const int w = int( std::ceil( fMapX * nWidth - 0.5f ) ) + nWidth / 2;
		const int h = int( std::ceil( fMapY * nHeight - 0.5f ) ) + nHeight / 2;
		picture.zeroPos = CVec2( std::ceil( vZero2.x - ( vSprite2.x - w ) ), std::ceil( vZero2.y - ( vSprite2.y - h ) ) );
	}
	pictures.push_back( picture );
	return true;
}

// What one span item of the tree holds.
struct SSpanItem
{
	const CBridgePartsItem *pParts = nullptr;
	std::string szBack, szFront, szSlab;
};

bool CollectSpans( const CTreeItem &kind, const char *pszWhat, std::vector<SSpanItem> &spans, SExportOutcome &outcome )
{
	for ( const auto &pChild : kind.GetChildren() )
	{
		SSpanItem span;
		span.pParts = dynamic_cast<const CBridgePartsItem *>( pChild.get() );
		if ( span.pParts == nullptr )
			continue;
		if ( span.pParts->nSpanIndex < 0 )
		{
			outcome.szError = std::string( pszWhat ) + ": a span item has no SpanIndex";
			return false;
		}
		const CTreeItem *pBack = RequireChild( *pChild, ETIT_BRIDGE_PART_PROPS_ITEM, 0, "Back girder", outcome );
		const CTreeItem *pFront = RequireChild( *pChild, ETIT_BRIDGE_PART_PROPS_ITEM, 1, "Front girder", outcome );
		const CTreeItem *pSlab = RequireChild( *pChild, ETIT_BRIDGE_PART_PROPS_ITEM, 2, "Slab", outcome );
		if ( pBack == nullptr || pFront == nullptr || pSlab == nullptr )
			return false;
		span.szBack = ValueStr( *pBack, 0 );
		span.szFront = ValueStr( *pFront, 0 );
		span.szSlab = ValueStr( *pSlab, 0 );
		spans.push_back( span );
	}
	return true;
}

// The segment, span and pack of every span of one stage, in the order
// ExportFrameData walked them.
bool BuildStage( int nStage, const CTreeItem &stage, const SFrame &frame, bool bHorizontal, const SExportContext &context, const GridProjection &projection,
                 SBridgeRPGStats &rpgStats, std::vector<NCompose::SPackPicture> &pictures, SExportOutcome &outcome )
{
	SBridgeRPGStats::SDamageState &state = rpgStats.states[nStage];
	int nPackSegmentIndex = 0;
	for ( int i = 0; i < 3; ++i )
	{
		const std::string szWhat = std::string( kStageNames[nStage] ) + " " + kSpanNames[i];
		const CTreeItem *pKind = RequireChild( stage, kSpanTypes[i], 0, szWhat.c_str(), outcome );
		if ( pKind == nullptr )
			return false;
		std::vector<SSpanItem> spans;
		if ( !CollectSpans( *pKind, szWhat.c_str(), spans, outcome ) )
			return false;

		const SVec3 vPapa = PapaOf( projection, MarkOf( frame, i ) );
		// Declared per kind as in MFC: a span without a girder keeps the previous one's index.
		int nFrontIndex = -1, nBackIndex = -1;
		int nActiveBridgePart = -1;
		for ( const SSpanItem &item : spans )
		{
			++nActiveBridgePart;
			TSegment segment;
			segment.eType = TSegment::GIRDER;

			auto Girder = [&]( const std::string &szPicture, float fOffset, int &nIndex ) -> bool
			{
				segment.eType = TSegment::GIRDER;
				segment.szModel = std::to_string( nStage + 1 );
				SVec3 vTemp = vPapa;
				if ( bHorizontal )
				{
					segment.vRelPos = CVec3( 0, fOffset, 0 );
					vTemp.y += fOffset;
				}
				else
				{
					segment.vRelPos = CVec3( fOffset, 0, 0 );
					vTemp.x += fOffset;
				}
				if ( !AddSpriteAndShadow( pictures, context, projection, szPicture, vTemp, outcome ) )
					return false;
				segment.nFrameIndex = nPackSegmentIndex++;
				nIndex = int( rpgStats.segments.size() );
				rpgStats.segments.push_back( segment );
				return true;
			};
			if ( !item.szBack.empty() && !Girder( item.szBack, frame.fBack, nBackIndex ) )
				return false;
			if ( !item.szFront.empty() && !Girder( item.szFront, frame.fFront, nFrontIndex ) )
				return false;
			if ( item.szSlab.empty() )
			{
				outcome.szError = "Error: There is no bottom part for this bridge, can not export bridge! (" + szWhat + ", span " + std::to_string( item.pParts->nSpanIndex ) + " has no slab picture)";
				return false;
			}

			segment.eType = TSegment::SLAB;
			segment.szModel = std::to_string( nStage + 1 );
			segment.vRelPos = CVec3( 0, 0, 0 );
			if ( !AddSpriteAndShadow( pictures, context, projection, item.szSlab, vPapa, outcome ) )
				return false;
			segment.nFrameIndex = nPackSegmentIndex++;

			int nUMinX = 0, nUMaxX = 0, nUMinY = 0, nUMaxY = 0;
			if ( nStage == 0 && nActiveBridgePart == 0 )
			{
				SaveSegmentInformation( segment, *item.pParts, vPapa, projection, nUMinX, nUMaxX, nUMinY, nUMaxY );
				pictures.back().pass = segment.passability;
				pictures.back().vPassOrigin = segment.vOrigin;
			}
			rpgStats.segments.push_back( segment );

			SBridgeRPGStats::SSpan span;
			span.nSlab = int( rpgStats.segments.size() ) - 1;
			span.nBackGirder = nBackIndex;
			span.nFrontGirder = nFrontIndex;
			span.fLength = span.fWidth = 0;
			if ( !item.pParts->unLockedTiles.empty() )
			{
				if ( bHorizontal )
				{
					span.fLength = float( nUMaxX - nUMinX + 1 );
					span.fWidth = float( nUMaxY - nUMinY + 1 );
				}
				else
				{
					span.fLength = float( nUMaxY - nUMinY + 1 );
					span.fWidth = float( nUMaxX - nUMinX + 1 );
				}
			}
			if ( nStage == 0 && i == 1 && span.fLength == 0 )
				outcome.warnings.push_back( "automatically computed length for the line part of the bridge is 0: fill the unlocked tiles of its first span" );

			const int nSpanIndex = item.pParts->nSpanIndex;
			if ( int( state.spans.size() ) < nSpanIndex + 1 )
				state.spans.resize( nSpanIndex + 1 );
			state.spans[nSpanIndex] = span;
			( i == 0 ? state.begins : i == 1 ? state.lines : state.ends ).push_back( nSpanIndex );
		}
	}
	return true;
}

// The loop at the end of ExportFrameData: every span of a kind takes the
// length and width of the first span of that kind in the whole stage, and every
// segment of it the grids of that span's slab (but the whole stage's own slabs).
void ShareGrids( SBridgeRPGStats &rpgStats )
{
	const SBridgeRPGStats::SDamageState &whole = rpgStats.states[0];
	const std::vector<int> *const firsts[3] = { &whole.begins, &whole.lines, &whole.ends };
	for ( int i = 0; i < 3; ++i )
	{
		const SBridgeRPGStats::SSpan &reference = whole.spans[( *firsts[i] )[0]];
		const TSegment source = rpgStats.segments[reference.nSlab];
		const float fLength = reference.fLength, fWidth = reference.fWidth;
		for ( int nStage = 0; nStage < 3; ++nStage )
		{
			SBridgeRPGStats::SDamageState &state = rpgStats.states[nStage];
			const std::vector<int> &list = i == 0 ? state.begins : i == 1 ? state.lines : state.ends;
			for ( int nSpan : list )
			{
				SBridgeRPGStats::SSpan &span = state.spans[nSpan];
				span.fLength = fLength;
				span.fWidth = fWidth;
				const int segments[3] = { span.nSlab, span.nBackGirder, span.nFrontGirder };
				for ( int z = 0; z < 3; ++z )
				{
					if ( ( nStage == 0 && z == 0 ) || segments[z] < 0 )
						continue;
					TSegment &segment = rpgStats.segments[segments[z]];
					segment.passability = source.passability;
					segment.vOrigin = source.vOrigin;
					segment.visibility = source.visibility;
					segment.vVisOrigin = source.vVisOrigin;
				}
			}
		}
	}
}

// One aimed point of the "RPG" chunk's lists, with the direction and (for a fire
// point) effect the tree's child overrides.
template <class TPoint>
void ReadAimed( const NResourceXml::Node &item, TPoint &point )
{
	point.vPos = ReadPos3( item, "Position" );
	point.vPos.z = 0;
	point.vPicturePosition = ReadPos2( item, "PicturePosition" );
	point.vWorldPosition = ReadPos3( item, "WorldPosition" );
	point.fDirection = AttrFloat( item, "Direction", point.fDirection );
	point.fVerticalAngle = AttrFloat( item, "VerticalAngle", point.fVerticalAngle );
}

// FillRPGStats: the defences from the tree, and the fire, smoke and directed
// explosion points from the tree's children and the "RPG" chunk.
bool FillRPGStats( SBridgeRPGStats &stats, const CTreeItem &root, const NResourceXml::Node &projectElement, SExportOutcome &outcome )
{
	const CTreeItem *pDefences = RequireChild( root, ETIT_BRIDGE_DEFENCES_ITEM, 0, "Defence", outcome );
	const CTreeItem *pFires = RequireChild( root, ETIT_BRIDGE_FIRE_POINTS_ITEM, 0, "Fire points", outcome );
	const CTreeItem *pSmokes = RequireChild( root, ETIT_BRIDGE_SMOKES_ITEM, 0, "Smoke points", outcome );
	if ( pDefences == nullptr || pFires == nullptr || pSmokes == nullptr )
		return false;
	for ( int i = 0; i < 6; ++i )
	{
		const CTreeItem *pDefProps = ChildItem( *pDefences, ETIT_BRIDGE_DEFENCE_PROPS_ITEM, i );
		if ( pDefProps == nullptr )
		{
			outcome.szError = "the Defence item has no defence number " + std::to_string( i + 1 );
			return false;
		}
		const int nIndex = DefenceIndex( pDefProps->GetDisplayName() );
		stats.defences[nIndex].nArmorMin = ValueInt( *pDefProps, 0 );
		stats.defences[nIndex].nArmorMax = ValueInt( *pDefProps, 1 );
		stats.defences[nIndex].fSilhouette = ValueFloat( *pDefProps, 2 );
	}

	const NResourceXml::Node *pRpg = NResourceXml::FindChild( projectElement, "RPG" );
	auto List = [&]( const char *pszName ) { return pRpg != nullptr ? Items( NResourceXml::FindChild( *pRpg, pszName ) ) : std::vector<const NResourceXml::Node *>(); };

	std::size_t nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "FirePoints" ) )
	{
		const CTreeItem *pChild = NthChild( *pFires, nPoint++ );
		SBridgeRPGStats::SFirePoint fire;
		ReadAimed( *pItem, fire );
		if ( pChild != nullptr )
		{
			fire.fDirection = ValueFloat( *pChild, 0 );
			fire.szFireEffect = ValueStr( *pChild, 1 );
		}
		else if ( const NResourceXml::Node *pEffect = NResourceXml::FindChild( *pItem, "FireEffect" ) )
			fire.szFireEffect = BodyText( *pEffect );
		stats.firePoints.push_back( fire );
	}

	stats.szSmokeEffect = ValueStr( *pSmokes, 0 );
	nPoint = 0;
	for ( const NResourceXml::Node *pItem : List( "SmokePoints" ) )
	{
		const CTreeItem *pChild = NthChild( *pSmokes, nPoint++ );
		SBridgeRPGStats::SFirePoint smoke;
		ReadAimed( *pItem, smoke );
		if ( pChild != nullptr )
			smoke.fDirection = ValueFloat( *pChild, 0 );
		stats.smokePoints.push_back( smoke );
	}

	// MFC never filled the five directed explosions, so the constructor's stay
	// unless the chunk holds others (the importer keeps a shipped bridge's).
	const std::vector<const NResourceXml::Node *> explosions = List( "DirExplosions" );
	if ( !explosions.empty() )
	{
		stats.dirExplosions.assign( explosions.size(), SBridgeRPGStats::SDirectionExplosion() );
		for ( std::size_t i = 0; i < explosions.size(); ++i )
			ReadAimed( *explosions[i], stats.dirExplosions[i] );
	}
	if ( pRpg != nullptr )
		if ( const NResourceXml::Node *pEffect = NResourceXml::FindChild( *pRpg, "DirExplosionEffect" ) )
			stats.szDirExplosionEffect = BodyText( *pEffect );
	return true;
}

void WarnLastError( SExportOutcome &outcome, const std::string &szPrefix )
{
	if ( outcome.szError.empty() )
		return;
	outcome.warnings.push_back( szPrefix + outcome.szError );
	outcome.szError.clear();
}

template <class T>
void SetValue( CTreeItem *pItem, std::size_t nSlot, const T &value )
{
	if ( pItem != nullptr && nSlot < pItem->MutableValues().size() )
		pItem->MutableValues()[nSlot].value = value;
}

CTreeItem *MutableChildOfType( CTreeItem &item, int nType, int nIndex = 0 )
{
	for ( const auto &pChild : item.GetChildren() )
		if ( pChild->GetItemType() == nType && nIndex-- == 0 )
			return pChild.get();
	return nullptr;
}

CTreeItem *AddItem( CTreeItem &container, int nType, const char *pszName )
{
	auto pChild = CTreeItemFactory::Instance().Create( nType );
	if ( !pChild )
		return nullptr;
	pChild->SetItemName( pszName );
	CTreeItem *pItem = pChild.get();
	container.AddChild( std::move( pChild ) );
	return pItem;
}

// The tiles of a grid read back: first tile at (nFirstX, nFirstY), the cell
// value's low nibble a locked tile and its high nibble an unlocked one
// (passability), or the whole cell a tile (visibility).
void GridTiles( const CArray2D<BYTE> &grid, const SVec3 &vPapa, const CVec2 &vOrigin, const GridProjection &projection, bool bPassability,
                CListOfTiles &locked, CListOfTiles &unlocked )
{
	if ( grid.GetSizeX() == 0 || grid.GetSizeY() == 0 )
		return;
	int nFirstX = 0, nFirstY = 0;
	projection.FirstTileOfGrid( vPapa, SVec3{ vOrigin.x, vOrigin.y, 0 }, nFirstX, nFirstY );
	for ( int y = 0; y < grid.GetSizeY(); ++y )
		for ( int x = 0; x < grid.GetSizeX(); ++x )
		{
			const int nCell = grid[y][x];
			if ( nCell == 0 )
				continue;
			SAITile tile;
			tile.nTileX = nFirstX + x;
			tile.nTileY = nFirstY + y;
			if ( !bPassability )
			{
				tile.nVal = nCell;
				locked.push_back( tile );
			}
			else if ( ( nCell >> 4 ) != 0 )
			{
				tile.nVal = nCell >> 4;
				unlocked.push_back( tile );
			}
			else
			{
				tile.nVal = nCell;
				locked.push_back( tile );
			}
		}
}

// One list of spans of one stage as span items.
void AddSpans( CTreeItem &kind, const SBridgeRPGStats &stats, int nStage, const std::vector<int> &list, int nKind, const SFrame &frame, const GridProjection &projection )
{
	const SBridgeRPGStats::SDamageState &state = stats.states[nStage];
	bool bFirst = true;
	for ( int nSpan : list )
	{
		if ( nSpan < 0 || nSpan >= int( state.spans.size() ) )
			continue;
		const SBridgeRPGStats::SSpan &span = state.spans[nSpan];
		CTreeItem *pItem = AddItem( kind, ETIT_BRIDGE_PARTS_ITEM, "Span parts" );
		CBridgePartsItem *pParts = dynamic_cast<CBridgePartsItem *>( pItem );
		if ( pParts == nullptr )
			continue;
		pParts->nSpanIndex = nSpan;

		const int segments[3] = { span.nBackGirder, span.nFrontGirder, span.nSlab };
		for ( int z = 0; z < 3; ++z )
		{
			CTreeItem *pProps = MutableChildOfType( *pParts, ETIT_BRIDGE_PART_PROPS_ITEM, z );
			if ( pProps == nullptr || segments[z] < 0 || segments[z] >= int( stats.segments.size() ) )
				continue;
			SetValue( pProps, 0, std::string( kImportedDir ) + std::to_string( nStage + 1 ) + "\\" + std::to_string( stats.segments[segments[z]].nFrameIndex ) + ".tga" );
		}

		if ( nStage == 0 && bFirst && span.nSlab >= 0 && span.nSlab < int( stats.segments.size() ) )
		{
			const TSegment &slab = stats.segments[span.nSlab];
			const SVec3 vPapa = PapaOf( projection, MarkOf( frame, nKind ) );
			CListOfTiles unused;
			GridTiles( slab.passability, vPapa, slab.vOrigin, projection, true, pParts->lockedTiles, pParts->unLockedTiles );
			GridTiles( slab.visibility, vPapa, slab.vVisOrigin, projection, false, pParts->transeparences, unused );
		}
		bFirst = false;
	}
}

// The girders' offset from the span centre: the first such girder's relative
// position along the bridge, as ExportFrameData took it from m_fBack and m_fFront.
float GirderOffset( const SBridgeRPGStats &stats, bool bBack )
{
	for ( int nStage = 0; nStage < 3; ++nStage )
		for ( const SBridgeRPGStats::SSpan &span : stats.states[nStage].spans )
		{
			const int nIndex = bBack ? span.nBackGirder : span.nFrontGirder;
			if ( nIndex >= 0 && nIndex < int( stats.segments.size() ) )
				return stats.direction == SBridgeRPGStats::HORIZONTAL ? stats.segments[nIndex].vRelPos.y : stats.segments[nIndex].vRelPos.x;
		}
	return 0;
}

SFrame ImportedFrame( const SBridgeRPGStats &stats )
{
	SFrame frame;
	frame.fBack = GirderOffset( stats, true );
	frame.fFront = GirderOffset( stats, false );
	return frame;
}

}

bool ExportBridge( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_BRIDGE_ROOT_ITEM, "bridge", outcome );
	if ( !pProject )
		return false;
	const CTreeItem &root = *pProject->root;
	const CTreeItem *pCommonProps = RequireChild( root, ETIT_BRIDGE_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	if ( pCommonProps == nullptr )
		return false;
	const SFrame frame = ReadFrame( pProject->document.root );

	SBridgeRPGStats rpgStats;
	const bool bHorizontal = ValueStr( *pCommonProps, 1 ) == "horizontal" || ValueStr( *pCommonProps, 1 ) == "Horizontal";
	rpgStats.direction = bHorizontal ? SBridgeRPGStats::HORIZONTAL : SBridgeRPGStats::VERTICAL;
	rpgStats.fMaxHP = float( ValueInt( *pCommonProps, 2 ) );
	rpgStats.fRepairCost = ValueFloat( *pCommonProps, 3 );
	for ( int i = 0; i < 4; ++i )
		if ( ValueBool( *pCommonProps, 4 + i ) )
			rpgStats.dwAIClasses |= kClasses[i].dwClass;

	SGroundCamera camera;
	if ( !context.groundCamera || !context.groundCamera( camera ) )
	{
		camera = DefaultEditorCamera();
		outcome.warnings.push_back( "no engine camera: the span positions and grid origins use the default editor camera" );
	}
	const GridProjection projection( camera );

	std::vector<NCompose::SPackPicture> stagePictures[3];
	for ( int nStage = 0; nStage < 3; ++nStage )
	{
		const CTreeItem *pStage = RequireChild( root, ETIT_BRIDGE_STAGE_PROPS_ITEM, nStage, kStageNames[nStage], outcome );
		if ( pStage == nullptr || !BuildStage( nStage, *pStage, frame, bHorizontal, context, projection, rpgStats, stagePictures[nStage], outcome ) )
			return false;
	}
	for ( int i = 0; i < 3; ++i )
	{
		const std::vector<int> &list = i == 0 ? rpgStats.states[0].begins : i == 1 ? rpgStats.states[0].lines : rpgStats.states[0].ends;
		if ( list.empty() )
		{
			outcome.szError = std::string( "the Whole stage has no span in its " ) + kSpanNames[i] + ": a bridge needs a begin, a center and an end span";
			return false;
		}
	}
	ShareGrids( rpgStats );
	if ( !FillRPGStats( rpgStats, root, pProject->document.root, outcome ) )
		return false;

	const std::string szFile = StatsFileName( project, context, kBridgeAddDir, false );
	const std::string szResultDir = DirectoryOf( szFile );
	outcome.szObjectName = szResultDir + "1";

	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;

	if ( !context.bStatsOnly )
	{
		const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
		for ( int nStage = 0; nStage < 3; ++nStage )
		{
			if ( stagePictures[nStage].empty() )
				continue;
			const std::string szName = szResultDir + std::to_string( nStage + 1 );
			bool bShadowFailed = false;
			if ( !NCompose::ComposeSpritesPack( context, gamma, GFXPF_ARGB1555, stagePictures[nStage], szName, &bShadowFailed, outcome ) )
				return false;
			if ( bShadowFailed )
				outcome.warnings.push_back( "the shadows of the " + std::string( kStageNames[nStage] ) + " stage could not be packed: " + szName + "s is not written" );
		}

		// SaveIconFile of the first begin span's slab.
		const CTreeItem *pWhole = ChildItem( root, ETIT_BRIDGE_STAGE_PROPS_ITEM, 0 );
		const CTreeItem *pBegin = pWhole != nullptr ? ChildItem( *pWhole, ETIT_BRIDGE_BEGIN_SPANS_ITEM, 0 ) : nullptr;
		const CTreeItem *pParts = pBegin != nullptr ? ChildItem( *pBegin, ETIT_BRIDGE_PARTS_ITEM, 0 ) : nullptr;
		const CTreeItem *pSlab = pParts != nullptr ? ChildItem( *pParts, ETIT_BRIDGE_PART_PROPS_ITEM, 2 ) : nullptr;
		if ( pSlab != nullptr && !NCompose::SaveIconFile( context, SourceFile( context, ValueStr( *pSlab, 0 ) ), szResultDir + "icon.tga", outcome ) )
			WarnLastError( outcome, "icon: " );
	}
	return true;
}

void BridgeStatsToTree( const SBridgeRPGStats &stats, CTreeItem &root, const GridProjection &projection, const std::string &szName )
{
	CTreeItem *pCommonProps = MutableChildOfType( root, ETIT_BRIDGE_COMMON_PROPS_ITEM );
	SetValue( pCommonProps, 0, szName );
	SetValue( pCommonProps, 1, std::string( stats.direction == SBridgeRPGStats::HORIZONTAL ? "horizontal" : "vertical" ) );
	SetValue( pCommonProps, 2, int( stats.fMaxHP ) );
	SetValue( pCommonProps, 3, stats.fRepairCost );
	for ( int i = 0; i < 4; ++i )
		SetValue( pCommonProps, 4 + i, ( stats.dwAIClasses & kClasses[i].dwClass ) != 0 );

	if ( CTreeItem *pDefences = MutableChildOfType( root, ETIT_BRIDGE_DEFENCES_ITEM ) )
		for ( int i = 0; i < 6; ++i )
		{
			CTreeItem *pDefProps = MutableChildOfType( *pDefences, ETIT_BRIDGE_DEFENCE_PROPS_ITEM, i );
			if ( pDefProps == nullptr )
				continue;
			const SDefenseRPGStats &defence = stats.defences[DefenceIndex( pDefProps->GetDisplayName() )];
			SetValue( pDefProps, 0, defence.nArmorMin );
			SetValue( pDefProps, 1, defence.nArmorMax );
			SetValue( pDefProps, 2, defence.fSilhouette < 0 || defence.fSilhouette > 1 ? 1.0f : defence.fSilhouette );
		}

	const SFrame frame = ImportedFrame( stats );
	for ( int nStage = 0; nStage < 3; ++nStage )
	{
		CTreeItem *pStage = MutableChildOfType( root, ETIT_BRIDGE_STAGE_PROPS_ITEM, nStage );
		if ( pStage == nullptr )
			continue;
		const SBridgeRPGStats::SDamageState &state = stats.states[nStage];
		const std::vector<int> *const lists[3] = { &state.begins, &state.lines, &state.ends };
		for ( int i = 0; i < 3; ++i )
			if ( CTreeItem *pKind = MutableChildOfType( *pStage, kSpanTypes[i] ) )
			{
				pKind->MutableChildren().clear();
				AddSpans( *pKind, stats, nStage, *lists[i], i, frame, projection );
			}
	}

	if ( CTreeItem *pFires = MutableChildOfType( root, ETIT_BRIDGE_FIRE_POINTS_ITEM ) )
	{
		pFires->MutableChildren().clear();
		for ( const SBridgeRPGStats::SFirePoint &fire : stats.firePoints )
		{
			CTreeItem *pChild = AddItem( *pFires, ETIT_BRIDGE_FIRE_POINT_PROPS_ITEM, kFirePointName );
			SetValue( pChild, 0, fire.fDirection );
			SetValue( pChild, 1, fire.szFireEffect );
		}
	}
	if ( CTreeItem *pSmokes = MutableChildOfType( root, ETIT_BRIDGE_SMOKES_ITEM ) )
	{
		SetValue( pSmokes, 0, stats.szSmokeEffect );
		pSmokes->MutableChildren().clear();
		for ( const SBridgeRPGStats::SFirePoint &smoke : stats.smokePoints )
		{
			CTreeItem *pChild = AddItem( *pSmokes, ETIT_BRIDGE_SMOKE_PROPS_ITEM, kSmokePointName );
			SetValue( pChild, 0, smoke.fDirection );
		}
	}
}

void WriteBridgeFrameData( NResourceXml::Node &root, const SBridgeRPGStats &stats )
{
	const SFrame frame = ImportedFrame( stats );
	NResourceXml::Node &ownData = ChildOrNew( root, "own_data" );
	for ( const char *pszMark : { "Begin", "End" } )
	{
		const SVec3 &v = pszMark[0] == 'B' ? frame.vBegin : frame.vEnd;
		NResourceXml::Node &node = ChildOrNew( ownData, pszMark );
		SetAttr( node, "x", MfcFloat( v.x ) );
		SetAttr( node, "y", MfcFloat( v.y ) );
		SetAttr( node, "z", MfcFloat( v.z ) );
	}
	SetAttr( ownData, "Front", MfcFloat( frame.fFront ) );
	SetAttr( ownData, "Back", MfcFloat( frame.fBack ) );

	NResourceXml::Node &rpg = ChildOrNew( root, "RPG" );
	auto Replace = [&]( const char *pszList, std::vector<NResourceXml::Node> &&items )
	{
		ChildOrNew( rpg, pszList ).children = std::move( items );
	};
	auto Point = [&]( float fDirection, float fVerticalAngle, const CVec3 &vPos, const CVec2 &vPicture, const CVec3 &vWorld, const std::string *pszEffect )
	{
		NResourceXml::Node item = NewElement( "item" );
		SetAttr( item, "Direction", MfcFloat( fDirection ) );
		SetAttr( item, "VerticalAngle", MfcFloat( fVerticalAngle ) );
		item.children.push_back( Vec3Node( "Position", vPos ) );
		if ( pszEffect != nullptr )
			item.children.push_back( StringElement( "FireEffect", *pszEffect ) );
		item.children.push_back( Vec2Node( "PicturePosition", vPicture ) );
		item.children.push_back( Vec3Node( "WorldPosition", vWorld ) );
		return item;
	};
	std::vector<NResourceXml::Node> items;
	for ( const auto &fire : stats.firePoints )
		items.push_back( Point( fire.fDirection, fire.fVerticalAngle, fire.vPos, fire.vPicturePosition, fire.vWorldPosition, &fire.szFireEffect ) );
	Replace( "FirePoints", std::move( items ) );
	items.clear();
	for ( const auto &smoke : stats.smokePoints )
		items.push_back( Point( smoke.fDirection, smoke.fVerticalAngle, smoke.vPos, smoke.vPicturePosition, smoke.vWorldPosition, nullptr ) );
	Replace( "SmokePoints", std::move( items ) );
	items.clear();
	for ( const auto &explosion : stats.dirExplosions )
		items.push_back( Point( explosion.fDirection, explosion.fVerticalAngle, explosion.vPos, explosion.vPicturePosition, explosion.vWorldPosition, nullptr ) );
	Replace( "DirExplosions", std::move( items ) );

	rpg.children.erase( std::remove_if( rpg.children.begin(), rpg.children.end(), []( const NResourceXml::Node &n ) { return n.kind == NResourceXml::Node::Element && n.name == "DirExplosionEffect"; } ), rpg.children.end() );
	rpg.children.push_back( StringElement( "DirExplosionEffect", stats.szDirExplosionEffect ) );
}

}
