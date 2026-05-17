#ifndef __RPGSTATS_H__
#define __RPGSTATS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <string>
#include <unordered_map>
#include "..\AILogic\AIConsts.h"
#include "..\Misc\BitData.h"
#include "..\StreamIO\RandomGen.h"
#include "..\zlib\zlib.h"
#include "iMain.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetRandom( int nAverage, int nRandom )
{
	return nRandom <= 0 ? nAverage : RandomCheck( nAverage - nRandom, nAverage + nRandom );
}
inline float GetRandom( float fAverage, int nRandom )
{
	return nRandom <= 0 ? fAverage : RandomCheck( fAverage - float(nRandom), fAverage + float(nRandom) );
}
inline int GetPositiveRandom( int nAverage, int nRandom )
{
	return Max( 0, GetRandom( nAverage, nRandom ) );
}
inline float GetPositiveRandom( float fAverage, int nRandom )
{
	return Max( 0.0f, GetRandom( fAverage, nRandom ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** effects
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFlashEffect
{
	int nPower;
	int nDuration;
	//
	SFlashEffect() : nPower ( -1 ), nDuration( 0 ) {  }
	const bool HasFlash() const { return (nPower > 0) && (nDuration > 10); }
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Power", &nPower );
		saver.Add( "Duration", &nDuration );
		return 0;
	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** user actions (64 bits)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUserActions
{
	DWORD actions[2];
public:
	CUserActions() { Clear(); }
	CUserActions( const DWORD _actions[2] ) { actions[0] = _actions[0]; actions[1] = _actions[1]; }
	CUserActions( const CUserActions &userActions ) { actions[0] = userActions.actions[0]; actions[1] = userActions.actions[1]; }
	//
	const CUserActions& operator=( const DWORD _actions[2] ) { actions[0] = _actions[0]; actions[1] = _actions[1]; return *this; }
	const CUserActions& operator=( const CUserActions &_actions ) { return this->operator=( _actions.actions ); }
	//
	bool operator==( const DWORD _actions[2] ) const { return (_actions[0] == actions[0]) && (_actions[1] == actions[1]); }
	bool operator==( const CUserActions &_actions ) const { return this->operator==( _actions.actions ); }
	bool operator!=( const DWORD _actions[2] ) const { return (_actions[0] != actions[0]) || (_actions[1] != actions[1]); }
	bool operator!=( const CUserActions &_actions ) const { return this->operator!=( _actions.actions ); }
	//
	void operator|=( const CUserActions &ua ) { actions[0] |= ua.actions[0]; actions[1] |= ua.actions[1]; }
	void operator&=( const CUserActions &ua ) { actions[0] &= ua.actions[0]; actions[1] &= ua.actions[1]; }
	//
	void Clear() { actions[0] = actions[1] = 0; }
	bool IsEmpty() const { return ( actions[0] | actions[1] ) == 0; }
	//
	const bool HasAction( const int nAction ) const
	{
		NI_ASSERT_SLOW_T( (nAction >= 0) && (nAction <= 63), NStr::Format("Invalid action %d must be in [0..63]", nAction) );
		const int nIndex = nAction >> 5;
		return actions[nIndex] & ( 1UL << (nAction - nIndex*32) );
	}
	void SetAction( const int nAction )
	{
		NI_ASSERT_SLOW_T( (nAction >= 0) && (nAction <= 63), NStr::Format("Invalid action %d must be in [0..63]", nAction) );
		const int nIndex = nAction >> 5;
		actions[nIndex] |= ( 1UL << (nAction - nIndex*32) );
	}
	void RemoveAction( const int nAction )
	{
		NI_ASSERT_SLOW_T( (nAction >= 0) && (nAction <= 63), NStr::Format("Invalid action %d must be in [0..63]", nAction) );
		const int nIndex = nAction >> 5;
		actions[nIndex] &= ~( 1UL << (nAction - nIndex*32) );
	}
	//
	DWORD GetActions( const int nIndex ) const { return actions[nIndex]; }
	void GetActions( DWORD *_actions ) const { _actions[0] = actions[0]; _actions[1] = actions[1]; }
	void GetActions( CUserActions *pActions ) const { GetActions( pActions->GetBuffer() ); }
	void SetActions( const DWORD _actions[2] ) { actions[0] = _actions[0]; actions[1] = _actions[1]; }
	DWORD* GetBuffer() { return &(actions[0]); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** common RPG stats (base)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCommonRPGStats : public IGDBObject
{
private:
	bool bCheckSumInitialized;
	uLong checkSum;												// stats checksum
public:
	std::string szKeyName;								// ключевое имя данного объекта - в основном используется в редакторе
	std::string szParentName;							// parent object key name. заполняется динамически при загрузке объекта
	std::string szStatsType;							// тип статсов - "crap", "mech", "infantry", "building", "weapon"
	//
	SCommonRPGStats() : bCheckSumInitialized( false ) {}
	SCommonRPGStats( const char *pszStatsType ) : szStatsType( pszStatsType ), bCheckSumInitialized( false ) {}
	SCommonRPGStats( const std::string &_szStatsType ) : szStatsType( _szStatsType ), bCheckSumInitialized( false ) {}
	virtual ~SCommonRPGStats() {  }

	virtual const char* STDCALL GetName() const { return szStatsType.c_str(); }
	virtual const char* STDCALL GetParentName() const { return szParentName.c_str(); }

	// преобразовать из человеческих единиц в AI
	virtual void STDCALL ToAIUnits() {}
	//
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB ) {  }
	// проверка статсов на корректность
	virtual bool STDCALL Validate() { return true; }
	//
	virtual const uLong STDCALL GetCheckSum() const
	{
		NI_ASSERT_T( bCheckSumInitialized, "Check sum hasn't been initialized" );
		return checkSum;
	}
	//
	virtual const uLong STDCALL GetCheckSum()
	{ 
		if ( bCheckSumInitialized ) 
			return checkSum; 
		else
		{
			bCheckSumInitialized = true;
			return ( checkSum = CalculateCheckSum() );
		}
	}

	virtual int STDCALL operator&( IDataTree &ss );

	virtual const uLong STDCALL CalculateCheckSum() const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** basic RPG interfaces
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// directions
// не менять нумерацию направлений!!!
enum EArmorDirection
{
	RPG_FRONT		= 0,
	RPG_LEFT		= 1,
	RPG_BACK		= 2,
	RPG_RIGHT		= 3,
	RPG_TOP			= 4,
	RPG_BOTTOM	= 5,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDefenseRPGStats
{
	int nArmorMin;											// min armor strength
	int nArmorMax;											// max armor strength
	float fSilhouette;										// silhuettee for strike probability
	//
	SDefenseRPGStats() : nArmorMin( 40 ), nArmorMax( 90 ), fSilhouette( 1.0f ) {  }
	//
	int operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHPObjectRPGStats : public SCommonRPGStats
{
	float fMaxHP;													// максимальное здоровье объекта
	std::vector<float> damagedHPs;				// последовательность процентов здоровья, при которых происходит смена состояния объекта
	float fRepairCost;										// repair cost (in RU) to repair ONE hit point!!!
	SDefenseRPGStats defences[6];					// defense stats for each direction
	//
	SHPObjectRPGStats( const char *pszType ) : 
		SCommonRPGStats( pszType ), fMaxHP( 100 ), fRepairCost( 1 ) {}
	
	virtual ~SHPObjectRPGStats() {}

	//
	virtual void STDCALL ToAIUnits();
	//
	float GetMapHP() const { return fMaxHP; }
	float GetHP( const float fHPPercentage ) const { return fMaxHP * fHPPercentage; }
	int GetDamagedState( const float fHPPercentage ) const
	{
		int i = damagedHPs.size();
		while ( --i >= 0 )
		{
			if ( fHPPercentage > damagedHPs[i] )
				return i;
		}
		return 0;
	}
	virtual int GetArmor( const int n ) const { return ( defences[n].nArmorMin + defences[n].nArmorMax ) / 2; }
	virtual int GetMinPossibleArmor( const int n ) const { return defences[n].nArmorMin; }
	virtual int GetMaxPossibleArmor( const int n ) const { return defences[n].nArmorMax; }
	virtual int GetRandomArmor( const int n ) const { return RandomCheck( defences[n].nArmorMin, defences[n].nArmorMax ); }
	//
	virtual const CUserActions* GetUserActions( bool bActionsBy ) const { return 0; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAIClass
{
	AI_CLASS_WHEEL			= 0x00000001,
	AI_CLASS_HALFTRACK	= 0x00000002,
	AI_CLASS_TRACK			= 0x00000004,
	AI_CLASS_HUMAN			= 0x00000008,

	AI_CLASS_TECHNICS		= AI_CLASS_WHEEL | AI_CLASS_HALFTRACK | AI_CLASS_TRACK,
	AI_CLASS_ANY				= AI_CLASS_TECHNICS | AI_CLASS_HUMAN,

	AI_CLASS_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStaticObjectRPGStats : public SHPObjectRPGStats
{
	DWORD dwAIClasses;										// AI classes, which cannot go through this object
	bool bBurn;														// горит ли здание после достижения 50% hp
	std::string szEffectExplosion;				// death with explosion (projectile hit)
	std::string szEffectDeath;						// "silent" death - squshed by tank

	SStaticObjectRPGStats( const char *pszType );
	virtual ~SStaticObjectRPGStats() {  }
	//
	virtual const CVec2& STDCALL GetOrigin( const int nIndex = -1 ) const = 0;
	virtual const CArray2D<BYTE>& STDCALL GetPassability( const int nIndex = -1 ) const = 0;
	virtual const CVec2& STDCALL GetVisOrigin( const int nIndex = -1 ) const = 0;
	virtual const CArray2D<BYTE>& STDCALL GetVisibility( const int nIndex = -1 ) const = 0;
	//
	virtual void STDCALL ToAIUnits();
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** weapon and base gun stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWeaponRPGStats : public SCommonRPGStats
{
	struct SShell
	{
		enum ETrajectoryType
		{
			TRAJECTORY_LINE				= 0,
			TRAJECTORY_HOWITZER		= 1,
			TRAJECTORY_BOMB				= 2,
			TRAJECTORY_CANNON			= 3,
			TRAJECTORY_ROCKET			= 4,
			TRAJECTORY_GRENADE		= 5
		};
		enum EDamageType
		{
			DAMAGE_HEALTH = 0,								// ordinary shell with physical damage
			DAMAGE_MORALE = 1,								// morale shell - does damage to unit's morale
			DAMAGE_FOG		= 2,								// fog screen shell
		};
		// shell type params
		EDamageType eDamageType;
		int nPiercing;											// (огурцы) бронепробиваемость
		int nPiercingRandom;								// (огурцы) random на бронепробиваемость
		float fDamagePower;									// (HP <=> HP) собственно, вред...
		int	nDamageRandom;									// (HP <=> HP) random на вред
		float fArea, fArea2;								// (метры <=> AI точки) радиус зоны покрытия от одного снаряда
		float fSpeed;												// (метры/секунду <=> AI точки/тик) скорость полёта снаряда
		float fTraceSpeedCoeff;             // коэфф. скорости трассера относительно скорости полета снаряда
		float fTraceProbability;            // вероятность появления трассера при выстреле [0;1]
		float fDetonationPower;							// степень дрожания камеры при разрыве снаряда
		ETrajectoryType trajectory;					// тип траектории
		float fBrokeTrackProbability;				// вероятность разбить трак
		// параметры для визуализации и озвучивания эффектов
		std::string szFireSound;						// звук при выстреле пехотинца
		std::string szEffectGunFire;				// выстрел из пушки этим снарядом
		std::string szEffectTrajectory;			// полёт снаряда (дым и т.д.)
		std::string szEffectHitDirect;			// прямое попадание
		std::string szEffectHitMiss;				// попали визуально, но промазали по combat system
		std::string szEffectHitReflect;			// попали, но броню не пробили
		std::string szEffectHitGround;			// попали в землю
		std::string szEffectHitWater;				// попали в воду
		std::string szEffectHitAir;					// попали в воздух - для зенитной артиллерии при заградительном огне
		std::vector<std::string> szCraters;	// craters after explosion
		SFlashEffect flashFire;							// flash on firing
		SFlashEffect flashExplosion;				// flash on explosion

		union { float fFireRate; int nFireRate; };		// (пули/минуту <=> ticks между вылетами пуль в очереди) скорострельность
		union { float fRelaxTime; int nRelaxTime; };	// (секунды <=> ticks) время на отходняк после выстрела
		
		CArray1Bit specials;								// спец. эффекты
		//
		SShell();
		//
		float GetRandomDamage() const { return GetPositiveRandom( fDamagePower, nDamageRandom ); }
		int GetRandomPiercing() const { return GetPositiveRandom( nPiercing, nPiercingRandom ); }
		int GetMaxPossiblePiercing() const { return nPiercing + nPiercingRandom; }
		int GetMinPossiblePiercing() const { return Max( 0, nPiercing - nPiercingRandom ); }
		const bool HasCraters() const { return !szCraters.empty(); }
		const std::string& GetRandomCrater() const { return szCraters[rand() % szCraters.size()]; }
		// преобразовать из человеческих единиц в AI
		bool ToAIUnits();
		//
		int operator&( IDataTree &ss );

		const uLong CalculateCheckSum() const;
	};
	//
	float fDispersion;										// (метры <=> AI точки) "попадучесть" (со слов Толстого)
	// NOTE{ прицелится (aiming) - пострелять очередью (nAmmoPerBurst*fFireRate) - отходняк (relax). если цель не сдвинулась, то aiming пропускаетс
	union { float fAimingTime; int nAimingTime; };// (секунды <=> ticks) время на прицеливание
	// NOTE}
	int nAmmoPerBurst;										// (штуки <=> штуки) сколько потронов уходит на очередь
	float fRangeMax;											// (метры <=> AI точки) как далеко бъёт
	float fRangeMin;											// (метры <=> AI точки) ближе не может стрелять

	// CRAP{ принадлежит gun'у, а не weapon'у непосредственно
	int nCeiling;													// (уровни <=> уровни) на сколько пушка бъёт вверх
	// CRAP}
	float fRevealRadius;									// радиус обнаружения для антиартиллерийной борьбы ( ??? <=> AI точки )

	WORD wDeltaAngle;											// (градусы <=> градусы65535) на сколько наводчик может искривить пушку силой своей воли. половина угла
	std::vector<SShell> shells;						// все возможные типы снарядов для этой пушки
	//
	SWeaponRPGStats();
	virtual ~SWeaponRPGStats() {  }

	virtual void STDCALL ToAIUnits();
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBaseGunRPGStats
{
	std::string szWeapon;									// собственно пушка (ссылка)
	// приоритет пушки: 0 - нужно остановиться, чтобы пострелять, > 0 - всё остальное, останавливаться не надо
	int nPriority;												// priority of this gun
	bool bPrimary;												// primary or secondary gun (для отображения снарядов)
	const SWeaponRPGStats *pWeapon;				// weapon shortcut
	int nAmmo;														// max amount of ammo in this gun
	WORD wDirection;											// this gun direction
	float fReloadCost;										// reload cost (in RU) to reload ONE shell of any type for this gun
	//
	SBaseGunRPGStats();
	//
	virtual bool STDCALL RetrieveShortcuts( IObjectsDB *pGDB );

	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** static object stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectBaseRPGStats : public SStaticObjectRPGStats
{
	CArray2D<BYTE> passability;						// проходимость AI тайлов этого объекта
	CVec2 vOrigin;												// нулевая точка объекта для passability
	CArray2D<BYTE> visibility;						// степень поглощения силы взгляда
	CVec2 vVisOrigin;											// нулевая точка объекта по visibility
	//
	std::string szAmbientSound;						// ambient sounds set
	std::string szCycledSound;						// cycled sounds set

	//
	SObjectBaseRPGStats( const char *pszType );
	virtual ~SObjectBaseRPGStats() {  }
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual const CVec2& STDCALL GetOrigin( const int nIndex = -1 ) const { return vOrigin; }
	virtual const CArray2D<BYTE>& STDCALL GetPassability( const int nIndex = -1 ) const { return passability; }

	virtual const CVec2& STDCALL GetVisOrigin( const int nIndex = -1 ) const { return vVisOrigin; }
	virtual const CArray2D<BYTE>& STDCALL GetVisibility( const int nIndex = -1 ) const { return visibility; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerraObjSetRPGStats : public SStaticObjectRPGStats
{
	struct SSegment
	{
		CArray2D<BYTE> passability;						// проходимость AI тайлов этого объекта
		CVec2 vOrigin;												// нулевая точка объекта для passability
		CArray2D<BYTE> visibility;						// степень поглощения силы взгляда
		CVec2 vVisOrigin;											// нулевая точка объекта по visibility
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	//
	std::vector<SSegment> segments;					// all available segments
	std::vector<int> fronts;								// segments to use as a front and side
	std::vector<int> backs;									// segments to use as a back
	//
	STerraObjSetRPGStats() : SStaticObjectRPGStats( "TerraObj" ) {  }
	virtual ~STerraObjSetRPGStats() {  }
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual const CVec2& STDCALL GetOrigin( const int nIndex = -1 ) const { return segments[nIndex].vOrigin; }
	virtual const CArray2D<BYTE>& STDCALL GetPassability( const int nIndex = -1 ) const { return segments[nIndex].passability; }

	virtual const CVec2& STDCALL GetVisOrigin( const int nIndex = -1 ) const { return segments[nIndex].vVisOrigin; }
	virtual const CArray2D<BYTE>& STDCALL GetVisibility( const int nIndex = -1 ) const { return segments[nIndex].visibility; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectRPGStats : public SObjectBaseRPGStats
{
	SObjectRPGStats() : SObjectBaseRPGStats( "ObjDesc" ) {  }
	virtual ~SObjectRPGStats() {  }
	//
	virtual int STDCALL operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBuildingRPGStats : public SObjectBaseRPGStats
{
	enum EType
	{
		TYPE_BULDING					= 0,
		TYPE_MAIN_RU_STORAGE	= 1,
		TYPE_TEMP_RU_STORAGE	= 2,
		TYPE_DOT							= 3
	};
	// entrance description
	struct SEntrance
	{
		CVec3 vPos;													// entrance placement
		bool bStormable;										// is this entrance stormable?

		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	// shoot slot description
	struct SSlot
	{
		CVec3 vPos;													// position
		union { float fDirection; WORD wDirection; };	// (угол <=> угол65536) fire/sight direction
		union { float fAngle; WORD wAngle; };					// (угол <=> угол65536) fire/sight angle
		SHMatrix matDirection;							// direction (as wDirection), but ready for visualization purposes
		float fSightMultiplier;							// sight multiplier
		float fCoverage;										// coverage for the unit in this slot [0..1]
		//
		SBaseGunRPGStats gun;								// mounted gun
		union { float fRotationSpeed; WORD wRotationSpeed; };				// (секунды на полный оборот <=> градусы65535/тик) vertical rotation speed
		// vis info
		bool bBeforeSprite;									// is this slot before main sprite?
		bool bShowFlashes;									// do we need to show flashes from this position?
		CVec2 vPicturePosition;							// 2D picture position
		CVec3 vWorldPosition;								// 3D position, calculated from picture one
		//
		SSlot();

		bool ToAIUnits();
		bool Validate();
		bool RetrieveShortcuts( IObjectsDB *pGDB );
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	// fire point description, огонь в здании
	struct SFirePoint
	{
		CVec3 vPos;													// position
		float fDirection;										// (угол <=> угол в радианах) direction
		float fVerticalAngle;								// (угол <=> угол в радианах) angle from horizontal plane in the vertical plane, which crosses 'fDirection'
		std::string szFireEffect;						// particle fire effect
		// vis info
		CVec2 vPicturePosition;							// 2D picture position
		CVec3 vWorldPosition;								// 3D position, calculated from picture one
		//
		SFirePoint();
		
		bool ToAIUnits();
		bool HasFireEffect() const { return !szFireEffect.empty(); }
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	// direction explosion description, взрыв по направлению стрельбы
	struct SDirectionExplosion
	{
		CVec3 vPos;													// position
		float fDirection;										// (угол <=> угол в радианах) direction
		float fVerticalAngle;								// (угол <=> угол в радианах) angle from horizontal plane in the vertical plane, which crosses 'fDirection'
		// vis info
		CVec2 vPicturePosition;							// 2D picture position
		CVec3 vWorldPosition;								// 3D position, calculated from picture one
		//
		SDirectionExplosion();
		
		bool ToAIUnits();
		int operator&( IDataTree &ss );
	};
	//
	EType eType;													// type of this building (see enum above for possible types)
	//
	int nRestSlots;												// slots for resting
	int nMedicalSlots;										// slots for medical assistance
	const SBaseGunRPGStats *pPrimaryGun;	// primary gun (mounted) for data extracting for stats
	//
	std::vector<SSlot> slots;							// shoot slots...
	std::vector<SEntrance> entrances;			// entrances
	std::vector<SFirePoint> firePoints;		// fire points...
	std::vector<SFirePoint> smokePoints;	// smoke points, при разрушении здания
	std::string szSmokeEffect;						// smoke effect, один на всех
	//
	enum EDirectionExplosionType
	{
		//не менять эти ID
		E_FRONT_LEFT,
		E_FRONT_RIGHT,
		E_BACK_RIGHT,
		E_BACK_LEFT,
		E_TOP_CENTER,
	};
	std::vector<SDirectionExplosion> dirExplosions;		// direction explosions, всего 5 штук, смотри EDirectionExplosionType
	std::string szDirExplosionEffect;			// direction explosion effect, один на всех
	//
	SBuildingRPGStats();
	virtual ~SBuildingRPGStats() {  }
	//
	virtual void STDCALL ToAIUnits();
	virtual bool STDCALL Validate();
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );

	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** unit RPG types and classes
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitRPGType
{
	// main types
	RPG_TYPE_INFANTRY						= 0x00010000,
	RPG_TYPE_TRANSPORT					= 0x00020000,
	RPG_TYPE_ARTILLERY					= 0x00040000,
	RPG_TYPE_SPG								= 0x00080000,
	RPG_TYPE_ARMOR							= 0x00100000,
	RPG_TYPE_AVIATION						= 0x00200000,
	RPG_TYPE_TRAIN							= 0x00400000,
	// infantry
	RPG_TYPE_SOLDIER						= 0x00010001,
	RPG_TYPE_ENGINEER						= 0x00010002,
	RPG_TYPE_SNIPER							= 0x00010003,
	RPG_TYPE_OFFICER						= 0x00010004,
	// transport
	RPG_TYPE_TRN_CARRIER				= 0x00020001,
	RPG_TYPE_TRN_SUPPORT				= 0x00020002,
	RPG_TYPE_TRN_MEDICINE				= 0x00020003,
	RPG_TYPE_TRN_TRACTOR				= 0x00020004,
	RPG_TYPE_TRN_MILITARY_AUTO	= 0x00020005,
	RPG_TYPE_TRN_CIVILIAN_AUTO	= 0x00020006,
	// artillery
	RPG_TYPE_ART_GUN						= 0x00040001,
	RPG_TYPE_ART_HOWITZER				= 0x00040002,
	RPG_TYPE_ART_HEAVY_GUN			= 0x00040003,
	RPG_TYPE_ART_AAGUN					= 0x00040004,
	RPG_TYPE_ART_ROCKET					= 0x00040005,
	RPG_TYPE_ART_SUPER					= 0x00040006,
	RPG_TYPE_ART_MORTAR					= 0x00040007,
	RPG_TYPE_ART_HEAVY_MG				= 0x00040008,
	// SPG
	RPG_TYPE_SPG_ASSAULT				= 0x00080001,
	RPG_TYPE_SPG_ANTITANK				= 0x00080002,
	RPG_TYPE_SPG_SUPER					= 0x00080003,
	RPG_TYPE_SPG_AAGUN					= 0x00080004,
	// armor
	RPG_TYPE_ARM_LIGHT					= 0x00100001,
	RPG_TYPE_ARM_MEDIUM					= 0x00100002,
	RPG_TYPE_ARM_HEAVY					= 0x00100003,
	RPG_TYPE_ARM_SUPER					= 0x00100004,
	// aviation
	RPG_TYPE_AVIA_SCOUT					= 0x00200001,
	RPG_TYPE_AVIA_BOMBER				= 0x00200002,
	RPG_TYPE_AVIA_ATTACK				= 0x00200003,
	RPG_TYPE_AVIA_FIGHTER				= 0x00200004,
	RPG_TYPE_AVIA_SUPER					= 0x00200005,
	RPG_TYPE_AVIA_LANDER				= 0x00200006,
	// train
	RPG_TYPE_TRAIN_LOCOMOTIVE		= 0x00400001,
	RPG_TYPE_TRAIN_CARGO				= 0x00400002,
	RPG_TYPE_TRAIN_CARRIER			= 0x00400003,
	RPG_TYPE_TRAIN_SUPER				= 0x00400004,
	RPG_TYPE_TRAIN_ARMOR				= 0x00400005
};
inline EUnitRPGType GetMainType( EUnitRPGType type ) { return EUnitRPGType( type & 0xffff0000 ); }
inline bool IsInfantry( EUnitRPGType type ) { return (type & RPG_TYPE_INFANTRY) != 0; }
inline bool IsTransport( EUnitRPGType type ) { return (type & RPG_TYPE_TRANSPORT) != 0; }
inline bool IsArtillery( EUnitRPGType type ) { return (type & RPG_TYPE_ARTILLERY) != 0; }
inline bool IsSPG( EUnitRPGType type ) { return (type & RPG_TYPE_SPG) != 0; }
inline bool IsArmor( EUnitRPGType type ) { return (type & RPG_TYPE_ARMOR) != 0; }
inline bool IsAviation( EUnitRPGType type ) { return (type & RPG_TYPE_AVIATION) != 0; }
inline bool IsTrain( EUnitRPGType type ) { return (type & RPG_TYPE_TRAIN) != 0; }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitRPGClass
{
	RPG_CLASS_UNKNOWN					= 0,
	RPG_CLASS_ARTILLERY				= 1,
	RPG_CLASS_TANK						= 2,
	RPG_CLASS_SNIPER					= 3,
	RPG_CLASS_FORCE_DWORD	= 0x7fffffff
};
inline const EUnitRPGClass GetRPGClass( const EUnitRPGType eType )
{
	switch ( eType ) 
	{
		case RPG_TYPE_ART_GUN:
		case RPG_TYPE_ART_AAGUN:
		case RPG_TYPE_ART_ROCKET:
		case RPG_TYPE_ART_HOWITZER:
		case RPG_TYPE_ART_HEAVY_GUN:
		case RPG_TYPE_ART_SUPER:
			return RPG_CLASS_ARTILLERY;
		case RPG_TYPE_SPG_SUPER:
		case RPG_TYPE_SPG_AAGUN:
		case RPG_TYPE_SPG_ASSAULT:
		case RPG_TYPE_SPG_ANTITANK:
		case RPG_TYPE_ARM_LIGHT:
		case RPG_TYPE_ARM_MEDIUM:
		case RPG_TYPE_ARM_SUPER:
		case RPG_TYPE_ARM_HEAVY:
			return RPG_CLASS_TANK;
		case RPG_TYPE_SNIPER:
			return RPG_CLASS_SNIPER;
	}
	return RPG_CLASS_UNKNOWN;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGStatsAutomagic : public IRPGStatsAutomagic
{
	OBJECT_COMPLETE_METHODS( CRPGStatsAutomagic );

	typedef std::unordered_map<int, std::string> CI2SMap;
	typedef std::unordered_map<std::string, int> CS2IMap;
	//
	CI2SMap i2s;
	CS2IMap s2i;
	std::string szUnknown;
	//
public:
	CRPGStatsAutomagic();
	//
	virtual const char* STDCALL ToStr( const int nVal ) const
	{
		CI2SMap::const_iterator it = i2s.find( nVal );
		return it != i2s.end() ? it->second.c_str() : szUnknown.c_str();
	}
	virtual const int STDCALL ToInt( const char* pszVal ) const
	{
		CS2IMap::const_iterator it = s2i.find( pszVal );
		return it != s2i.end() ? it->second : -1;
	}

	virtual const char* STDCALL GetFirstStr() const;
	virtual const int STDCALL GetFirstInt() const;
	virtual bool STDCALL IsLastStr( const char* pszVal ) const;
	virtual bool STDCALL IsLastInt( const int nVal ) const;
	virtual const char* STDCALL GetNextStr( const char* pszVal );
	virtual const int STDCALL GetNextInt( const int nVal );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** Acknowledgement types
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitAckType
{
	ACK_POSITIVE			= 0,
	ACK_NEGATIVE			= 1,
	ACK_SELECTED			= 2,

	ACK_NONE	= 3,

	// атака ( нумерацию атаки не менять, т.к. пронумеровано по приоритетам - чем меньше, тем выше приоритет )
	_ACK_ATTACK_BEGIN								= 5,
	ACK_INVALID_TARGET							= _ACK_ATTACK_BEGIN,
	ACK_DONT_SEE_THE_ENEMY				  = 6,
	ACK_NOT_IN_ATTACK_ANGLE					= 7,
	ACK_NOT_IN_FIRE_RANGE						= 8,
	ACK_ENEMY_IS_TO_CLOSE						= 9,
	ACK_NO_AMMO											= 10,
	ACK_CANNOT_PIERCE								= 11,
	ACK_CANNOT_FIND_PATH_TO_TARGET	= 12,
	ACK_ENEMY_ISNT_IN_FIRE_SECTOR		= 13,
	_ACK_ATTACK_END									= ACK_ENEMY_ISNT_IN_FIRE_SECTOR,

	// движение
	ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE	= 14,
	ACK_CANNOT_MOVE_TRACK_DAMAGED							= 15,

	// снабжение
	ACK_GOING_TO_STORAGE											= 16,
	ACK_CANNOT_SUPPLY_NOT_PATH								= 17,
	ACK_NO_RESOURCES_CANT_FIND_DEPOT					= 18,
	ACK_NO_RESOURCES_CANT_FIND_PATH_TO_DEPOT	= 19,
	ACK_CANNOT_MOVE_WAITING_FOR_LOADERS				= 20,
	ACK_START_SERVICE_RESUPPLY								= 21,
	ACK_START_SERVICE_REPAIR									= 22,
	
	//ACK_NO_CANNOT_BUILD_STORAGE_HERE					= 23,
	
	
	// aviation
	ACK_PLANE_TAKING_OFF											= 24,
	ACK_PLANE_LEAVING													= 25,

	// нужно проинсталлироваться для выполнения этой команды
	ACK_NEED_INSTALL													= 26,

	ACK_NO_ENGINEERS_CANNOT_REACH_BUILDPOINT	= 27,
	//ACK_NO_ENGINEERS_BUILD_INPOSSIBLE					= 28,

	ACK_NO_TOO_HEAVY_ARTILLERY_FOR_TRANSPORT	= 29,
	ACK_NO_CANNOT_UNHOOK_ARTILLERY_HERE				= 30,

	//ACK_NO_CANNOT_BUILD_PIT_HERE							= 31,
	//ACK_NO_CANNOT_LEAVE_PIT										= 32,
	//ACK_NO_CANNOT_ENTER_PIT_TOO_SMALL					= 33,
	//ACK_NO_CANNOT_ENTER_PIT_BUSY							= 34,

	//ACK_NO_CANNOT_BUILD_ANTITANK_HERE					= 35,
	
	_ACK_BORED_BEGIN											= 36,
	ACK_BORED_RUSH												= _ACK_BORED_BEGIN,// в агресивной формации атака. враг виден.
	ACK_BORED_ATTACK											= 37,
	ACK_BORED_LOW_AMMO										= 38,
	ACK_BORED_NO_AMMO											=	39,
	ACK_BORED_IDLE												= 40,

	ACK_BORED_SNIPER_SNEAK								= 41,
	ACK_BORED_MINIMUM_MORALE							= 42,
	ACK_BORED_LOW_HIT_POINTS							= 43,
	ACK_BORED_INFANTRY_TRAVEL							= 44,
	
	_ACK_BORED_END												= ACK_BORED_INFANTRY_TRAVEL,



	ACK_GETTING_AMMO											= 45,
	ACK_ATTACKING_AVIATION								= 46,
	//ACK_BUILDING_COMPLETE									= 47,
	ACK_BEING_ATTACKED_BY_AVIATION				= 48,

	ACK_NO_CANNOT_HOOK_ARTILLERY_NO_PATH	= 49,
	ACK_SELECTION_TO_MUCH									= 50,
	ACK_PLANE_REACH_POINT_START_ATTACK		= 51,
	
	ACK_KILLED_ENEMY											= 52,

	ACK_BUILDING_FINISHED									= 53,
	ACK_CANNOT_START_BUILD								= 54,
	ACK_CANNOT_FINISH_BUILD								= 55,

	ACK_KILLED_ENEMY_INFANTRY							= 56,
	ACK_KILLED_ENEMY_AVIATION							= 57,
	ACK_KILLED_ENEMY_TANK									= 58,
	ACK_UNIT_DIED													= 59,

	// всегда должна иметь наибольший номер
	_ACK_END,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** Acks stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAckRPGStats : public SCommonRPGStats
{
	struct SType
	{
		struct SAck
		{
			std::string szSound;							// reference to sound
			float fProbability;								// probability of this ack
			//
			int operator&( IDataTree &ss );
		};
		//
		EUnitAckType eType;									// ack type
		std::vector<SAck> acks;							// all acks of this type
		//
		int operator&( IDataTree &ss );
	};
	//
	std::vector<SType> types;							// all types 
	//
	SAckRPGStats();
	virtual ~SAckRPGStats();
	//
	virtual void STDCALL ToAIUnits();
	//
	bool ChooseAcknowledgement( const float fRandom, const EUnitAckType type, std::string *pResult ) const;
	//
	virtual int STDCALL operator&( IDataTree &ss );

	virtual const uLong STDCALL CalculateCheckSum() const { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base unit stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitBaseRPGStats : public SHPObjectRPGStats
{
	// animation descriptor
	struct SAnimDesc
	{
		int nLength;												// length of this animation (in ticks)
		int nAction;												// action point (in ticks with respect to animation's start)
		int nIndex;													// order index of all animations list
		int nAABB_A;												// AABB for animation index (-1 if no). refer to aabb_as
		int nAABB_D;												// AABB for final state index (-1 if no). refer to aabb_ds
		//
		SAnimDesc() : nLength( 0 ), nAction( 0 ), nIndex( -1 ), nAABB_A( -1 ), nAABB_D( -1 ) {  }
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "Length", &nLength );
			saver.Add( "Action", &nAction );
			saver.Add( "Index", &nIndex );
			saver.Add( "AABB_A", &nAABB_A );
			saver.Add( "AABB_D", &nAABB_D );
			return 0;
		}
	};
	//
	struct SAABBDesc
	{
		CVec2 vCenter;											// center
		CVec2 vHalfSize;										// half size
		//
		int ToAIUnits();
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "Center", &vCenter );
			saver.Add( "HalfSize", &vHalfSize );
			return 0;
		}
	};
	//
	EUnitRPGType type;										// unit type
	EAIClass aiClass;											// AI class of this unit
	//
	std::vector<std::string> szAcksNames;		// names of acks sets
	std::vector<const SAckRPGStats *> pAcksSets;	// shortcuts 
	//
	const SBaseGunRPGStats *pPrimaryGun;	// primary gun for data extracting for stats
	int nAmmos[2];												// primary and secondary max ammo counts (for visualization)
	//
	float fSight;													// (метры <=> AI тайлы) sight range
	float fSightPower;										// sight power
	float fSpeed;													// (киломерты/час <=> точки/тик) speed on the road
	float fRotateSpeed;										// (градусы/сек <=> угол 65536/тик) speed of rotation (in direction units (2pi = 65536))
	float fPassability;										// (огурцы) проходимость
	int nPriority;												// приоритет для "уступания" дороги
	float fCamouflage;										// (% от метров) способность к маскировке - множитель чужое зрение
	// max и min - считаются для минимально возможной брони для каждой из стороны
	int nMaxArmor;											// maximum armor \| these two parameters make sense only for non-inf units,
	int nMinArmor;											// minimum armor /| in other case they are equal
	// installation
	union { float fUninstallRotate; int nUninstallRotate; };				// uninstall for rotation and install from it
	union { float fUninstallTransport; int nUninstallTransport; };	// uninstall for transporting and install from it

	int nBoundTileRadius;									// (AI tiles) радиус, ограничивающий физический объект в тайлах

	float fWeight;												// (килограмы <=> килограмы) вес
	float fPrice;													// (огурцы) ценность юнита

	// axis-aligned bounding box
	CVec2 vAABBCenter;										// aabb center
	CVec2 vAABBHalfSize;									// aabb half size
	CVec2 vAABBVisCenter;
	CVec2 vAABBVisHalfSize;
	std::vector<SAABBDesc> aabb_as;				// AABBs for animation process
	std::vector<SAABBDesc> aabb_ds;				// AABBs for animation finish
	float fSmallAABBCoeff;								// small AABB coeff
	//
	CArray1Bit availCommands;							// available commands, which unit can do
	CArray1Bit availExposures;						// available exposures, which one can do with this unit
	CUserActions availUserActions;				// re-map AI commands to user actions
	CUserActions availUserExposures;			// re-map AI commands to user actions
	//
	std::vector< std::vector<SAnimDesc> > animdescs; // animation descriptions by animation types
	//
	SUnitBaseRPGStats( const char *pszType );
	virtual ~SUnitBaseRPGStats() {  }
	//
	virtual void STDCALL ToAIUnits();
	//	
	int GetAnimTime( int nAnim ) const { return nAnim >= animdescs.size() || animdescs[nAnim].empty() ? 0 : animdescs[nAnim][0].nLength; }
	int GetAnimActionTime( int nAnim ) const { return nAnim >= animdescs.size() || animdescs[nAnim].empty() ? 0 : animdescs[nAnim][0].nAction; }
	// chooses sound for given ack type and writes it to passed string
	// returns false if no acknowledgement is chosen
	virtual bool STDCALL ChooseAcknowledgement( const float fRandom, const EUnitAckType type, std::string * str, const int nSet ) const;
	const EUnitRPGClass GetRPGClass() const { return ::GetRPGClass( type ); }
	EUnitRPGType GetMainType() const { return ::GetMainType( type ); }
	int IsInfantry() const { return ::IsInfantry( type ); }
	int IsTransport() const { return ::IsTransport( type ); }
	int IsArtillery() const { return ::IsArtillery( type ); }
	int IsSPG() const { return ::IsSPG( type ); }
	int IsArmor() const { return ::IsArmor( type ); }
	int IsAviation() const { return ::IsAviation( type ); }
	int IsTrain() const { return ::IsTrain( type ); }

	virtual int GetArmor( const int n ) const = 0;
	virtual int GetMinPossibleArmor( const int n ) const = 0;
	virtual int GetMaxPossibleArmor( const int n ) const = 0;
	virtual int GetRandomArmor( const int n ) const = 0;
	virtual const SBaseGunRPGStats& GetGun( const int n ) const = 0;
	//
	template <class TGun>
		void CountPrimaryGuns( std::vector<TGun> &guns )
		{
			// find min and max gun priority
			int nMin = 1000000000, nMax = -1000000000;
			for ( std::vector<TGun>::const_iterator it = guns.begin(); it != guns.end(); ++it )
			{
				nMin = Min( nMin, it->nPriority );
				nMax = Max( nMax, it->nPriority );
			}
			// set priority and count ammos
			Zero( nAmmos );
			for ( std::vector<TGun>::iterator it = guns.begin(); it != guns.end(); ++it )
			{
				if ( (pPrimaryGun == 0) && (it->nPriority == nMin) )
					pPrimaryGun = &( *it );
				it->bPrimary = it->nPriority == nMin;
				nAmmos[it->bPrimary ? 0 : 1] += it->nAmmo;
			}
		}
	// 
	template <class TGun>
		void CountShellTypes( std::vector<TGun> &guns )
		{
			// count shell types
			int nDamageTypes[3] = { 0, 0, 0 };
			for ( std::vector<TGun>::const_iterator it = guns.begin(); it != guns.end(); ++it )
			{
				for ( std::vector<SWeaponRPGStats::SShell>::const_iterator shell = it->pWeapon->shells.begin(); shell != it->pWeapon->shells.end(); ++shell )
					nDamageTypes[shell->eDamageType] = 1;
			}
			// set actions
			if ( nDamageTypes[0] + nDamageTypes[1] + nDamageTypes[2] > 1 )
			{
				AddCommand( ACTION_COMMAND_CHANGE_SHELLTYPE );
				availUserActions.SetAction( USER_ACTION_CHANGE_SHELL );
				if ( nDamageTypes[SWeaponRPGStats::SShell::DAMAGE_HEALTH] != 0 ) 
					availUserActions.SetAction( USER_ACTION_USE_SHELL_DAMAGE );
				if ( nDamageTypes[SWeaponRPGStats::SShell::DAMAGE_MORALE] != 0 ) 
					availUserActions.SetAction( USER_ACTION_USE_SHELL_AGIT );
				if ( nDamageTypes[SWeaponRPGStats::SShell::DAMAGE_FOG] != 0 ) 
					availUserActions.SetAction( USER_ACTION_USE_SHELL_SMOKE );
			}
			else
			{
				RemoveCommand( ACTION_COMMAND_CHANGE_SHELLTYPE );
				availUserActions.RemoveAction( USER_ACTION_CHANGE_SHELL );
				availUserActions.RemoveAction( USER_ACTION_USE_SHELL_DAMAGE );
				availUserActions.RemoveAction( USER_ACTION_USE_SHELL_AGIT );
				availUserActions.RemoveAction( USER_ACTION_USE_SHELL_SMOKE );
			}
		}
	//
	const bool HasCommand( const int nCmd ) const
	{
		//NI_ASSERT_T( (nCmd >= 0 && nCmd < availCommands.GetSize()) || (nCmd >= 1000), NStr::Format( "Wrong command ( %d )\n", nCmd ) );
		return nCmd >= 1000 ? true : ( nCmd < availCommands.GetSize() ? availCommands.GetData(nCmd) : false );
	}
	void AddCommand( const int nCmd )
	{
		NI_ASSERT_T( nCmd < 1000, NStr::Format( "Wrong command ( %d )\n", nCmd ) );
		if ( availCommands.GetSize() <= nCmd )
			availCommands.SetSize( nCmd + 1 );
		availCommands.SetData( nCmd );
	}
	void RemoveCommand( const int nCmd )
	{
		//NI_ASSERT_T( nCmd >= 0 && nCmd < availCommands.GetSize(), NStr::Format( "Wrong command ( %d )\n", nCmd ) );
		if ( nCmd >= 0 && nCmd < availCommands.GetSize() )
			availCommands.RemoveData( nCmd );
	}
	//
	void GetUserActions( CUserActions *pActions ) const { availUserActions.GetActions( pActions ); }
	const bool HasUserAction( const int nAction ) const { return availUserActions.HasAction( nAction ); }
	//
	const std::vector<SAnimDesc>* GetAnims( const int nType ) const { return nType < animdescs.size() ? &(animdescs[nType]) : 0; }
	//
	virtual const CUserActions* GetUserActions( bool bActionsBy ) const { return bActionsBy ? &availUserActions : &availUserExposures; }
	//
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** mech unit stats (tank, auto, artillery, etc.)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMechUnitRPGStats : public SUnitBaseRPGStats
{
	struct SConstraint
	{
		union { float fMin; WORD wMin; };
		union { float fMax; WORD wMax; };
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );
	};
	struct SGun : public SBaseGunRPGStats
	{
		int nShootPoint;									// индекс точки выстрела
		bool bRecoil;											// есть ли отдача при выстреле
		float fRecoilLength;							// длина отката при отдаче
		DWORD recoilTime;									// время отката при отдаче
		int nModelPart;										// часть модели, которая представляет эту пушку (индекс).
			                                // имеет смысл только для 'main gun'. для остальных = -1
		int nRecoilShakeTime;							// время на вздрагивание (msec)
		float fRecoilShakeAngle;					// угол поворота при вздрагивании (radian)
		//
		SGun();
		virtual int STDCALL operator&( IDataTree &ss );

		//
		virtual const uLong STDCALL CalculateCheckSum() const;
	};
	struct SPlatform
	{
		union { float fHorizontalRotationSpeed; WORD wHorizontalRotationSpeed; };		// (секунды на полный оборот (360 градусов) <=> градусы65535/тик) horizontal rotation speed
		union { float fVerticalRotationSpeed; WORD wVerticalRotationSpeed; };				// (секунды на полный оборот <=> градусы65535/тик) vertical rotation speed
		int nModelPart;											// часть модели, которая представляет эту платформу (индекс)
		SConstraint constraint;							// constraint for procedural animation
		DWORD dwGunCarriageParts; 					// "станок" ствола - для всех стволов, которые могут наводиться вертикально
		SConstraint constraintVertical;			// ограничение на вертикальную наводку ствола
		// пушки на платформе.
		// главная пушка идёт первой
		int nFirstGun;											// first platform gun index in the 'guns' array
		int nNumGuns;												// number of guns on this platform
		//
		SPlatform();
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	struct SArmor
	{
		union { float fMin; int nMin; };		// armor value for one side
		union { float fMax; int nMax; };		// random for armor value
		//
		SArmor() : nMin( 0 ), nMax( 0 ) {  }
		bool ToAIUnits();
		int operator&( IDataTree &ss );
	};
	//
	struct SJoggingParams
	{
		// value = fAmp1 * cos( time/fPeriod1*2pi + fPhase1 ) + fAmp2 * cos( time/fPeriod2*2pi + fPhase2 )
		float fPeriod1, fPeriod2;						// 'cos' wave periods
		float fAmp1, fAmp2;									// -~- amplitude
		float fPhase1, fPhase2;							// -~- phase
		//
		SJoggingParams();
		int operator&( IDataTree &ss );
	};
	//
	std::vector<SGun> guns;								// all guns in the next order: all guns on the base platform, all guns on the turret
	std::vector<SPlatform> platforms;			// all platforms. [0] - base, [1] - turret
	//
	SArmor armors[6];											// (огурцы) armor from 6 directions (see EDirection)
	float fTowingForce;										// (килограммы <=> килограммы) тяговое усилие
	int nCrew;														// number of crew members, required to operate
	int nPassangers;											// number of possible passangers to carry
	//
	float fTurnRadius;										// (метры <=> AI точки) радиус разворота
	// additional points...
	std::vector<int> exhaustPoints;				// точки выхлопа
	std::vector<int> damagePoints;				// точки взрыва при смерти
	int nTowPoint;												// точка буксировки
	int nEntrancePoint;										// точка входа в машинку
	std::vector<int> peoplePointIndices;	// точки расположения людей при этой машинке
	int nFatalitySmokePoint;							// точка для выброса эффекта при fatality
	int nShootDustPoint;									// точка для выброса пыли при выстреле из РСЗО/гаубиц. если -1, то брать центр юнита
	// реальные положения некоторых точек, необходимых для быстрого доступа из AI
	CVec2 vTowPoint;											// 2D положение точки буксировки
	CVec2 vEntrancePoint;									// 2D положение точки входа
	std::vector<CVec2> vPeoplePoints;			// 2D положения точек расположения людей при этой машинке
	CVec2 vAmmoPoint;											// 2D point for ammo box
	std::vector< std::vector<CVec2> > vGunners;	// gunners information in the different modes: operate, rotate, move
	CVec2 vHookPoint;											// 2D точка подцепления вагона (только для вагонов)
	CVec2 vFrontWheel;										// 2D точка передних колёс у вагона (только для вагонов)
	CVec2 vBackWheel;											// 2D точка задних колёс у вагона (только для вагонов)
	// effects
	std::string szEffectDiesel;						// diesel smoke effect
	std::string szEffectSmoke;						// smoke effect
	std::string szEffectWheelDust;				// пыль из под колес
	std::string szEffectShootDust;				// пыль при выстреле
	std::string szEffectFatality;					// fatality effect
	std::string szEffectEntrenching;			// entrenching for technics
	std::string szEffectDisappear;				// эффект при исчезании 'трупа' техники
	// jogging params
	SJoggingParams jx, jy, jz;						// jogging in 3 axises
	// tracks
	bool bLeavesTracks;										// оставляет ли следы
	float fTrackWidth;										// ширина одного трека в процентах от ширины AABB
	float fTrackOffset;										// отступ от края AABB
	float fTrackStart;										// отступ начала трека от начала AABB
	float fTrackEnd;											// отступ конца трека от конца AABB
	float fTrackIntensity;								// интенсивность трека (1 - alpha)
	int nTrackLifetime;										// время жизни трека
	// sounds
	std::string szSoundMoveStart;					// start movement
	std::string szSoundMoveCycle;					// cycle movement sound
	std::string szSoundMoveStop;					// stop movement
	// locators, different for 3 models
	// for aviation units
	float fMaxHeight;
	union{ float fDivingAngle; WORD wDivingAngle; };
	union{ float fClimbAngle; WORD wClimbingAngle; };
	union{ float fTiltAngle; WORD wTiltAngle; };
	float fTiltRatio;
	// когда техника умирает, под ней остается death crater
	std::vector< std::string > deathCraters;
	//
	SMechUnitRPGStats();
	virtual ~SMechUnitRPGStats() {  }
	//
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );
	// преобразовать из человеческих единиц в AI
	virtual void STDCALL ToAIUnits();
	// проверка статсов на корректность
	virtual bool STDCALL Validate();
	//
	virtual int GetArmor( const int n ) const { return ( armors[n].nMin + armors[n].nMax ) / 2; }
	virtual int GetMinPossibleArmor( const int n ) const { return armors[n].nMin; }
	virtual int GetMaxPossibleArmor( const int n ) const { return armors[n].nMax; }
	virtual int GetRandomArmor( const int n ) const { return RandomCheck( armors[n].nMin, armors[n].nMax ); }
	virtual const SBaseGunRPGStats& GetGun( const int n ) const { return guns[n]; }
	//
	bool HasDieselEffect() const { return !szEffectDiesel.empty(); }
	bool HasSmokeEffect() const { return !szEffectSmoke.empty(); }
	bool HasMoveStartSound() const { return !szSoundMoveStart.empty(); }
	bool HasMoveSound() const { return !szSoundMoveCycle.empty(); }
	bool HasMoveStopSound() const { return szSoundMoveStop.empty(); }
	//
	const std::vector<CVec2>* GetGunners( const int nMode ) const { return nMode < vGunners.size() ? &(vGunners[nMode]) : 0; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** infantry stats (soldier, officer, engineer, etc.)
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInfantryRPGStats : public SUnitBaseRPGStats
{
	struct SGun : SBaseGunRPGStats
	{
		virtual int STDCALL operator&( IDataTree &ss );
	};
	//
	std::vector<SGun> guns;								// guns[0] - main gun (rifle, machinegun, etc.), guns[1] - (if it is) secondary weapon - grenade
	// 'attack' animation in 'stand' and in 'lie' positions
	bool bCanAttackUp;										// может атаковать сто
	bool bCanAttackDown;									// может атаковать лёжа
	// visual speed
	float fRunSpeed;											// runing speed
	float fCrawlSpeed;										// crawling speed
	// animation lengthes
	std::vector<int> animtimes;						// length of all animations in msec (0 if no such animation)
	// AI settings
	/*
	std::vector<int> commands;						// доступные команды
	std::vector<int> targets;							// предпочитаемые цели (в порядке предпочтения)
	std::vector<int> behavior;						// поведенческие установки...
	*/
	//
	SInfantryRPGStats();
	virtual ~SInfantryRPGStats() {  }

	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual int GetArmor( const int n ) const { return 0; }
	virtual int GetMinPossibleArmor( const int n ) const { return 0; }
	virtual int GetMaxPossibleArmor( const int n ) const { return 0; }
	virtual int GetRandomArmor( const int n ) const { return 0; }
	virtual const SBaseGunRPGStats& GetGun( const int n ) const { return guns[n]; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** entrenchment set stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEntrenchmentRPGStats : public SHPObjectRPGStats
{
	enum
	{
		ENTRENCHMENT_LINE				= 0x00000001,
		ENTRENCHMENT_FIREPLACE	= 0x00000002,
		ENTRENCHMENT_TERMINATOR	= 0x00000004,
		ENTRENCHMENT_ARC				= 0x00000008
	};
	enum EEntrenchSegmType
	{
		EST_LINE					= 0,
		EST_FIREPLACE			= 1,
		EST_TERMINATOR		= 2,
		EST_ARC						= 3
	};

	struct SSegmentRPGStats
	{
		std::string szModel;								// model name
		CVec2 vFirePlace;										// fireplace position (VNULL2 - no fireplace)
		CVec2 vAABBCenter;									// AABB center 2D point
		CVec3 vAABBHalfSize;								// AABB half size (x, y, z)
		float fCoverage;										// coverage for the unit in the fireplace
		std::vector<CVec2> fireplaces;			// all fireplaces in this segment

		EEntrenchSegmType eType;
		//
		SSegmentRPGStats() : vFirePlace( VNULL2 ) {  }
		//
		float GetLength() const;
		float GetHalfLength() const;
		const CVec2 GetVisFirePlace() const;
		const CVec2 GetVisAABBCenter() const;
		const CVec3 GetVisAABBHalfSize() const;
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	//
	std::vector<SSegmentRPGStats> segments;
	//
	std::vector<int> lines;
	std::vector<int> fireplaces;
	std::vector<int> terminators;
	std::vector<int> arcs;
	//
	SEntrenchmentRPGStats();
	virtual ~SEntrenchmentRPGStats() {  }
	//
	int GetIndexLocal( int nIndex, const std::vector<int> &indices, const char *pszName, int *pCurRandomSeed = 0 ) const
	{
		//NI_ASSERT_SLOW_TF( !indices.empty() && (nIndex < indices.size()), NStr::Format("Can't find any \"%s\" segment for entrenchment", pszName), return -1 );
		if ( nIndex == -1 )
		{
			if ( pCurRandomSeed == 0 )
				return indices[ rand() % indices.size() ];
			else
			{
				*pCurRandomSeed %= indices.size();
				return indices[*pCurRandomSeed];
			}
		}
		else
			return indices[nIndex];
	}
	//
	int GetLineIndex( int nIndex = -1 ) const { return GetIndexLocal( nIndex, lines, "line", 0 ); }
	int GetFirePlaceIndex( int nIndex = -1 ) const { return GetIndexLocal( nIndex, fireplaces, "fireplace", 0 ); }
	int GetTerminatorIndex( int nIndex = -1 ) const { return GetIndexLocal( nIndex, terminators, "terminator", 0 ); }
	int GetArcIndex( int nIndex = -1 ) const { return GetIndexLocal( nIndex, arcs, "arc", 0 ); }
	//
	int GetLineIndex( int *pCurRandomSeed ) const { return GetIndexLocal( -1, lines, "line", pCurRandomSeed ); }
	int GetFirePlaceIndex( int *pCurRandomSeed ) const { return GetIndexLocal( -1, fireplaces, "fireplace", pCurRandomSeed ); }
	int GetTerminatorIndex( int *pCurRandomSeed ) const { return GetIndexLocal( -1, terminators, "terminator", pCurRandomSeed ); }
	int GetArcIndex( int *pCurRandomSeed ) const { return GetIndexLocal( -1, arcs, "arc", pCurRandomSeed ); }
	//
	const SSegmentRPGStats& GetSegmentStats( const int nIndex ) const
	{
		NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < segments.size(), "Invalid segment index" );
		return segments[nIndex];
	}
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual const int STDCALL GetTypeFromIndex( const int nIndex ) const;
	virtual const int STDCALL GetIndexFromType( const int nType, int *pCurRandomSeed = 0 ) const;
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** fence set stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFenceRPGStats : public SStaticObjectRPGStats
{
	enum
	{
		FENCE_DIRECTION_0		= 0x00000001,
		FENCE_DIRECTION_1		= 0x00000002,
		FENCE_DIRECTION_2		= 0x00000004,
		FENCE_DIRECTION_3		= 0x00000008,
		FENCE_TYPE_NORMAL		= 0x00010000,
		FENCE_TYPE_LDAMAGE	= 0x00020000,
		FENCE_TYPE_RDAMAGE	= 0x00040000,
		FENCE_TYPE_CDAMAGE	= 0x00080000,
	};
	//
	struct SSegmentRPGStats
	{
		CArray2D<BYTE> passability;					// проходимость AI тайлов этого сегмента
		CVec2 vOrigin;											// нулевая точка сегмента для passability
		CArray2D<BYTE> visibility;					// степень поглощения силы взгляда
		CVec2 vVisOrigin;										// нулевая точка сегмента для visibility
		int nIndex;													// sprite index
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	//
	struct SDir
	{
		// segment indices from the common array
		std::vector<int> centers;						// center segment indices
		std::vector<int> ldamages;					// left-damaged segment indices
		std::vector<int> rdamages;					// right-damaged segment indices
		std::vector<int> cdamages;					// center-damaged (destroyed) segment indices
		//
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	//
	std::vector<SSegmentRPGStats> stats;
	std::vector<SDir> dirs;								// all available directions
	//
	SFenceRPGStats() : SStaticObjectRPGStats( "Fence" ) {  }
	virtual ~SFenceRPGStats() {  }
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual const CVec2& STDCALL GetOrigin( const int nIndex = -1 ) const
	{
		NI_ASSERT_SLOW_T( nIndex > -1 && nIndex < stats.size(), NStr::Format("Index %d for the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), stats.size()) );
		return stats[nIndex].vOrigin;
	}
	virtual const CArray2D<BYTE>& STDCALL GetPassability( const int nIndex = -1 ) const
	{
		NI_ASSERT_SLOW_T( nIndex > -1 && nIndex < stats.size(), NStr::Format("Index %d for the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), stats.size()) );
		return stats[nIndex].passability;
	}
	virtual const CVec2& STDCALL GetVisOrigin( const int nIndex = -1 ) const
	{
		NI_ASSERT_SLOW_T( nIndex > -1 && nIndex < stats.size(), NStr::Format("Index %d for the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), stats.size()) );
		return stats[nIndex].vVisOrigin;
	}
	virtual const CArray2D<BYTE>& STDCALL GetVisibility( const int nIndex = -1 ) const
	{
		NI_ASSERT_SLOW_T( nIndex > -1 && nIndex < stats.size(), NStr::Format("Index %d for the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), stats.size()) );
		return stats[nIndex].visibility;
	}
	// helper functions
	int GetIndexLocal( int nIndex, const std::vector<int> &indices, const char *pszDirName, int *pRandomSeed ) const
	{
		NI_ASSERT_TF( indices.size() > 0, NStr::Format("Fence set \"%s\" must have at least one %s tile", szKeyName.c_str(), pszDirName), return 0 );
		if ( nIndex > 0 && nIndex < indices.size() )
		{
			NI_ASSERT_TF( indices[nIndex] == stats[indices[nIndex]].nIndex, NStr::Format("Indices in %s fence stats \"%s\" must be the same!", pszDirName, szKeyName.c_str()), return 0 );
			return indices[nIndex];
		}
		else
		{
			if ( pRandomSeed == 0 )
				nIndex = rand() % indices.size();
			else
			{
				*pRandomSeed %= indices.size();
				nIndex = *pRandomSeed;
			}

			return indices[nIndex];
		}
	}
	//
	int GetCenterIndex( int nDir, int nIndex = -1 ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( nIndex, dirs[nDir].centers, "center", 0 );
	}
	int GetLDamageIndex( int nDir, int nIndex = -1 ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( nIndex, dirs[nDir].ldamages, "ldamage", 0 );
	}
	int GetRDamageIndex( int nDir, int nIndex = -1 ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( nIndex, dirs[nDir].rdamages, "rdamage", 0 );
	}
	int GetCDamageIndex( int nDir, int nIndex = -1 ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( nIndex, dirs[nDir].cdamages, "cdamage", 0 );
	}
	//
	int GetCenterIndex( int nDir, int *pCurRandomSeed ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( -1, dirs[nDir].centers, "center", pCurRandomSeed );
	}
	int GetLDamageIndex( int nDir, int *pCurRandomSeed ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( -1, dirs[nDir].ldamages, "ldamage", pCurRandomSeed );
	}
	int GetRDamageIndex( int nDir, int *pCurRandomSeed ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( -1, dirs[nDir].rdamages, "rdamage", pCurRandomSeed );
	}
	int GetCDamageIndex( int nDir, int *pCurRandomSeed ) const
	{
		NI_ASSERT_TF( nDir > -1 && nDir < dirs.size(), NStr::Format("Direction (%d) for fence must be in range [0..%d]", nDir, dirs.size()), return 0 );
		return GetIndexLocal( -1, dirs[nDir].cdamages, "cdamage", pCurRandomSeed );
	}
	//
	virtual const int STDCALL GetTypeFromIndex( const int nIndex ) const;
	virtual const int STDCALL GetIndexFromType( const int nType, int *pCurRandomSeed = 0 ) const;
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** squad RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSquadRPGStats : public SHPObjectRPGStats
{
	enum ESquadType
	{
		RIFLEMANS						= 0,
		INFANTRY						= 1,
		SUBMACHINEGUNNERS		= 2,
		MACHINEGUNNERS			= 3,
		AT_TEAM							= 4,
		MORTAR_TEAM					= 5,
		SNIPERS							= 6,	
		GUNNERS							= 7,
		ENGINEERS						= 8,
	};
	enum EEVents
	{
		HIT_NEAR = 0,												// взвод обстрелян
	};
	//
	struct SFormation
	{
		enum EType
		{
			DEFAULT   = 0,
			MOVEMENT  = 1,
			DEFENSIVE = 2,
			OFFENSIVE = 3,
			SNEAK			= 4
		};
		struct SEntry
		{
			std::string szSoldier;						// soldier name
			const SInfantryRPGStats *pSoldier;// shortcut
			CVec2 vPos;												// (vis point <=> AI point) position, relative to formation center
			union { float fDir; int nDir; };	// (degree <=> degree65535)  soldier direction, relative to formation direction
			//
			int operator&( IDataTree &ss );
			void RetrieveShortcuts( IObjectsDB *pGDB );
			bool ToAIUnits();

			//
			const uLong CalculateCheckSum() const;
		};
		//
		EType type;													// formation type
		//
		std::vector<SEntry> order;					// all soldiers in this formation
		// bonuses of formation
		BYTE cLieFlag;	// 0 - стандартное поведение, 1 - всегда стоять, 2 - всегда лежать
		float fSpeedBonus;
		float fDispersionBonus;
		float fFireRateBonus;
		float fRelaxTimeBonus;
		float fCoverBonus;
		float fVisibleBonus;
		
		std::vector<int> changesByEvent;			// в какую формацию перейти по событию, -1 - никуда не переходить
		//
		int operator&( IDataTree &ss );
		void RetrieveShortcuts( IObjectsDB *pGDB );
		bool ToAIUnits();

		SFormation();

		//
		const uLong CalculateCheckSum() const;
	};
	//
	std::string szIcon;										// icon picture
	ESquadType type;											// squad type
	std::vector<std::string> memberNames;	// squad members
	std::vector<const SInfantryRPGStats*> members; // shortcuts to member RPG stats
	std::vector<SFormation> formations;		// available formations
	CUserActions availActions;						// available actions by this formation
	CUserActions availExposures;					// available exposures to this formation
	//
	SSquadRPGStats() : SHPObjectRPGStats( "Squad" ) {  }
	virtual ~SSquadRPGStats() {  }
	//
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );
	//
	virtual void STDCALL ToAIUnits();
	//
	virtual bool STDCALL Validate();
	//
	virtual const CUserActions* GetUserActions( bool bActionsBy ) const { return bActionsBy ? &availActions : &availExposures; }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** mine RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMineRPGStats : public SObjectBaseRPGStats
{
	enum EType
	{
		INFANTRY = 0,
		TECHNICS = 1
	};
	//
	std::string szWeapon;									// weapon name
	const SWeaponRPGStats *pWeapon;				// weapon shortcut
	EType type;														// anti-infantry or anti-tank
	//
	float fWeight;												// вес, необходимый для срабатывани
	std::string szFlagModel;							// флажок над обнаруженой миной
	//
	SMineRPGStats();
	virtual ~SMineRPGStats() {  }
	//
	virtual void STDCALL RetrieveShortcuts( IObjectsDB *pGDB );
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** bridge RPG stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// сам мост локает тайлы для всех классов юнитов, а в dwAIClasses заданы классы, которые не могут проехать по мосту
struct SBridgeRPGStats : public SStaticObjectRPGStats
{
private:
	virtual const int STDCALL GetIndexFromTypeLocal( const int nType, const int nDamageState, int *pCurRandomSeed ) const;
public:
	enum
	{
		BRIDGE_SPAN_TYPE_BEGIN	= 0x00000001,
		BRIDGE_SPAN_TYPE_CENTER	= 0x00000002,
		BRIDGE_SPAN_TYPE_END		= 0x00000004,
	};
	enum EDirection
	{
		VERTICAL = 0,
		HORIZONTAL = 1
	};
	struct SSegmentRPGStats
	{
		enum EType
		{
			SLAB = 0,
			GIRDER = 1
		};
		//
		EType eType;												// segment type - slab or girder
		// pass/vis and origin params
		CArray2D<BYTE> passability;					// проходимость AI тайлов этого сегмента
		CVec2 vOrigin;											// нулевая точка сегмента для passability
		CArray2D<BYTE> visibility;					// степень поглощения силы взгляда
		CVec2 vVisOrigin;										// нулевая точка сегмента для visibility
		//
		CVec3 vRelPos;											// part relative position with respect to the span center
		std::string szModel;								// model name
		int nFrameIndex;										// index of the sprite inside packed model
		//
		SSegmentRPGStats();
		//
		bool ToAIUnits();
		int operator&( IDataTree &ss );

		//
		const uLong CalculateCheckSum() const;
	};
	//
	struct SSpan
	{
		// indices
		int nSlab;													// slab part of the span segment index
		int nBackGirder;										// back part of the span's girder segment index
		int nFrontGirder;										// front part of the span's girder segment index
		// lengthes
		float fWidth;												// span width
		float fLength;											// span length
		//
		int operator&( IDataTree &ss );
	};
	//
	struct SDamageState
	{
		std::vector<SSpan> spans;						// all spans of the bridge
		std::vector<int> begins;						// indices of the 'begin' segments in the 'spans'
		std::vector<int> lines;							// indices of the 'line' segments in the 'spans'
		std::vector<int> ends;							// indices of the 'end' segments in the 'spans'
		//
		int operator&( IDataTree &ss );
	};
	//
	// fire point description, огонь на мосту
	struct SFirePoint
	{
		CVec3 vPos;													// position
		float fDirection;										// (угол <=> угол в радианах) direction
		float fVerticalAngle;								// (угол <=> угол в радианах) angle from horizontal plane in the vertical plane, which crosses 'fDirection'
		std::string szFireEffect;						// particle fire effect
		// vis info
		CVec2 vPicturePosition;							// 2D picture position
		CVec3 vWorldPosition;								// 3D position, calculated from picture one
		//
		SFirePoint();
		
		bool ToAIUnits();
		bool HasFireEffect() const { return !szFireEffect.empty(); }
		int operator&( IDataTree &ss );
	};
	// direction explosion description, взрыв по направлению стрельбы
	struct SDirectionExplosion
	{
		CVec3 vPos;													// position
		float fDirection;										// (угол <=> угол в радианах) direction
		float fVerticalAngle;								// (угол <=> угол в радианах) angle from horizontal plane in the vertical plane, which crosses 'fDirection'
		// vis info
		CVec2 vPicturePosition;							// 2D picture position
		CVec3 vWorldPosition;								// 3D position, calculated from picture one
		//
		SDirectionExplosion();
		
		bool ToAIUnits();
		int operator&( IDataTree &ss );
	};
	//
	EDirection direction;									// vertical or horizontal bridge
	std::vector<SSegmentRPGStats> segments;	// all segments
	SDamageState states[3];								// alive, damaged and completelly destroyed bridge stats
	//
	std::vector<SFirePoint> firePoints;		// fire points...
	std::vector<SFirePoint> smokePoints;	// smoke points, при разрушении моста
	std::string szSmokeEffect;						// smoke effect, один на всех
	//
	enum EDirectionExplosionType
	{
		//не менять эти ID
		E_FRONT_LEFT,
		E_FRONT_RIGHT,
		E_BACK_RIGHT,
		E_BACK_LEFT,
		E_TOP_CENTER,
	};
	std::vector<SDirectionExplosion> dirExplosions;		// direction explosions, всего 5 штук, смотри EDirectionExplosionType
	std::string szDirExplosionEffect;			// direction explosion effect, один на всех
	//
	SBridgeRPGStats();
	virtual ~SBridgeRPGStats() { }
	//
	int GetIndexLocal( int nIndex, const std::vector<int> &indices, const char *pszName, int *pRandomSeed = 0 ) const
	{
		//NI_ASSERT_SLOW_TF( !indices.empty() && (nIndex < indices.size()), NStr::Format("Can't find any \"%s\" segment for bridge", pszName), return -1 );
		if ( nIndex == -1 )
		{
			if ( pRandomSeed == 0 )
				return indices[ rand() % indices.size() ];
			else
			{
				*pRandomSeed %= indices.size();
				return indices[*pRandomSeed];
			}
		}
		else
			return indices[nIndex];
	}
	//
	int GetRandomBeginIndex( int nIndex = -1, int nState = 0 )	const { return GetIndexLocal( nIndex, states[nState].begins, "begin", 0 ); }
	int GetRandomLineIndex( int nIndex = -1, int nState = 0 )		const { return GetIndexLocal( nIndex, states[nState].lines, "line", 0 ); }
	int GetRandomEndIndex( int nIndex = -1, int nState = 0 )		const { return GetIndexLocal( nIndex, states[nState].ends, "end", 0 ); }
	//
	int GetRandomBeginIndex( int nIndex, int nState, int *pRandomSeed )	const { return GetIndexLocal( nIndex, states[nState].begins, "begin", pRandomSeed ); }
	int GetRandomLineIndex( int nIndex, int nState, int *pRandomSeed )  const { return GetIndexLocal( nIndex, states[nState].lines, "line", pRandomSeed ); }
	int GetRandomEndIndex( int nIndex, int nState, int *pRandomSeed )   const { return GetIndexLocal( nIndex, states[nState].ends, "end", pRandomSeed ); }
	//
	bool IsBeginIndex( int nIndex ) const 
	{ 
		return	( std::find( states[0].begins.begin(), states[0].begins.end(), nIndex ) != states[0].begins.end() ) ||
						( std::find( states[1].begins.begin(), states[1].begins.end(), nIndex ) != states[1].begins.end() ) ||
						( std::find( states[2].begins.begin(), states[2].begins.end(), nIndex ) != states[2].begins.end() );
	}
	bool IsEndIndex( int nIndex ) const 
	{ 
		return	( std::find( states[0].ends.begin(), states[0].ends.end(), nIndex ) != states[0].ends.end() ) ||
						( std::find( states[1].ends.begin(), states[1].ends.end(), nIndex ) != states[1].ends.end() ) ||
						( std::find( states[2].ends.begin(), states[2].ends.end(), nIndex ) != states[2].ends.end() );
	}
	bool IsLineIndex( int nIndex ) const 
	{ 
		return	( std::find( states[0].lines.begin(), states[0].lines.end(), nIndex ) != states[0].lines.end() ) ||
						( std::find( states[1].lines.begin(), states[1].lines.end(), nIndex ) != states[1].lines.end() ) ||
						( std::find( states[2].lines.begin(), states[2].lines.end(), nIndex ) != states[2].lines.end() );
	}
	//
	const SSegmentRPGStats& GetSegmentStats( const int nIndex ) const
	{
		NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < segments.size(), NStr::Format("Index %d for the segments of the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), segments.size()) );
		return segments[nIndex];
	}
	//
	const SSpan& GetSpanStats( const int nIndex, const int nState = 0 ) const
	{
		NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < states[nState].spans.size(), NStr::Format("Index %d for the spans of the \"%s\"must be in the range [0..%d]", nIndex, szKeyName.c_str(), states[nState].spans.size()) );
		return states[nState].spans[nIndex];
	}
	// в следующих четырёх функциях 'nIndex' обозначает не 'segment', а 'span', из которого надо выдернуть 'nSlab' segment и вернуть его данные
	virtual const CVec2& STDCALL GetOrigin( const int nIndex = -1 ) const
	{
		const int nSegment = GetSpanStats(nIndex).nSlab;
		return GetSegmentStats(nSegment).vOrigin;
	}
	virtual const CArray2D<BYTE>& STDCALL GetPassability( const int nIndex = -1 ) const
	{
		const int nSegment = GetSpanStats(nIndex).nSlab;
		return GetSegmentStats(nSegment).passability;
	}
	virtual const CVec2& STDCALL GetVisOrigin( const int nIndex = -1 ) const
	{
		const int nSegment = GetSpanStats(nIndex).nSlab;
		return GetSegmentStats(nSegment).vVisOrigin;
	}
	virtual const CArray2D<BYTE>& STDCALL GetVisibility( const int nIndex = -1 ) const
	{
		const int nSegment = GetSpanStats(nIndex).nSlab;
		return GetSegmentStats(nSegment).visibility;
	}
	//
	virtual void STDCALL ToAIUnits();
	//

	virtual const int STDCALL GetTypeFromIndex( const int nIndex, const int nDamageState = 0 ) const;
	virtual const int STDCALL GetIndexFromType( const int nType, const int nDamageState = 0 ) const
		{ return GetIndexFromTypeLocal( nType, nDamageState, 0 ); }
	virtual const int STDCALL GetIndexFromType( const int nType, int *pCurRandomSeed ) const
		{ return GetIndexFromTypeLocal( nType, 0, pCurRandomSeed ); }
	//
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual const uLong STDCALL CalculateCheckSum() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** sound stats
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSoundRPGStats : public SCommonRPGStats
{
	struct SSound
	{
		std::string szPathName;								// sound file path name
		float fMinDist;												// minimal distance
		float fMaxDist;												// distance
		float fProbability;										// prabability inside set
		bool bPeacefull;											// should we shut this sound up during combat?
		//
		SSound();
		//
		int operator&( IDataTree &ss );
	};
	//
	std::vector<SSound> sounds;
	bool bLooped;														// is whole set looped
	//
	int virtual GetRandomSoundIndex() const;
	//
	SSoundRPGStats();
	virtual ~SSoundRPGStats();
	//
	virtual void STDCALL ToAIUnits();
	virtual int STDCALL operator&( IDataTree &ss );

	virtual const uLong STDCALL CalculateCheckSum() const { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** exp levels
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIExpLevel : public IGDBObject
{
	struct SLevel
	{
		int nExp;
		float fBonusSpeed;
		float fBonusRelaxTime;
		float fBonusDispersion;
		float fBonusSight;
		float fBonusFireRange;
		float fBonusRotate;
		//
		SLevel() : nExp( 0 ), fBonusSpeed( 1.0f ), fBonusRelaxTime( 1.0f ), fBonusDispersion( 1.0f ), fBonusSight( 1.0f ),
							 fBonusFireRange( 1.0f ), fBonusRotate( 1.0f ) { }
		
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;

			saver.Add( "Exp", &nExp );
			saver.Add( "BonusSpeed", &fBonusSpeed );
			saver.Add( "BonusRelaxTime", &fBonusRelaxTime );
			saver.Add( "BonusDispersion", &fBonusDispersion );
			saver.Add( "BonusSight", &fBonusSight );
			saver.Add( "BonusFireRange", &fBonusFireRange );
			saver.Add( "BonusRotate", &fBonusRotate );

			return 0;
		}
	};
	//
	std::string szTypeName;
	EUnitRPGType eType;
	std::vector<SLevel> levels;
	//
	SAIExpLevel() : levels( 1 ) { }
	
	virtual const char* STDCALL GetName() const { return "AIExpLevel"; }
	virtual const char* STDCALL GetParentName() const { return szTypeName.c_str(); }
	virtual const uLong STDCALL GetCheckSum() const { return 0L; }
	//
	virtual int STDCALL operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RPGSTATS_H__
