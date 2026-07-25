#ifndef __ICONS_H__
#define __ICONS_H__
#pragma ONCE
enum EIconType
{
	ICON_HP_BAR							= 0,
	ICON_GROUP							= 1,
	ICON_LEVEL							= 2,
	ICON_WILL_BE_NEW_ICON1	= 3,
	ICON_WILL_BE_NEW_ICON2	= 4,
	ICON_UNINSTALL					= 5,
	ICON_CAMOUFLAGE					= 6,
	ICON_AMBUSH							= 7,
	ICON_TRACK							= 8,
	ICON_LOW_AMMO						= 9,
	ICON_NO_AMMO						= 10,
	ICON_LOW_MORAL					= 11,
	ICON_UNIT_NO_SUPPLY			= 12,
	ICON_STORAGE_NO_SUPPLY	= 13,
	ICON_STORAGE_MINE				= 14,
	ICON_STORAGE_FRIEND			= 15,
	ICON_STORAGE_FOE				= 16,
	ICON_ALT_SHELL					= 17,
	ICON_ENTRENCHED					= 18,
	ICON_NUM_ICONS
};
inline const char* GetIconName( int nIconType )
{
	switch ( nIconType & 0xffff )
	{
		case ICON_GROUP:							return NStr::Format( "icons\\%d", ( nIconType >> 16 ) & 0xffff );
		case ICON_LEVEL:							return NStr::Format( "icons\\level%d", ((nIconType >> 16) & 0xffff) );
		case ICON_UNINSTALL:					return "icons\\uninstall";
		case ICON_AMBUSH:							return "icons\\ambush";
		case ICON_TRACK:							return "icons\\track";
		case ICON_LOW_AMMO:						return "icons\\lowammo";
		case ICON_NO_AMMO:						return "icons\\noammo";
		case ICON_LOW_MORAL:					return "icons\\lowmoral";
		case ICON_UNIT_NO_SUPPLY:			return "icons\\unit_no_supply";
		case ICON_STORAGE_NO_SUPPLY:	return "icons\\storage_no_supply";
		case ICON_STORAGE_FRIEND:			return "icons\\storage_friend";
		case ICON_STORAGE_FOE:				return "icons\\storage_foe";
		case ICON_ALT_SHELL:					return "icons\\alt_shell";
		case ICON_ENTRENCHED:					return "icons\\spade";
	}
	return "";
}
inline const DWORD MakeHPBarColor( const float fHP )
{
	// HP can exceed [0..1]; unclamped values make the float->DWORD casts below
	// negative, which is undefined behavior (traps under UBSan).
	const float fClamped = fHP < 0.0f ? 0.0f : ( fHP > 1.0f ? 1.0f : fHP );
	if ( fClamped >= 0.5f )
	{
		const DWORD r = DWORD( ( 1.0f - fClamped ) * 510.0f );
		return 0xff00ff00 | (r << 16);
	}
	else
	{
		const DWORD g = DWORD( fClamped * 510.0f );
		return 0xffff0000 | (g << 8);
	}
}
#endif // __ICONS_H__
