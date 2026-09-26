#if !defined(__MiniMap__Types__)
#define __MiniMap__Types__

#include "../Image/Image.h"

struct SRMImageCreateParameter
{
	static const float INTERMISSION_IMAGE_BRIGHTNESS;
	static const float INTERMISSION_IMAGE_CONSTRAST;
	static const float INTERMISSION_IMAGE_GAMMA;
	
	std::string szImageFileName;
	CTPoint<int> size;
	bool bDDS;
	bool bColorCorrection;
	float fBrightness;
	float fContrast;
	float fGamma;
	// Written as the Ultra image alone, uncompressed, instead of the
	// "_c"/"_l"/"_h" trio, and szImageFileName is then the full path of the
	// file rather than a data-storage name. Runtime only: not part of the saved
	// parameters.
	bool bUltra;

	SRMImageCreateParameter() : size( 0, 0 ), bDDS( true ),	bColorCorrection( false ), fBrightness( 0.0f ), fContrast( 0.0f ), fGamma( 0.0f ), bUltra( false ) {}
	SRMImageCreateParameter( const std::string &rszImageFileName,
													 const CTPoint<int> &rSize,
													 bool _bDDS,
													 bool _bColorCorrection = false,
													 float _fBrightness = 0.0f,
													 float _fContrast = 0.0f,
													 float _fGamma = 0.0f,
													 bool _bUltra = false )
		: szImageFileName( rszImageFileName ),
			size( rSize ),
			bDDS( _bDDS ),
			bColorCorrection( _bColorCorrection ),
			fBrightness( _fBrightness ),
			fContrast( _fContrast ),
			fGamma( _fGamma ),
			bUltra( _bUltra ) {}
	SRMImageCreateParameter( const SRMImageCreateParameter &rImageCreateParameter )
		: szImageFileName( rImageCreateParameter.szImageFileName ),
			size( rImageCreateParameter.size ),
			bDDS( rImageCreateParameter.bDDS ),
			bColorCorrection( rImageCreateParameter.bColorCorrection ),
			fBrightness( rImageCreateParameter.fBrightness ),
			fContrast( rImageCreateParameter.fContrast ),
			fGamma( rImageCreateParameter.fGamma ),
			bUltra( rImageCreateParameter.bUltra ) {}
	SRMImageCreateParameter& operator=( const SRMImageCreateParameter &rImageCreateParameter )
	{
		if( &rImageCreateParameter != this )
		{
			szImageFileName = rImageCreateParameter.szImageFileName;
			size = rImageCreateParameter.size,
			bDDS = rImageCreateParameter.bDDS;
			bColorCorrection = rImageCreateParameter.bColorCorrection;
			fBrightness = rImageCreateParameter.fBrightness;
			fContrast = rImageCreateParameter.fContrast;
			fGamma = rImageCreateParameter.fGamma;
			bUltra = rImageCreateParameter.bUltra;
		}
		return *this;
	}	

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};
typedef std::vector<SRMImageCreateParameter> CRMImageCreateParameterList;

struct SRMMiniMapCreateParameter
{
	enum MINI_MAP_LAYERS
	{
		MML_BRIDGES		= 0,
		MML_BUILDINGS	= 1,
		MML_RIVERS		= 2,
		MML_RAILROADS	= 3,
		MML_ROADS3D		= 4,
		MML_FORESTS		= 5,
		MML_TERRAIN		= 6,
		MML_COUNT			= 7,
	};

	enum EMBOSS_TYPE
	{
		ET_INNER			= 0,
		ET_OUTER			= 1,
		ET_EMBOSS			= 2,
		ET_COUNT			= 3,
	};

	struct SMiniMapLayer
	{
		SColor color;
		SColor borderColor;

		CTPoint<int> shadowPoint;
		CTPoint<int> embossPoint;
		int embossFilterSize;
		EMBOSS_TYPE embossType;
		std::string noiseImage;
		bool bScaleNoise;
		EImageScaleMethod scaleMethod;

		SMiniMapLayer() : color( 0 ), borderColor( 0 ), shadowPoint( 0, 0 ), embossPoint( 0, 0 ), embossFilterSize( 0 ), embossType( ET_INNER ), bScaleNoise( false ), scaleMethod( ISM_FILTER ) {}

		virtual int STDCALL operator&( IStructureSaver &ss );
		virtual int STDCALL operator&( IDataTree &ss );
	};

	std::vector<SMiniMapLayer> layers;
	int nWoodRadius;
	float fTerrainShadeRatio;
	bool bAllBuildingPassability;
	bool bTerrainShades;
	DWORD dwMinAlpha;
	DWORD dwBridgeWidth;

	SRMMiniMapCreateParameter() : nWoodRadius( 0 ), fTerrainShadeRatio( 0.0f ), bAllBuildingPassability( false ), bTerrainShades( false ), dwMinAlpha( 0 ), dwBridgeWidth( 0 ) {}

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};
#endif // #if !defined(__MiniMap__Types__)
