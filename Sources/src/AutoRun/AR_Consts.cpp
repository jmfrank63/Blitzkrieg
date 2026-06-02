#include "StdAfx.h"
#include "AR_Types.h"
DWORD SARButton::GAME_INSTALLED = 0x02;
DWORD SARButton::GAME_NOT_INSTALLED = 0x01;

DWORD SARMainSection::ALIGN_LEFT = 0x01;
DWORD SARMainSection::ALIGN_RIGHT = 0x02;
DWORD SARMainSection::ALIGN_HOR_CENTER = SARMainSection::ALIGN_LEFT | SARMainSection::ALIGN_RIGHT;
DWORD SARMainSection::ALIGN_TOP = 0x04;
DWORD SARMainSection::ALIGN_BOTTOM = 0x08;
DWORD SARMainSection::ALIGN_VER_CENTER = SARMainSection::ALIGN_TOP | SARMainSection::ALIGN_BOTTOM;
DWORD SARMainSection::ALIGN_CENTER = SARMainSection::ALIGN_HOR_CENTER | SARMainSection::ALIGN_VER_CENTER;
const char* SARButton::ACTION_NAMES[ACTION_COUNT] =
{
	"open",
	"shellopen",
	"menu",
	"return",
	"exit",
};

const char* SARButton::ACTION_BEHAVIOUR_NAMES[AB_COUNT] =
{
	"lock",
	"keep",
	"exit",
};


const char* SARMainSection::RT_NAMES[RT_COUNT] =
{
	"hklm",
	"hkcu",
};

const char* CARMenuSelector::DATA_FILE_NAME = "AutoRun.pak";
const char* CARMenuSelector::CONFIGURATION_FILE_NAME = "AutoRun.ini";

const char* CARMenuSelector::ACTION_FOLDER_NAMES[AF_COUNT] =
{
	"install",
	"current",
	"uninstall",
};
