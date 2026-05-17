#include "StdAfx.h"

#include "RPGStats.h"
#include "..\AILogic\AIConsts.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Common\Actions.h"
#include "..\Misc\CheckSums.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// преобразовать из человеческих единиц в AI
bool SWeaponRPGStats::SShell::ToAIUnits()
{
	// метры <=> AI точки
	fArea *= float( SAIConsts::TILE_SIZE );
	fArea2 *= float( SAIConsts::TILE_SIZE );
	// метры/секунду <=> AI точки/тик
	fSpeed *= float( SAIConsts::TILE_SIZE ) / 1000.0f;
	// пули/минуту <=> ticks между вылетами пуль в очереди
	nFireRate = int( 60000.0f / fFireRate );
	// секунды <=> ticks
	nRelaxTime = int( fRelaxTime * 1000.0f );
	// [0..100] <=> [0..1]
	fBrokeTrackProbability *= 0.01f;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SWeaponRPGStats::ToAIUnits() 
{ 
	SCommonRPGStats::ToAIUnits();
	// секунды <=> ticks
	nAimingTime = int( fAimingTime * 1000.0f );
	// метры <=> AI точки
	fDispersion *= float( SAIConsts::TILE_SIZE );
	fRangeMax *= float( SAIConsts::TILE_SIZE );
	fRangeMin *= float( SAIConsts::TILE_SIZE );
	fRevealRadius *= float( SAIConsts::TILE_SIZE );
	// градусы <=> градусы65535
	wDeltaAngle = ( DWORD( float( wDeltaAngle / 2 ) * (65536.0f / 360.0f) ) ) % 65536;
	// shell types
	std::for_each( shells.begin(), shells.end(), [](SShell& shell){ shell.ToAIUnits(); } );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SWeaponRPGStats::SShell::SShell()
: specials( 32 )
{
	eDamageType = DAMAGE_HEALTH;
	trajectory = TRAJECTORY_LINE;
	nPiercing = 1;
	nPiercingRandom = 1;
	fDamagePower = 1;
	nDamageRandom = 1;
	fArea = 1.0f;
	fArea2 = 2.0f;
	fSpeed = 1000.0f;
	fDetonationPower = 0.0f;
	fFireRate = 10;
	fRelaxTime = 10;
	fTraceSpeedCoeff = 0.2f;
	fTraceProbability = 0.0f;
	fBrokeTrackProbability = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SWeaponRPGStats::SWeaponRPGStats() 
: SCommonRPGStats( "Weapon" ), shells( 1 ), fRevealRadius( 0.0f )
{
	fDispersion = 1;
	nAmmoPerBurst = 1;
	fRangeMax = 30;
	fRangeMin = 1;
	//
	nCeiling = 100;
	//
	wDeltaAngle = 0;

	//
	fAimingTime = 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGStatsAutomagic::CRPGStatsAutomagic()
{
	// infantry
	i2s[RPG_TYPE_SOLDIER] = "soldier";
	i2s[RPG_TYPE_ENGINEER] = "engineer";
	i2s[RPG_TYPE_SNIPER] = "sniper";
	i2s[RPG_TYPE_OFFICER] = "officer";
	// transport
	i2s[RPG_TYPE_TRN_CARRIER] = "trn_carrier";
	i2s[RPG_TYPE_TRN_SUPPORT] = "trn_support";
	i2s[RPG_TYPE_TRN_MEDICINE] = "trn_medicine";
	i2s[RPG_TYPE_TRN_TRACTOR] = "trn_tractor";
	i2s[RPG_TYPE_TRN_MILITARY_AUTO] = "trn_military_auto";
	i2s[RPG_TYPE_TRN_CIVILIAN_AUTO] = "trn_civilian_auto";
	// artillery
	i2s[RPG_TYPE_ART_GUN] = "art_gun";
	i2s[RPG_TYPE_ART_HOWITZER] = "art_howitzer";
	i2s[RPG_TYPE_ART_HEAVY_GUN] = "art_heavy_gun";
	i2s[RPG_TYPE_ART_AAGUN] = "art_aagun";
	i2s[RPG_TYPE_ART_ROCKET] = "art_rocket";
	i2s[RPG_TYPE_ART_SUPER] = "art_super";
	i2s[RPG_TYPE_ART_MORTAR] = "art_mortar";
	i2s[RPG_TYPE_ART_HEAVY_MG] = "art_heavy_mg";
	// SPG
	i2s[RPG_TYPE_SPG_ASSAULT] = "spg_assault";
	i2s[RPG_TYPE_SPG_ANTITANK] = "spg_antitank";
	i2s[RPG_TYPE_SPG_SUPER] = "spg_super";
	i2s[RPG_TYPE_SPG_AAGUN] = "spg_aagun";
	// armor
	i2s[RPG_TYPE_ARM_LIGHT] = "arm_light";
	i2s[RPG_TYPE_ARM_MEDIUM] = "arm_medium";
	i2s[RPG_TYPE_ARM_SUPER] = "arm_super";
	i2s[RPG_TYPE_ARM_HEAVY] = "arm_heavy";
	// aviation
	i2s[RPG_TYPE_AVIA_SCOUT] = "avia_scout";
	i2s[RPG_TYPE_AVIA_BOMBER] = "avia_bomber";
	i2s[RPG_TYPE_AVIA_ATTACK] = "avia_attack";
	i2s[RPG_TYPE_AVIA_FIGHTER] = "avia_fighter";
	i2s[RPG_TYPE_AVIA_SUPER] = "avia_super";
	i2s[RPG_TYPE_AVIA_LANDER] = "avia_lander";
	// train
	i2s[RPG_TYPE_TRAIN_LOCOMOTIVE] = "train_locomotive";
	i2s[RPG_TYPE_TRAIN_CARGO] = "train_cargo";
	i2s[RPG_TYPE_TRAIN_CARRIER] = "train_carrier";
	i2s[RPG_TYPE_TRAIN_SUPER] = "train_super";
	i2s[RPG_TYPE_TRAIN_ARMOR] = "train_armor";
	//
	// AI classes
	i2s[AI_CLASS_WHEEL] = "wheel";
	i2s[AI_CLASS_HALFTRACK] = "half-track";
	i2s[AI_CLASS_TRACK] = "track";
	i2s[AI_CLASS_HUMAN] = "human";
	//
	//
	for ( CI2SMap::const_iterator it = i2s.begin(); it != i2s.end(); ++it ) 
		s2i[it->second] = it->first;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CRPGStatsAutomagic::GetFirstStr() const
{
	return s2i.begin()->first.c_str();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CRPGStatsAutomagic::GetFirstInt() const
{
	return i2s.begin()->first;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRPGStatsAutomagic::IsLastStr( const char* pszVal ) const
{
	CS2IMap::const_iterator iter = s2i.find( pszVal );
	return iter == s2i.end() || ++iter == s2i.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRPGStatsAutomagic::IsLastInt( const int nVal ) const
{
	CI2SMap::const_iterator iter = i2s.find( nVal );
	return iter == i2s.end() || ++iter == i2s.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CRPGStatsAutomagic::GetNextStr( const char* pszVal )
{
	if ( IsLastStr( pszVal ) )
		return szUnknown.c_str();
	else
	{
		CS2IMap::const_iterator iter = s2i.find( pszVal );
		return (++iter)->first.c_str();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CRPGStatsAutomagic::GetNextInt( const int nVal )
{
	if ( IsLastInt( nVal ) )
		return -1;
	else
	{
		CI2SMap::const_iterator iter = i2s.find( nVal );
		return (++iter)->first;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CPtr<IRPGStatsAutomagic> pAutomagic = new CRPGStatsAutomagic();
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NCheckSums;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** stats...
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SCommonRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "KeyName", &szKeyName );
	saver.Add( "StatsType", &szStatsType );

	if ( saver.IsReading() )
		bCheckSumInitialized = false;

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SDefenseRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "MinArmor", &nArmorMin );
	saver.Add( "MaxArmor", &nArmorMax );
	saver.Add( "Silhouette", &fSilhouette );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SHPObjectRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, fMaxHP );

	for ( int i = 0; i < damagedHPs.size(); ++i )
		CopyToBuf( &buf, damagedHPs[i] );

	CopyToBuf( &buf, fRepairCost );

	for ( int i = 0; i < 6; ++i )
		CopyToBuf( &buf, defences[i] );

	const uLong result = crc32( 0L, &(buf.buf[0]), buf.nCnt );

	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SHPObjectRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SCommonRPGStats*>(this) );
	saver.Add( "MaxHP", &fMaxHP );
	saver.Add( "DamagedHPs", &damagedHPs );
	saver.Add( "RepairCost", &fRepairCost );
	saver.Add( "Defence0", &defences[0] );
	saver.Add( "Defence1", &defences[1] );
	saver.Add( "Defence2", &defences[2] );
	saver.Add( "Defence3", &defences[3] );
	saver.Add( "Defence4", &defences[4] );
	saver.Add( "Defence5", &defences[5] );
	if ( saver.IsReading() && ( (fMaxHP < 1) || (fabs(fMaxHP) > 10000) ) )
		fMaxHP = 100;
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SHPObjectRPGStats::ToAIUnits()
{
	SCommonRPGStats::ToAIUnits();

	fRepairCost *= GetGlobalVar( "RepairCostAdjust", 1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base static object
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SStaticObjectRPGStats::SStaticObjectRPGStats( const char *pszType ) 
: SHPObjectRPGStats( pszType ), dwAIClasses( 0 ) 
{
  bBurn = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SStaticObjectRPGStats::ToAIUnits()
{
	SHPObjectRPGStats::ToAIUnits();
	dwAIClasses ^= AI_CLASS_ANY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SStaticObjectRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.AddTypedSuper( static_cast<SHPObjectRPGStats*>(this) );
	saver.Add( "AIClasses", &dwAIClasses );
	saver.Add( "Burn", &bBurn );
	saver.Add( "EffectExplosion", &szEffectExplosion );
	saver.Add( "EffectDeath", &szEffectDeath );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SStaticObjectRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, dwAIClasses );
	CopyToBuf( &buf, bBurn );

	const uLong result = crc32( SHPObjectRPGStats::CalculateCheckSum(), &(buf.buf[0]), buf.nCnt );

	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base gun stats
// **
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SWeaponRPGStats::SShell::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, eDamageType );
	CopyToBuf( &buf, nPiercing );
	CopyToBuf( &buf, nPiercingRandom );
	CopyToBuf( &buf, fDamagePower );
	CopyToBuf( &buf, nDamageRandom );
	CopyToBuf( &buf, fArea );
	CopyToBuf( &buf, fArea2 );
	CopyToBuf( &buf, fSpeed );
	CopyToBuf( &buf, fDetonationPower );
	CopyToBuf( &buf, trajectory );
	CopyToBuf( &buf, fFireRate );
	CopyToBuf( &buf, fRelaxTime );
	for ( int i = 0; i < specials.GetSize(); ++i )
		CopyToBuf( &buf, specials.GetData( i ) );

	const uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SWeaponRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, fDispersion );
	CopyToBuf( &buf, fAimingTime );
	CopyToBuf( &buf, nAmmoPerBurst );
	CopyToBuf( &buf, fRangeMax );
	CopyToBuf( &buf, fRangeMin );
	CopyToBuf( &buf, nCeiling );
	CopyToBuf( &buf, fRevealRadius );
	CopyToBuf( &buf, wDeltaAngle );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( std::vector<SShell>::const_iterator iter = shells.begin(); iter != shells.end(); ++iter )
		checkSum = GetCRC( iter->CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SBaseGunRPGStats::SBaseGunRPGStats()
: szWeapon( "generic" )
{
	pWeapon = 0;
	nAmmo = 20;
	fReloadCost = 1;
	wDirection = 0;
	nPriority = 1;
	bPrimary = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SBaseGunRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	pWeapon = static_cast<const SWeaponRPGStats*>( pGDB->GetAddStats( szWeapon.c_str(), IObjectsDB::WEAPON ) );
	return pWeapon != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBaseGunRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Weapon", &szWeapon );
	saver.Add( "Priority", &nPriority );
	saver.Add( "Ammo", &nAmmo );
	saver.Add( "Direction", &wDirection );
	saver.Add( "ReloadCost", &fReloadCost );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SBaseGunRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, nPriority );
	CopyToBuf( &buf, bPrimary );	
	CopyToBuf( &buf, nAmmo );
	CopyToBuf( &buf, wDirection );
	CopyToBuf( &buf, fReloadCost );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	if ( pWeapon )
		checkSum = GetCRC( pWeapon->CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** terrain objects set (mesh)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool STerraObjSetRPGStats::SSegment::ToAIUnits()
{
	Vis2AI( &vOrigin );
	Vis2AI( &vVisOrigin );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong STerraObjSetRPGStats::SSegment::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < passability.GetSizeY(); ++i )
	{
		for ( int j = 0; j < passability.GetSizeX(); ++j )
			CopyToBuf( &buf, passability[i][j] );
	}
	CopyToBuf( &buf, vOrigin );

	for ( int i = 0; i < visibility.GetSizeY(); ++i )
	{
		for ( int j = 0; j < visibility.GetSizeX(); ++j )
			CopyToBuf( &buf, visibility[i][j] );
	}
	CopyToBuf( &buf, vVisOrigin );
	
	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STerraObjSetRPGStats::SSegment::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Passability", &passability );
	saver.Add( "Origin", &vOrigin );
	saver.Add( "Visibility", &visibility );
	saver.Add( "VisOrigin", &vVisOrigin );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STerraObjSetRPGStats::ToAIUnits()
{
	SStaticObjectRPGStats::ToAIUnits();
	std::for_each( segments.begin(), segments.end(), [](SSegment& seg){ seg.ToAIUnits(); } );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STerraObjSetRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Segments", &segments );
	saver.Add( "Fronts", &fronts );
	saver.Add( "Backs", &backs );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong STerraObjSetRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < fronts.size(); ++i )
		CopyToBuf( &buf, fronts[i] );
	for ( int i = 0; i < backs.size(); ++i )
		CopyToBuf( &buf, backs[i] );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < segments.size(); ++i )
		checkSum = GetCRC( segments[i].CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SStaticObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** объекты (не-юниты)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SObjectBaseRPGStats::SObjectBaseRPGStats( const char *pszType ) 
: SStaticObjectRPGStats( pszType ) 
{
	vOrigin = VNULL2;
	vVisOrigin = VNULL2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SObjectBaseRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SStaticObjectRPGStats*>(this) );
	saver.Add( "passability", &passability );
	saver.Add( "origin", &vOrigin );
	saver.Add( "VisOrigin", &vVisOrigin );
	saver.Add( "visibility", &visibility );
	saver.Add( "CycledSound", &szCycledSound );
	saver.Add( "AmbientSound", &szAmbientSound );
	// don't load wrong origin - reset it to 0
	if ( saver.IsReading() )
	{
		if ( fabs2(vOrigin.x) > 1e8 )
			vOrigin.x = 0;
		if ( fabs2(vOrigin.y) > 1e8 )
			vOrigin.y = 0;
		if ( fabs2(vVisOrigin.x) > 1e8 )
			vVisOrigin.x = 0;
		if ( fabs2(vVisOrigin.y) > 1e8 )
			vVisOrigin.y = 0;
	}
	return 0;
}
void SObjectBaseRPGStats::ToAIUnits()
{
	SStaticObjectRPGStats::ToAIUnits();
	Vis2AI( &vOrigin );
	Vis2AI( &vVisOrigin );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SObjectRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SObjectBaseRPGStats*>(this) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SObjectBaseRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < passability.GetSizeY(); ++i )
	{
		for ( int j = 0; j < passability.GetSizeX(); ++j )
			CopyToBuf( &buf, passability[i][j] );
	}
	CopyToBuf( &buf, vOrigin );

	for ( int i = 0; i < visibility.GetSizeY(); ++i )
	{
		for ( int j = 0; j < visibility.GetSizeX(); ++j )
			CopyToBuf( &buf, visibility[i][j] );
	}
	CopyToBuf( &buf, vVisOrigin );
	
	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	checkSum = GetCRC( SStaticObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** building
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SBuildingRPGStats::SEntrance::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, vPos );
	CopyToBuf( &buf, bStormable );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SBuildingRPGStats::SSlot::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, vPos );
	CopyToBuf( &buf, fDirection );
	CopyToBuf( &buf, fAngle );
	CopyToBuf( &buf, fSightMultiplier );
	CopyToBuf( &buf, fCoverage );
	CopyToBuf( &buf, fRotationSpeed );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	checkSum = GetCRC( gun.CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SBuildingRPGStats::SFirePoint::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, vPos );
	CopyToBuf( &buf, fDirection );
	CopyToBuf( &buf, fVerticalAngle );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SBuildingRPGStats::SBuildingRPGStats() 
: SObjectBaseRPGStats( "Building" ), dirExplosions( 5 )
{  
	nRestSlots = 0;
	nMedicalSlots = 0;
	pPrimaryGun = 0;
	//
	eType = TYPE_BULDING;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SBuildingRPGStats::Validate()
{
	NI_ASSERT_SLOW_TF( nRestSlots <= 100, NStr::Format("Wrong number of rest slots (%d) in \"%s\"", nRestSlots, szKeyName.c_str()), return false );
	NI_ASSERT_SLOW_TF( nMedicalSlots <= 100, NStr::Format("Wrong number of medical slots (%d) in \"%s\"", nMedicalSlots, szKeyName.c_str()), return false );
	NI_ASSERT_SLOW_TF( slots.size() <= 100, NStr::Format("Wrong number of fireplaces (%d) in \"%s\"", slots.size(), szKeyName.c_str()), return false );
	//
	std::for_each( slots.begin(), slots.end(), [](SSlot& slot){ slot.Validate(); } );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SBuildingRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, eType );
	CopyToBuf( &buf, nRestSlots );
	CopyToBuf( &buf, nMedicalSlots );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	if ( pPrimaryGun )
		checkSum = GetCRC( pPrimaryGun->CalculateCheckSum(), checkSum );
	for ( int i = 0; i < slots.size(); ++i )
		checkSum = GetCRC( slots[i].CalculateCheckSum(), checkSum );
	for ( int i = 0; i < entrances.size(); ++i )
		checkSum = GetCRC( entrances[i].CalculateCheckSum(), checkSum );
	for ( int i = 0; i < firePoints.size(); ++i )
		checkSum = GetCRC( firePoints[i].CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SObjectBaseRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*									Entrance of a building													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildingRPGStats::SEntrance::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Stormable", &bStormable );
	return 0;
}
bool SBuildingRPGStats::SEntrance::ToAIUnits()
{
	Vis2AI( &vPos );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												Slot of a builing													*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildingRPGStats::SSlot::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &fDirection );
	saver.Add( "Angle", &fAngle );
	saver.Add( "SightMultiplier", &fSightMultiplier );
	saver.Add( "Coverage", &fCoverage );
	saver.Add( "Weapon", &gun.szWeapon );
	saver.Add( "Ammo", &gun.nAmmo );
	saver.Add( "GunPriority", &gun.nPriority );
	saver.Add( "RotationSpeed", &fRotationSpeed );
	// vis info
	saver.Add( "BeforeSprite", &bBeforeSprite );
	saver.Add( "ShowFlashes", &bShowFlashes );
	saver.Add( "PicturePosition", &vPicturePosition );
	saver.Add( "WorldPosition", &vWorldPosition );
	// CRAP{ disable "generic" weapon - тяжёлое наследие "автоматических" статсов
	if ( saver.IsReading() && (gun.szWeapon == "generic") ) 
		gun.szWeapon.clear();
	// CRAP}
	// CRAP{ выпрямляем кривые статсы
	if ( (fSightMultiplier < 0) || (fSightMultiplier > 3) )
		fSightMultiplier = 1;
	// CRAP}
	return 0;
}
SBuildingRPGStats::SSlot::SSlot()
{
	vPos = VNULL3;
	fDirection = 0;
	fAngle = 30;
	fSightMultiplier = 1.0f;
	fCoverage = 1.0f;
	gun.szWeapon.clear();
	gun.pWeapon = 0;
	gun.nAmmo = 0;
	gun.nPriority = 1;
	fRotationSpeed = 0;
	bBeforeSprite = true;
	bShowFlashes = true;
	vPicturePosition = VNULL2;
	vWorldPosition = VNULL3;
}
bool SBuildingRPGStats::SSlot::ToAIUnits()
{
	Vis2AI( &vPos );
	//
	CQuat quat = CQuat( -FP_PI2, V3_AXIS_X );
	quat *= CQuat( ToRadian(fDirection), V3_AXIS_Z );
	matDirection.Set( quat );
	//	
	wDirection = WORD( fDirection * 65536.0f / 360.0f );
	wAngle = WORD( fAngle * 32768.0f / 360.0f );
	fRotationSpeed = 1.0f / fRotationSpeed * 65535.0f / 1000.0f;
	wRotationSpeed = fRotationSpeed <= 65535.0f ? WORD( fRotationSpeed ) : 65535;

	return true;
}
bool SBuildingRPGStats::SSlot::Validate()
{
	NI_ASSERT_SLOW_TF( (fSightMultiplier >= 0) && (fSightMultiplier <= 3), NStr::Format("sight multiplier (%g) for fireplace in building must be in range [0..3]", fSightMultiplier), return false );
	return true;
}
bool SBuildingRPGStats::SSlot::RetrieveShortcuts( IObjectsDB *pGDB )
{
	if ( !gun.szWeapon.empty() )
		gun.pWeapon = static_cast<const SWeaponRPGStats*>( pGDB->GetAddStats( gun.szWeapon.c_str(), IObjectsDB::WEAPON ) );
	else
		gun.pWeapon = 0;
	return true;
}
void SBuildingRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	// retrieve weapon shortcut and find min and max gun priority
	int nMin = 1000000000, nMax = -1000000000;
	for ( std::vector<SSlot>::iterator it = slots.begin(); it != slots.end(); ++it )
	{
		it->RetrieveShortcuts( pGDB );
		nMin = Min( nMin, it->gun.nPriority );
		nMax = Max( nMax, it->gun.nPriority );
	}
	// find first primary gun
	for ( std::vector<SSlot>::const_iterator it = slots.begin(); it != slots.end(); ++it )
	{
		if ( (it->gun.nPriority == nMin) && (it->gun.pWeapon != 0) )
		{
			pPrimaryGun = &( it->gun );
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													Fire point															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildingRPGStats::SFirePoint::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &fDirection );
	saver.Add( "VerticalAngle", &fVerticalAngle );
	saver.Add( "FireEffect", &szFireEffect );
	// vis info
	saver.Add( "PicturePosition", &vPicturePosition );
	saver.Add( "WorldPosition", &vWorldPosition );
	return 0;
}
SBuildingRPGStats::SFirePoint::SFirePoint() 
: vPos( VNULL3 ), vPicturePosition( VNULL2 ), vWorldPosition( VNULL3 )
{
	fDirection = 0;
	fVerticalAngle = 78.0f;
}
bool SBuildingRPGStats::SFirePoint::ToAIUnits()
{
	fDirection = ToRadian( fDirection );
	fVerticalAngle = ToRadian( fVerticalAngle );
	Vis2AI( &vPos );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													Direction explosion											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildingRPGStats::SDirectionExplosion::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &fDirection );
	saver.Add( "VerticalAngle", &fVerticalAngle );
	// vis info
	saver.Add( "PicturePosition", &vPicturePosition );
	saver.Add( "WorldPosition", &vWorldPosition );
	return 0;
}
SBuildingRPGStats::SDirectionExplosion::SDirectionExplosion() 
: vPos( VNULL3 ), vPicturePosition( VNULL2 ), vWorldPosition( VNULL3 )
{
	fDirection = 0;
	fVerticalAngle = 78.0f;
}
bool SBuildingRPGStats::SDirectionExplosion::ToAIUnits()
{
	fDirection = ToRadian( fDirection );
	fVerticalAngle = ToRadian( fVerticalAngle );
	Vis2AI( &vPos );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  Building																*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBuildingRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SObjectBaseRPGStats*>(this) );
	saver.Add( "BuildingType", &eType );
	saver.Add( "RestSlots", &nRestSlots );
	saver.Add( "MedicalSlots", &nMedicalSlots );
	//
	saver.Add( "FireSlots", &slots );
	saver.Add( "Entrances", &entrances );
	saver.Add( "FirePoints", &firePoints );
	saver.Add( "SmokePoints", &smokePoints );
	saver.Add( "SmokeEffect", &szSmokeEffect );
	saver.Add( "DirExplosions", &dirExplosions );
	saver.Add( "DirExplosionEffect", &szDirExplosionEffect );
	//ambient sounds
	saver.Add( "AmbientSound", &szAmbientSound );
	//
	// CRAP{ выправим кривые данные
	if ( nRestSlots > 100 )
		nRestSlots = 0;
	if ( nMedicalSlots > 100 )
		nMedicalSlots = 0;
	// CRAP}
	return 0;
}
void SBuildingRPGStats::ToAIUnits()
{
	SObjectBaseRPGStats::ToAIUnits();
	//
	std::for_each( entrances.begin(), entrances.end(), [](SEntrance& ent){ ent.ToAIUnits(); } );
	std::for_each( slots.begin(), slots.end(), [](SSlot& slot){ slot.ToAIUnits(); } );
	std::for_each( firePoints.begin(), firePoints.end(), [](SFirePoint& fp){ fp.ToAIUnits(); } );
	std::for_each( smokePoints.begin(), smokePoints.end(), [](SFirePoint& fp){ fp.ToAIUnits(); } );
	std::for_each( dirExplosions.begin(), dirExplosions.end(), [](SDirectionExplosion& de){ de.ToAIUnits(); } );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** описание оружия
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SWeaponRPGStats::SShell::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	int nTrajectoryType = trajectory;
	saver.Add( "TrajectoryType", &nTrajectoryType );
	if ( saver.IsReading() )
		trajectory = SWeaponRPGStats::SShell::ETrajectoryType( nTrajectoryType );
	//
	saver.Add( "Piercing", &nPiercing );
	saver.Add( "PiercingRandom", &nPiercingRandom );
	saver.Add( "DamagePower", &fDamagePower );
	saver.Add( "DamageRandom", &nDamageRandom );
	saver.Add( "Area", &fArea );
	saver.Add( "Area2", &fArea2 );
	saver.Add( "ProjectileSpeed", &fSpeed );
	saver.Add( "DetonationPower", &fDetonationPower );
	saver.Add( "Specials", &specials );
	saver.Add( "BrokeTrackProbability", &fBrokeTrackProbability );
	// параметры для визуализации и озвучивания эффектов
	saver.Add( "InfantryFireSound", &szFireSound );
	saver.Add( "EffectGunFire", &szEffectGunFire );
	saver.Add( "EffectTrajectory", &szEffectTrajectory );
	saver.Add( "EffectHitDirect", &szEffectHitDirect );
	saver.Add( "EffectHitMiss", &szEffectHitMiss );
	saver.Add( "EffectHitReflect", &szEffectHitReflect );
	saver.Add( "EffectHitGround", &szEffectHitGround );
	saver.Add( "EffectHitWater", &szEffectHitWater );
	saver.Add( "EffectHitAir", &szEffectHitAir );
	saver.Add( "Craters", &szCraters );
	saver.Add( "FlashFire", &flashFire );
	saver.Add( "FlashExplosion", &flashExplosion );
	saver.Add( "FireRate", &fFireRate );
	saver.Add( "RelaxTime", &fRelaxTime );
	int nDamageType = eDamageType ;
	saver.Add( "DamageType", &nDamageType );
	saver.Add( "TraceProbability", &fTraceProbability );
	saver.Add( "TraceSpeedCoeff", &fTraceSpeedCoeff );
	if ( saver.IsReading() )
		eDamageType = static_cast<SWeaponRPGStats::SShell::EDamageType>( nDamageType );
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SWeaponRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.AddTypedSuper( static_cast<SCommonRPGStats*>(this) );
	saver.Add( "Dispersion", &fDispersion );
	saver.Add( "AimingTime", &fAimingTime );
	saver.Add( "AmmoPerBurst", &nAmmoPerBurst );
	saver.Add( "RangeMax", &fRangeMax );
	saver.Add( "RangeMin", &fRangeMin );
	saver.Add( "Ceiling", &nCeiling );
	saver.Add( "Shells", &shells );
	saver.Add( "DeltaAngle", &wDeltaAngle );
	saver.Add( "RevealRadius", &fRevealRadius );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** Acks stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SAckRPGStats::SAckRPGStats()
: SCommonRPGStats( "Ack" )
{
}
SAckRPGStats::~SAckRPGStats()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SAckRPGStats::ToAIUnits()
{
	SCommonRPGStats::ToAIUnits();
	// find max type index
	int nMaxType = 0;
	for ( std::vector<SType>::const_iterator it = types.begin(); it != types.end(); ++it )
		nMaxType = Max( nMaxType, int(it->eType) );
	NI_ASSERT_T( nMaxType + 1 >= types.size(), NStr::Format("Wrong acks in \"%s\" set", szParentName.c_str()) );
	// copy acks to the new positions in accordance with it's type
	std::vector<SType> types2( nMaxType + 1 );
	for ( std::vector<SType>::const_iterator it = types.begin(); it != types.end(); ++it )
		types2[it->eType] = *it;
	// swap old and new vectors
	std::swap( types2, types );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAckRPGStats::SType::SAck::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Probability", &fProbability );
	saver.Add( "Sound", &szSound );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAckRPGStats::SType::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	//
	int nType = eType;
	saver.Add( "Type", &nType );
	eType = EUnitAckType( nType );
	//
	saver.Add( "Acks", &acks );
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAckRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Types", &types );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAckRPGStats::ChooseAcknowledgement( const float fRandom, const EUnitAckType type, std::string *pResult ) const
{
	if ( type >= types.size() ) 
		return false;
	float fSum = 0;
	int nVariant = 0;
	while ( nVariant < types[type].acks.size() )
	{
		fSum += types[type].acks[nVariant].fProbability;
		if ( fRandom*100 <= fSum )
		{
			if ( pResult )
				*pResult = types[type].acks[nVariant].szSound;
			return !types[type].acks[nVariant].szSound.empty();
		}
		++nVariant;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base unit
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const int commandsmap[][2] = 
{
	{ ACTION_COMMAND_MOVE_TO					, USER_ACTION_MOVE },
	{ ACTION_COMMAND_ATTACK_UNIT			, USER_ACTION_ATTACK },
	{ ACTION_COMMAND_ATTACK_OBJECT		, USER_ACTION_ATTACK },
	{ ACTION_COMMAND_SWARM_TO					, USER_ACTION_SWARM },
	{ ACTION_COMMAND_LOAD							, USER_ACTION_BOARD },
	{ ACTION_COMMAND_UNLOAD						, USER_ACTION_LEAVE },
	{ ACTION_COMMAND_ENTER						, USER_ACTION_BOARD },
	{ ACTION_COMMAND_LEAVE						, USER_ACTION_LEAVE },
	{ ACTION_COMMAND_ROTATE_TO				, USER_ACTION_ROTATE },
	{ ACTION_COMMAND_STOP							, USER_ACTION_STOP },
	{ ACTION_COMMAND_PARADE						, USER_ACTION_FORMATION },
	{ ACTION_COMMAND_STAND_GROUND			, USER_ACTION_STAND_GROUND },
	{ ACTION_COMMAND_CALL_BOMBERS			, USER_ACTION_OFFICER_CALL_BOMBERS },
	{ ACTION_COMMAND_CALL_FIGHTERS		, USER_ACTION_OFFICER_CALL_FIGHTERS },
	{ ACTION_COMMAND_CALL_SCOUT				, USER_ACTION_OFFICER_CALL_SPY },
	{ ACTION_COMMAND_PARADROP					, USER_ACTION_OFFICER_CALL_PARADROPERS },
	{ ACTION_COMMAND_CALL_SHTURMOVIKS	, USER_ACTION_OFFICER_CALL_GUNPLANES	},
	{ ACTION_COMMAND_PLACEMINE				, USER_ACTION_ENGINEER_PLACE_MINE_AP },
	{ ACTION_COMMAND_CLEARMINE				, USER_ACTION_ENGINEER_CLEAR_MINES },
	{ ACTION_COMMAND_GUARD						, USER_ACTION_GUARD },
	{ ACTION_COMMAND_AMBUSH						, USER_ACTION_AMBUSH },
	{ ACTION_COMMAND_RANGE_AREA				, USER_ACTION_RANGING },
	{ ACTION_COMMAND_ART_BOMBARDMENT	, USER_ACTION_SUPPRESS },
	{ ACTION_COMMAND_INSTALL					, USER_ACTION_INSTALL },
	{ ACTION_COMMAND_UNINSTALL				, USER_ACTION_UNINSTALL },
	{ ACTION_COMMAND_RESUPPLY					, USER_ACTION_SUPPORT_RESUPPLY },
	{ ACTION_COMMAND_RESUPPLY_HR			, USER_ACTION_HUMAN_RESUPPLY },
	{ ACTION_COMMAND_REPAIR						, USER_ACTION_ENGINEER_REPAIR },
	{ ACTION_COMMAND_BUILD_FENCE_BEGIN, USER_ACTION_ENGINEER_BUILD_FENCE },
	{ ACTION_COMMAND_ENTRENCH_BEGIN		, USER_ACTION_ENGINEER_BUILD_ENTRENCHMENT },
	{ ACTION_COMMAND_PLACE_ANTITANK		, USER_ACTION_ENGINEER_BUILD_ANTITANK			},
	{ ACTION_COMMAND_REPEAR_OBJECT		, USER_ACTION_ENGINEER_REPAIR_BUILDING		},
	{ ACTION_COMMAND_BUILD_BRIDGE			, USER_ACTION_ENGINEER_BUILD_BRIDGE				},
	//{ ACTION_COMMAND_CATCH_ARTILLERY	, USER_ACTION_GUNNER_ASSIGN_TO_GUN				},
	{ ACTION_COMMAND_USE_SPYGLASS			, USER_ACTION_OFFICER_BINOCULARS					},
	{ ACTION_COMMAND_TAKE_ARTILLERY		, USER_ACTION_HOOK_ARTILLERY							},
	{ ACTION_COMMAND_DEPLOY_ARTILLERY	, USER_ACTION_DEPLOY_ARTILLERY						},
	{ ACTION_COMMAND_DISBAND_FORMATION, USER_ACTION_DISBAND_SQUAD								},
	{ ACTION_COMMAND_FORM_FORMATION		, USER_ACTION_FORM_SQUAD									},
	{ ACTION_COMMAND_FOLLOW						, USER_ACTION_FOLLOW											},
	{ ACTION_COMMAND_ENTRENCH_SELF		, USER_ACTION_ENTRENCH_SELF								},
	{ ACTION_COMMAND_CHANGE_SHELLTYPE	, USER_ACTION_CHANGE_SHELL								},
	{ ACTION_COMMAND_FILL_RU					, USER_ACTION_FILL_RU											},
	{ ACTION_COMMAND_MOVE_TO_GRID			, USER_ACTION_MOVE_TO_GRID								},
	{ ACTION_COMMAND_CATCH_ARTILLERY	, USER_ACTION_CAPTURE_ARTILLERY						},
	{ -1, -1 }
};
static std::unordered_map<int, int> commandsremap;
inline int GetUserAction( int nCommand )
{
	std::unordered_map<int, int>::const_iterator pos = commandsremap.find( nCommand );
	return pos != commandsremap.end() ? pos->second : -1;
}
// re-map AI commands to USER actions
class CAICommands2UserActionsAutomagic
{
public:
	CAICommands2UserActionsAutomagic()
	{
		for ( int i = 0; commandsmap[i][0] != -1; ++i )
			commandsremap[commandsmap[i][0]] = commandsmap[i][1];
	}
};
static CAICommands2UserActionsAutomagic theAICommands2UserActionsAutomagic;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitBaseRPGStats::SUnitBaseRPGStats( const char *pszType ) 
: SHPObjectRPGStats( pszType ), nBoundTileRadius( 0 ), fPassability( 1 ),
  vAABBCenter( VNULL2 ), vAABBHalfSize( 12, 12 ), fSmallAABBCoeff( 1 ), fRotateSpeed( 0 ),
	availCommands( 128 ), availExposures( 128 ), fWeight( 1 ), fPrice( 1.0f )
{
	for ( int i = 0; i < availCommands.GetSize(); ++i )
		availCommands.SetData( i );
	for ( int i = 0; i < availExposures.GetSize(); ++i )
		availExposures.RemoveData( i );
	//
	pPrimaryGun = 0;
	//pAcks = 0;
	//
	fSightPower = 1.0f;
	//
	//acknowledgements.resize( _ACK_END );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUnitBaseRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SHPObjectRPGStats*>(this) );
	// type
	if ( saver.IsReading() )
	{
		std::string szData;
		saver.Add( "Type", &szData );
		type = EUnitRPGType( pAutomagic->ToInt( szData.c_str() ) );
	}
	else
	{
		std::string szData = pAutomagic->ToStr( type );
		saver.Add( "Type", &szData );
	}
	// ai class
	if ( saver.IsReading() )
	{
		std::string szData;
		saver.Add( "AIClass", &szData );
		if ( !szData.empty() )
			aiClass = EAIClass( pAutomagic->ToInt( szData.c_str() ) );
	}
	else
	{
		std::string szData = pAutomagic->ToStr( aiClass );
		saver.Add( "AIClass", &szData );
	}
	//
	saver.Add( "Sight", &fSight );
	saver.Add( "SightPower", &fSightPower );
	saver.Add( "Speed", &fSpeed );
	saver.Add( "Passability", &fPassability );
	saver.Add( "Priority", &nPriority );
	saver.Add( "Camouflage", &fCamouflage );
	saver.Add( "Weight", &fWeight );
	saver.Add( "Price", &fPrice );
	// install/uninstall
	saver.Add( "UninstallRotate", &fUninstallRotate );
	saver.Add( "UninstallTransport", &fUninstallTransport );
	// available commands
	saver.Add( "Commands", &availCommands );
	saver.Add( "Exposures", &availExposures );
	// animation descriptions
	saver.Add( "AnimDescs", &animdescs );
	saver.Add( "AABB_As", &aabb_as );
	saver.Add( "AABB_Ds", &aabb_ds );
	//acks
	//saver.Add( "Acks", &acknowledgements );

	saver.Add( "AcksRefs", &szAcksNames );

	//CRAP{ FOR OLD ACKS TO WORK
	std::string szTmp;
	saver.Add( "AcksRef", &szTmp );
	if ( saver.IsReading() && !szTmp.empty() )
	{
		szAcksNames.push_back( szTmp );
	}
	//CRAP}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void AddValue( CArray1Bit &array, int nBit )
{
	if ( array.GetSize() <= nBit )
		array.SetSize( nBit + 1 );
	array.SetData( nBit );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUnitBaseRPGStats::SAABBDesc::ToAIUnits()
{
	Vis2AI( &vCenter );
	Vis2AI( &vHalfSize );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ReMapCommands( CArray1Bit &ai, CUserActions &user )
{
	user.Clear();
	for ( int i = 0; i != ai.GetSize(); ++i ) 
	{
		if ( ai.GetData(i) != 0 )
		{
			const int nUserAction = GetUserAction( i );
			if ( nUserAction != -1 ) 
				user.SetAction( nUserAction );
			if ( nUserAction == USER_ACTION_ENGINEER_PLACE_MINE_AP )
				user.SetAction( USER_ACTION_ENGINEER_PLACE_MINE_AT );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SUnitBaseRPGStats::ToAIUnits()
{
	SHPObjectRPGStats::ToAIUnits();
	fSight *= 1.0f;
	// киломерты/час <=> точки/тик
	fSpeed *= float( ( 1000.0 * double( SAIConsts::TILE_SIZE ) ) / ( 3600.0 * 1000.0 ) );
	// сек. <=> тик
	nUninstallRotate = fUninstallRotate * 1000.0f;
	nUninstallTransport = fUninstallTransport * 1000.0f;
	fCamouflage /= 100.0f;
	//fPassability /= 100.0f;
	// AABBs
	// Vis points <=> AI points
	vAABBVisCenter = vAABBCenter;
	vAABBVisHalfSize = vAABBHalfSize;
	Vis2AI( &vAABBCenter );
	Vis2AI( &vAABBHalfSize );
	std::for_each( aabb_as.begin(), aabb_as.end(), [](SAABBDesc& aabb){ aabb.ToAIUnits(); } );
	std::for_each( aabb_ds.begin(), aabb_ds.end(), [](SAABBDesc& aabb){ aabb.ToAIUnits(); } );
	//
	// re-map AI actions to user commands
	// every unit can perform STOP, GUARD and FOLLOW and can be followed by
	if ( IsAviation() ) 
	{
		AddValue( availCommands, ACTION_COMMAND_PLANE_ADD_POINT );
		AddValue( availCommands, ACTION_COMMAND_PLANE_TAKEOFF_NOW );
	}
	AddValue( availCommands, ACTION_COMMAND_STOP );
	AddValue( availCommands, ACTION_COMMAND_GUARD );
	AddValue( availCommands, USER_ACTION_MOVE_TO_GRID );
	if ( szStatsType != "TankPit" ) 
		AddValue( availExposures, ACTION_COMMAND_FOLLOW );	
	//AddValue( availCommands, ACTION_COMMAND_FOLLOW );
	//AddValue( availExposures, ACTION_COMMAND_FOLLOW );
	//
	ReMapCommands( availCommands, availUserActions );
	ReMapCommands( availExposures, availUserExposures );
	// every object can be attacked 
	availUserExposures.SetAction( USER_ACTION_ATTACK );
	availUserExposures.SetAction( USER_ACTION_UNKNOWN );
	//
	availUserActions.SetAction( USER_ACTION_UNKNOWN );
	availUserActions.SetAction( USER_ACTION_MOVE_TO_GRID );
	// for repair and resupply trucks set automatically FILL_RU action
	if ( availUserActions.HasAction(USER_ACTION_ENGINEER_REPAIR) || availUserActions.HasAction(USER_ACTION_SUPPORT_RESUPPLY) ) 
	{
		availUserActions.SetAction( USER_ACTION_FILL_RU );
		AddValue( availCommands, ACTION_COMMAND_FILL_RU );	
	}
	//
	nBoundTileRadius = IsInfantry() ? 0 : int(ceil( (2 * vAABBHalfSize.x) / float(SAIConsts::TILE_SIZE) )) / 2;
	// convert AI price
	{
		const std::string szVarName = NStr::Format( "AIPrice.%x", int(type) );
		fPrice *= GetGlobalVar( szVarName.c_str(), 1.0f );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SUnitBaseRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	const int nSize = szAcksNames.size();
	for ( int i = 0; i < nSize; ++i )
	{
		if ( !szAcksNames[i].empty() )
			pAcksSets.push_back( static_cast<const SAckRPGStats*>( pGDB->GetAddStats( szAcksNames[i].c_str(), IObjectsDB::ACKS ) ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SUnitBaseRPGStats::ChooseAcknowledgement( const float fRandom, const EUnitAckType type, std::string *str, const int nSet ) const
{
	if ( pAcksSets.size() > nSet && pAcksSets[nSet]->ChooseAcknowledgement( fRandom, type, str ) )
		return true;
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SUnitBaseRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;	

	CopyToBuf( &buf, type );
	CopyToBuf( &buf, aiClass );
	CopyToBuf( &buf, fSight );
	CopyToBuf( &buf, fSightPower );
	CopyToBuf( &buf, fSpeed );
	CopyToBuf( &buf, fRotateSpeed );
	CopyToBuf( &buf, fPassability );
	CopyToBuf( &buf, nPriority );
	CopyToBuf( &buf, fCamouflage );
	CopyToBuf( &buf, nMaxArmor );
	CopyToBuf( &buf, nMinArmor );
	CopyToBuf( &buf, fUninstallRotate );
	CopyToBuf( &buf, fUninstallTransport );
	CopyToBuf( &buf, nBoundTileRadius );
	CopyToBuf( &buf, fWeight );
	CopyToBuf( &buf, fPrice );
	CopyToBuf( &buf, vAABBCenter );
	CopyToBuf( &buf, vAABBHalfSize );
	CopyToBuf( &buf, fSmallAABBCoeff );
	for ( int i = 0; i < availCommands.GetSize(); ++i )
		CopyToBuf( &buf, availCommands.GetData( i ) );
	for ( int i = 0; i < availExposures.GetSize(); ++i )
		CopyToBuf( &buf, availExposures.GetData( i ) );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	if ( pPrimaryGun )
		checkSum = GetCRC( pPrimaryGun->CalculateCheckSum(), checkSum );
	checkSum = GetCRC( SHPObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** mech unit (пушки, танки, и т.д. - вся техника)RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SMechUnitRPGStats::SGun::CalculateCheckSum() const
{
	return SBaseGunRPGStats::CalculateCheckSum();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SMechUnitRPGStats::SPlatform::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, fHorizontalRotationSpeed );
	CopyToBuf( &buf, fVerticalRotationSpeed );
	CopyToBuf( &buf, constraintVertical.fMin );
	CopyToBuf( &buf, constraintVertical.fMax );
	CopyToBuf( &buf, nFirstGun );
	CopyToBuf( &buf, nNumGuns );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::SConstraint::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Min", &fMin );
	saver.Add( "Max", &fMax );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::SGun::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SBaseGunRPGStats*>(this) );
	saver.Add( "ShootPoint", &nShootPoint );
	saver.Add( "Recoil", &bRecoil );
	saver.Add( "RecoilLength", &fRecoilLength );
	saver.Add( "RecoilTime", &recoilTime );
	saver.Add( "ModelPart", &nModelPart );
	saver.Add( "RecoilShakeTime", &nRecoilShakeTime );
	saver.Add( "RecoilShakeAngle", &fRecoilShakeAngle );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::SPlatform::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "VerticalRotateSpeed", &fVerticalRotationSpeed );
	saver.Add( "HorizontalRotationSpeed", &fHorizontalRotationSpeed );
	saver.Add( "ModelPart", &nModelPart );
	saver.Add( "Constraint", &constraint );
	saver.Add( "GunCarriageParts", &dwGunCarriageParts );
	saver.Add( "ConstraintVertical", &constraintVertical );
	saver.Add( "FirstGun", &nFirstGun );
	saver.Add( "NumGuns", &nNumGuns );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::SArmor::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Min", &fMin );
	saver.Add( "Max", &fMax );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::SJoggingParams::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Period1", &fPeriod1 );
	saver.Add( "Period2", &fPeriod2 );
	saver.Add( "Amp1", &fAmp1 );
	saver.Add( "Amp2", &fAmp2 );
	saver.Add( "Phase1", &fPhase1 );
	saver.Add( "Phase2", &fPhase2 );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SMechUnitRPGStats::SArmor::ToAIUnits()
{
	int nArmorMin = fMin;
	int nArmorMax = fMax;
	nMin = nArmorMin;
	nMax = nArmorMax;

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SMechUnitRPGStats::SConstraint::ToAIUnits()
{
	wMin = fMin >= FP_2PI ? 65535 : WORD( fMin / FP_2PI * 65535.0f );
	wMax = fMax >= FP_2PI ? 65535 : WORD( fMax / FP_2PI * 65535.0f );
	
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SMechUnitRPGStats::SPlatform::ToAIUnits()
{
	constraint.ToAIUnits();
	constraintVertical.ToAIUnits();
	// (секунды на полный оборот <=> градусы65535/тик)
	fHorizontalRotationSpeed = 1.0f / fHorizontalRotationSpeed * 65535.0f / 1000.0f;
	if ( fHorizontalRotationSpeed != 0 )
		fHorizontalRotationSpeed = Max( 1.0f, fHorizontalRotationSpeed );
	wHorizontalRotationSpeed = fHorizontalRotationSpeed <= 65535.0f ? WORD( fHorizontalRotationSpeed ) : 65535;
	fVerticalRotationSpeed = 1.0f / fVerticalRotationSpeed * 65535.0f / 1000.0f;
	if ( fVerticalRotationSpeed != 0 )
		fVerticalRotationSpeed = Max( 1.0f, fVerticalRotationSpeed );
	wVerticalRotationSpeed = fVerticalRotationSpeed <= 65535.0f ? WORD( fVerticalRotationSpeed ) : 65535;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SMechUnitRPGStats::ToAIUnits()
{
	SUnitBaseRPGStats::ToAIUnits();
	// градусы/сек <=> угол 65536/тик
	fRotateSpeed *= float( (65536.0 / 360.0) / 1000.0 );
	// метры <=> AI точки
	fTurnRadius *= float( SAIConsts::TILE_SIZE );
	// Vis points <=> AI points
	Vis2AI( &vTowPoint );
	Vis2AI( &vHookPoint );
	Vis2AI( &vFrontWheel );
	Vis2AI( &vBackWheel );
	Vis2AI( &vEntrancePoint );
	Vis2AI( &vAmmoPoint );
	for ( std::vector<CVec2>::iterator it = vPeoplePoints.begin(); it != vPeoplePoints.end(); ++it )
		Vis2AI( &(*it) );
	for ( std::vector< std::vector<CVec2> >::iterator gunners = vGunners.begin(); gunners != vGunners.end(); ++gunners )
	{
		for ( std::vector<CVec2>::iterator place = gunners->begin(); place != gunners->end(); ++place )
			Vis2AI( &(*place) );
		//if ( gunners->size() >= 3 ) // исправления кривостей, котрые сделали художники.
			//std::swap( (*gunners)[1], (*gunners)[2] );
	}
	// armor
	std::for_each( &(armors[0]), &(armors[6]), [](SArmor& armor){ armor.ToAIUnits(); } );
	// проинициализировать min/max Armor
	{
		nMinArmor = armors[0].nMin;
		nMaxArmor = armors[0].nMax;
		for ( int i = 1; i < 4; ++i )
		{
			nMinArmor = Min( nMinArmor, armors[i].nMin );
			nMaxArmor = Max( nMaxArmor, armors[i].nMax );
		}
	}
	//
	std::for_each( platforms.begin(), platforms.end(), [](SPlatform& plat){ plat.ToAIUnits(); } );
	//
	if ( nPriority == 0 )
		nPriority = 1;
	//
	CountPrimaryGuns( guns );
	//
	wDivingAngle = fDivingAngle * 65535.0f / 360.0f;
	wClimbingAngle = fClimbAngle * 65535.0f / 360.0f;
	wTiltAngle = fTiltAngle * 65535.0f / 360.0f;
	// make action flags, based on available shells
	CountShellTypes( guns );
	// disable track for non-track units
	if ( !(aiClass & (AI_CLASS_HALFTRACK | AI_CLASS_TRACK)) )
		bLeavesTracks = false;

	// exchange 0 and 2 gunners if exists
	//if ( vGunners.size() >= 3 )
	//{
		//std::swap( vGunners[0], vGunners[2] );
	//}
	
	if ( HasCommand( ACTION_COMMAND_MOVE_TO ) )
		AddCommand( ACTION_COMMAND_MOVE_TO_GRID );
	// artillery can be captured
	if ( IsArtillery() && !vGunners.empty() && !vGunners[0].empty() ) 
	{
		if ( availExposures.GetSize() <= ACTION_COMMAND_CATCH_ARTILLERY )
			availExposures.SetSize( ACTION_COMMAND_CATCH_ARTILLERY + 1 );
		//
		availExposures.SetData( ACTION_COMMAND_CATCH_ARTILLERY );
		availUserExposures.SetAction( USER_ACTION_CAPTURE_ARTILLERY );
	}
	if ( HasCommand(ACTION_COMMAND_RESUPPLY_HR) ) 
	{
		AddCommand( ACTION_COMMAND_CATCH_ARTILLERY );
		availUserActions.SetAction( USER_ACTION_CAPTURE_ARTILLERY );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMechUnitRPGStats::SGun::SGun()
: nShootPoint( -1 ), bRecoil( false ), fRecoilLength( 0 ), recoilTime( 50 ), nModelPart( -1 )
{  
	nRecoilShakeTime = 400;
	fRecoilShakeAngle = ToRadian( 3.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMechUnitRPGStats::SPlatform::SPlatform()
: fVerticalRotationSpeed( 10 ), fHorizontalRotationSpeed( 60 ), nModelPart( 0 ), dwGunCarriageParts( -1 ), nFirstGun( 0 ), nNumGuns( 1 )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMechUnitRPGStats::SJoggingParams::SJoggingParams()
{
	fPeriod1 = 1;
	fPeriod2 = 0.3f;
	fAmp1 = 1;
	fAmp2 = 0.6f;
	fPhase1 = 0;
	fPhase2 = 2.1f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMechUnitRPGStats::SMechUnitRPGStats() 
: SUnitBaseRPGStats( "Mech" ), platforms( 2 ), guns( 1 )
{
	platforms[0].nFirstGun = 0;
	platforms[0].nNumGuns = 1;
	platforms[1].nFirstGun = 1;
	platforms[1].nNumGuns = 0;
	//
	type = RPG_TYPE_ARM_LIGHT;
	aiClass = AI_CLASS_TRACK;
	//
	fMaxHP = 10;
	fSight = 20;
	fSpeed = 0.075f * ( 3600.0f * 1000.0f ) / ( 1000.0f * float( SAIConsts::TILE_SIZE ) );
	fRotateSpeed = 20;//20 * 1000.0f / (65536.0f / 360.0f);
	fPassability = 1;
	fTowingForce = 100;
	nCrew = 4;
	nPassangers = 0;
	nPriority = 1;

	fCamouflage = 0.2f;
	//
	nUninstallRotate = 1;
	nUninstallTransport = 1;
	//
	nBoundTileRadius = 1;
	fTurnRadius = 5;
	//
	nTowPoint = -1;
	nEntrancePoint = -1;
	nFatalitySmokePoint = -1;
	nShootDustPoint = -1;
	//
	vTowPoint = VNULL2;
	vHookPoint = VNULL2;
	vFrontWheel = VNULL2;
	vBackWheel = VNULL2;
	vEntrancePoint = VNULL2;
	vAmmoPoint = VNULL2;
	//
	vAABBCenter = VNULL2;
	vAABBHalfSize.x = 12;
	vAABBHalfSize.y = 12;
	vAABBVisCenter = vAABBCenter;
	vAABBVisHalfSize = vAABBHalfSize;
	//
	szEffectDiesel = "diselsmoke";
	szEffectSmoke = "";
	//
	jx.fPeriod1 = 1;
	jx.fPeriod2 = 0.3f;
	jx.fAmp1 = 1;
	jx.fAmp2 = 0.6f;
	jx.fPhase1 = 0;
	jx.fPhase2 = 2.1f;

	jy.fPeriod1 = 0.7f;
	jy.fPeriod2 = 0.2f;
	jy.fAmp1 = 0.6f;
	jy.fAmp2 = 0.4f;
	jy.fPhase1 = 1.9f;
	jy.fPhase2 = 0;
	//
	bLeavesTracks = true;
	fTrackWidth = 0.15f;
	fTrackOffset = 0.15f;
	fTrackStart = 0.2f;
	fTrackEnd = 0.2f;
	fTrackIntensity = 0.7f;
	nTrackLifetime = 10000;
	//
	fMaxHeight = 500.0f;
	fDivingAngle = 20.0f;
	fClimbAngle = 20.0f;
	fTiltAngle = 30.0f;
	fTiltRatio = 2.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SMechUnitRPGStats::Validate()
{
	if ( IsAviation() )
	{
		NI_ASSERT_SLOW_TF( fTurnRadius > 0, NStr::Format("Turn radius for planes must be > 0! (%s)", szParentName.c_str()), return false );
	}

	NI_ASSERT_SLOW_TF( nPriority > 0, NStr::Format("Priority for non-infantry must be > 0! (%s)", szParentName.c_str()), return false );

	NI_ASSERT_SLOW_TF( nBoundTileRadius <= 5, NStr::Format("Bound tile radius must be <= 5 in order to survive :) (%s)", szParentName.c_str()), return false );
	// validate gun indices
	for ( int i=0; i<platforms.size(); ++i )
	{
		if ( !guns.empty() )
		{
			if ( platforms[i].nNumGuns > 0 )
			{
				NI_ASSERT_TF( platforms[i].nFirstGun >= 0 && platforms[i].nFirstGun < guns.size(), NStr::Format("first gun must be valid (%s)", szParentName.c_str()), return false );
				NI_ASSERT_TF( platforms[i].nNumGuns >= 0 && platforms[i].nFirstGun + platforms[i].nNumGuns <= guns.size(), NStr::Format("num guns must be valid (%s)", szParentName.c_str()), return false );
			}
		}
		if ( i == 0 )
		{
			NI_ASSERT_TF( platforms[i].nFirstGun == 0, NStr::Format("First gun for first platform must be 0 (%s)", szParentName.c_str()), return false );
		}
		else
		{
			NI_ASSERT_TF( platforms[i].nFirstGun == platforms[i - 1].nFirstGun + platforms[i - 1].nNumGuns, NStr::Format("first gun of the %d platform must match last gun of the %d platform, unit (%s)", i, i - 1, szParentName.c_str()), return false );
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMechUnitRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SUnitBaseRPGStats*>(this) );
	// platforms & guns
	saver.Add( "Platforms", &platforms );
	saver.Add( "Guns", &guns );
	//
	// armor
	saver.Add( "ArmorLeft", &armors[RPG_LEFT] );
	saver.Add( "ArmorRight", &armors[RPG_RIGHT] );
	saver.Add( "ArmorTop", &armors[RPG_TOP] );
	saver.Add( "ArmorBottom", &armors[RPG_BOTTOM] );
	saver.Add( "ArmorFront", &armors[RPG_FRONT] );
	saver.Add( "ArmorBack", &armors[RPG_BACK] );
	//
	saver.Add( "RotateSpeed", &fRotateSpeed );
	saver.Add( "TurnRadius",  &fTurnRadius );
	saver.Add( "TowingForce", &fTowingForce );

	saver.Add( "Crew", &nCrew );
	saver.Add( "Passangers", &nPassangers );

	saver.Add( "BoundTileRadius", &nBoundTileRadius );

	saver.Add( "AABBCenter", &vAABBCenter );
	saver.Add( "AABBHalfSize", &vAABBHalfSize );
	saver.Add( "SmallAABBCoeff", &fSmallAABBCoeff );

	saver.Add( "ExhaustPoints", &exhaustPoints );
	saver.Add( "DamagePoints", &damagePoints );
	saver.Add( "TowPoint", &nTowPoint );
	saver.Add( "EntrancePoint", &nEntrancePoint );
	saver.Add( "PeoplePoints", &peoplePointIndices );
	saver.Add( "FatalitySmokePoint", &nFatalitySmokePoint );
	saver.Add( "ShootDustPoint", &nShootDustPoint );

	saver.Add( "TowPoint2D", &vTowPoint );
	saver.Add( "HookPoint", &vHookPoint );
	saver.Add( "FrontWheel", &vFrontWheel );
	saver.Add( "BackWheel", &vBackWheel );
	saver.Add( "EntrancePoint2D", &vEntrancePoint );
	saver.Add( "PeoplePoints2D", &vPeoplePoints );
	saver.Add( "AmmoPoint2D", &vAmmoPoint );
	saver.Add( "Gunners", &vGunners );
	//
	saver.Add( "EffectDiesel", &szEffectDiesel );
	saver.Add( "EffectSmoke", &szEffectSmoke );
	saver.Add( "EffectWheelDust", &szEffectWheelDust );
	saver.Add( "EffectShootDust", &szEffectShootDust );
	saver.Add( "EffectFatality", &szEffectFatality );
	saver.Add( "EffectEntrenching", &szEffectEntrenching );
	saver.Add( "EffectDisappear", &szEffectDisappear );
	//
	saver.Add( "JoggingX", &jx );
	saver.Add( "JoggingY", &jy );
	saver.Add( "JoggingZ", &jz );
	//
	saver.Add( "LeavesTracks", &bLeavesTracks );
	saver.Add( "TrackOffset", &fTrackOffset );
	saver.Add( "TrackWidth", &fTrackWidth );
	saver.Add( "TrackStart", &fTrackStart );
	saver.Add( "TrackEnd", &fTrackEnd );
	saver.Add( "TrackIntensity", &fTrackIntensity );
	saver.Add( "TrackLifetime", &nTrackLifetime );
	//
	saver.Add( "SoundMoveStart", &szSoundMoveStart );
	saver.Add( "SoundMove", &szSoundMoveCycle );
	saver.Add( "SoundMoveStop", &szSoundMoveStop );
	//
	saver.Add( "MaxHeight", &fMaxHeight );
	saver.Add( "DivingAngle", &fDivingAngle );
	saver.Add( "ClimbAngle", &fClimbAngle );
	saver.Add( "TiltAngle", &fTiltAngle );
	saver.Add( "TiltRatio", &fTiltRatio );
	//
	saver.Add( "DeathCraters", &deathCraters );
	//
	if ( saver.IsReading() ) 
	{
		// reset tracks for aviation
		if ( IsAviation() ) 
			bLeavesTracks = false;
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SMechUnitRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	SUnitBaseRPGStats::RetrieveShortcuts( pGDB );
	for ( std::vector<SGun>::iterator gun = guns.begin(); gun != guns.end(); ++gun )
		gun->RetrieveShortcuts( pGDB );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SMechUnitRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, fTowingForce );
	CopyToBuf( &buf, nCrew );
	CopyToBuf( &buf, nPassangers );
	CopyToBuf( &buf, fTurnRadius );
	CopyToBuf( &buf, vTowPoint );
	CopyToBuf( &buf, vEntrancePoint );
	for ( int i = 0; i < vPeoplePoints.size(); ++i )
		CopyToBuf( &buf, vPeoplePoints[i] );
	CopyToBuf( &buf, vAmmoPoint );

	for ( int i = 0; i < vGunners.size(); ++i )
	{
		for ( int j = 0; j < vGunners[i].size(); ++j )
			CopyToBuf( &buf, vGunners[i][j] );
	}

	CopyToBuf( &buf, vHookPoint );
	CopyToBuf( &buf, vFrontWheel );
	CopyToBuf( &buf, vBackWheel );
	CopyToBuf( &buf, fMaxHeight );
	CopyToBuf( &buf, fDivingAngle );
	CopyToBuf( &buf, fClimbAngle );
	CopyToBuf( &buf, fTiltAngle );
	CopyToBuf( &buf, fTiltRatio );
	for ( int i = 0; i < 6; ++i )
	{
		CopyToBuf( &buf, armors[i].fMin );
		CopyToBuf( &buf, armors[i].fMax );
	}

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < guns.size(); ++i )
		checkSum = GetCRC( guns[i].CalculateCheckSum(), checkSum );
	for ( int i = 0; i < platforms.size(); ++i )
		checkSum = GetCRC( platforms[i].CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SUnitBaseRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** пехотинец
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SInfantryRPGStats::SInfantryRPGStats()
: SUnitBaseRPGStats( "Infa" ), guns( 1 )
{
	type = RPG_TYPE_SOLDIER;
	aiClass = AI_CLASS_HUMAN;
	//
	fMaxHP = 10;
	fSight = 20;
	nMinArmor = nMaxArmor = 0;
	fSpeed = 1;
	fPassability = 1;
	fCamouflage = 0.2f;
	nPriority = 0;
	//
	bCanAttackUp = bCanAttackDown = true;
	//
	nUninstallRotate = 1;
	nUninstallTransport = 1;
	//
	fRunSpeed = 1;
	fCrawlSpeed = 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SInfantryRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	SUnitBaseRPGStats::RetrieveShortcuts( pGDB );
	for ( std::vector<SGun>::iterator gun = guns.begin(); gun != guns.end(); ++gun )
		gun->RetrieveShortcuts( pGDB );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SInfantryRPGStats::ToAIUnits()
{
	SUnitBaseRPGStats::ToAIUnits();
	availUserActions.RemoveAction( USER_ACTION_LEAVE );
	availUserActions.RemoveAction( USER_ACTION_FORM_SQUAD );
	availUserActions.RemoveAction( USER_ACTION_DISBAND_SQUAD );
	//
	nPriority = 0;
	//
	CountPrimaryGuns( guns );
	// make action flags, based on available shells
	CountShellTypes( guns );

	if ( HasCommand( ACTION_COMMAND_MOVE_TO ) )
		AddCommand( ACTION_COMMAND_MOVE_TO_GRID );

	if ( type != RPG_TYPE_SNIPER ) 
	{
		availUserActions.SetAction( USER_ACTION_CAPTURE_ARTILLERY );
		AddCommand( ACTION_COMMAND_CATCH_ARTILLERY );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SInfantryRPGStats::SGun::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SBaseGunRPGStats*>(this) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SInfantryRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SUnitBaseRPGStats*>(this) );
	// armor
	saver.Add( "Armor", &nMaxArmor );
	nMinArmor = nMaxArmor;
	//
	saver.Add( "Guns", &guns );
	saver.Add( "CanAttackUp", &bCanAttackUp );
	saver.Add( "CanAttackDown", &bCanAttackDown );
	saver.Add( "WalkSpeed", &fRunSpeed );
	saver.Add( "CrawlSpeed", &fCrawlSpeed );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SInfantryRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, bCanAttackUp );
	CopyToBuf( &buf, bCanAttackDown );
	CopyToBuf( &buf, fRunSpeed );
	CopyToBuf( &buf, fCrawlSpeed );
	for ( int i = 0; i < animtimes.size(); ++i )
		CopyToBuf( &buf, animtimes[i] );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < guns.size(); ++i )
		checkSum = GetCRC( guns[i].CalculateCheckSum(), checkSum );
	
	checkSum = GetCRC( SUnitBaseRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** entrenchment
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SEntrenchmentRPGStats::SSegmentRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;
	
	CopyToBuf( &buf, vFirePlace );
	CopyToBuf( &buf, vAABBCenter );
	CopyToBuf( &buf, vAABBHalfSize );
	CopyToBuf( &buf, fCoverage );
	for ( int i = 0; i < fireplaces.size(); ++i )
		CopyToBuf( &buf, fireplaces[i] );
	CopyToBuf( &buf, eType );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SEntrenchmentRPGStats::SEntrenchmentRPGStats() 
: SHPObjectRPGStats( "Entrenchment" ) 
{ 
	fMaxHP = 1000;	
	
	defences[RPG_FRONT].nArmorMin		= 300;
	defences[RPG_FRONT].nArmorMax		= 300;
	defences[RPG_LEFT].nArmorMin		= 300;
	defences[RPG_LEFT].nArmorMax		= 300;
	defences[RPG_BACK].nArmorMin		= 300;
	defences[RPG_BACK].nArmorMax		= 300;
	defences[RPG_RIGHT].nArmorMin		= 300;
	defences[RPG_RIGHT].nArmorMax		= 300;
	defences[RPG_TOP].nArmorMin			= 0;
	defences[RPG_TOP].nArmorMax			= 0;
	defences[RPG_BOTTOM].nArmorMin  = 300;
	defences[RPG_BOTTOM].nArmorMax	= 300;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SEntrenchmentRPGStats::SSegmentRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Model", &szModel );
	saver.Add( "FirePlace", &vFirePlace );
	saver.Add( "AABBCenter", &vAABBCenter );
	saver.Add( "AABBHalfSize", &vAABBHalfSize );
	saver.Add( "Coverage", &fCoverage );
	saver.Add( "SegmentType", &eType );
	saver.Add( "FirePlaces", &fireplaces );
	return 0;
}
int SEntrenchmentRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SHPObjectRPGStats*>(this) );
	saver.Add( "Segments", &segments );
	saver.Add( "Lines", &lines );
	saver.Add( "FirePlaces", &fireplaces );
	saver.Add( "Terminators", &terminators );
	saver.Add( "Arcs", &arcs );
	// CRAP{ need to write entrenchment segment type
	for ( int i=0; i<lines.size(); ++i )
		segments[lines[i]].eType = SEntrenchmentRPGStats::EST_LINE;
	for ( int i=0; i<fireplaces.size(); ++i )
		segments[fireplaces[i]].eType = SEntrenchmentRPGStats::EST_FIREPLACE;
	for ( int i=0; i<terminators.size(); ++i )
		segments[terminators[i]].eType = SEntrenchmentRPGStats::EST_TERMINATOR;
	for ( int i=0; i<arcs.size(); ++i )
		segments[arcs[i]].eType = SEntrenchmentRPGStats::EST_ARC;
	// CRAP}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SEntrenchmentRPGStats::SSegmentRPGStats::ToAIUnits()
{
	Vis2AI( &vFirePlace );
	Vis2AI( &vAABBCenter );
	Vis2AI( &vAABBHalfSize );
	for ( std::vector<CVec2>::iterator it = fireplaces.begin(); it != fireplaces.end(); ++it )
		Vis2AI( &(*it) );
	return true;
}
void SEntrenchmentRPGStats::ToAIUnits()
{
	SHPObjectRPGStats::ToAIUnits();
	std::for_each( segments.begin(), segments.end(), std::mem_fun_ref(SSegmentRPGStats::ToAIUnits) ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float SEntrenchmentRPGStats::SSegmentRPGStats::GetLength() const 
{ 
	return vAABBHalfSize.x*2.0f*fAITileXCoeff;
}
float SEntrenchmentRPGStats::SSegmentRPGStats::GetHalfLength() const 
{ 
	return vAABBHalfSize.x*fAITileXCoeff; 
}
const CVec2 SEntrenchmentRPGStats::SSegmentRPGStats::GetVisFirePlace() const
{
	CVec2 vTemp;
	AI2Vis( &vTemp, vFirePlace );
	return vTemp;
}
const CVec2 SEntrenchmentRPGStats::SSegmentRPGStats::GetVisAABBCenter() const
{
	CVec2 vTemp;
	AI2Vis( &vTemp, vAABBCenter );
	return vTemp;
}
const CVec3 SEntrenchmentRPGStats::SSegmentRPGStats::GetVisAABBHalfSize() const
{
	CVec3 vTemp;
	AI2Vis( &vTemp, vAABBHalfSize );
	return vTemp;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int SEntrenchmentRPGStats::GetTypeFromIndex( const int nIndex ) const
{
	if ( std::find(lines.begin(), lines.end(), nIndex) != lines.end() )
		return ENTRENCHMENT_LINE;
	if ( std::find(fireplaces.begin(), fireplaces.end(), nIndex) != fireplaces.end() )
		return ENTRENCHMENT_FIREPLACE;
	if ( std::find(terminators.begin(), terminators.end(), nIndex) != terminators.end() )
		return ENTRENCHMENT_TERMINATOR;
	if ( std::find(arcs.begin(), arcs.end(), nIndex) != arcs.end() )
		return ENTRENCHMENT_ARC;
	NI_ASSERT_T( false, NStr::Format("Wrong index %d in entrenchment \"%s\"", nIndex, szParentName.c_str()) );
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int SEntrenchmentRPGStats::GetIndexFromType( const int nType, int *pCurRandomSeed ) const
{
	switch ( nType ) 
	{
		case ENTRENCHMENT_LINE:
			return GetLineIndex( pCurRandomSeed );
		case ENTRENCHMENT_FIREPLACE:
			return GetFirePlaceIndex( pCurRandomSeed );
		case ENTRENCHMENT_TERMINATOR:
			return GetTerminatorIndex( pCurRandomSeed );
		case ENTRENCHMENT_ARC:
			return GetArcIndex( pCurRandomSeed );
	}
	NI_ASSERT_T( false, NStr::Format("Wrong type %d in entrenchment \"%s\"", nType, szParentName.c_str()) );
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SEntrenchmentRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < lines.size(); ++i )
		CopyToBuf( &buf, lines[i] );
	for ( int i = 0; i < fireplaces.size(); ++i )
		CopyToBuf( &buf, fireplaces[i] );
	for ( int i = 0; i < terminators.size(); ++i )
		CopyToBuf( &buf, terminators[i] );
	for ( int i = 0; i < arcs.size(); ++i )
		CopyToBuf( &buf, arcs[i] );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < segments.size(); ++i )
		checkSum = GetCRC( segments[i].CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SHPObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** fence set RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SFenceRPGStats::SSegmentRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Passability", &passability );
	saver.Add( "Origin", &vOrigin );
	saver.Add( "Visibility", &visibility );
	saver.Add( "VisOrigin", &vVisOrigin );
	saver.Add( "Index", &nIndex );
	return 0;
}
const uLong SFenceRPGStats::SSegmentRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < passability.GetSizeY(); ++i )
	{
		for ( int j = 0; j < passability.GetSizeX(); ++j )
			CopyToBuf( &buf, passability[i][j] );
	}
	CopyToBuf( &buf, vOrigin );
	for ( int i = 0; i < visibility.GetSizeY(); ++i )
	{
		for ( int j = 0; j < visibility.GetSizeX(); ++j )
			CopyToBuf( &buf, visibility[i][j] );
	}
	CopyToBuf( &buf, vVisOrigin );
	CopyToBuf( &buf, nIndex );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}
int SFenceRPGStats::SDir::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Centers", &centers );
	saver.Add( "LDamages", &ldamages );
	saver.Add( "RDamages", &rdamages );
	saver.Add( "CDamages", &cdamages );
	return 0;
}
const uLong SFenceRPGStats::SDir::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	for ( int i = 0; i < centers.size(); ++i )
		CopyToBuf( &buf, centers[i] );
	for ( int i = 0; i < ldamages.size(); ++i )
		CopyToBuf( &buf, ldamages[i] );
	for ( int i = 0; i < rdamages.size(); ++i )
		CopyToBuf( &buf, rdamages[i] );
	for ( int i = 0; i < cdamages.size(); ++i )
		CopyToBuf( &buf, cdamages[i] );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}

// local functor for stats sorting
struct SFenceSegmentStatsLess
{
	bool operator()( const SFenceRPGStats::SSegmentRPGStats &s1, const SFenceRPGStats::SSegmentRPGStats &s2 ) const
	{
		return s1.nIndex < s2.nIndex;
	}
};
//
int SFenceRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SStaticObjectRPGStats*>(this) );
	// for by index and validate before writing
	if ( !saver.IsReading() )
		std::sort( stats.begin(), stats.end(), SFenceSegmentStatsLess() );
	saver.Add( "Stats", &stats );
	saver.Add( "Dirs", &dirs );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SFenceRPGStats::SSegmentRPGStats::ToAIUnits()
{
	Vis2AI( &vOrigin );
	Vis2AI( &vVisOrigin );
	
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SFenceRPGStats::ToAIUnits()
{
	SStaticObjectRPGStats::ToAIUnits();
	std::for_each( stats.begin(), stats.end(), [](SSegmentRPGStats& stat){ stat.ToAIUnits(); } );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int SFenceRPGStats::GetTypeFromIndex( const int nIndex ) const
{
	for ( int i = 0; i < dirs.size(); ++i )
	{
		if ( std::find(dirs[i].centers.begin(), dirs[i].centers.end(), nIndex) != dirs[i].centers.end() )
			return ( 1 << i ) | FENCE_TYPE_NORMAL;
		if ( std::find(dirs[i].ldamages.begin(), dirs[i].ldamages.end(), nIndex) != dirs[i].ldamages.end() )
			return ( 1 << i ) | FENCE_TYPE_LDAMAGE;
		if ( std::find(dirs[i].rdamages.begin(), dirs[i].rdamages.end(), nIndex) != dirs[i].rdamages.end() )
			return ( 1 << i ) | FENCE_TYPE_RDAMAGE;
		if ( std::find(dirs[i].cdamages.begin(), dirs[i].cdamages.end(), nIndex) != dirs[i].cdamages.end() )
			return ( 1 << i ) | FENCE_TYPE_CDAMAGE;
	}
	return -1;
}
const int SFenceRPGStats::GetIndexFromType( const int nType, int *pCurRandomSeed ) const
{
	const int nDirection = GetMSB( nType & 0xffff );
	NI_ASSERT_T( (nDirection >= 0) && (nDirection < dirs.size()), NStr::Format("Wrong direction %d in fence \"%s\" (must be [0..%d])", nDirection, szParentName.c_str(), dirs.size()) );
	switch ( nType & 0xffff0000 ) 
	{
		case FENCE_TYPE_NORMAL:
			return GetCenterIndex( nDirection, pCurRandomSeed );
		case FENCE_TYPE_LDAMAGE:
			return GetLDamageIndex( nDirection, pCurRandomSeed );
		case FENCE_TYPE_RDAMAGE:
			return GetRDamageIndex( nDirection, pCurRandomSeed );
		case FENCE_TYPE_CDAMAGE:
			return GetCDamageIndex( nDirection, pCurRandomSeed );
	}
	NI_ASSERT_T( false, NStr::Format("Unknown type (0x%x) in fence \"%s\"", nType, szParentName.c_str()) );
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SFenceRPGStats::CalculateCheckSum() const
{
	uLong checkSum = 0;

	for ( int i = 0; i < stats.size(); ++i )
		checkSum = GetCRC( stats[i].CalculateCheckSum(), checkSum );
	for ( int i = 0; i < dirs.size(); ++i )
		checkSum = GetCRC( dirs[i].CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SStaticObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** Squad RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSquadRPGStats::SFormation::SFormation() 
: cLieFlag( 0 ), fSpeedBonus( 1 ), fDispersionBonus( 1 ), fFireRateBonus( 1 ), fRelaxTimeBonus( 1 ), fCoverBonus( 1 ), 
  fVisibleBonus( 1 ), changesByEvent( 1 )
{ 
	type = DEFAULT;
	changesByEvent[HIT_NEAR] = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSquadRPGStats::SFormation::SEntry::RetrieveShortcuts( IObjectsDB *pGDB )
{
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( szSoldier.c_str() );
	pSoldier = static_cast<const SInfantryRPGStats*>( pGDB->GetRPGStats( pDesc ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSquadRPGStats::SFormation::RetrieveShortcuts( IObjectsDB *pGDB )
{
	for ( std::vector<SEntry>::iterator it = order.begin(); it != order.end(); ++it )
		it->RetrieveShortcuts( pGDB );
}
void SSquadRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	members.clear();
	for ( std::vector<std::string>::const_iterator it = memberNames.begin(); it != memberNames.end(); ++it )
	{
		const SGDBObjectDesc *pDesc = pGDB->GetDesc( it->c_str() );
		members.push_back( static_cast<const SInfantryRPGStats*>( pGDB->GetRPGStats( pDesc ) ) );
	}
	//
	for ( std::vector<SFormation>::iterator it = formations.begin(); it != formations.end(); ++it )
		it->RetrieveShortcuts( pGDB );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SSquadRPGStats::SFormation::SEntry::ToAIUnits()
{
	Vis2AI( &vPos, vPos );
	nDir = int( fDir / 360.0f * 65535.0f ) % 65535;
	return true;
}
bool SSquadRPGStats::SFormation::ToAIUnits()
{
	std::for_each( order.begin(), order.end(), [](SEntry& entry){ entry.ToAIUnits(); } );
	return true;
}
void SSquadRPGStats::ToAIUnits()
{
	SHPObjectRPGStats::ToAIUnits();
	std::for_each( formations.begin(), formations.end(), [](SFormation& form){ form.ToAIUnits(); } );
	// set available user actions
	for ( std::vector<SFormation>::const_iterator it = formations.begin(); it != formations.end(); ++it )
		availActions.SetAction( USER_ACTION_FORMATION_0 + it->type );
	// each squad can follow and can be followed by
	availActions.SetAction( USER_ACTION_FOLLOW );
	availExposures.SetAction( USER_ACTION_FOLLOW );
	//
	if ( memberNames.size() == 1 ) 
	{
		availActions.RemoveAction( USER_ACTION_DISBAND_SQUAD );
		availActions.SetAction( USER_ACTION_FORM_SQUAD );
	}
	else if ( memberNames.size() > 1 )
	{
		availActions.SetAction( USER_ACTION_DISBAND_SQUAD );
		availActions.SetAction( USER_ACTION_FORM_SQUAD );
	}
	else if ( memberNames.empty() ) 
		availActions.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SSquadRPGStats::Validate()
{
	// проверить, что в каждой формации присутствуют все юниты из взвода
	// 
	std::vector<std::string> squad = memberNames;
	std::sort( squad.begin(), squad.end() );
	//
	for ( std::vector<SFormation>::const_iterator it = formations.begin(); it != formations.end(); ++it )
	{
		NI_ASSERT_TF( it->order.size() == memberNames.size(), "size mismatch in formation order => wrong formation order", return false );
		std::vector<std::string> formation;
		formation.reserve( it->order.size() );
		for ( std::vector<SFormation::SEntry>::const_iterator entry = it->order.begin(); entry != it->order.end(); ++entry )
			formation.push_back( entry->szSoldier );
		//
		std::sort( formation.begin(), formation.end() );
		//
		NI_ASSERT_TF( squad == formation, "formation doen't contain all squad members!", return false );

		NI_ASSERT_TF( it->fSpeedBonus >= 0, NStr::Format( "Wrong formation (squad %s) speed bonus (%g)", szKeyName, it->fSpeedBonus ), return false );
		NI_ASSERT_TF( it->fDispersionBonus >= 0, NStr::Format( "Wrong formation (squad %s) fDispersionBonus bonus (%g)", szKeyName, it->fDispersionBonus ), return false );
		NI_ASSERT_TF( it->fFireRateBonus >= 0, NStr::Format( "Wrong formation (squad %s) fFireRateBonus bonus (%g)", szKeyName, it->fFireRateBonus ), return false );
		NI_ASSERT_TF( it->fRelaxTimeBonus >= 0, NStr::Format( "Wrong formation (squad %s) fRelaxTimeBonus bonus (%g)", szKeyName, it->fRelaxTimeBonus ), return false );
		NI_ASSERT_TF( it->fCoverBonus >= 0, NStr::Format( "Wrong formation (squad %s) fCoverBonus bonus (%g)", szKeyName, it->fCoverBonus ), return false );
		NI_ASSERT_TF( it->fVisibleBonus >= 0, NStr::Format( "Wrong formation (squad %s) fVisibleBonus bonus (%g)", szKeyName, it->fVisibleBonus ), return false );
	}
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSquadRPGStats::SFormation::SEntry::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Soldier", &szSoldier );
	saver.Add( "Pos", &vPos );
	saver.Add( "Dir", &fDir );
	return 0;
}

const uLong SSquadRPGStats::SFormation::SEntry::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, vPos );
	CopyToBuf( &buf, fDir );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	checkSum = GetCRC( pSoldier->CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSquadRPGStats::SFormation::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Type", &type );
	saver.Add( "Order", &order );
	saver.Add( "LieFlag", &cLieFlag );
	saver.Add( "SpeedBonus", &fSpeedBonus );
	saver.Add( "DispersionBonus", &fDispersionBonus );
	saver.Add( "FireRateBonus", &fFireRateBonus );
	saver.Add( "RelaxTimeBonus", &fRelaxTimeBonus );
	saver.Add( "CoverBonus", &fCoverBonus );
	saver.Add( "VisibleBonus", &fVisibleBonus	);
	
	saver.Add( "ChangesByEvent", &changesByEvent );

	return 0;
}
const uLong SSquadRPGStats::SFormation::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, type );
	CopyToBuf( &buf, cLieFlag );
	CopyToBuf( &buf, fSpeedBonus );
	CopyToBuf( &buf, fDispersionBonus );
	CopyToBuf( &buf, fFireRateBonus );
	CopyToBuf( &buf, fRelaxTimeBonus );
	CopyToBuf( &buf, fCoverBonus );
	CopyToBuf( &buf, fVisibleBonus );
	for ( int i = 0; i < changesByEvent.size(); ++i )
		CopyToBuf( &buf, changesByEvent[i] );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < order.size(); ++i )
		checkSum = GetCRC( order[i].CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSquadRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Icon", &szIcon );
	saver.Add( "Type", &type );
	saver.Add( "Members", &memberNames );
	saver.Add( "Formations", &formations );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SSquadRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, type );
	CopyToBuf( &buf, availActions.GetActions( 0 ) );
	CopyToBuf( &buf, availActions.GetActions( 1 ) );
	CopyToBuf( &buf, availExposures.GetActions( 0 ) );
	CopyToBuf( &buf, availExposures.GetActions( 1 ) );
	
	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < formations.size(); ++i )
		checkSum = GetCRC( formations[i].CalculateCheckSum(), checkSum );

	for ( int i = 0; i < members.size(); ++i )
		checkSum = GetCRC( members[i]->CalculateCheckSum(), checkSum );

	checkSum = GetCRC( SHPObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** mine RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMineRPGStats::SMineRPGStats()
: SObjectBaseRPGStats( "Mine" ), szWeapon( "generic" ), szFlagModel( "1" )
{
	fWeight = 0;
	type = INFANTRY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SMineRPGStats::RetrieveShortcuts( IObjectsDB *pGDB )
{
	pWeapon = static_cast<const SWeaponRPGStats*>( pGDB->GetAddStats( szWeapon.c_str(), IObjectsDB::WEAPON ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMineRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SObjectBaseRPGStats*>(this) );
	saver.Add( "Weapon", &szWeapon );
	saver.Add( "Weight", &fWeight );
	saver.Add( "FlagModel", &szFlagModel );

	type = fWeight == 0 ? INFANTRY : TECHNICS;

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uLong SMineRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, type );
	CopyToBuf( &buf, fWeight );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	checkSum = GetCRC( pWeapon->CalculateCheckSum(), checkSum );
	checkSum = GetCRC( SObjectBaseRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** bridge RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SBridgeRPGStats::SSegmentRPGStats::SSegmentRPGStats()
{
	vOrigin = VNULL2;
	vVisOrigin = VNULL2;
	vRelPos = VNULL3;
	nFrameIndex = 0;
}
SBridgeRPGStats::SBridgeRPGStats()
: SStaticObjectRPGStats( "Bridge" ), dirExplosions( 5 )
{
	direction = VERTICAL;
	dwAIClasses = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SBridgeRPGStats::SSegmentRPGStats::ToAIUnits()
{
	Vis2AI( &vOrigin );
	Vis2AI( &vVisOrigin );
	// tiles to world points
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SBridgeRPGStats::ToAIUnits()
{
	SStaticObjectRPGStats::ToAIUnits();
	std::for_each( segments.begin(), segments.end(), std::mem_fun_ref(SSegmentRPGStats::ToAIUnits) ); 
	std::for_each( firePoints.begin(), firePoints.end(), std::mem_fun_ref(SFirePoint::ToAIUnits) ); 
	std::for_each( smokePoints.begin(), smokePoints.end(), std::mem_fun_ref(SFirePoint::ToAIUnits) ); 
	std::for_each( dirExplosions.begin(), dirExplosions.end(), std::mem_fun_ref(SDirectionExplosion::ToAIUnits) ); 
	// CRAP{ сейчас не проставляется AIclasses для мостов
	dwAIClasses = 0;
	// CRAP}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBridgeRPGStats::SSegmentRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Passability", &passability );
	saver.Add( "Origin", &vOrigin );
	saver.Add( "Visibility", &visibility );
	saver.Add( "VisOrigin", &vVisOrigin );

	saver.Add( "Model", &szModel );
	saver.Add( "FrameIndex", &nFrameIndex );
	saver.Add( "RelPos", &vRelPos );

	return 0;
}

const uLong SBridgeRPGStats::SSegmentRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, eType );
	for ( int i = 0; i < passability.GetSizeY(); ++i )
	{
		for ( int j = 0; j < passability.GetSizeX(); ++j )
			CopyToBuf( &buf, passability[i][j] );
	}
	CopyToBuf( &buf, vOrigin );
	for ( int i = 0; i < visibility.GetSizeY(); ++i )
	{
		for ( int j = 0; j < visibility.GetSizeX(); ++j )
			CopyToBuf( &buf, visibility[i][j] );
	}
	CopyToBuf( &buf, vVisOrigin );
	CopyToBuf( &buf, nFrameIndex );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	return checkSum;
}

int SBridgeRPGStats::SSpan::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Slab", &nSlab );
	saver.Add( "BackGirder", &nBackGirder );
	saver.Add( "FrontGirder", &nFrontGirder );
	saver.Add( "Width", &fWidth );
	saver.Add( "Length", &fLength );
	return 0;
}

int SBridgeRPGStats::SDamageState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Spans", &spans );
	saver.Add( "Begins", &begins );
	saver.Add( "Lines", &lines );
	saver.Add( "Ends", &ends );
	return 0;
}

const uLong SBridgeRPGStats::CalculateCheckSum() const
{
	SCheckSumBufferStorage buf;

	CopyToBuf( &buf, direction );

	uLong checkSum = crc32( 0, &(buf.buf[0]), buf.nCnt );

	for ( int i = 0; i < segments.size(); ++i )
		checkSum = GetCRC( segments[i].CalculateCheckSum(), checkSum );
	checkSum = GetCRC( SStaticObjectRPGStats::CalculateCheckSum(), checkSum );

	return checkSum;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													Fire point															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBridgeRPGStats::SFirePoint::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &fDirection );
	saver.Add( "VerticalAngle", &fVerticalAngle );
	saver.Add( "FireEffect", &szFireEffect );
	// vis info
	saver.Add( "PicturePosition", &vPicturePosition );
	saver.Add( "WorldPosition", &vWorldPosition );
	return 0;
}
SBridgeRPGStats::SFirePoint::SFirePoint() 
: vPos( VNULL3 ), vPicturePosition( VNULL2 ), vWorldPosition( VNULL3 )
{
	fDirection = 0;
	fVerticalAngle = 78.0f;
}
bool SBridgeRPGStats::SFirePoint::ToAIUnits()
{
	fDirection = ToRadian( fDirection );
	fVerticalAngle = ToRadian( fVerticalAngle );
	Vis2AI( &vPos );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*													Direction explosion											*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SBridgeRPGStats::SDirectionExplosion::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Position", &vPos );
	saver.Add( "Direction", &fDirection );
	saver.Add( "VerticalAngle", &fVerticalAngle );
	// vis info
	saver.Add( "PicturePosition", &vPicturePosition );
	saver.Add( "WorldPosition", &vWorldPosition );
	return 0;
}
SBridgeRPGStats::SDirectionExplosion::SDirectionExplosion() 
: vPos( VNULL3 ), vPicturePosition( VNULL2 ), vWorldPosition( VNULL3 )
{
	fDirection = 0;
	fVerticalAngle = 78.0f;
}
bool SBridgeRPGStats::SDirectionExplosion::ToAIUnits()
{
	fDirection = ToRadian( fDirection );
	fVerticalAngle = ToRadian( fVerticalAngle );
	Vis2AI( &vPos );

	return true;
}

int SBridgeRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<SStaticObjectRPGStats*>(this) );
	saver.Add( "Direction", &direction );
	saver.Add( "Segments", &segments );
	// main stats (for backward compatibility)
	saver.AddTypedSuper( &states[0] );
	saver.Add( "Damaged", &states[1] );
	saver.Add( "Destroyed", &states[2] );

	saver.Add( "FirePoints", &firePoints );
	saver.Add( "SmokePoints", &smokePoints );
	saver.Add( "SmokeEffect", &szSmokeEffect );
	saver.Add( "DirExplosions", &dirExplosions );
	saver.Add( "DirExplosionEffect", &szDirExplosionEffect );
	
	// set segment types
	if ( saver.IsReading() )
	{
		for ( int i = 0; i < 3; ++i )
		{
			for ( std::vector<SSpan>::const_iterator it = states[i].spans.begin(); it != states[i].spans.end(); ++it )
			{
				segments[it->nSlab].eType = SSegmentRPGStats::SLAB;
				if ( it->nBackGirder >= 0 ) 
					segments[it->nBackGirder].eType = SSegmentRPGStats::GIRDER;
				if ( it->nFrontGirder >= 0 ) 
					segments[it->nFrontGirder].eType = SSegmentRPGStats::GIRDER;
			}
		}
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int SBridgeRPGStats::GetTypeFromIndex( const int nIndex, const int nDamageState ) const
{
	if ( std::find(states[nDamageState].begins.begin(), states[nDamageState].begins.end(), nIndex) != states[nDamageState].begins.end() ) 
		return BRIDGE_SPAN_TYPE_BEGIN;
	if ( std::find(states[nDamageState].lines.begin(), states[nDamageState].lines.end(), nIndex) != states[nDamageState].lines.end() ) 
		return BRIDGE_SPAN_TYPE_CENTER;
	if ( std::find(states[nDamageState].ends.begin(), states[nDamageState].ends.end(), nIndex) != states[nDamageState].ends.end() ) 
		return BRIDGE_SPAN_TYPE_END;
	//
	NI_ASSERT_T( false, NStr::Format("Unknown span index %d in bridge \"%s\"", nIndex, szParentName.c_str()) );
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int SBridgeRPGStats::GetIndexFromTypeLocal( const int nType, const int nDamageState, int *pCurRandomSeed ) const
{
	switch ( nType ) 
	{
		case BRIDGE_SPAN_TYPE_BEGIN:
			return GetRandomBeginIndex( -1, nDamageState, pCurRandomSeed );
		case BRIDGE_SPAN_TYPE_CENTER:
			return GetRandomLineIndex( -1, nDamageState, pCurRandomSeed );
		case BRIDGE_SPAN_TYPE_END:
			return GetRandomEndIndex( -1, nDamageState, pCurRandomSeed );
	}
	NI_ASSERT_T( false, NStr::Format("Unknown span type %d of damage state %d for the bridge \"%s\"", nType, nDamageState, szParentName.c_str()) );
	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** sound
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSoundRPGStats::SSound::SSound()
{  
	fMinDist = 1;
	fMaxDist = 100;
	fProbability = 1;
	bPeacefull = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSoundRPGStats::SSound::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "MinDist", &fMinDist );
	saver.Add( "MaxDist", &fMaxDist );
	saver.Add( "Probability", &fProbability );
	saver.Add( "Peacefull", &bPeacefull );
	saver.Add( "SoundPath", &szPathName );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSoundRPGStats::SSoundRPGStats()
: SCommonRPGStats( "Sound" ), bLooped( false )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSoundRPGStats::~SSoundRPGStats()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSoundRPGStats::ToAIUnits()
{
	SCommonRPGStats::ToAIUnits();
	float fDenominator = 0;
	for ( std::vector<SSound>::const_iterator it = sounds.begin(); it != sounds.end(); ++it )
		fDenominator += it->fProbability;
	NI_ASSERT_T( fabsf(fDenominator) > 1e-3, NStr::Format("Too small total probability for sounds set \"%s\"", szParentName.c_str()) );
	fDenominator = 1.0f / fDenominator;
	for ( std::vector<SSound>::iterator it = sounds.begin(); it != sounds.end(); ++it )
		it->fProbability *= fDenominator;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSoundRPGStats::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Sounds", &sounds );
	saver.Add( "Looped", &bLooped );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SSoundRPGStats::GetRandomSoundIndex() const
{
	const float fRand = 1.0f * rand() / RAND_MAX;
	float fSum = 0;
	for ( int i = 0; i < sounds.size(); ++i )
	{
		fSum += sounds[i].fProbability;
		if ( fSum > fRand ) return i;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** exp levels
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAIExpLevel::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Type", &szTypeName );
	saver.Add( "Levels", &levels );
	if ( saver.IsReading() ) 
		eType = (EUnitRPGType)pAutomagic->ToInt( szTypeName.c_str() );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
