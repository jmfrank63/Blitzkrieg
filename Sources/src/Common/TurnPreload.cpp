#include "StdAfx.h"

#include "TurnPreload.h"
#include "../Main/TurnTrace.h"

#include "../Anim/Animation.h"
#include "../GFX/GFX.H"
// The answers of the mission so far. The storage's tree does not change
// during a mission (a mod is chosen in the menus), so an answer holds until
// Clear. Outside a Game mission (the Map Editor) the storage answers itself.
static bool bActive = false;
static std::unordered_map<std::string, bool> exists;
static std::unordered_map<std::string, bool> warmed;
bool NTurnPreload::Exists( const std::string &szName )
{
	if ( !bActive )
		return GetSingleton<IDataStorage>()->IsStreamExist( szName.c_str() );
	std::unordered_map<std::string, bool>::const_iterator pos = exists.find( szName );
	if ( pos != exists.end() )
		return pos->second;
	const bool bExists = GetSingleton<IDataStorage>()->IsStreamExist( szName.c_str() );
	exists[szName] = bExists;
	return bExists;
}
bool NTurnPreload::Warm( const std::string &szPack )
{
	if ( !bActive )
		return false;
	std::unordered_map<std::string, bool>::const_iterator pos = warmed.find( szPack );
	if ( pos != warmed.end() )
		return pos->second;
	// The animation is a new object over the shared pack: dropped at once,
	// the pack stays in the animation manager's share.
	CPtr<ISpriteAnimation> pAnimation = GetSingleton<IAnimationManager>()->GetSpriteAnimation( (szPack + ".san").c_str() );
	if ( pAnimation != 0 )
		GetSingleton<ITextureManager>()->GetTexture( szPack.c_str() );
	warmed[szPack] = pAnimation != 0;
	return pAnimation != 0;
}
bool NTurnPreload::Missing( const std::string &szPack )
{
	std::unordered_map<std::string, bool>::const_iterator pos = warmed.find( szPack );
	return pos != warmed.end() && !pos->second;
}
static int nDrawn = 0;
static unsigned int nDrawnSum = 0;
void NTurnPreload::NoteDrawn( const std::string &szPack )
{
	if ( !NTurnTrace::On() )
		return;
	unsigned int nHash = 2166136261u;							// FNV-1a of the name, case folded
	for ( int i = 0; i < int( szPack.size() ); ++i )
		nHash = ( nHash ^ (unsigned char)tolower( (unsigned char)szPack[i] ) ) * 16777619u;
	++nDrawn;
	nDrawnSum += nHash;
}
void NTurnPreload::TakeDrawn( int *pnCount, unsigned int *pnSum )
{
	*pnCount = nDrawn;
	*pnSum = nDrawnSum;
	nDrawn = 0;
	nDrawnSum = 0;
}
void NTurnPreload::Begin()
{
	bActive = true;
}
void NTurnPreload::Clear()
{
	bActive = false;
	// Swapped, not cleared: a cleared hash keeps its buckets.
	std::unordered_map<std::string, bool>().swap( exists );
	std::unordered_map<std::string, bool>().swap( warmed );
}
