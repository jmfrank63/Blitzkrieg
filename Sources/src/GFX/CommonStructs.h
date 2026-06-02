#ifndef __COMMONSTRUCTS_H__
#define __COMMONSTRUCTS_H__
typedef std::pair<int, int> SRangeLimits;
struct SAdapterDesc
{
	std::string szName;                   // short name of this adapter
	std::string szDescription;            // long description
	GUID guid;                            // GUID for any purposes
	int nIndex;                           // internal DX index...
	std::list<D3DDISPLAYMODE> modes;      // available video modes (which is compatible with the 3D device)
	mutable std::vector<SGFXDisplayMode> extmodes;// modes for ewxternal enumeration
	D3DCAPS8 capsHWDevice;                // т.к. есть только один HW device дл€ адаптера, то он нас и интересует
	DWORD dwBehavior;
};
inline D3DFORMAT GFXPixelFormatToD3D( EGFXPixelFormat format )
{
	switch ( format )
	{
		case GFXPF_DXT1:			return D3DFMT_DXT1;
		case GFXPF_DXT2:			return D3DFMT_DXT2;
		case GFXPF_DXT3:			return D3DFMT_DXT3;
		case GFXPF_DXT4:			return D3DFMT_DXT4;
		case GFXPF_DXT5:			return D3DFMT_DXT5;
		case GFXPF_ARGB8888:	return D3DFMT_A8R8G8B8;
		case GFXPF_ARGB4444:	return D3DFMT_A4R4G4B4;
		case GFXPF_ARGB1555:	return D3DFMT_A1R5G5B5;
		case GFXPF_ARGB0565:	return D3DFMT_R5G6B5;
		case GFXPF_UV88:			return D3DFMT_V8U8;
		case GFXPF_LUV655:		return D3DFMT_L6V5U5;
		case GFXPF_DP3:				return D3DFMT_A8R8G8B8;
	}
	return D3DFMT_UNKNOWN;
}
inline EGFXPixelFormat D3DToGFXPixelFormat( D3DFORMAT format )
{
	switch ( format )
	{
		case D3DFMT_DXT1:			return GFXPF_DXT1;
		case D3DFMT_DXT2:			return GFXPF_DXT2;
		case D3DFMT_DXT3:			return GFXPF_DXT3;
		case D3DFMT_DXT4:			return GFXPF_DXT4;
		case D3DFMT_DXT5:			return GFXPF_DXT5;
		case D3DFMT_A8R8G8B8:	return GFXPF_ARGB8888;
		case D3DFMT_A4R4G4B4:	return GFXPF_ARGB4444;
		case D3DFMT_A1R5G5B5:	return GFXPF_ARGB1555;
		case D3DFMT_R5G6B5:		return GFXPF_ARGB0565;
		case D3DFMT_V8U8:			return GFXPF_UV88;
		case D3DFMT_L6V5U5:		return GFXPF_LUV655;
	}
	return GFXPF_UNKNOWN;
}
inline int GetBPP( D3DFORMAT format )
{
	switch ( format )
	{
		case D3DFMT_DXT1:			return 4;
		case D3DFMT_DXT2:			return 8;
		case D3DFMT_DXT3:			return 8;
		case D3DFMT_DXT4:			return 8;
		case D3DFMT_DXT5:			return 8;
		case D3DFMT_A8R8G8B8:	return 32;
		case D3DFMT_X8R8G8B8:	return 32;
		case D3DFMT_A4R4G4B4:	return 16;
		case D3DFMT_X4R4G4B4:	return 16;
		case D3DFMT_A1R5G5B5:	return 16;
		case D3DFMT_X1R5G5B5:	return 16;
		case D3DFMT_R5G6B5:		return 16;
		case D3DFMT_V8U8:			return 16;
		case D3DFMT_L6V5U5:		return 16;
	}
	return 0;
}
#endif // __COMMONSTRUCTS_H__