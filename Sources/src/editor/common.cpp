#include "StdAfx.h"
#include "..\scene\scene.h"
#include "..\GFX\gfx.h"
#include "..\GFX\GFXHelper.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Formats\fmtMap.h"
#include "..\Scene\builders.h"

#include "common.h"

const std::string szTGAFilter = "TGA Files (*.tga)|*.tga||";
const std::string szTextFilter = "Text files (*.txt)|*.txt||";
const std::string szLuaFilter = "LUA files (*.lua)|*.lua||";
const std::string szMusicFilter = "Music files (*.ogg)|*.ogg||";
const std::string szMovieFilter = "Bik movie files (*.bik)|*.bik||";
const std::string szXMLFilter = "XML files (*.xml)|*.xml||";
const std::string szMapFilter = "Map files (*.xml, *.bzm)|*.xml;*.bzm||";
const std::string szSoundFilter = "Sound Files (*.wav, *.ogg)|*.wav;*.ogg||";
const std::string szMODFilter = "MOD Files (*.mod)|*.mod||";
const std::string szSANFilter = "Structure animation files (*.san)|*.san||";
const std::string szDDSFilter = "DDS compressed textures (*.dds)|*.dds||";


void FillTileMaps( int nSizeX, int nSizeY, vector<STileMapsDesc> &tileMaps, bool bGenerateInverse )
{
	int nNumColumn = 0;
	for ( int i=0; i<nSizeY-32; i+=32 )
	{
		CVec2 maps[4];
		STileMapsDesc tileMap;
		
		for ( int k=0; k<4; k++ )
		{
			GetPrimaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetPrimaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}
		
		for ( int k=0; k<3; k++ )
		{
			GetSecondaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetSecondaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}

		nNumColumn++;
	}
	
	{
		CVec2 maps[4];
		STileMapsDesc tileMap;
		
		for ( int k=0; k<4; k++ )
		{
			GetPrimaryMaps( k, nNumColumn, false, maps, nSizeX, nSizeY );
			tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
			tileMaps.push_back( tileMap );
			
			if ( bGenerateInverse )
			{
				GetPrimaryMaps( k, nNumColumn, true, maps, nSizeX, nSizeY );
				tileMap = STileMapsDesc( maps[0], maps[1], maps[2], maps[3] );
				tileMaps.push_back( tileMap );
			}
		}
	}
}

bool IsSpriteHit( IObjVisObj *pSprite, const CVec2 &point, CVec2 *pShift )
{
	*pShift = VNULL2;
	IGFX *pGFX = GetSingleton<IGFX>();
	
	CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	SHMatrix matTransform = mstack();
	mstack.Pop( 3 );
	return pSprite->IsHit( matTransform, point, pShift );
}

bool CheckDDSExtension( const char *pszFileName )
{
	std::string szName = pszFileName;
	int nPos = szName.rfind( '\\' );
	if ( nPos != std::string::npos )
		szName = szName.substr( nPos + 1 );

	const int nRightShift = 6;
	if ( szName.size() <= nRightShift )
	{
		std::string szErr = "Error: file name is too short: ";
		szErr += szName;
		szErr += "\nFile name must contain _h.dds at the end";
		AfxMessageBox( szErr.c_str() );
		return false;
	}

	std::string szSearch = szName.c_str() + szName.size() - nRightShift;
	if ( szSearch != "_h.dds" )
	{
		std::string szErr = "Error: file name must contain _h.dds at the end: ";
		szErr += szName;
		AfxMessageBox( szErr.c_str() );
		return false;
	}

	return true;
}
