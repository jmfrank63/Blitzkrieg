#ifndef __TERRAGEN_H__
#define __TERRAGEN_H__

#include "BetaSpline.h"

#define ALG_DIR( str )  "Landscapes"##str

void  InitGR( float sigma );
float GaussRand();

const float DEF_GRID_STEP = 1.0f;
const float HF_SCALE      = 100.0f;

enum ETerGenAlgs
{
  TG_FBM,
  TG_MULTI,
  TG_HETERO,
  TG_HYBRID,
  TG_RIDGED,
  TG_NULL,
  TG_MAX_ALG
};

struct SfBmValues
{
  ETerGenAlgs alg;
  float h;
  float lacunarity;
  float octaves;
  float offset;
  float gain;
  float featSize;
  float fBaseHeight;
  float fRange;

	SfBmValues() : alg( TG_NULL ), h( 0.0f ), lacunarity( 0.0f ), octaves( 0.0f ), offset( 0.0f ), gain( 0.0f ), featSize( 0.0f ), fBaseHeight( 0.0f ), fRange( 0.0f ) {}
	SfBmValues(  ETerGenAlgs _alg, float _h, float _lacunarity, float _octaves, float _offset, float _gain, float _featSize, float _fBaseHeight, float _fRange )
		: alg( _alg ), h( _h ), lacunarity( _lacunarity ), octaves( _octaves ), offset( _offset ), gain( _gain ), featSize( _featSize ), fBaseHeight( _fBaseHeight ), fRange( _fRange ) {}
	SfBmValues(  const SfBmValues &rfBmValues )
		: alg( rfBmValues.alg ), h( rfBmValues.h ), lacunarity( rfBmValues.lacunarity ), octaves( rfBmValues.octaves ), offset( rfBmValues.offset ), gain( rfBmValues.gain ), featSize( rfBmValues.featSize ), fBaseHeight( rfBmValues.fBaseHeight ), fRange( rfBmValues.fRange ) {}
	SfBmValues& operator=( const SfBmValues &rfBmValues )
	{
		if( &rfBmValues != this )
		{
			alg					= rfBmValues.alg;
			h						= rfBmValues.h;
			lacunarity	= rfBmValues.lacunarity;
			octaves			= rfBmValues.octaves;
			offset			= rfBmValues.offset;
			gain				= rfBmValues.gain;
			featSize		= rfBmValues.featSize;
			fBaseHeight	= rfBmValues.fBaseHeight;
			fRange			= rfBmValues.fRange;
		}
		return *this;
	}

	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &alg );	
		saver.Add( 2, &h );
		saver.Add( 3, &lacunarity );
		saver.Add( 4, &octaves );
		saver.Add( 5, &offset );	
		saver.Add( 6, &gain );
		saver.Add( 7, &featSize );	
		saver.Add( 9, &fBaseHeight );	
		saver.Add( 10, &fRange );	
		
		return 0;	
	}
	virtual int STDCALL operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss; 

		saver.Add( "alg", &alg );	
		saver.Add( "h", &h );
		saver.Add( "lacunarity", &lacunarity );
		saver.Add( "octaves", &octaves );
		saver.Add( "offset", &offset );	
		saver.Add( "gain", &gain );
		saver.Add( "featSize", &featSize );	
		saver.Add( "fBaseHeight", &fBaseHeight );	
		saver.Add( "fRange", &fRange );	

		return 0;
	}
};

class CHField
{
private:
  DWORD  dwDimX;
  DWORD  dwDimY;
	std::vector<float> hf;
  CBetaSpline  smoothFunc;

  bool   bChanged;
  float  fGridStep;

	std::vector<float>  fBmExponents;

  void   InitFBmExp( float h, float lacunarity, float octaves );
  float  FBm( CVec3 pt, float lacunarity, float octaves );
  float  Multifractal( CVec3 pt, float lacunarity, float octaves, float offset );
  float  HeteroTerrain( CVec3 pt, float lacunarity, float octaves, float offset );
  float  HybridMultifractal( CVec3 pt, float lacunarity, float octaves, float offset );
  float  RidgedMultifractal( CVec3 pt, float lacunarity, float octaves, float offset, float gain );

public:
  const static SfBmValues fBmDefVals[TG_MAX_ALG];

  CHField();
  CHField( int width, int height, float fGridStep = DEF_GRID_STEP, float val = 0 );
  ~CHField();

  void   Init( int width, int height, float fGridStep = DEF_GRID_STEP, float val = 0 );
  void   Scale( float scale );
  float  MinHeight() const;
  float  MaxHeight() const;
  float  AveHeight() const;
  float  AltitudeRange( float *pMin, float *pMax ) const;
  void   Translate( float shift );

  DWORD  Width()  const { return dwDimX; }
  DWORD  Height() const { return dwDimY; }

  float  H( int x, int y ) const { return hf[y * dwDimX + x]; }
  float& H( int x, int y ) { bChanged = true; return hf[y * dwDimX + x]; }
  float& H( int offset ) { bChanged = true; return hf[offset]; }
  float  H( int offset ) const { return hf[offset]; }
  float  HCheck( int x, int y ) const;
  float* GetRawBuf() { bChanged = true; return &hf[0]; }
  float  GetGridStep() const { return fGridStep; }
  float  GetHApprox( float x, float y ) const;
  CVec3  GetNormal( float x, float y ) const;
  float  Volume();
  void   Pow( float exp );

  static const SfBmValues& GetAlgDefVals( ETerGenAlgs alg ) { return fBmDefVals[alg]; }
  void   GenTest();
  void   Generate( const SfBmValues &vals );
  void   GenFBm( float h, float lacunarity, float octaves, float scale = 1.0f );
  void   GenMultifractal( float h, float lacunarity, float octaves, float offset, float scale = 1.0f );
  void   GenHeteroTerrain( float h, float lacunarity, float octaves, float offset, float scale = 1.0f );
  void   GenHybridMultifractal( float h, float lacunarity, float octaves, float offset, float scale = 1.0f );
  void   GenRidgedMultifractal( float h, float lacunarity, float octaves, float offset, float gain, float scale = 1.0f );

  void operator= ( const CHField &val );
  CHField& operator- ();
  void   Sum( const CHField &op1, const CHField &op2 );
};

inline float CHField::HCheck( int x, int y ) const
{
  x = x < 0 ? x = 0 : x >= dwDimX ? dwDimX - 1 : x;
  y = y < 0 ? y = 0 : y >= dwDimY ? dwDimY - 1 : y;
  return hf[y * dwDimX + x];
}

#endif // __TERRAGEN_H__