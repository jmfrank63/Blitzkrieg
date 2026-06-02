#include "stdafx.h"

#include "MiniMap_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const float SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS = 0.0f;
const float SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST = 0.0f;
const float SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA = 0.0f;

int SRMImageCreateParameter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szImageFileName );
	saver.Add( 2, &size );
	saver.Add( 3, &bDDS );
	saver.Add( 4, &bColorCorrection );
	saver.Add( 5, &fBrightness );
	saver.Add( 6, &fContrast );
	saver.Add( 7, &fGamma );

	return 0;
}

int SRMImageCreateParameter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "FileName", &szImageFileName );
	saver.Add( "Size", &size );
	saver.Add( "DDSFormat", &bDDS );
	saver.Add( "ColorCorrection", &bColorCorrection );
	saver.Add( "Brightness", &fBrightness );
	saver.Add( "Contrast", &fContrast );
	saver.Add( "Gamma", &fGamma );

	return 0;
}

int SRMMiniMapCreateParameter::SMiniMapLayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &color );
	saver.Add( 2, &borderColor );
	saver.Add( 3, &shadowPoint );
	saver.Add( 4, &embossPoint );
	saver.Add( 5, &embossFilterSize );
	saver.Add( 6, &embossType );
	saver.Add( 7, &noiseImage );
	saver.Add( 8, &bScaleNoise );
	saver.Add( 9, &scaleMethod );

	return 0;
}

int SRMMiniMapCreateParameter::SMiniMapLayer::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Color", &color );
	saver.Add( "BorderColor", &borderColor );
	saver.Add( "ShadowPoint", &shadowPoint );
	saver.Add( "EmbossPoint", &embossPoint );
	saver.Add( "EmbossFilterSize", &embossFilterSize );
	saver.Add( "EmbossType", &embossType );
	saver.Add( "NoiseImage", &noiseImage );
	saver.Add( "ScaleNoise", &bScaleNoise );
	saver.Add( "ScaleMethod", &scaleMethod );

	return 0;
}

int SRMMiniMapCreateParameter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &layers );
	saver.Add( 3, &nWoodRadius ); 
	saver.Add( 4, &fTerrainShadeRatio );
	saver.Add( 5, &bAllBuildingPassability );
	saver.Add( 6, &bTerrainShades );
	saver.Add( 7, &dwMinAlpha );
	saver.Add( 8, &dwBridgeWidth );
	
	return 0;
}

int SRMMiniMapCreateParameter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Layers", &layers );
	saver.Add( "WoodRadius", &nWoodRadius ); 
	saver.Add( "TerrainShadeRatio", &fTerrainShadeRatio );
	saver.Add( "ShowAllBuildingsPassability", &bAllBuildingPassability );
	saver.Add( "ShowTerrainShades", &bTerrainShades );
	saver.Add( "MinAlpha", &dwMinAlpha );
	saver.Add( "BridgeWidth", &dwBridgeWidth );

	return 0;
}
