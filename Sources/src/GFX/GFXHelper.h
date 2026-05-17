#ifndef __GFXHELPER_H__
#define __GFXHELPER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline const DWORD MAKE_ARGB( const DWORD a, const DWORD r, const DWORD g, const DWORD b ) { return (a << 24) | (r << 16) | (g << 8) | b; }
inline const DWORD MAKE_RGB( const DWORD a, const DWORD r, const DWORD g, const DWORD b ) { return 0xff000000 | (r << 16) | (g << 8) | b; }
inline DWORD MakeShade( const DWORD dwShade ) { return 0xff000000 | (dwShade << 16) | (dwShade << 8) | dwShade; }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** color format structures
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGFXColor8888
{
	union
	{
		DWORD color;
		struct
		{
			DWORD b : 8;
			DWORD g : 8;
			DWORD r : 8;
			DWORD a : 8;
		};
	};
	//
	SGFXColor8888() {  }
	SGFXColor8888( DWORD dwColor )	: color( dwColor ) {  }
	SGFXColor8888( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	//
	operator DWORD() const { return color; }
};
struct SGFXColor1555
{
	union
	{
		WORD color;
		struct
		{
			WORD b : 5;
			WORD g : 5;
			WORD r : 5;
			WORD a : 1;
		};
	};
	//
	SGFXColor1555() {  }
	SGFXColor1555( WORD wColor )	: color( wColor ) {  }
	SGFXColor1555( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	//
	operator WORD() const { return color; }
};
struct SGFXColor4444
{
	union
	{
		WORD color;
		struct
		{
			WORD b : 4;
			WORD g : 4;
			WORD r : 4;
			WORD a : 4;
		};
	};
	//
	SGFXColor4444() {  }
	SGFXColor4444( WORD wColor )	: color( wColor ) {  }
	SGFXColor4444( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	//
	operator WORD() const { return color; }
};
struct SGFXColor0555
{
	union
	{
		WORD color;
		struct
		{
			WORD b : 5;
			WORD g : 5;
			WORD r : 5;
			WORD a : 1;
		};
	};
	//
	SGFXColor0555() {  }
	SGFXColor0555( WORD wColor )	: color( wColor ) {  }
	SGFXColor0555( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	//
	operator WORD() const { return color; }
};
struct SGFXColor0565
{
	union
	{
		WORD color;
		struct
		{
			WORD b : 5;
			WORD g : 6;
			WORD r : 5;
			WORD a : 1;
		};
	};
	//
	SGFXColor0565() {  }
	SGFXColor0565( WORD wColor )	: color( wColor ) {  }
	SGFXColor0565( BYTE _a, BYTE _r, BYTE _g, BYTE _b ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
	//
	operator WORD() const { return color; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** vertex format structures
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGFXVertex
{
	enum { format = GFXFVF_XYZ | GFXFVF_NORMAL | GFXFVF_TEX1 };
	union
	{
		struct
		{
			float x, y, z;
			float nx, ny, nz;
			float tu, tv;
		};
		struct
		{
			CVec3 pos;
			CVec3 norm;
			CVec2 tex;
		};
	};
	//
	void Setup( float _x, float _y, float _z, float _nx, float _ny, float _nz, float _tu, float _tv )
	{
		x = _x;
		y = _y;
		z = _z;
		nx = _nx;
		ny = _ny;
		nz = _nz;
		tu = _tu;
		tv = _tv;
	}
	void Setup( const CVec3 &_pos, const CVec3 &_norm, const CVec2 &_tex )
	{
		pos = _pos;
		norm = _norm;
		tex = _tex;
	}
};
struct SGFXVertex2
{
	enum { format = GFXFVF_XYZ | GFXFVF_NORMAL | GFXFVF_TEX2 };
	union
	{
		struct
		{
			float x, y, z;
			float nx, ny, nz;
			float tu, tv;
			float tu1, tv1;
		};
		struct
		{
			CVec3 pos;
			CVec3 norm;
			CVec2 tex;
			CVec2 tex1;
		};
	};
	//
	void Setup( float _x, float _y, float _z, float _nx, float _ny, float _nz, float _tu, float _tv, float _tu1, float _tv1 )
	{
		x = _x;
		y = _y;
		z = _z;
		nx = _nx;
		ny = _ny;
		nz = _nz;
		tu = _tu;
		tv = _tv;
		tu1 = _tu1;
		tv1 = _tv1;
	}
	void Setup( const CVec3 &_pos, const CVec3 &_norm, const CVec2 &_tex, const CVec2 &_tex1 )
	{
		pos = _pos;
		norm = _norm;
		tex = _tex;
		tex1 = _tex1;
	}
};
struct SGFXLVertex
{
	enum { format = GFXFVF_XYZ | GFXFVF_DIFFUSE | GFXFVF_SPECULAR | GFXFVF_TEX1 };
	union
	{
		struct
		{
			float x, y, z;
			DWORD color, specular;
			float tu, tv;
		};
		struct
		{
			CVec3 pos;
			DWORD color, specular;
			CVec2 tex;
		};
	};
	//
	SGFXLVertex() : x(0), y(0), z(0), color(0), specular(0), tu(0), tv(0) {}
	//
	void Setup( float _x, float _y, float _z, DWORD _color, DWORD _specular, float _tu, float _tv )
	{
		x = _x;
		y = _y;
		z = _z;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
	}
	void Setup( const CVec3 &_pos, DWORD _color, DWORD _specular, const CVec2 &_tex )
	{
		pos = _pos;
		color = _color;
		specular = _specular;
		tex = _tex;
	}
	// this two setup functions use fake '_rhw' parameter for TL => non-TL vertices compatibility
	void Setup( float _x, float _y, float _z, float _rhw, DWORD _color, DWORD _specular, float _tu, float _tv )
	{
		x = _x;
		y = _y;
		z = _z;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
	}
	void Setup( const CVec3 &_pos, float _rhw, DWORD _color, DWORD _specular, const CVec2 &_tex )
	{
		pos = _pos;
		color = _color;
		specular = _specular;
		tex = _tex;
	}
};
struct SGFXLVertex2
{
	enum { format = GFXFVF_XYZ | GFXFVF_DIFFUSE | GFXFVF_SPECULAR | GFXFVF_TEX2 };
	union
	{
		struct
		{
			float x, y, z;
			DWORD color, specular;
			float tu, tv;
			float tu1, tv1;
		};
		struct
		{
			CVec3 pos;
			DWORD color, specular;
			CVec2 tex, tex1;
		};
	};
	//
	void Setup( float _x, float _y, float _z, DWORD _color, DWORD _specular, float _tu, float _tv, float _tu1, float _tv1 )
	{
		x = _x;
		y = _y;
		z = _z;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
		tu1 = _tu1;
		tv1 = _tv1;
	}
	void Setup( const CVec3 &_pos, DWORD _color, DWORD _specular, const CVec2 &_tex, const CVec2 &_tex1 )
	{
		pos = _pos;
		color = _color;
		specular = _specular;
		tex = _tex;
		tex1 = _tex1;
	}
	// this two setup functions use fake '_rhw' parameter for TL => non-TL vertices compatibility
	void Setup( float _x, float _y, float _z, float _rhw, DWORD _color, DWORD _specular, float _tu, float _tv, float _tu1, float _tv1 )
	{
		x = _x;
		y = _y;
		z = _z;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
		tu1 = _tu1;
		tv1 = _tv1;
	}
	void Setup( const CVec3 &_pos, float _rhw, DWORD _color, DWORD _specular, const CVec2 &_tex, const CVec2 &_tex1 )
	{
		pos = _pos;
		color = _color;
		specular = _specular;
		tex = _tex;
		tex1 = _tex1;
	}
};
struct SGFXTLVertex
{
	enum { format = GFXFVF_XYZRHW | GFXFVF_DIFFUSE  | GFXFVF_SPECULAR | GFXFVF_TEX1 };
	union
	{
		struct
		{
			float x, y, z, rhw;
			DWORD color, specular;
			float tu, tv;
		};
		struct
		{
			CVec4 pos;
			DWORD color, specular;
			CVec2 tex;
		};
	};
	//
	void Setup( float _sx, float _sy, float _sz, float _rhw, DWORD _color, DWORD _specular, float _tu, float _tv )
	{
		x = _sx;
		y = _sy;
		z = _sz;
		rhw = _rhw;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
	}
	void Setup( const CVec3 &_pos, float _rhw, DWORD _color, DWORD _specular, const CVec2 &_tex )
	{
		pos.Set( _pos.x, _pos.y, _pos.z, _rhw );
		color = _color;
		specular = _specular;
		tex = _tex;
	}
};
struct SGFXTLVertex2
{
	enum { format = GFXFVF_XYZRHW | GFXFVF_DIFFUSE  | GFXFVF_SPECULAR | GFXFVF_TEX2 };
	union
	{
		struct
		{
			float x, y, z, rhw;
			DWORD color, specular;
			float tu, tv;
			float tu1, tv1;
		};
		struct
		{
			CVec4 pos;
			DWORD color, specular;
			CVec2 tex;
			CVec2 tex1;
		};
	};
	//
	void Setup( float _x, float _y, float _z, float _rhw, DWORD _color, DWORD _specular, float _tu, float _tv, float _tu1, float _tv1 )
	{
		x = _x;
		y = _y;
		z = _z;
		rhw = _rhw;
		color = _color;
		specular = _specular;
		tu = _tu;
		tv = _tv;
		tu1 = _tu1;
		tv1 = _tv1;
	}
	void Setup( const CVec3 &_pos, float _rhw, DWORD _color, DWORD _specular, const CVec2 &_tex, const CVec2 &_tex1 )
	{
		pos.Set( _pos.x, _pos.y, _pos.z, _rhw );
		color = _color;
		specular = _specular;
		tex = _tex;
		tex1 = _tex1;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGFXTLPoint
{
	enum { format = GFXFVF_XYZRHW | GFXFVF_DIFFUSE };
	union
	{
		struct
		{
			float x, y, z, rhw;
			DWORD color;
		};
		struct
		{
			CVec4 pos;
			DWORD color;
		};
	};
	//
	void Setup( float _sx, float _sy, float _sz, float _rhw, DWORD _color )
	{
		x = _sx;
		y = _sy;
		z = _sz;
		rhw = _rhw;
		color = _color;
	}
	void Setup( const CVec4 &_pos, DWORD _color )
	{
		pos = _pos;
		color = _color;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGFXLineVertex
{
	enum { format = GFXFVF_XYZ | GFXFVF_DIFFUSE };
	union
	{
		struct
		{
			float x, y, z;
			DWORD color;
		};
		struct
		{
			CVec3 pos;
			DWORD color;
		};
	};
	//
	void Setup( float _x, float _y, float _z, DWORD _color )
	{
		x = _x;
		y = _y;
		z = _z;
		color = _color;
	}
	void Setup( const CVec3 &_pos, DWORD _color )
	{
		pos = _pos;
		color = _color;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** texture and surface lockers
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TColor>
class CTextureLock
{
	CPtr<IGFXTexture> pTexture;
	int nLevel;
	SSurfaceLockInfo lockinfo;
	std::vector<void*> rows;
public:
	CTextureLock( IGFXTexture *_pTexture, int _nLevel )
		: pTexture( _pTexture ), nLevel( _nLevel ), rows( _pTexture->GetSizeY( _nLevel ) )
	{
		if ( pTexture->Lock( nLevel, &lockinfo ) )
		{
			for ( int i=0; i<rows.size(); ++i )
				rows[i] = reinterpret_cast<void*>( DWORD(lockinfo.pData) + i*lockinfo.nPitch );
		}
		else
			rows.clear();
	}
	~CTextureLock()
	{
		if ( !rows.empty() )
			pTexture->Unlock( nLevel );
	}

	TColor* operator[]( int nRow ) { return reinterpret_cast<TColor*>( rows[nRow] ); }
	int GetSizeX() const { return pTexture->GetSizeX( nLevel ); }
	int GetSizeY() const { return rows.size(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// texture locker with easy and convinient data access
template <class TColor>
class CSurfaceLock
{
	CPtr<IGFXSurface> pSurface;
	SSurfaceLockInfo lockinfo;
	std::vector<void*> rows;
public:
	CSurfaceLock( IGFXSurface *_pSurface )
		: pSurface( _pSurface ), rows( _pSurface->GetSizeY() )
	{
		if ( pSurface->Lock( &lockinfo ) )
		{
			for ( int i=0; i<rows.size(); ++i )
				rows[i] = reinterpret_cast<void*>( DWORD(lockinfo.pData) + i*lockinfo.nPitch );
		}
		else
			rows.clear();
	}
	~CSurfaceLock()
	{
		if ( !rows.empty() )
			pSurface->Unlock();
	}

	TColor* operator[]( int nRow ) { return reinterpret_cast<TColor*>( rows[nRow] ); }
	int GetSizeX() const { return pSurface->GetSizeX(); }
	int GetSizeY() const { return rows.size(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** vertex/index buffer lockers
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TBuffer, class TData>
class CBaseGeometryLock
{
	CPtr<TBuffer> pBuffer;
	TData *pMemory;
public:
	CBaseGeometryLock( TBuffer *_pBuffer )
		: pBuffer( _pBuffer ), pMemory( 0 )
	{
		pMemory = reinterpret_cast<TData*>( pBuffer->Lock() );
	}
	~CBaseGeometryLock()
	{
		if ( pMemory )
			pBuffer->Unlock();
	}
	//
	TData* GetBuffer() const { return pMemory; }
	TData& operator[]( int nIndex ) { return pMemory[nIndex]; }
	const TData& operator[]( int nIndex ) const { return pMemory[nIndex]; }
};
template <class TVertex>
class CVerticesLock : public CBaseGeometryLock<IGFXVertices, TVertex>
{
public:
	CVerticesLock<TVertex>( IGFXVertices *pVertices )
		: CBaseGeometryLock<IGFXVertices, TVertex>( pVertices ) {  }
};
template <class TIndex>
class CIndicesLock : public CBaseGeometryLock<IGFXIndices, TIndex>
{
public:
	CIndicesLock<TIndex>( IGFXIndices *pIndices )
		: CBaseGeometryLock<IGFXIndices, TIndex>( pIndices ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TData>
class CTempBufferLock
{
	TData *pData;
public:
	CTempBufferLock( void *_pData )
		: pData( reinterpret_cast<TData*>(_pData) ) {  }
	//
	CTempBufferLock& operator=( void *_pData ) { pData = reinterpret_cast<TData*>( _pData ); return *this; }
	CTempBufferLock& operator=( const std::vector<TData> &data )
	{
		NI_ASSERT_T( !data.empty(), "Can't fill temp buffer from empty container" );
		memcpy( pData, &(data[0]), data.size()*sizeof(TData) );
		return *this;
	}
	CTempBufferLock& operator=( const std::list<TData> &data )
	{
		NI_ASSERT_T( !data.empty(), "Can't fill temp buffer from empty container" );
		TData *pTempData = pData;
		for ( std::list<TData>::const_iterator it = data.begin(); it != data.end(); ++it )
			*pTempData++ = *it;
		return *this;
	}
	//
	operator const TData*() const { return pData; }
	operator TData*() { return pData; }
	const TData* operator->() const { return pData; }
	TData* operator->() { return pData; }
	//
	CTempBufferLock& operator++() { ++pData; return *this; }
	CTempBufferLock& operator--() { --pData; return *this; }
	//
	TData* GetBuffer() const { return pData; }
	TData& operator[]( int nIndex ) { return pData[nIndex]; }
	const TData& operator[]( int nIndex ) const { return pData[nIndex]; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** gamma correction funtction
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// brightness, contrast and gamma are in range [-1..1]
inline bool SetGammaCorrectionBounded( const float fBrightness, const float fContrast, const float fGamma, IGFX *pGFX, bool bCalibrate, bool bStoreValues = true )
{
  // calculate equation params for Y = A*X + B
  // contrast: a*x + b
  // если contrast < 0, то a = 1/a (наклон <45 градусов)
  float fA = 1.0f + 4.0f*fabs( fContrast );
  if ( fContrast < 0 )
    fA = 1.0f / fA;
  float fB = 0.5f*( 1.0f - fA );
  // gamma: x^power
  float fPower = 1;
  {
    if ( fGamma > 0 )
      fPower = 1.0f / ( 5.0f*fGamma + 1 );
    else if ( fGamma < 0 )
      fPower = 1.0f / ( 0.5f*fGamma + 1 );
  }
  // brightness: x + b
  //
  SGFXGammaRamp ramp;
  for ( int i = 0; i < 256; ++i )
  {
    const float fVal = float( i ) / 255.0f;
    const float fGammaValue = pow( fVal, fPower );
    const float fContrastValue = Clamp( fA*fGammaValue + fB, 0.0f, 1.0f );
    const float fResult = Clamp( fContrastValue + fBrightness, 0.0f, 1.0f );
    ramp.red[i] = ramp.green[i] = ramp.blue[i] = fResult * 256 * 255;
  }
	//
	if ( bStoreValues ) 
		pGFX->SetGammaCorrectionValues( fBrightness, fContrast, fGamma );
	return pGFX->SetGammaRamp( ramp, bCalibrate );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool SetGammaCorrection( float fBrightness, float fContrast, float fGamma, IGFX *pGFX, bool bCalibrate )
{
  // build ramp from the brightness, contrast and gamma values
  // y = a*x + b
  //
  fBrightness = Clamp( fBrightness, -1.0f, 1.0f ) * 0.5f; // to avoid complete dark and complete white values
  fContrast = Clamp( fContrast, -1.0f, 1.0f ) * 0.5f;
  fGamma = Clamp( fGamma, -1.0f, 1.0f ) * 0.5f;
	//
	return SetGammaCorrectionBounded( fBrightness, fContrast, fGamma, pGFX, bCalibrate );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** temp draw helper
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
inline bool DrawTemp( interface IGFX *pGFX, const std::vector<TYPE> &vertices, const std::vector<WORD> &indices, 
										  EGFXPrimitiveType eGFXPT = GFXPT_TRIANGLELIST )
{
	if ( vertices.empty() || indices.empty() ) 
		return false;
	NI_ASSERT_SLOW_TF( vertices.size() < 65536, NStr::Format("Can't draw more then 65536 vertices, but %d sent to render", vertices.size()), return false );
	CTempBufferLock<TYPE> verts = pGFX->GetTempVertices( vertices.size(), TYPE::format, eGFXPT );
	verts = vertices;
	CTempBufferLock<WORD> inds = pGFX->GetTempIndices( indices.size(), GFXIF_INDEX16, eGFXPT );
	inds = indices;
	//
	return pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GFXHELPER_H__
