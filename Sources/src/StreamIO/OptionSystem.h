#ifndef __OPTIONSSYSTEM_H__
#define __OPTIONSSYSTEM_H__
#pragma ONCE
#include "../Misc/VarSystem.h"
enum EOptionEditorType
{
	EOET_NUMERIC_ENTRY						= 1,				// NUMERIC EDIT BOX( NUMERIC INPUT )
	EOET_SLIDER										= 2,				// SLIDER CONSTROL TO ADJUST
	EOET_CLICK_SWITCHES						= 3,				// SEVERAL STRING VALUES.
	EOET_TEXT_ENTRY								= 4,				// TEXT FEILD (ANY INPUT)
	EOET_GAMESPY_TEXT_ENTRY				= 5,				// text feild, GameSpy allowed Characters only
};
enum EOptionsFlag
{
	OPTION_FLAG_GENERIC_OPTION						= 1<<0,
	OPTION_FLAG_DEVELOPER_ONLY						= 1<<1,
	OPTION_FLAG_MULTIPLAYER_SCREEN				= 1<<2,
	OPTION_FLAG_BEFORE_MULTIPLAYER_START	= 1<<3,		// SETTINGS FOR MULTIPLAYER SERVER.
	OPTION_FLAG_CHANGE_IN_MISSION					= 1<<4,
	OPTION_FLAG_CHANGE_IN_MISSION_INCLUDING_MP = 1<<5,		// options, that can be changed during MP game
	OPTION_FLAG_MULTIPLAYER_START					= 1<<6,		// SETTINGS FOR MULTIPLAYER (staging room)


	OPTION_FLAG_MAIN_OPTIONS	= OPTION_FLAG_GENERIC_OPTION | OPTION_FLAG_MULTIPLAYER_SCREEN | OPTION_FLAG_BEFORE_MULTIPLAYER_START |OPTION_FLAG_CHANGE_IN_MISSION |OPTION_FLAG_CHANGE_IN_MISSION_INCLUDING_MP,
	OPTION_FLAG_IN_MISSION	= OPTION_FLAG_CHANGE_IN_MISSION,
	OPTION_FLAG_IN_MP_MISSION = OPTION_FLAG_CHANGE_IN_MISSION_INCLUDING_MP,
};
struct SOptionDesc
{
	std::string szDivision;								// division name. to create file name with localized name just add '.name' to it
	std::string szName;										// option name. to create  file name with localized name just add '.name' to it
	int nDataType;												// data type like in 'VARIANT'
	int nEditorType;											// control to edit this option
	DWORD dwFlags;												// flags. for example, can this option be changed in mission etc.
	variant_t defaultValue;								// default value for this option
	bool bInstantApply;										// option must be applied instantly.
};
struct SOptionDropListValue
{
	std::string szProgName;								// program name
};
interface IOptionSystemIterator : public IVarIterator
{
	virtual const SOptionDesc* STDCALL GetDesc() const = 0;
	virtual const std::vector<SOptionDropListValue>& STDCALL GetDropValues() const = 0;
};
interface IOptionSystem : public IVarSystem
{
	enum { tidTypeID = -4 };
	virtual const SOptionDesc* STDCALL GetDesc( const std::string &szVarName ) const = 0;
	virtual const std::vector<SOptionDropListValue>& STDCALL GetDropValues( const std::string &szVarName ) const = 0;
	virtual IOptionSystemIterator* STDCALL CreateIterator( const DWORD dwMask = 0xffffffff ) = 0;
	virtual bool STDCALL SerializeConfig( IDataTree *pSS ) = 0;
	virtual void STDCALL Init() = 0;
	virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault ) = 0;
};
#endif // __OPTIONSSYSTEM_H__
