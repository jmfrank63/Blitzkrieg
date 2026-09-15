#ifndef __MINIMAPCREATION_H__
#define __MINIMAPCREATION_H__
#pragma ONCE
// Map pictures the game generates for itself. The original regenerated a
// map's shipped pictures into Data whenever the map was newer than them - an
// edited map, or a mod map shipped without pictures. That fails on a protected
// installation and writes over assets that are not the game's to change, so a
// generated picture now lives in the user's cache instead:
// <user cache>\minimaps\<mod or base>\<map>_<revision>_u.dds, 512x512,
// uncompressed, keyed by a hash of the map file. The Windows editor keeps its
// own copy of this file and still authors the pictures in Data.
class CMinimapCreation
{
public:
	// Whether the map is newer than the picture it ships with, or ships none.
	static bool IsShippedImageStale( const std::string &szTerrainName, const std::string &szImageName );
	// The cached picture of the map's current source revision, generated from
	// the map data when there is none yet. A full path, or empty when the map
	// cannot be read or the cache cannot be written.
	static std::string GetCachedMapImage( const std::string &szTerrainName );
	// What a briefing or multiplayer screen shows for a map: the cached picture
	// when the shipped image is out of date, otherwise the shipped image. Null
	// only when neither exists.
	static CPtr<interface IGFXTexture> GetMapImageTexture( const std::string &szTerrainName, const std::string &szImageName );
};
#endif // __MINIMAPCREATION_H__
