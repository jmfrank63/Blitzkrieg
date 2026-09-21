// A field-by-field comparison of two maps, in the declaration order of
// SLoadMapInfo (Formats/fmtMap.h:372-405) and of each struct it holds. The
// field lists here are the ones the serialisers visit, so what is compared is
// exactly what is written.
//
// The static_assert at the bottom is the point of the file. Add a field to
// SLoadMapInfo and this stops compiling, which is the only way a new field
// cannot slip silently past the preservation tests. When it fires: add the
// field to CompareMap, then update the size from what the test prints.
#include "StdAfx.h"
#include "MapEquivalence.h"
#include "../Formats/fmtMap.h"
#include <cstdio>

namespace NMapFile
{
namespace {

// Carries where we are, so a mismatch deep in a patch can name itself.
class CPath
{
	std::string *pWhere;
	std::string szPrefix;
public:
	CPath( std::string *_pWhere, const std::string &_szPrefix ) : pWhere( _pWhere ), szPrefix( _szPrefix ) {  }
	bool Fail( const std::string &szField ) const
	{
		if ( pWhere )
			*pWhere = szPrefix.empty() ? szField : szPrefix + "." + szField;
		return false;
	}
	CPath Under( const std::string &szField ) const
	{
		return CPath( pWhere, szPrefix.empty() ? szField : szPrefix + "." + szField );
	}
	static std::string At( const std::string &szField, int nIndex )
	{
		char szBuffer[32];
		std::snprintf( szBuffer, sizeof szBuffer, "[%d]", nIndex );
		return szField + szBuffer;
	}
};

bool Same( const CVec2 &l, const CVec2 &r ) { return l.x == r.x && l.y == r.y; }
bool Same( const CVec3 &l, const CVec3 &r ) { return l.x == r.x && l.y == r.y && l.z == r.z; }

// Any forward-iterable container, compared elementwise. Used for the three
// std::list members and for soundsList, whose container type (TMapSoundInfoList)
// is used throughout the engine but typedef'd in no header this can include.
template<class C, class F>
bool CompareSequence( const C &l, const C &r, const std::string &szField, const CPath &path, F compare )
{
	if ( l.size() != r.size() )
		return path.Fail( szField + ".size" );
	typename C::const_iterator li = l.begin(), ri = r.begin();
	for ( int i = 0; li != l.end(); ++li, ++ri, ++i )
		if ( !compare( *li, *ri, path.Under( CPath::At( szField, i ) ) ) )
			return false;
	return true;
}

// std::vector<T> of a type with a Same() overload, compared elementwise.
template<class T, class F>
bool CompareVector( const std::vector<T> &l, const std::vector<T> &r, const std::string &szField, const CPath &path, F compare )
{
	if ( l.size() != r.size() )
		return path.Fail( szField + ".size" );
	for ( size_t i = 0; i < l.size(); ++i )
		if ( !compare( l[i], r[i], path.Under( CPath::At( szField, int( i ) ) ) ) )
			return false;
	return true;
}

// The plain ones, where a mismatch needs no inner path.
template<class T>
bool CompareFlatVector( const std::vector<T> &l, const std::vector<T> &r, const std::string &szField, const CPath &path )
{
	if ( l.size() != r.size() )
		return path.Fail( szField + ".size" );
	for ( size_t i = 0; i < l.size(); ++i )
		if ( !( l[i] == r[i] ) )
			return path.Fail( CPath::At( szField, int( i ) ) );
	return true;
}

bool CompareCross( const SCrossTileInfo &l, const SCrossTileInfo &r, const CPath &path )
{
	if ( l.x != r.x ) return path.Fail( "x" );
	if ( l.y != r.y ) return path.Fail( "y" );
	if ( l.tile != r.tile ) return path.Fail( "tile" );
	if ( l.cross != r.cross ) return path.Fail( "cross" );
	if ( l.flags != r.flags ) return path.Fail( "flags" );
	return true;
}

bool CompareCrossList( const STerrainPatchInfo::CCrossesList &l, const STerrainPatchInfo::CCrossesList &r,
                       const std::string &szField, const CPath &path )
{
	return CompareVector( l, r, szField, path, CompareCross );
}

bool ComparePatch( const STerrainPatchInfo &l, const STerrainPatchInfo &r, const CPath &path )
{
	if ( l.nStartX != r.nStartX ) return path.Fail( "nStartX" );
	if ( l.nStartY != r.nStartY ) return path.Fail( "nStartY" );
	if ( !CompareCrossList( l.basecrosses, r.basecrosses, "basecrosses", path ) ) return false;
	if ( l.layercrosses.size() != r.layercrosses.size() ) return path.Fail( "layercrosses.size" );
	for ( size_t i = 0; i < l.layercrosses.size(); ++i )
		if ( !CompareCrossList( l.layercrosses[i], r.layercrosses[i], CPath::At( "layercrosses", int( i ) ), path ) )
			return false;
	if ( !CompareCrossList( l.noisecrosses, r.noisecrosses, "noisecrosses", path ) ) return false;
	// Heights are recomputed by FillMinMaxHeights rather than serialised, so a
	// difference here means the recomputation disagreed - which is still a
	// difference the preservation tests must see.
	if ( l.fMinHeight != r.fMinHeight ) return path.Fail( "fMinHeight" );
	if ( l.fMaxHeight != r.fMaxHeight ) return path.Fail( "fMaxHeight" );
	for ( int i = 0; i < 4; ++i )
	{
		if ( l.fSubMinHeight[i] != r.fSubMinHeight[i] ) return path.Fail( CPath::At( "fSubMinHeight", i ) );
		if ( l.fSubMaxHeight[i] != r.fSubMaxHeight[i] ) return path.Fail( CPath::At( "fSubMaxHeight", i ) );
	}
	return true;
}

bool CompareVsoLayer( const SVectorStripeObjectDesc::SLayer &l, const SVectorStripeObjectDesc::SLayer &r, const CPath &path )
{
	if ( l.opacityCenter != r.opacityCenter ) return path.Fail( "opacityCenter" );
	if ( l.opacityBorder != r.opacityBorder ) return path.Fail( "opacityBorder" );
	if ( l.fStreamSpeed != r.fStreamSpeed ) return path.Fail( "fStreamSpeed" );
	if ( l.fTextureStep != r.fTextureStep ) return path.Fail( "fTextureStep" );
	if ( l.nNumCells != r.nNumCells ) return path.Fail( "nNumCells" );
	if ( l.bAnimated != r.bAnimated ) return path.Fail( "bAnimated" );
	if ( l.szTexture != r.szTexture ) return path.Fail( "szTexture" );
	if ( l.fDisturbance != r.fDisturbance ) return path.Fail( "fDisturbance" );
	if ( l.fRelWidth != r.fRelWidth ) return path.Fail( "fRelWidth" );
	return true;
}

bool CompareVsoPoint( const SVectorStripeObjectPoint &l, const SVectorStripeObjectPoint &r, const CPath &path )
{
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	if ( !Same( l.vNorm, r.vNorm ) ) return path.Fail( "vNorm" );
	if ( l.fRadius != r.fRadius ) return path.Fail( "fRadius" );
	if ( l.fWidth != r.fWidth ) return path.Fail( "fWidth" );
	if ( l.bKeyPoint != r.bKeyPoint ) return path.Fail( "bKeyPoint" );
	if ( l.fOpacity != r.fOpacity ) return path.Fail( "fOpacity" );
	return true;
}

bool CompareControlPoint( const CVec3 &l, const CVec3 &r, const CPath &path )
{
	return Same( l, r ) ? true : path.Fail( "" );
}

bool CompareVso( const SVectorStripeObject &l, const SVectorStripeObject &r, const CPath &path )
{
	// The descriptor half first, in its serialiser's order (fmtVSO.cpp).
	if ( !CompareVsoLayer( l.bottom, r.bottom, path.Under( "bottom" ) ) ) return false;
	if ( !CompareVector( l.bottomBorders, r.bottomBorders, "bottomBorders", path, CompareVsoLayer ) ) return false;
	if ( !CompareVector( l.layers, r.layers, "layers", path, CompareVsoLayer ) ) return false;
	if ( l.miniMapCenterColor.color != r.miniMapCenterColor.color ) return path.Fail( "miniMapCenterColor" );
	if ( l.miniMapBorderColor.color != r.miniMapBorderColor.color ) return path.Fail( "miniMapBorderColor" );
	if ( l.nPriority != r.nPriority ) return path.Fail( "nPriority" );
	if ( l.eType != r.eType ) return path.Fail( "eType" );
	if ( l.fPassability != r.fPassability ) return path.Fail( "fPassability" );
	if ( l.szAmbientSound != r.szAmbientSound ) return path.Fail( "szAmbientSound" );
	if ( l.dwAIClasses != r.dwAIClasses ) return path.Fail( "dwAIClasses" );
	if ( l.cSoilParams != r.cSoilParams ) return path.Fail( "cSoilParams" );
	// Then the object half.
	if ( !CompareVector( l.points, r.points, "points", path, CompareVsoPoint ) ) return false;
	if ( !CompareVector( l.controlpoints, r.controlpoints, "controlpoints", path, CompareControlPoint ) ) return false;
	if ( l.nID != r.nID ) return path.Fail( "nID" );
	if ( l.szDescName != r.szDescName ) return path.Fail( "szDescName" );
	return true;
}

bool CompareVsoList( const TVSOList &l, const TVSOList &r, const std::string &szField, const CPath &path )
{
	return CompareVector( l, r, szField, path, CompareVso );
}

bool CompareAltitude( const SVertexAltitude &l, const SVertexAltitude &r, const CPath &path )
{
	if ( l.fHeight != r.fHeight ) return path.Fail( "fHeight" );
	if ( l.shade != r.shade ) return path.Fail( "shade" );
	return true;
}

bool CompareTerrain( const STerrainInfo &l, const STerrainInfo &r, const CPath &path )
{
	if ( l.szTilesetDesc != r.szTilesetDesc ) return path.Fail( "szTilesetDesc" );
	if ( l.szCrossetDesc != r.szCrossetDesc ) return path.Fail( "szCrossetDesc" );
	if ( l.szNoise != r.szNoise ) return path.Fail( "szNoise" );
	// Sizes first: a size mismatch indexed blindly is a crash, not a failed test.
	if ( l.patches.GetSizeX() != r.patches.GetSizeX() || l.patches.GetSizeY() != r.patches.GetSizeY() )
		return path.Fail( "patches.size" );
	for ( int y = 0; y < l.patches.GetSizeY(); ++y )
		for ( int x = 0; x < l.patches.GetSizeX(); ++x )
			if ( !ComparePatch( l.patches[y][x], r.patches[y][x], path.Under( CPath::At( CPath::At( "patches", y ), x ) ) ) )
				return false;
	if ( l.tiles.GetSizeX() != r.tiles.GetSizeX() || l.tiles.GetSizeY() != r.tiles.GetSizeY() )
		return path.Fail( "tiles.size" );
	for ( int y = 0; y < l.tiles.GetSizeY(); ++y )
		for ( int x = 0; x < l.tiles.GetSizeX(); ++x )
			if ( l.tiles[y][x].tile != r.tiles[y][x].tile || l.tiles[y][x].noise != r.tiles[y][x].noise )
				return path.Fail( CPath::At( CPath::At( "tiles", y ), x ) );
	if ( !CompareVsoList( l.rivers, r.rivers, "rivers", path ) ) return false;
	if ( !CompareVsoList( l.roads3, r.roads3, "roads3", path ) ) return false;
	if ( l.altitudes.GetSizeX() != r.altitudes.GetSizeX() || l.altitudes.GetSizeY() != r.altitudes.GetSizeY() )
		return path.Fail( "altitudes.size" );
	for ( int y = 0; y < l.altitudes.GetSizeY(); ++y )
		for ( int x = 0; x < l.altitudes.GetSizeX(); ++x )
			if ( !CompareAltitude( l.altitudes[y][x], r.altitudes[y][x], path.Under( CPath::At( CPath::At( "altitudes", y ), x ) ) ) )
				return false;
	return true;
}

// The packed frame index is compared, never an unpacked one: what is on disk
// is what this tier is about.
bool CompareObject( const SMapObjectInfo &l, const SMapObjectInfo &r, const CPath &path )
{
	if ( l.szName != r.szName ) return path.Fail( "szName" );
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	if ( l.nDir != r.nDir ) return path.Fail( "nDir" );
	if ( l.nPlayer != r.nPlayer ) return path.Fail( "nPlayer" );
	if ( l.nScriptID != r.nScriptID ) return path.Fail( "nScriptID" );
	if ( l.fHP != r.fHP ) return path.Fail( "fHP" );
	if ( l.nFrameIndex != r.nFrameIndex ) return path.Fail( "nFrameIndex" );
	if ( l.link.nLinkID != r.link.nLinkID ) return path.Fail( "link.nLinkID" );
	if ( l.link.bIntention != r.link.bIntention ) return path.Fail( "link.bIntention" );
	if ( l.link.nLinkWith != r.link.nLinkWith ) return path.Fail( "link.nLinkWith" );
	return true;
}

bool CompareEntrenchment( const SEntrenchmentInfo &l, const SEntrenchmentInfo &r, const CPath &path )
{
	if ( l.sections.size() != r.sections.size() ) return path.Fail( "sections.size" );
	for ( size_t i = 0; i < l.sections.size(); ++i )
		if ( !CompareFlatVector( l.sections[i], r.sections[i], CPath::At( "sections", int( i ) ), path ) )
			return false;
	return true;
}

bool CompareIntVector( const std::vector<int> &l, const std::vector<int> &r, const CPath &path )
{
	return CompareFlatVector( l, r, "", path );
}

bool CompareReinforcements( const SReinforcementGroupInfo &l, const SReinforcementGroupInfo &r, const CPath &path )
{
	if ( l.groups.size() != r.groups.size() ) return path.Fail( "groups.size" );
	for ( std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator it = l.groups.begin();
	      it != l.groups.end(); ++it )
	{
		std::unordered_map<int, SReinforcementGroupInfo::SGroupsVector>::const_iterator other = r.groups.find( it->first );
		if ( other == r.groups.end() )
			return path.Fail( CPath::At( "groups", it->first ) );
		if ( !CompareFlatVector( it->second.ids, other->second.ids, CPath::At( "groups", it->first ) + ".ids", path ) )
			return false;
	}
	return true;
}

bool CompareScriptArea( const SScriptArea &l, const SScriptArea &r, const CPath &path )
{
	if ( l.eType != r.eType ) return path.Fail( "eType" );
	if ( !Same( l.center, r.center ) ) return path.Fail( "center" );
	if ( !Same( l.vAABBHalfSize, r.vAABBHalfSize ) ) return path.Fail( "vAABBHalfSize" );
	if ( l.fR != r.fR ) return path.Fail( "fR" );
	if ( l.szName != r.szName ) return path.Fail( "szName" );
	return true;
}

bool CompareAircraft( const SUCAircraft &l, const SUCAircraft &r, const CPath &path )
{
	if ( l.szName != r.szName ) return path.Fail( "szName" );
	if ( l.nFormationSize != r.nFormationSize ) return path.Fail( "nFormationSize" );
	if ( l.nPlanes != r.nPlanes ) return path.Fail( "nPlanes" );
	return true;
}

bool CompareAppearPoint( const CVec3 &l, const CVec3 &r, const CPath &path )
{
	return Same( l, r ) ? true : path.Fail( "" );
}

bool CompareAviation( const SUCAviation &l, const SUCAviation &r, const CPath &path )
{
	if ( !CompareVector( l.aircrafts, r.aircrafts, "aircrafts", path, CompareAircraft ) ) return false;
	if ( l.szParadropSquadName != r.szParadropSquadName ) return path.Fail( "szParadropSquadName" );
	if ( l.nRelaxTime != r.nRelaxTime ) return path.Fail( "nRelaxTime" );
	if ( !CompareSequence( l.vAppearPoints, r.vAppearPoints, "vAppearPoints", path, CompareAppearPoint ) ) return false;
	if ( l.nParadropSquadCount != r.nParadropSquadCount ) return path.Fail( "nParadropSquadCount" );
	return true;
}

bool CompareUnitCreation( const SUnitCreation &l, const SUnitCreation &r, const CPath &path )
{
	if ( !CompareAviation( l.aviation, r.aviation, path.Under( "aviation" ) ) ) return false;
	if ( l.szPartyName != r.szPartyName ) return path.Fail( "szPartyName" );
	return true;
}

bool CompareStartCommand( const SAIStartCommand &l, const SAIStartCommand &r, const CPath &path )
{
	if ( l.cmdType != r.cmdType ) return path.Fail( "cmdType" );
	if ( !CompareFlatVector( l.unitLinkIDs, r.unitLinkIDs, "unitLinkIDs", path ) ) return false;
	if ( l.linkID != r.linkID ) return path.Fail( "linkID" );
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	if ( l.fromExplosion != r.fromExplosion ) return path.Fail( "fromExplosion" );
	if ( l.fNumber != r.fNumber ) return path.Fail( "fNumber" );
	return true;
}

bool CompareBattlePosition( const SBattlePosition &l, const SBattlePosition &r, const CPath &path )
{
	if ( l.nArtilleryLinkID != r.nArtilleryLinkID ) return path.Fail( "nArtilleryLinkID" );
	if ( l.nTruckLinkID != r.nTruckLinkID ) return path.Fail( "nTruckLinkID" );
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	return true;
}

bool CompareMapSound( const SMapSoundInfo &l, const SMapSoundInfo &r, const CPath &path )
{
	if ( l.szName != r.szName ) return path.Fail( "szName" );
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	if ( l.timeRepeat != r.timeRepeat ) return path.Fail( "timeRepeat" );
	if ( l.timeRepeatRandom != r.timeRepeatRandom ) return path.Fail( "timeRepeatRandom" );
	if ( l.nMinRadius != r.nMinRadius ) return path.Fail( "nMinRadius" );
	if ( l.nMaxRadius != r.nMaxRadius ) return path.Fail( "nMaxRadius" );
	if ( l.bMuteDuringCombat != r.bMuteDuringCombat ) return path.Fail( "bMuteDuringCombat" );
	return true;
}

// soundsList holds CMapSoundInfo (Formats/fmtSound.h) - name and position
// only - which is a different struct from the SMapSoundInfo in `sounds`.
bool CompareSoundEntry( const CMapSoundInfo &l, const CMapSoundInfo &r, const CPath &path )
{
	if ( l.szName != r.szName ) return path.Fail( "szName" );
	if ( !Same( l.vPos, r.vPos ) ) return path.Fail( "vPos" );
	return true;
}

bool CompareReinforcePoint( const SAIGeneralParcelInfo::SReinforcePointInfo &l,
                            const SAIGeneralParcelInfo::SReinforcePointInfo &r, const CPath &path )
{
	if ( !Same( l.vCenter, r.vCenter ) ) return path.Fail( "vCenter" );
	if ( l.wDir != r.wDir ) return path.Fail( "wDir" );
	return true;
}

bool CompareParcel( const SAIGeneralParcelInfo &l, const SAIGeneralParcelInfo &r, const CPath &path )
{
	if ( !CompareVector( l.reinforcePoints, r.reinforcePoints, "reinforcePoints", path, CompareReinforcePoint ) ) return false;
	if ( l.eType != r.eType ) return path.Fail( "eType" );
	if ( !Same( l.vCenter, r.vCenter ) ) return path.Fail( "vCenter" );
	if ( l.fRadius != r.fRadius ) return path.Fail( "fRadius" );
	if ( l.wDefenceDirection != r.wDefenceDirection ) return path.Fail( "wDefenceDirection" );
	return true;
}

bool CompareAiSide( const SAIGeneralSideInfo &l, const SAIGeneralSideInfo &r, const CPath &path )
{
	if ( !CompareFlatVector( l.mobileScriptIDs, r.mobileScriptIDs, "mobileScriptIDs", path ) ) return false;
	if ( !CompareVector( l.parcels, r.parcels, "parcels", path, CompareParcel ) ) return false;
	return true;
}

bool CompareCameraAnchor( const CVec3 &l, const CVec3 &r, const CPath &path )
{
	return Same( l, r ) ? true : path.Fail( "" );
}

bool CompareMap( const SLoadMapInfo &l, const SLoadMapInfo &r, const CPath &path )
{
	if ( !CompareTerrain( l.terrain, r.terrain, path.Under( "terrain" ) ) ) return false;
	if ( !CompareVector( l.objects, r.objects, "objects", path, CompareObject ) ) return false;
	if ( !CompareVector( l.scenarioObjects, r.scenarioObjects, "scenarioObjects", path, CompareObject ) ) return false;
	if ( !CompareVector( l.entrenchments, r.entrenchments, "entrenchments", path, CompareEntrenchment ) ) return false;
	if ( l.bridges.size() != r.bridges.size() ) return path.Fail( "bridges.size" );
	for ( size_t i = 0; i < l.bridges.size(); ++i )
		if ( !CompareFlatVector( l.bridges[i], r.bridges[i], CPath::At( "bridges", int( i ) ), path ) )
			return false;
	if ( !CompareReinforcements( l.reinforcements, r.reinforcements, path.Under( "reinforcements" ) ) ) return false;
	if ( l.szScriptFile != r.szScriptFile ) return path.Fail( "szScriptFile" );
	if ( !CompareVector( l.scriptAreas, r.scriptAreas, "scriptAreas", path, CompareScriptArea ) ) return false;
	if ( !Same( l.vCameraAnchor, r.vCameraAnchor ) ) return path.Fail( "vCameraAnchor" );
	if ( !CompareVector( l.playersCameraAnchors, r.playersCameraAnchors, "playersCameraAnchors", path, CompareCameraAnchor ) ) return false;
	if ( l.nSeason != r.nSeason ) return path.Fail( "nSeason" );
	if ( l.szSeasonFolder != r.szSeasonFolder ) return path.Fail( "szSeasonFolder" );
	if ( !CompareFlatVector( l.diplomacies, r.diplomacies, "diplomacies", path ) ) return false;
	if ( !CompareVector( l.unitCreation.units, r.unitCreation.units, "unitCreation.units", path, CompareUnitCreation ) ) return false;
	if ( !CompareSequence( l.startCommandsList, r.startCommandsList, "startCommandsList", path, CompareStartCommand ) ) return false;
	if ( !CompareSequence( l.reservePositionsList, r.reservePositionsList, "reservePositionsList", path, CompareBattlePosition ) ) return false;
	// soundsList is not serialised - CMapInfo::operator& writes `sounds` and
	// derives this - so it is compared for completeness, not for preservation.
	if ( !CompareSequence( l.soundsList, r.soundsList, "soundsList", path, CompareSoundEntry ) ) return false;
	if ( l.szForestCircleSounds != r.szForestCircleSounds ) return path.Fail( "szForestCircleSounds" );
	if ( l.szForestAmbientSounds != r.szForestAmbientSounds ) return path.Fail( "szForestAmbientSounds" );
	if ( l.szChapterName != r.szChapterName ) return path.Fail( "szChapterName" );
	if ( l.nMissionIndex != r.nMissionIndex ) return path.Fail( "nMissionIndex" );
	if ( l.nType != r.nType ) return path.Fail( "nType" );
	if ( l.nAttackingSide != r.nAttackingSide ) return path.Fail( "nAttackingSide" );
	if ( !CompareVector( l.sounds.sounds, r.sounds.sounds, "sounds.sounds", path, CompareMapSound ) ) return false;
	if ( !CompareVector( l.aiGeneralMapInfo.sidesInfo, r.aiGeneralMapInfo.sidesInfo, "aiGeneralMapInfo.sidesInfo", path, CompareAiSide ) ) return false;
	if ( l.szMODName != r.szMODName ) return path.Fail( "szMODName" );
	if ( l.szMODVersion != r.szMODVersion ) return path.Fail( "szMODVersion" );
	return true;
}
}

bool AreEquivalent( const SLoadMapInfo &rLeft, const SLoadMapInfo &rRight, std::string *pWhere )
{
	if ( pWhere )
		pWhere->clear();
	return CompareMap( rLeft, rRight, CPath( pWhere, std::string() ) );
}

bool CompareAltitudeArrays( const STerrainInfo &rLeft, const STerrainInfo &rRight )
{
	if ( rLeft.altitudes.GetSizeX() != rRight.altitudes.GetSizeX() ||
	     rLeft.altitudes.GetSizeY() != rRight.altitudes.GetSizeY() )
		return false;
	for ( int y = 0; y < rLeft.altitudes.GetSizeY(); ++y )
		for ( int x = 0; x < rLeft.altitudes.GetSizeX(); ++x )
			if ( rLeft.altitudes[y][x].fHeight != rRight.altitudes[y][x].fHeight ||
			     rLeft.altitudes[y][x].shade != rRight.altitudes[y][x].shade )
				return false;
	return true;
}

unsigned long LoadMapInfoSize() { return (unsigned long)sizeof( SLoadMapInfo ); }

// The guard. CompareMap above visits every member of SLoadMapInfo; this makes
// that claim enforceable. Add a field and the struct grows, this stops
// compiling, and whoever added it has to teach the comparator about it before
// the preservation tests can pass again. Measured on a 64-bit build
// (aarch64-macos, 2026-09-21); the test prints the live value as
// "map-file: sizeof(SLoadMapInfo)=N" so a change is one build away from an
// answer. 32-bit builds are not asserted: the tier's targets are all 64-bit,
// and a second number would be a second thing to keep true.
#if defined(__LP64__) || defined(_WIN64)
static_assert( sizeof( SLoadMapInfo ) == 760,
               "SLoadMapInfo changed: add the new field to CompareMap in this file, then update this size" );
#endif
}
