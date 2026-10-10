#include "StdAfx.h"

#include "Cheats.h"
#include "Diplomacy.h"
#include "../zlib/zlib.h"
SCheats theCheats;
extern CDiplomacy theDipl;
void SCheats::ResetFlags()
{
	immortals.clear();
	immortals.resize( SAIConsts::MAX_NUM_OF_PLAYERS + 1, 0 );
	firstShoot.clear();
	firstShoot.resize( SAIConsts::MAX_NUM_OF_PLAYERS + 1, 0 );

	bWarFog = true;
	bLoadObjects = true;
	nPartyForWarFog = 0;
	bTurnOffWarFog = false;

	bPasswordOK = false;
}
void SCheats::Init()
{
	ResetFlags();
	bPasswordOK = ( GetGlobalVar( "EnableCheats", 0 ) == 1 );
}
// theCheats is built while the module loads, before its GlobalsLoader has necessarily set
// g_pGlobalSingleton: static constructors in different files run in no fixed order, and Zig
// 0.17's Mach-O linker runs this one first. So the constructor reads no global variables;
// CAILogic::CommonInit calls Init() before every map, which reads EnableCheats.
SCheats::SCheats()
{
	ResetFlags();
}
void SCheats::SetWarFog( bool _bWarFog )
{
	if ( !theDipl.IsNetGame() && bPasswordOK )
		bWarFog = _bWarFog;
}
void SCheats::SetNPartyForWarFog( const int _nPartyForWarFog, bool bUnconditionly )
{
	if ( !theDipl.IsNetGame() && bPasswordOK || bUnconditionly )
		nPartyForWarFog = _nPartyForWarFog;
}
void SCheats::SetLoadObjects( bool _bLoadObjects )
{
	if ( !theDipl.IsNetGame() && bPasswordOK )
		bLoadObjects = _bLoadObjects;
}
void SCheats::SetTurnOffWarFog( bool _bTurnOffWarFog )
{
	if ( !theDipl.IsNetGame() && bPasswordOK )
		bTurnOffWarFog = _bTurnOffWarFog;
}
void SCheats::SetImmortals( const int nParty, const BYTE cValue )
{
	if ( !theDipl.IsNetGame() && bPasswordOK )
		immortals[nParty] = cValue;
}
void SCheats::SetFirstShoot( const int nParty, const BYTE cValue )
{
	if ( !theDipl.IsNetGame() && bPasswordOK )
		firstShoot[nParty] = cValue;
}
int SCheats::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &bWarFog );
	saver.Add( 2, &nPartyForWarFog );
	saver.Add( 3, &bLoadObjects );
	saver.Add( 4, &immortals );
	saver.Add( 5, &firstShoot );
	saver.Add( 6, &bTurnOffWarFog );
	saver.Add( 7, &bHistoryPlaying );

	return 0;
}
const int s_nKey2Length = 20;
BYTE s_cKey2[s_nKey2Length] = { 151, 186, 179, 161, 73, 225, 127, 233, 147, 69, 6, 46, 90, 162, 2, 30, 101, 251, 13, 48 };
uLong ulPass = 3702409162;
const unsigned long SCheats::MakeCheckSum( const std::string &_szPassword )
{
/*	
	std::string szPassword = "Password";

	std::vector<BYTE> checksum;
	checksum.reserve( 100 );
	checksum.insert( checksum.end(), szPassword.begin(), szPassword.end() );
	checksum.insert( checksum.end(), s_cKey2, s_cKey2 + s_nKey2Length );
	const uLong uCheckSum = crc32( 0L, &(checksum[0]), checksum.size() );

	return uCheckSum;
*/
	return 0;
}
void SCheats::CheckPassword( const std::string &szPassword )
{
	std::vector<BYTE> checksum;
	checksum.reserve( 100 );
	checksum.insert( checksum.end(), szPassword.begin(), szPassword.end() );
	checksum.insert( checksum.end(), s_cKey2, s_cKey2 + s_nKey2Length );
	const uLong uCheckSum = crc32( 0L, &(checksum[0]), checksum.size() );

	bPasswordOK = ( uCheckSum == ulPass );
}
