#ifndef __GFXTYPES_H__
#define __GFXTYPES_H__
#pragma ONCE
enum EGFXFullscreen
{
	GFXFS_FULLSCREEN  = 1,
	GFXFS_WINDOWED    = 2,
	GFXFS_FORCE_DWORD = 0x7fffffff
};
enum EGFXDynamic
{
	GFXD_DYNAMIC	= 1,
	GFXD_STATIC		= 2,
	GFXD_SYSMEM		= 3,

	GFXD_FORCE_DWORD = 0x7fffffff
};
enum EGFXPrimitiveType
{
	GFXPT_POINTLIST     = 1,
	GFXPT_LINELIST      = 2,
	GFXPT_LINESTRIP     = 3,
	GFXPT_TRIANGLELIST  = 4,
	GFXPT_TRIANGLESTRIP = 5,

	GFXPT_FORCE_DWORD   = 0x7fffffff
};
enum EGFXCull
{
	GFXC_NONE        = 1,
	GFXC_CW          = 2,
	GFXC_CCW         = 3,
	GFXC_FORCE_DWORD = 0x7fffffff
};
enum EGFXDepthBuffer
{
	GFXDB_NONE				= 0,
	GFXDB_USE_Z				= 1,
	GFXDB_USE_W				= 2,
	GFXDB_FORCE_DWORD = 0x7fffffff
};
enum EGFXCmpFunction
{
	GFXCMP_DEFAULT      = 0,							// set cmp function to default: GFXCMP_LESSEQUAL for z-buffer, GFXCMP_ALWAYS for alpha cmp and stencil
  GFXCMP_NEVER        = 1,							// always fail the test
  GFXCMP_LESS         = 2,							// accept the new pixel if its value is less than the value of the current pixel
  GFXCMP_EQUAL        = 3,							// accept the new pixel if its value equals the value of the current pixel
  GFXCMP_LESSEQUAL    = 4,							// accept the new pixel if its value is less than or equal to the value of the current pixel
  GFXCMP_GREATER      = 5,							// accept the new pixel if its value is greater than the value of the current pixel
  GFXCMP_NOTEQUAL     = 6,							// accept the new pixel if its value does not equal the value of the current pixel
  GFXCMP_GREATEREQUAL = 7,							// accept the new pixel if its value is greater than or equal to the value of the current pixel
  GFXCMP_ALWAYS       = 8,							// always pass the test
	GFXCMP_FORCE_DWORD  = 0x7fffffff
};
enum EGFXFVF
{
	GFXFVF_XYZ              = 0x002,
	GFXFVF_XYZRHW           = 0x004,
	GFXFVF_XYZB1            = 0x006,
	GFXFVF_XYZB2            = 0x008,
	GFXFVF_XYZB3            = 0x00a,
	GFXFVF_XYZB4            = 0x00c,
	GFXFVF_XYZB5            = 0x00e,
	GFXFVF_NORMAL           = 0x010,
	GFXFVF_PSIZE            = 0x020,
	GFXFVF_DIFFUSE          = 0x040,
	GFXFVF_SPECULAR         = 0x080,
	GFXFVF_TEX0             = 0x000,
	GFXFVF_TEX1             = 0x100,
	GFXFVF_TEX2             = 0x200,
	GFXFVF_TEX3             = 0x300,
	GFXFVF_TEX4             = 0x400,
	GFXFVF_TEX5             = 0x500,
	GFXFVF_TEX6             = 0x600,
	GFXFVF_TEX7             = 0x700,
	GFXFVF_TEX8             = 0x800,
	GFXFVF_LASTBETA_UBYTE4  = 0x1000,

	GFXFVF_FORCE_DWORD	= 0x7fffffff
};
enum EGFXIndexFormat
{
	GFXIF_INDEX16     = 101,
	GFXIF_INDEX32     = 102,
	GFXIF_FORCE_DWORD = 0x7fffffff
};
enum EGFXClear
{
	GFXCLEAR_TARGET  = 0x00000001l,		// Clear target surface
	GFXCLEAR_ZBUFFER = 0x00000002l,		// Clear target z buffer
	GFXCLEAR_STENCIL = 0x00000004l,		// Clear stencil planes
	GFXCLEAR_ALL = GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER | GFXCLEAR_STENCIL,

	GFXCLEAR_FORCE_DWORD = 0x7fffffff
};
enum EGFXFontFormat
{
	FNT_FORMAT_LEFT					= 0x00000001,	// format to left
	FNT_FORMAT_RIGHT				= 0x00000002,	// format to right
	FNT_FORMAT_CENTER				= 0x00000004,	// format to center
	FNT_FORMAT_JUSTIFY			= 0x00000008,	// format to width (justify)
	FNT_FORMAT_SINGLE_LINE	= 0x00010000,	// single line with clipping

	FNT_FORMAT_FORCE_DWORD = 0x7fffffff
};
enum EGFXPixelFormat
{
	GFXPF_UNKNOWN  = 0,
	GFXPF_DXT1     = 1,
	GFXPF_DXT2     = 2,
	GFXPF_DXT3     = 3,
	GFXPF_DXT4     = 4,
	GFXPF_DXT5     = 5,
	GFXPF_ARGB8888 = 6,
	GFXPF_ARGB4444 = 7,
	GFXPF_ARGB1555 = 8,
	GFXPF_ARGB0565 = 9,
	GFXPF_UV88     = 10,
	GFXPF_LUV655   = 11,
	GFXPF_DP3      = 12,
	GFXPF_FORCE_DWORD = 0x7fffffff
};
inline int GetBPP( EGFXPixelFormat format )
{
	switch ( format )
	{
		case GFXPF_DXT1:			return 4;
		case GFXPF_DXT2:			return 8;
		case GFXPF_DXT3:			return 8;
		case GFXPF_DXT4:			return 8;
		case GFXPF_DXT5:			return 8;
		case GFXPF_ARGB8888:	return 32;
		case GFXPF_ARGB4444:	return 16;
		case GFXPF_ARGB1555:	return 16;
		case GFXPF_ARGB0565:	return 16;
		case GFXPF_UV88:			return 16;
		case GFXPF_LUV655:		return 16;
		case GFXPF_DP3:				return 32;
	}
	return 0;
}
struct SSurfaceLockInfo
{
	int nPitch;
	void *pData;
};
struct SGFXRect2
{
	CTRect<float> rect;										// rect in screen space
	CTRect<float> maps;										// texture coords mapping
	DWORD color, specular;								// color components
	float fZ;															// depth value
	SGFXRect2() 
		: color( 0xffffffff ), specular( 0xff000000 ), fZ( 0.00001f ) {  }
	SGFXRect2( const SGFXRect2 &rect ) 
		: rect( rect.rect ), maps( rect.maps ), color( rect.color ), specular( rect.specular ), fZ( rect.fZ ) {  }
};
struct SGFXLightBase
{
	CVec4 vDiffuse;												// diffuse color emitted by the light
	CVec4 vSpecular;											// specular color emitted by the light
	CVec4 vAmbient;												// ambient color emitted by the light
};
struct SGFXLightDirectional : public SGFXLightBase
{
	CVec3 vDir;														// direction that the light is pointing in world space
};
struct SGFXLightPoint : public SGFXLightBase
{
	CVec3 vPos;														// position of the light in world space
	float fRange;													// distance beyond which the light has no effect
	float fAttenuation0;									// how the light intensity changes over distance
	float fAttenuation1;									// A = 1 / ( A0 + D*A1 + D*D*A2 )
	float fAttenuation2;									//
};
struct SGFXLightSpot : public SGFXLightPoint
{
	CVec3 vDir;														// direction that the light is pointing in world space
	float fFalloff;												// decrease in illumination between a spotlight's inner cone (the angle specified by 'fTheta') and the outer edge of the outer cone (the angle specified by 'fPhi')
	float fTheta;													// angle, in radians, of a spotlight's inner cone—that is, the fully illuminated spotlight cone
	float fPhi;														// angle, in radians, defining the outer edge of the spotlight's outer cone
};
struct SGFXMaterial
{
	CVec4 vDiffuse;												// diffuse color reflection properties
	CVec4 vAmbient;												// ambient color reflection properties
	CVec4 vSpecular;											// specular color reflection properties
	CVec4 vEmissive;											// color emission
	float fPower;													// sharpness of specular highlights
};
struct SGFXBoundSphere
{
	CVec3 vCenter;												// center of the bounding sphere
	float fRadius;												// radius of the bounding sphere
};
struct SGFXAABB
{
	CVec3 vCenter;												// center of the axis-aligned bounding box
	CVec3 vHalfSize;											// half size of the axis-aligned bounding box
};
enum EGFXClipPlanes
{
	GFXCP_LEFT		= 0,
	GFXCP_RIGHT		= 1,
	GFXCP_TOP			= 2,
	GFXCP_BOTTOM	= 3,
	GFXCP_NEAR		= 4,
	GFXCP_FAR			= 5,

	GFXCP_OUT     = 0xffffffff,

	GFXCP_FORCE_DWORD = 0x7fffffff
};
struct SGFXGammaRamp
{
	WORD red[256];
	WORD green[256];
	WORD blue[256];
};
struct SGFXDisplayMode
{
	int nWidth;														// display width (size x)
	int nHeight;													// display height (size y)
	int nBPP;															// display bits per pixel (32 or 16)
};
enum EGFXVideoCard
{
	GFXVC_DEFAULT,
	GFXVC_GEFORCE1,
	GFXVC_GEFORCE2,
	GFXVC_GEFORCE2MX,
	GFXVC_GEFORCE3,
	GFXVC_GEFORCE4,
	GFXVC_GEFORCE4MX,
	GFXVC_GEFORCEFX,
	GFXVC_RADEON7X00,
	GFXVC_RADEON9000,
	GFXVC_RADEON9100,
	GFXVC_RADEON9500,
	GFXVC_RADEON9700
};
#endif // __GFXTYPES_H__
