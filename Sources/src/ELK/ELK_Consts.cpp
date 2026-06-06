#include "StdAfx.h"

#include "ELK_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

LPCTSTR SELKTextProperty::STATE_LABELS[STATE_COUNT] =
{
	_T( "Not_Translated" ),
	_T( "Outdated" ),
	_T( "Translated" ),
	_T( "Approved" ),
};

LPCTSTR SELKTextProperty::STATE_NAMES[STATE_COUNT] =
{
	_T( "Not translated" ),
	_T( "Outdated" ),
	_T( "Translated" ),
	_T( "Approved" ),
};

const TCHAR SELKElement::DATA_BASE_FOLDER[] = _T( "_DATA_BASE\\" );
const TCHAR SELKElement::DATA_BASE_RESERVE_FOLDER[] = _T( "_DATA_BASE_RESERVE\\" );


const int CELK::DEFAULT_CODE_PAGE = 0;
const TCHAR CELK::GAME_REGISTRY_FOLDER[] = _T( "Software\\Nival Interactive\\Blitzkrieg" );
const TCHAR CELK::GAME_REGISTRY_KEY[] = _T( "InstallFolder" );
const TCHAR CELK::TEXTS_PAK_FILE_NAME[] = _T( "Data\\Texts" );
const TCHAR CELK::PAK_FILE_NAME[] = _T( "game" );
const TCHAR CELK::ELK_FILE_NAME[] = _T( "elk" );
const TCHAR CELK::FOLDER_DESC_FILE_NAME[] = _T( "_folder" );
const TCHAR CELK::GAME_FILE_NAME[] = _T( "game.exe" );
const TCHAR CELK::GAME_PARAMETERS[] = _T( " -windowed" );
const TCHAR CELK::TEMP_FOLDER[] = _T( ".TEMP_FOLDER\\" );
const TCHAR CELK::GAME_DATA_FOLDER[] = _T( "data\\" );

const TCHAR CELK::PAK_DESCRIPTION[] = _T( "resource" );

const TCHAR CELK::PAK_EXTENTION[] = _T( ".pak" );
const TCHAR CELK::UPD_EXTENTION[] = _T( ".upd" );
const TCHAR CELK::XML_EXTENTION[] = _T( ".xml" );
const TCHAR CELK::ELK_EXTENTION[] = _T( ".elk" );
const TCHAR CELK::TXT_EXTENTION[] = _T( ".txt" );
const TCHAR CELK::DSC_EXTENTION[] = _T( ".dsc" );
const TCHAR CELK::PAK_DESCRIPTION_EXTENTION[] = _T( ".description" );

const TCHAR CELK::ZIP_EXE[] = _T( "zip.exe" );
const TCHAR CELK::ELK_CHM[] = _T( "elk.chm" );


const int CFontGen::LEADING_PIXELS = 2;
const TCHAR CFontGen::FONT_NAME[] = _T( "Times New Roman" );
const TCHAR CFontGen::FONT_FILE_NAME[] = _T( "1" );
const TCHAR CFontGen::FONT_FILE_EXTENTION[] = _T( ".tfd" );

const DWORD CFontGen::FONTS_SIZE[FONTS_COUNT] = { 8, 16, 24, 48 };
LPCTSTR CFontGen::FONTS_FOLDER[FONTS_COUNT] =
{
	_T( "fonts\\tiny\\" ),
	_T( "fonts\\small\\" ),
	_T( "fonts\\medium\\" ),
	_T( "fonts\\large\\" ),
};
