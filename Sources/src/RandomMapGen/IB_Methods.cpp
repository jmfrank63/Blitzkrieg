#include "StdAfx.h"

#include "IB_Types.h"
#include "../Formats/fmtTerrain.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CSpritesPackBuilder::nCellSizeY = static_cast<int>( fCellSizeY );
const int CSpritesPackBuilder::nCellSizeX = static_cast<int>( fCellSizeX );

const SColor CRMImageBuilder::BLACK_COLOR					= SColor( 0xFF, 0, 0, 0 );
const SColor CRMImageBuilder::WHITE_COLOR					= SColor( 0xFF, 0xFF, 0xFF, 0xFF );
const SColor CRMImageBuilder::GRAY_LIGHTER_COLOR	= SColor( 0xFF, 0x80, 0x80, 0x80 );
const SColor CRMImageBuilder::GRAY_DARKER_COLOR		= SColor( 0xFF, 0x7F, 0x7F, 0x7F );
const SColor CRMImageBuilder::BASE_EMBOSS_COLOR		= SColor( 0xFF, 0x7F, 0x7F, 0x7F );
