#include "StdAfx.h"

#include "GraphicsEngine.h"

#include "..\Image\Image.h"

#include "Texture.h"
#include "GeometryBuffer.h"
#include "GeometryMesh.h"
#include "Text.h"

#include "TextureManager.h"
#include "GeometryManager.h"

#include "GFXHelper.h"
#include "GFXTypes.h"

#include "VideoCheck.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CGraphicsEngine::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pCurrentFont );
	saver.Add( 2, &nCurrFrameNumber );
	saver.Add( 3, &bUseOptimizedBuffers );
	if ( saver.IsReading() ) 
		ClearStates();
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ** adapter enumeration and GFX creation...
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CD3DDisplayModeMatchFunctional
{
	const D3DDISPLAYMODE &mode;
public:
	explicit CD3DDisplayModeMatchFunctional( const D3DDISPLAYMODE &_mode )
		: mode( _mode ) {  }
	bool operator()( const D3DDISPLAYMODE &check )
	{
		return (mode.Width == check.Width) && (mode.Height == check.Height) && 
			     (mode.Format == check.Format) && (check.RefreshRate <= mode.RefreshRate);
	}
};
class CD3DDisplayModeFilterFunctional
{
	IDirect3D8 *pD3D;
	int nAdapter;
	D3DDEVTYPE devtype;
	bool bWindowed;
public:
	CD3DDisplayModeFilterFunctional( IDirect3D8 *_pD3D, int _nAdapter, D3DDEVTYPE _devtype, bool _bWindowed )
		: pD3D( _pD3D ), nAdapter( _nAdapter ), devtype( _devtype ), bWindowed( _bWindowed ) {  }
	bool operator()( const D3DDISPLAYMODE &check ) const
	{
		// Verifies whether or not a certain device type can be used on this adapter
		HRESULT hr1 = pD3D->CheckDeviceType( nAdapter, devtype, check.Format, check.Format, bWindowed );
		// Determines whether a surface format is available as a RENDER_TARGET
  	HRESULT hr2 = pD3D->CheckDeviceFormat( nAdapter, devtype, check.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, check.Format );
		return ( FAILED(hr1) || FAILED(hr2) );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enumerate adapters with HW caps and texture support
// enumerate video modes for them and check compatibility with D3D device
bool EnumAdapters( std::list<SAdapterDesc> *pAdapters )
{
	pAdapters->clear();
	// �������� ��������� D3D ��� ������������ ����������� ����������
	NWin32Helper::com_ptr<IDirect3D8> pD3D = Direct3DCreate8( D3D_SDK_VERSION );
	NI_ASSERT_TF( pD3D != 0, NStr::Format("Can't create Direct3D8 of build %d. Pls, install latest DX", D3D_SDK_VERSION), return false );
	pD3D->Release();
	//
	for ( int i = 0; i < pD3D->GetAdapterCount(); ++i )
	{
		// get adapter identifier
		D3DADAPTER_IDENTIFIER8 adapterInfo;
		HRESULT dxrval = pD3D->GetAdapterIdentifier( i, D3DENUM_NO_WHQL_LEVEL, &adapterInfo );
		if ( FAILED(dxrval) )
			continue;
		// get HW device caps for this adapter
		D3DCAPS8 capsDevice;
		Zero( capsDevice );
		dxrval = pD3D->GetDeviceCaps( i, D3DDEVTYPE_HAL, &capsDevice );
		// deny device if it is not support (at least) HW rasterization and textures
		if ( FAILED(dxrval) || (capsDevice.DeviceType != D3DDEVTYPE_HAL) ||
			   ((capsDevice.DevCaps & D3DDEVCAPS_HWRASTERIZATION) == 0) ||
			   ((capsDevice.DevCaps & (D3DDEVCAPS_TEXTURESYSTEMMEMORY |
				                         D3DDEVCAPS_TEXTUREVIDEOMEMORY |
																 D3DDEVCAPS_TEXTURENONLOCALVIDMEM ) ) == 0) )
			continue;
		// ������ �� �������, ��� ����� ���� device. ��������� �� ��� video modes
		std::list<D3DDISPLAYMODE> modes;
    DWORD dwNumAdapterModes = pD3D->GetAdapterModeCount( i );
		CD3DDisplayModeFilterFunctional dispfilter = CD3DDisplayModeFilterFunctional( pD3D, i, capsDevice.DeviceType, false );
    for( int nMode=0; nMode<dwNumAdapterModes; ++nMode )
    {
      // Get the display mode attributes
      D3DDISPLAYMODE displayMode;
      pD3D->EnumAdapterModes( i, nMode, &displayMode );
      // Filter out low-resolution modes
      if ( (displayMode.Width < 640) || (displayMode.Height < 400) )
        continue;
			// ����������� ������ �� video modes, ������� compatible with D3D device.
			if ( dispfilter(displayMode) ) 
				continue;
      // Check if the mode already exist (to filter out refresh rates)
			modes.remove_if( CD3DDisplayModeMatchFunctional(displayMode) );
			modes.push_back( displayMode );
    }
		//
		DWORD dwBehavior = 0;
		if ( capsDevice.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT )
			dwBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		else
			dwBehavior = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		//
		// ����� ����, ��� �� ������� ��� �������� � ������������, ������� ������� � ������
		//
		SAdapterDesc ad;
		ad.szName = adapterInfo.Driver;
		ad.szDescription = adapterInfo.Description;
		ad.guid = adapterInfo.DeviceIdentifier;
		ad.nIndex = i;
		ad.modes = modes;
		ad.capsHWDevice = capsDevice;
		ad.dwBehavior = dwBehavior;
		pAdapters->push_back( ad );
	}
	// destroy temporary D3D
	pD3D = 0;
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ����� �������� ������� �� �����...
const SAdapterDesc* FindAdapter( const char *pszName, const std::list<SAdapterDesc> &adapters )
{
	for ( std::list<SAdapterDesc>::const_iterator pos = adapters.begin(); pos != adapters.end(); ++pos )
	{
		if ( pos->szDescription == pszName )
			return &( *pos );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPSVSVersionSortFunctional
{
public:
	bool operator()( const SAdapterDesc &desc1, const SAdapterDesc &desc2 )
	{
		if ( desc1.capsHWDevice.VertexShaderVersion == desc2.capsHWDevice.VertexShaderVersion )
			return desc1.capsHWDevice.PixelShaderVersion < desc2.capsHWDevice.PixelShaderVersion;
		else
			return desc1.capsHWDevice.VertexShaderVersion < desc2.capsHWDevice.VertexShaderVersion;
	}
};
// ����� ��� ���������� ��������
// iterate all drivers in order to find one with best 3D caps.
// main criteria of the best 3D caps - existence of the HW T&L and pixel & vertex shaders
// if can't find best driver with 3D then return last one
std::string FindBestAdapter( const std::list<SAdapterDesc> &adapters )
{
	std::list<SAdapterDesc> candidates;
	for ( std::list<SAdapterDesc>::const_iterator pos = adapters.begin(); pos != adapters.end(); ++pos )
	{
		if ( pos->capsHWDevice.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT  )
			candidates.push_back( *pos );
	}
	if ( !candidates.empty() )
	{
		candidates.sort( CPSVSVersionSortFunctional() );
		return candidates.back().szDescription;
	}
	else if ( !adapters.empty() )
		return adapters.back().szDescription;
	else
		return "";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** main initialization functions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVideoCardType
{
	DWORD dwVendorID;
	DWORD dwDeviceID;
	DWORD dwDeviceIDMask;
	EGFXVideoCard eType;
};
#define VENDOR_NV 0x000010DE
#define VENDOR_ATI 0x00001002
static SVideoCardType videoCardsArray[] =
{
	{ VENDOR_NV, 0x0100, 0x0FF0, GFXVC_GEFORCE1 },
	{ VENDOR_NV, 0x0110, 0x0FF0, GFXVC_GEFORCE2MX },
	{ VENDOR_NV, 0x01A0, 0x0FF0, GFXVC_GEFORCE2MX }, // nForce
	{ VENDOR_NV, 0x0150, 0x0FF0, GFXVC_GEFORCE2 },
	{ VENDOR_NV, 0x0170, 0x0FF0, GFXVC_GEFORCE4MX },
	{ VENDOR_NV, 0x0200, 0x0FF0, GFXVC_GEFORCE3 },
	{ VENDOR_NV, 0x0250, 0x0FF0, GFXVC_GEFORCE4 },
	{ VENDOR_NV, 0x0280, 0x0FF0, GFXVC_GEFORCE4 }, //// AGP 8x
	{ VENDOR_NV, 0x0300, 0x0FF0, GFXVC_GEFORCEFX },
	/////
	{ VENDOR_ATI, 0x00005159, 0xFFFF, GFXVC_RADEON7X00 },
	{ VENDOR_ATI, 0x00005144, 0xFFFF, GFXVC_RADEON7X00 },
	{ VENDOR_ATI, 0x00005157, 0xFFFF, GFXVC_RADEON7X00 },
	{ VENDOR_ATI, 0x00004900, 0xFF00, GFXVC_RADEON9000 },
	{ VENDOR_ATI, 0x0000514C, 0xFFFF, GFXVC_RADEON9100 },
	{ VENDOR_ATI, 0x00004100, 0xFF00, GFXVC_RADEON9500 },
	{ VENDOR_ATI, 0x00004e00, 0xFF00, GFXVC_RADEON9700 }
};
EGFXVideoCard CGraphicsEngine::GetVideoCard()
{
	D3DADAPTER_IDENTIFIER8 sID;
	HRESULT dxrval;
	if ( pD3D ) 
		dxrval = pD3D->GetAdapterIdentifier( D3DADAPTER_DEFAULT, 0, &sID );
	else
	{
		NWin32Helper::com_ptr<IDirect3D8> pD3DTemp = Direct3DCreate8( D3D_SDK_VERSION );
		NI_ASSERT_TF( pD3DTemp != 0, NStr::Format("Can't create Direct3D8 of build %d. Pls, install latest DX", D3D_SDK_VERSION), return GFXVC_DEFAULT );
		pD3DTemp->Release();
		dxrval = pD3DTemp->GetAdapterIdentifier( D3DADAPTER_DEFAULT, 0, &sID );
	}
	if ( FAILED(dxrval) )
		return GFXVC_DEFAULT;

	for ( int i = 0; i < ARRAY_SIZE(videoCardsArray); ++i )
	{
		const SVideoCardType &sType = videoCardsArray[i];
		if ( (sID.VendorId == sType.dwVendorID) && ((sID.DeviceId & sType.dwDeviceIDMask) == (sType.dwDeviceID & sType.dwDeviceIDMask)) )
			return sType.eType;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetMode( int nSizeX, int nSizeY, int nBpp, int nStencilBPP, EGFXFullscreen eFullscreen, int nFreq )
{
	if ( (pD3D != 0) && (pD3DDevice == 0) ) 
	{
    HRESULT dxrval = pD3D->GetAdapterDisplayMode( adapter.nIndex, &desktopmode );
		if ( FAILED(dxrval) ) 
			Zero( desktopmode );
	}
	if ( !FillPresentationParams( nSizeX, nSizeY, nBpp, nStencilBPP, eFullscreen, nFreq ) )
		return false;
	SetRect( &rcScreen, 0, 0, displaymode.Width, displaymode.Height );
	ResetDevice();
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::FillPresentationParams( int nWidth, int nHeight, int nBPP, int nStencilBPP, EGFXFullscreen eFullscreen, int nFreq )
{
	// fill presentation parameters
	Zero( pp );
	pp.MultiSampleType = D3DMULTISAMPLE_NONE;
	pp.hDeviceWindow = hWindow;
	// now select and setup mode
  int nRenderSurfaceBPP = 0;
	if ( eFullscreen == GFXFS_WINDOWED )
	{
		pp.Windowed = true;
		pp.BackBufferCount = 1;
		pp.SwapEffect = D3DSWAPEFFECT_COPY;
		pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
		pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
		//
    HRESULT dxrval = pD3D->GetAdapterDisplayMode( adapter.nIndex, &displaymode );
		NI_ASSERTHR_TF( dxrval, "Can't get current display mode for windowed case", return false );
		//
		displaymode.Width = Min( int(displaymode.Width), nWidth );
		displaymode.Height = Min( int(displaymode.Height), nHeight );
		displaymode.RefreshRate = 0;
		//
		pp.BackBufferFormat = displaymode.Format;
		pp.BackBufferWidth = displaymode.Width;
		pp.BackBufferHeight = displaymode.Height;
    //
    switch ( displaymode.Format )
    {
			case D3DFMT_R5G6B5:
			case D3DFMT_X1R5G5B5:
			case D3DFMT_A1R5G5B5:
			case D3DFMT_A4R4G4B4:
			case D3DFMT_X4R4G4B4:
				nRenderSurfaceBPP = 16;
				break;
			case D3DFMT_R8G8B8:
				nRenderSurfaceBPP = 24;
				break;
			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
				nRenderSurfaceBPP = 32;
				break;
    }
	}
	else
	{
		pp.Windowed = false;
		pp.BackBufferCount = 1;
		pp.SwapEffect = D3DSWAPEFFECT_FLIP;
		pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;//D3DPRESENT_INTERVAL_IMMEDIATE;		
		pp.FullScreen_RefreshRateInHz = nFreq;//D3DPRESENT_RATE_DEFAULT; //D3DPRESENT_RATE_DEFAULT;  // D3DPRESENT_RATE_UNLIMITED
		//
    nRenderSurfaceBPP = nBPP;
		D3DFORMAT bpp16[3] = { D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5 };
		D3DFORMAT bpp24[1] = { D3DFMT_R8G8B8 };
		D3DFORMAT bpp32[2] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8 };
		int nNumFormats = 0;
		D3DFORMAT *bpp = 0;
		switch ( nBPP )
		{
			case 16:
				bpp = &( bpp16[0] ), nNumFormats = 3;
				break;
			case 24:
				bpp = &( bpp24[0] ), nNumFormats = 1;
				break;
			case 32:
				bpp = &( bpp32[0] ), nNumFormats = 2;
				break;
			default:
				NI_ASSERT_TF( 0, NStr::Format("Unknown bpp mode requested (%d)", nBPP), return false );
		}
		//
		displaymode.Width = nWidth;
		displaymode.Height = nHeight;
		displaymode.RefreshRate = 1000;
		bool bModeFound = false;
		for ( int i=0; i<nNumFormats; ++i )
		{
			displaymode.Format = bpp[i];
			std::list<D3DDISPLAYMODE>::const_iterator pos =
				std::find_if( adapter.modes.begin(), adapter.modes.end(), CD3DDisplayModeMatchFunctional(displaymode) );
			if ( pos != adapter.modes.end() )
			{
				displaymode.RefreshRate = pos->RefreshRate;
				bModeFound = true;
				break;
			}
		}
		//
		if ( bModeFound == false )
			return false;
		//
		if ( nFreq == 1 )
			pp.FullScreen_RefreshRateInHz = displaymode.RefreshRate;
		else if ( (nWidth <= desktopmode.Width) && (nHeight <= desktopmode.Height) && (desktopmode.RefreshRate <= displaymode.RefreshRate) ) 
		{
			// try to find allowed frequency
			const int nNumAdapterModes = pD3D->GetAdapterModeCount( adapter.nIndex );
			for( int i = 0; i < nNumAdapterModes; ++i )
			{
				// Get the display mode attributes
				D3DDISPLAYMODE mode;
				pD3D->EnumAdapterModes( adapter.nIndex, i, &mode );
				// Filter out low-resolution modes
				if ( (nWidth == mode.Width) && (nHeight == mode.Height) && (mode.RefreshRate == desktopmode.RefreshRate) ) 
				{
					pp.FullScreen_RefreshRateInHz = desktopmode.RefreshRate;
					break;
				}
			}
		}
		pp.BackBufferFormat = displaymode.Format;
		pp.BackBufferWidth = displaymode.Width;
		pp.BackBufferHeight = displaymode.Height;
	}
	// at rest we must select appropriate depth buffer format
	if ( nStencilBPP >= 0 ) 
	{
		pp.AutoDepthStencilFormat = D3DFORMAT( -1 );
		nStencilBPP <<= 1;
		do 
		{
			nStencilBPP >>= 1;
			FindDepthStencilFormat( nRenderSurfaceBPP, nStencilBPP );
		} while ( (pp.AutoDepthStencilFormat == D3DFORMAT(-1)) && (nStencilBPP != 0) );
		NI_ASSERT_TF( pp.AutoDepthStencilFormat != -1, NStr::Format("Can't find appropriate depth buffer format with stencil = %d", nStencilBPP), return false );
		this->nStencilBPP = 0;
		switch ( pp.AutoDepthStencilFormat )
		{
			case D3DFMT_D15S1:		this->nStencilBPP = 1;	break;
			case D3DFMT_D24S8:		this->nStencilBPP = 8;	break;
			case D3DFMT_D24X4S4:	this->nStencilBPP = 4;	break;
		}
	}
	else
	{
		pp.EnableAutoDepthStencil = false;
		pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
		this->nStencilBPP = -1;
	}
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::FindDepthStencilFormat( int nBPP, int nStencilBPP )
{
  // 1st index = render surface bit depth (16, 24, 32)
  // 2nd index = stencil bit depth ( 0, 1, 4, 8 )
	/*
  static const D3DFORMAT fmtDepthes[2][4][6] = 
  {
    { // 16:0, 16:1, 16:4, 16:8
      { D3DFMT_D16    , D3DFMT_D15S1  , D3DFMT_D32    , D3DFMT_D24X8  , D3DFMT_D24S8  , D3DFMT_D24X4S4 },
      { D3DFMT_D15S1  , D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN },
      { D3DFMT_D24X4S4, D3DFMT_D24S8  , D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN },
      { D3DFMT_D24S8  , D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN }
    },
    { // 32:0, 32:1, 32:4, 32:8
      { D3DFMT_D32    , D3DFMT_D24X8  , D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D16    , D3DFMT_D15S1   },
      { D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN },
      { D3DFMT_D24X4S4, D3DFMT_D24S8  , D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN },
      { D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_D24X8  , D3DFMT_D32    , D3DFMT_D16     }
    }
  };
	*/
  static const D3DFORMAT fmtDepthes[2][4][6] = 
  {
    { // 16:0, 16:1, 16:4, 16:8
      { D3DFMT_D15S1, D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32 },
      { D3DFMT_D15S1, D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32 },
      { D3DFMT_D15S1, D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32 },
      { D3DFMT_D15S1, D3DFMT_D24S8  , D3DFMT_D24X4S4, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32 },
    },
    { // 32:0, 32:1, 32:4, 32:8
      { D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_D24X8, D3DFMT_D32, D3DFMT_D16 },
      { D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_D24X8, D3DFMT_D32, D3DFMT_D16 },
      { D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_D24X8, D3DFMT_D32, D3DFMT_D16 },
      { D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D15S1  , D3DFMT_D24X8, D3DFMT_D32, D3DFMT_D16 },
    }
  };
	const D3DFORMAT *fmtDepth = 0;
	int nNumFormats = 6;
  {
    int nRTIndex = nBPP == 16 ? 0 : 1;
    int nSSIndex = nStencilBPP == 0 ? 0 : (nStencilBPP == 1 ? 1 : (nStencilBPP == 4 ? 2 : 3) );
    fmtDepth = &( fmtDepthes[nRTIndex][nSSIndex][0] );
  }
	//
	for ( int i=0; i<nNumFormats; ++i )
	{
  	HRESULT dxrval = pD3D->CheckDeviceFormat( adapter.nIndex, adapter.capsHWDevice.DeviceType, displaymode.Format,
			                                        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, fmtDepth[i] );
		if ( FAILED(dxrval) )
			continue;
		dxrval = pD3D->CheckDepthStencilMatch( adapter.nIndex, adapter.capsHWDevice.DeviceType, 
				                                   displaymode.Format, displaymode.Format, fmtDepth[i] );
		if ( SUCCEEDED(dxrval) )
		{
			pp.AutoDepthStencilFormat = fmtDepth[i];
			pp.EnableAutoDepthStencil = true;
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::Init( const char *pszAdapterName, HWND hWnd )
{
	std::string szAdapterName = pszAdapterName != 0 ? pszAdapterName : "";
	std::list<SAdapterDesc> adapters;
	if ( EnumAdapters( &adapters ) == false )
		return false;
	if ( szAdapterName.empty() )
		szAdapterName = FindBestAdapter( adapters );
	NI_ASSERT_TF( !szAdapterName.empty(), "Can't find any suitable adapter", return false );
	if ( szAdapterName.empty() ) 
		return false;
	const SAdapterDesc *pAdapter = FindAdapter( szAdapterName.c_str(), adapters );
	NI_ASSERT_TF( pAdapter != 0, "Can't find adapter by name", return false );
	adapter = *pAdapter;
	// assign window handle
	hWindow = hWnd;
	// create D3D
	pD3D.Create( Direct3DCreate8(D3D_SDK_VERSION) );
	NI_ASSERT_TF( pD3D != 0, NStr::Format("Can't create Direct3D8 of build %d. Pls, install latest DX", D3D_SDK_VERSION), return false );
	// 
	// CRAP{ for shaders testing
	SetupShaders();
	// CRAP}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::Done()
{
	pScreenDepth = 0;
	pScreenColor = 0;
	pD3DDevice = 0;
	pD3D = 0;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::Clear()
{
	ClearTempData();
	nCurrFrameNumber = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::ClearTempData()
{
	tempVBs.clear();
	tempIBs.clear();
	GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
	GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAL_UNREFERENCED );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::FreeVideoMemory( const int nUsage, const int nAmount, const bool bClearTempData )
{
	if ( bClearTempData ) 
		ClearTempData();
	GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAR_LRU, nUsage, nAmount );
	GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAR_LRU, nUsage, nAmount );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator==( const SGFXDisplayMode &m1, const SGFXDisplayMode &m2 )
{
	return (m1.nWidth == m2.nWidth) && (m1.nHeight == m2.nHeight) && (m1.nBPP == m2.nBPP);
}
inline bool operator<( const SGFXDisplayMode &m1, const SGFXDisplayMode &m2 )
{
	if ( m1.nWidth == m2.nWidth ) 
	{
		if ( m1.nHeight == m2.nHeight ) 
			return m1.nBPP < m2.nBPP;
		else
			return m1.nHeight < m2.nHeight;
	}
	else
		return m1.nWidth < m2.nWidth;
}
const SGFXDisplayMode* CGraphicsEngine::GetDisplayModes() const
{
	const int nMaxModeSizeX = GetGlobalVar( "GFX.Limit.Mode.SizeX", 1000000 );
	const int nMaxModeSizeY = GetGlobalVar( "GFX.Limit.Mode.SizeY", 1000000 );
	const int nMaxModeBPP = GetGlobalVar( "GFX.Limit.Mode.BPP", 32 );
	//
	adapter.extmodes.clear();
	adapter.extmodes.reserve( adapter.modes.size() );
	for ( std::list<D3DDISPLAYMODE>::const_iterator it = adapter.modes.begin(); it != adapter.modes.end(); ++it )
	{
		if ( (it->Width >= 640) && (it->Height >= 480) && (it->Width <= nMaxModeSizeX) && (it->Height <= nMaxModeSizeY) ) 
		{
			SGFXDisplayMode enumode;
			enumode.nWidth = it->Width;
			enumode.nHeight = it->Height;
			if ( (it->Format == D3DFMT_A8R8G8B8) || (it->Format == D3DFMT_X8R8G8B8) ) 
				enumode.nBPP = 32;
			else if ( (it->Format == D3DFMT_R5G6B5) || (it->Format == D3DFMT_X1R5G5B5) || (it->Format == D3DFMT_A1R5G5B5) || (it->Format == D3DFMT_A4R4G4B4) )
				enumode.nBPP = 16;
			//
			if ( (enumode.nBPP <= nMaxModeBPP) /*&& (float(it->Height)/float(it->Width) == 3.0f/4.0f)*/ ) 
			{
				std::remove( adapter.extmodes.begin(), adapter.extmodes.end(), enumode );
				adapter.extmodes.push_back( enumode );
			}
		}
	}
	SGFXDisplayMode enumode;
	std::sort( adapter.extmodes.begin(), adapter.extmodes.end() );
	enumode.nWidth = enumode.nHeight = enumode.nBPP = 0;
	adapter.extmodes.push_back( enumode );
	return adapter.extmodes.empty() ? 0 : &( adapter.extmodes[0] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::DestroyAllObjects()
{
	pSVB = 0;
	pSIB = 0;
	// temp buffers
	tempVBs.clear();
	pTVB = 0;
	tempIBs.clear();
	pTIB = 0;
	// dynamic buffers
	dynVBs.clear();
	dynIBs.clear();
	// last formats for flushing
	dwLastTempBufferFormat = -1;
	dwLastVertexShader = -1;
	//
	if ( pD3DDevice )
	{
		pD3DDevice->SetIndices( 0, 0 );
		pD3DDevice->SetStreamSource( 0, 0, 4 );
		for ( int i = 0; i < adapter.capsHWDevice.MaxSimultaneousTextures; ++i )
			pD3DDevice->SetTexture( i, 0 );
	}
	//
	static_cast<CTextureManager*>( GetSingleton<ITextureManager>() )->ClearContainers();
	static_cast<CMeshManager*>( GetSingleton<IMeshManager>() )->ClearContainers();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::ReCreateAllObjects()
{
	static_cast<CTextureManager*>( GetSingleton<ITextureManager>() )->ReloadAllData();
	static_cast<CMeshManager*>( GetSingleton<IMeshManager>() )->ReloadAllData();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SNameFormat
{
	const char *pszName;
	D3DFORMAT format;
};
static const SNameFormat formats[] = 
{
	{ "ARGB8888", D3DFMT_A8R8G8B8 },
	{ "ARGB0565", D3DFMT_R5G6B5		},
	{ "ARGB1555", D3DFMT_A1R5G5B5 },
	{ "ARGB4444", D3DFMT_A4R4G4B4 },
	{ 0					, D3DFMT_UNKNOWN	}
};
static const SNameFormat formatsDXT[] = 
{
	{ "DXT1", D3DFMT_DXT1 },
	{ "DXT2", D3DFMT_DXT2 },
	{ "DXT3", D3DFMT_DXT3 },
	{ "DXT4", D3DFMT_DXT4 },
	{ "DXT5", D3DFMT_DXT5 },
	{ 0			, D3DFMT_UNKNOWN	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::ResetDevice()
{
	pScreenColor = 0;
	pScreenDepth = 0;
	//
	if ( (pD3D != 0) && (pD3DDevice == 0) )
	{
		// create new device
		HRESULT dxrval = pD3D->CreateDevice( adapter.nIndex, adapter.capsHWDevice.DeviceType, hWindow,
																 	       adapter.dwBehavior, &pp, pD3DDevice.GetAddr() );
		NI_ASSERTHR_TF( dxrval, "Can't create D3D device", return false );
		// 
		{
			// setup global vars with caps
			const D3DCAPS8 &caps = adapter.capsHWDevice;
			SetGlobalVar( "GFX.Caps.Gamma.Calibrate", int((caps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA) != 0) );
			SetGlobalVar( "GFX.Caps.Gamma.Fullscreen", int((caps.Caps2 & D3DCAPS2_FULLSCREENGAMMA) != 0) );
			SetGlobalVar( "GFX.Caps.Texture.NonPow2Conditional", int((caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0) );
			SetGlobalVar( "GFX.Caps.Texture.NonPow2", int((caps.TextureCaps & D3DPTEXTURECAPS_POW2) == 0) );
			SetGlobalVar( "GFX.Caps.Texture.SquareOnly", int((caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0) );
			// setup global vars with texture formats
			// check DXT# formats
			bool bHasDXT = true;
			const SNameFormat *pFormat = formatsDXT;
			while ( pFormat->pszName != 0 )
			{
				HRESULT dxrval = pD3D->CheckDeviceFormat( adapter.nIndex, D3DDEVTYPE_HAL, pp.BackBufferFormat, 
																									0, D3DRTYPE_TEXTURE, pFormat->format );
				bHasDXT = bHasDXT && SUCCEEDED( dxrval );
				++pFormat;
			}
			SetGlobalVar( "GFX.Caps.Texture.Format.DXT", int(bHasDXT) );
			// check other formats
			pFormat = formats;
			while ( pFormat->pszName != 0 )
			{
				dxrval = pD3D->CheckDeviceFormat( adapter.nIndex, D3DDEVTYPE_HAL, pp.BackBufferFormat, 
																					0, D3DRTYPE_TEXTURE, pFormat->format );
				const std::string szCapsName = NStr::Format( "GFX.Caps.Texture.Format.%s", pFormat->pszName );
				SetGlobalVar( szCapsName.c_str(), int( SUCCEEDED(dxrval) != 0 ) );
				++pFormat;
			}
		}
		// initial setup
		ChangeViewport( displaymode.Width, displaymode.Height );
		// CRAP{ this must be setted up by the shader
		for ( int i=0; i<adapter.capsHWDevice.MaxSimultaneousTextures; ++i )
		{
			SetTextureStageState( i, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( i, D3DTSS_MINFILTER, D3DTEXF_POINT );
		}
		SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		SetRenderState( D3DRS_LIGHTING, false );
		//SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		// CRAP}
	}
	else
	{
		// reset old device
		DestroyAllObjects();
		HRESULT dxrval = pD3DDevice->Reset( &pp );
		if ( FAILED(dxrval) ) 
			return false;
//		NI_ASSERTHR_TF( dxrval, "Can't reset D3D device", return false );
		// initial setup
		ChangeViewport( displaymode.Width, displaymode.Height );
		// CRAP{ this must be setted up by the shader
		for ( int i=0; i<adapter.capsHWDevice.MaxSimultaneousTextures; ++i )
		{
			SetTextureStageState( i, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( i, D3DTSS_MINFILTER, D3DTEXF_POINT );
		}
		SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		SetRenderState( D3DRS_LIGHTING, false );
		//SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		// CRAP}
		ReCreateAllObjects();
		// re-setup all transform matrices
		SHMatrix matrix = matProjection;
		SetProjectionTransform( matrix );
		matrix = matView;
		SetViewTransform( matrix );
		SetWorldTransforms( 0, &MONE, 1 );
	}
	// retrieve screen color and depth surfaces
	{
		// retrieve screen color surface
		HRESULT dxrval = pD3DDevice->GetRenderTarget( pScreenColor.GetAddr() );
		NI_ASSERTHR_T( dxrval, "Can't get screen render target" );
		// retrieve screen depth surface
		dxrval = pD3DDevice->GetDepthStencilSurface( pScreenDepth.GetAddr() );
		if ( FAILED(dxrval) ) 
			pScreenDepth = 0;
	}
	//
	// set window position
	SetWindowPos( hWindow, HWND_NOTOPMOST, 0, 0, pp.BackBufferWidth, pp.BackBufferHeight, SWP_SHOWWINDOW );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::MoveTo( int nX, int nY )
{
	int nSizeX = rcScreen.right - rcScreen.left;
	int nSizeY = rcScreen.bottom - rcScreen.top;
	SetRect( &rcScreen, nX, nY, nX + nSizeX, nY + nSizeY );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetRenderTarget( IGFXRTexture *_pRT )
{
	if ( pCurrRT == _pRT ) 
		return true;
	//
	if ( _pRT != 0 ) 
	{
		// reset all textures in order to avoid render-target-as-texture simultaneous usage
		for ( int i = 0; i < 8; ++i )
			SetTexture( i, 0 );
		CRenderTargetTexture *pRT = checked_cast<CRenderTargetTexture*>( _pRT );
		HRESULT dxrval = pD3DDevice->SetRenderTarget( pRT->GetColorSurface(), pRT->GetDepthSurface() );
		NI_ASSERTHR_T( dxrval, "Can't set texture as render target" );
	}
	else if ( pScreenColor != 0 ) 
	{
		HRESULT dxrval = pD3DDevice->SetRenderTarget( pScreenColor, pScreenDepth );
		NI_ASSERTHR_T( dxrval, "Can't set original render target" );
	}
	//
	pCurrRT = _pRT;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// **                                           T&L setup and helper functions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetupViewport( const D3DVIEWPORT8 &viewport )
{
	Zero( matViewport );
	matViewport._11 = viewport.Width / 2.0f;
	matViewport._14 = viewport.X + viewport.Width / 2.0f;
	matViewport._22 = -(viewport.Height / 2.0f);
	matViewport._24 = viewport.Y + viewport.Height / 2.0f;
	matViewport._33 = viewport.MaxZ - viewport.MinZ;
	matViewport._34 = viewport.MinZ;
	matViewport._44 = 1.0f;
	//
	//
	HRESULT dxrval = pD3DDevice->SetViewport( &viewport );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set viewport (%d:%d) : (%d:%d) : (%g:%g)", viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinZ, viewport.MaxZ), return false );
	UpdatePickMatrix();
	return true;
}
void CGraphicsEngine::PushViewport()
{
	viewports.push_back( currviewport );
}
bool CGraphicsEngine::PopViewport()
{
	if ( viewports.empty() )
		return false;
	currviewport = viewports.back();
	viewports.pop_back();
	return SetupViewport( currviewport );
}
bool CGraphicsEngine::ChangeViewport( int nX, int nY, int nWidth, int nHeight, float fMinZ, float fMaxZ )
{
  currviewport.X = nX;
  currviewport.Y = nY;
  currviewport.Width  = nWidth;
  currviewport.Height = nHeight;
  currviewport.MinZ = fMinZ;
  currviewport.MaxZ = fMaxZ;
	return SetupViewport( currviewport );
}
bool CGraphicsEngine::ChangeViewport( int nWidth, int nHeight )
{
	return ChangeViewport( 0, 0, nWidth, nHeight, 0.0f, 1.0f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NTransformTypes
{
	D3DTRANSFORMSTATETYPE tstTexture[] = { D3DTS_TEXTURE0, D3DTS_TEXTURE1,
		                                     D3DTS_TEXTURE2, D3DTS_TEXTURE3,
																			   D3DTS_TEXTURE4, D3DTS_TEXTURE5,
																			   D3DTS_TEXTURE6, D3DTS_TEXTURE7 };

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetWorldTransforms( const int nStartIndex, const SHMatrix *pMatrices, const int nNumMatrices )
{
	NI_ASSERT_SLOW_TF( (nStartIndex >= 0) && (nStartIndex + nNumMatrices - 1 <= 255), NStr::Format("Can't set world transform matrix - invalid index range (%d : %d).", nStartIndex, nNumMatrices), return false );
	SHMatrix matrix;
	for ( int nIndex=0; nIndex<nNumMatrices; ++nIndex )
	{
		// setup world transform
		Transpose( &matrix, pMatrices[nIndex] );
		HRESULT dxrval = pD3DDevice->SetTransform( D3DTS_WORLDMATRIX(nStartIndex + nIndex), reinterpret_cast<D3DMATRIX*>( &matrix ) );
		NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set world%d transform matrix.", nStartIndex + nIndex), return false );
	}
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetViewTransform( const SHMatrix &matView1 )
{
	NI_ASSERT_SLOW_TF( !bDirectTransform, "Can't set view transform during direct rendering mode", return false );
	if ( bDirectTransform ) 
		return false;
	matView = matView1;
	// prepare billboarding matrix
	Invert( &matInvView, matView );
	matBillboard = matInvView;//.HomogeneousInverse( matView );
	matBillboard._14 = matBillboard._24 = matBillboard._34 = 0;
	// transpose matrix before setup, as required by DX
	SHMatrix matrix;
	Transpose( &matrix, matView );
	// setup view transform
	HRESULT dxrval = pD3DDevice->SetTransform( D3DTS_VIEW, reinterpret_cast<D3DMATRIX*>( &matrix ) );
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't set view transform matrix.", return false );
	//
	UpdatePickMatrix();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetTextureTransform( int nIndex, const SHMatrix &matTexture )
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex <= 7), NStr::Format("Can't set texture transform matrix - invalid index (%d).", nIndex), return false );
	// setup texture transform
	SHMatrix matrix;
	Transpose( &matrix, matTexture );
	HRESULT dxrval = pD3DDevice->SetTransform( NTransformTypes::tstTexture[nIndex], reinterpret_cast<D3DMATRIX*>( &matrix ) );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set texture%d transform matrix.", nIndex), return false );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetProjectionTransform( const SHMatrix &matProj )
{
	matProjection = matProj;
	SHMatrix matrix;
	Transpose( &matrix, matProjection );
	HRESULT dxrval = pD3DDevice->SetTransform( D3DTS_PROJECTION, reinterpret_cast<D3DMATRIX*>( &matrix ) );
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't set projection transform matrix.", return false );
	UpdatePickMatrix();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetupDirectTransform()
{
	if ( bDirectTransform ) 
		return true;
	const SHMatrix &matProjection = GetProjectionMatrix();
	const CTRect<float> rcScreen = GetScreenRect();
	const float fWidth = Max( rcScreen.Width(), 1.0f );
	const float fHeight = Max( rcScreen.Height(), 1.0f );
	SHMatrix matrix;
	Zero( matrix );
	matrix._11 = 1;
	matrix._14 = -fWidth / 2;
	matrix._22 = -1;
	matrix._24 = fHeight / 2;
	matrix._33 = 1.0f / matProjection._33;//-( far_plane - near_plane );
	matrix._34 = -matProjection._34 / matProjection._33;//-near_plane;
	matrix._44 = 1;

	matViewDirectStored = matView;
	matInvViewDirectStored = matInvView;
	matBillboardDirectStored = matBillboard;
	SetViewTransform( matrix );
	SetWorldTransforms( 0, &MONE, 1 );
	bDirectTransform = true;
	SetRenderState( D3DRS_LIGHTING, false );

	return true;


	/*
	//
	const SHMatrix &matProjection = GetProjectionMatrix();
	const CTRect<float> rcScreen = GetScreenRect();
	const float fWidth = Max( rcScreen.Width(), 1.0f );
	const float fHeight = Max( rcScreen.Height(), 1.0f );
	SHMatrix matrix;
	Zero( matrix );
	matrix._11 = 1;
	matrix._14 = -fWidth / 2;
	matrix._22 = -1;
	matrix._24 = fHeight / 2;
	matrix._33 = 1.0f / matProjection._33;//-( far_plane - near_plane );
	matrix._34 = -matProjection._34 / matProjection._33;//-near_plane;
	matrix._44 = 1;
	//
	SHMatrix matWorld;
	Multiply( &matWorld, GetInverseViewMatrix(), matrix );

	CMatrixStack<8> mstack;
	mstack.Push( matViewport );
	mstack.Push( matProjection );
	mstack.Push( matrix );
	//mstack.Push( matWorld );
	return SetWorldTransforms( 0, &matWorld, 1 );
	*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::RestoreTransform()
{
	bDirectTransform = false;
	return SetViewTransform( matViewDirectStored );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetupLight( int nIndex, const D3DLIGHT8 &light )
{
	HRESULT dxrval = pD3DDevice->SetLight( nIndex, &light );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set light %d.", nIndex), return false );
	return true;
}
inline void Assign( D3DCOLORVALUE *pColor, const CVec4 &color ) { pColor->a = color.a; pColor->r = color.r; pColor->g = color.g; pColor->b = color.b; }
inline void Assign( D3DVECTOR *pVec, const CVec3 &vec ) { pVec->x = vec.x; pVec->y = vec.y; pVec->z = vec.z; }
void CGraphicsEngine::SetLight( int nIndex, const SGFXLightDirectional &light )
{
	D3DLIGHT8 lt;
	Zero( lt );
	lt.Type = D3DLIGHT_DIRECTIONAL;
	Assign( &lt.Ambient, light.vAmbient );
	Assign( &lt.Diffuse, light.vDiffuse );
	Assign( &lt.Specular, light.vSpecular );
	Assign( &lt.Direction, light.vDir );
	SetupLight( nIndex, lt );
}
void CGraphicsEngine::SetLight( int nIndex, const SGFXLightPoint &light )
{
	D3DLIGHT8 lt;
	Zero( lt );
	lt.Type = D3DLIGHT_POINT;
	Assign( &lt.Ambient, light.vAmbient );
	Assign( &lt.Diffuse, light.vDiffuse );
	Assign( &lt.Specular, light.vSpecular );
	Assign( &lt.Position, light.vPos );
	lt.Range = light.fRange;
	lt.Attenuation0 = light.fAttenuation0;
	lt.Attenuation1 = light.fAttenuation1;
	lt.Attenuation2 = light.fAttenuation2;
	SetupLight( nIndex, lt );
}
void CGraphicsEngine::SetLight( int nIndex, const SGFXLightSpot &light )
{
	D3DLIGHT8 lt;
	Zero( lt );
	lt.Type = D3DLIGHT_SPOT;
	Assign( &lt.Ambient, light.vAmbient );
	Assign( &lt.Diffuse, light.vDiffuse );
	Assign( &lt.Specular, light.vSpecular );
	Assign( &lt.Position, light.vPos );
	Assign( &lt.Direction, light.vDir );
	lt.Range = light.fRange;
	lt.Attenuation0 = light.fAttenuation0;
	lt.Attenuation1 = light.fAttenuation1;
	lt.Attenuation2 = light.fAttenuation2;
	lt.Falloff = light.fFalloff;
	lt.Theta = light.fTheta;
	lt.Phi = light.fPhi;
	SetupLight( nIndex, lt );
}
void CGraphicsEngine::EnableLight( int nIndex, bool bEnable )
{
	HRESULT dxrval = pD3DDevice->LightEnable( nIndex, bEnable );
	NI_ASSERTHR_SLOW_T( dxrval, NStr::Format("Can't %s light %d.", bEnable ? "enable" : "disable", nIndex) );
}
void CGraphicsEngine::SetMaterial( const SGFXMaterial &material )
{
	D3DMATERIAL8 mat;
	Zero( mat );
	Assign( &mat.Ambient, material.vAmbient );
	Assign( &mat.Diffuse, material.vDiffuse );
	Assign( &mat.Specular, material.vSpecular );
	Assign( &mat.Emissive, material.vEmissive );
	mat.Power = material.fPower;
	HRESULT dxrval = pD3DDevice->SetMaterial( &mat );
	NI_ASSERTHR_SLOW_T( dxrval, "Can't set material properties." );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetTexture( int nStage, IGFXBaseTexture *pTexture )
{
	// check, if this texture already set&
	if ( nStage >= usedtextures.size() )
		usedtextures.resize( nStage + 1, (IGFXBaseTexture*)(-1) );
	if ( usedtextures[nStage] == pTexture )
		return true;
	// set this texture
	usedtextures[nStage] = pTexture;
	IDirect3DBaseTexture8 *pD3DTexture = 0;
	if ( pTexture )
	{
		if ( CTexture *pTex = dynamic_cast<CTexture*>(pTexture) ) 
		{
			pTex->SetSharedResourceLastUsage( nCurrFrameNumber );
			pD3DTexture = pTex->GetInternalContainer();
		}
		else if ( CRenderTargetTexture *pTex = dynamic_cast<CRenderTargetTexture*>(pTexture) ) 
			pD3DTexture = pTex->GetInternalContainer();
	}
	HRESULT dxrval = pD3DDevice->SetTexture( nStage, pD3DTexture );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set texture for stage %d", nStage), return false );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::SetRenderState( D3DRENDERSTATETYPE state, int nValue )
{
	//sctRS.SetState( state, nValue );
	pD3DDevice->SetRenderState( state, nValue );
	/*
	HRESULT dxrval = pD3DDevice->SetRenderState( state, nValue );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set render state %d:%d", state, nValue), return false );
	return true;
	*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::SetTextureStageState( DWORD stage, D3DTEXTURESTAGESTATETYPE type, int value )
{
	//sctTSS[stage].SetState( type, value );
	pD3DDevice->SetTextureStageState( stage, type, value );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::ApplyRenderStates()
{
	for ( CStateChangesTracker::iterator it = sctRS.begin(); it != sctRS.end(); ++it )
	{
		pD3DDevice->SetRenderState( D3DRENDERSTATETYPE( (*it)->type ), (*it)->dwNeedValue );
		(*it)->dwCurrValue = (*it)->dwNeedValue;
	}
	sctRS.clear();
}
void CGraphicsEngine::ApplyTextureStageStates()
{
	for ( int i = 0; i < 8; ++i )
	{
		for ( CStateChangesTracker::iterator it = sctTSS[i].begin(); it != sctTSS[i].end(); ++it )
		{
			pD3DDevice->SetTextureStageState( i, D3DTEXTURESTAGESTATETYPE( (*it)->type ), (*it)->dwNeedValue );
			(*it)->dwCurrValue = (*it)->dwNeedValue;
		}
		sctTSS[i].clear();
	}
}
void CGraphicsEngine::ClearStates() 
{ 
	ApplyStates();
	sctRS.ClearStates(); 
	for ( int i = 0; i < 8; ++i )
		sctTSS[i].ClearStates(); 
	usedtextures.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetWireframe( bool bWireframe )
{
  SetRenderState( D3DRS_FILLMODE, bWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
	return true;
}
bool CGraphicsEngine::EnableLighting( bool bLighting )
{
  SetRenderState( D3DRS_LIGHTING, bLighting );
	return true;
}
bool CGraphicsEngine::EnableSpecular( bool bEnable )
{
  SetRenderState( D3DRS_SPECULARENABLE, bEnable );
	return true;
}
bool CGraphicsEngine::SetCullMode( EGFXCull cull )
{
	SetRenderState( D3DRS_CULLMODE, cull );
	return true;
}
bool CGraphicsEngine::SetDepthBufferMode( EGFXDepthBuffer depth, EGFXCmpFunction cmp )
{
	cmp = (cmp == GFXCMP_DEFAULT) ? GFXCMP_LESSEQUAL : cmp;
	SetRenderState( D3DRS_ZFUNC, cmp );
	switch ( depth )
	{
		case GFXDB_NONE:
			SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
			return true;
		case GFXDB_USE_Z:
			SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
			return true;
		case GFXDB_USE_W:
			SetRenderState( D3DRS_ZENABLE, D3DZB_USEW );
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetFont( IGFXFont *pFont )
{
	pCurrentFont = static_cast<CFont*>( pFont );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CGraphicsEngine::GetViewMatrix() const 
{ 
	return bDirectTransform ? matViewDirectStored : matView; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CGraphicsEngine::GetInverseViewMatrix() const 
{ 
	return bDirectTransform ? matInvViewDirectStored : matInvView; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CGraphicsEngine::GetBillboardMatrix() const 
{ 
	return bDirectTransform ? matBillboardDirectStored : matBillboard; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CGraphicsEngine::GetProjectionMatrix() const 
{ 
	return matProjection; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CGraphicsEngine::GetViewportMatrix() const 
{ 
	return matViewport; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void ScaleClipVertex( SPlane *pRes, const CVec4 &vClipPlane, const SHMatrix &matrix )
{
	CVec4 len;
	matrix.RotateHVector( &len, vClipPlane );
	pRes->Set( len.x, len.y, len.z, len.w, true );
}
void CGraphicsEngine::GetViewVolume( SPlane *pPlanes ) const
{
	SHMatrix matrix;
	Multiply( &matrix, GetProjectionMatrix(), GetViewMatrix() );
	// 
	Transpose( &matrix );

	ScaleClipVertex( pPlanes + 0, CVec4( 1, 0, 0, 1), matrix );
	ScaleClipVertex( pPlanes + 1, CVec4(-1, 0, 0, 1), matrix );
	ScaleClipVertex( pPlanes + 2, CVec4( 0, 1, 0, 1), matrix );
	ScaleClipVertex( pPlanes + 3, CVec4( 0,-1, 0, 1), matrix );
	ScaleClipVertex( pPlanes + 4, CVec4( 0, 0, 1, 1), matrix );
	ScaleClipVertex( pPlanes + 5, CVec4( 0, 0,-1, 1), matrix );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::UpdatePickMatrix()
{
	static CMatrixStack<4> mstack;
	mstack.Clear();
	mstack.Push( GetViewportMatrix() );
	mstack.Push( GetProjectionMatrix() );
	mstack.Push( GetViewMatrix() );
	const SHMatrix &m = mstack();
	// x
	matPick._11 = -(-m._33*m._44*m._22 - m._43*m._32*m._24 + m._42*m._24*m._33 + m._23*m._44*m._32 - m._34*m._42*m._23 + m._34*m._43*m._22 );
	matPick._12 = -( m._43*m._14*m._32 - m._42*m._33*m._14 - m._44*m._13*m._32 - m._34*m._12*m._43 + m._34*m._13*m._42 + m._33*m._12*m._44 );
	matPick._13 = -( m._23*m._14*m._42 - m._43*m._14*m._22 - m._44*m._12*m._23 + m._43*m._12*m._24 - m._24*m._13*m._42 + m._44*m._13*m._22 );
	matPick._14 = -( m._34*m._12*m._23 + m._33*m._14*m._22 + m._24*m._13*m._32 - m._34*m._13*m._22 - m._23*m._14*m._32 - m._33*m._12*m._24 );
	// y
	matPick._21 =  (-m._24*m._31*m._43 - m._21*m._33*m._44 + m._21*m._34*m._43 - m._34*m._41*m._23 + m._41*m._33*m._24 + m._23*m._31*m._44 );
	matPick._22 =  ( m._43*m._31*m._14 + m._33*m._11*m._44 - m._44*m._31*m._13 - m._41*m._33*m._14 - m._34*m._11*m._43 + m._41*m._34*m._13 );
	matPick._23 =  ( m._43*m._11*m._24 - m._21*m._43*m._14 - m._24*m._41*m._13 + m._21*m._44*m._13 + m._23*m._41*m._14 - m._44*m._11*m._23 );
	matPick._24 =  ( m._21*m._33*m._14 + m._34*m._11*m._23 - m._33*m._11*m._24 - m._21*m._34*m._13 + m._24*m._31*m._13 - m._23*m._31*m._14 );
	// z
	matPick._31 = -( m._42*m._21*m._34 - m._41*m._34*m._22 - m._44*m._21*m._32 + m._41*m._32*m._24 - m._31*m._42*m._24 + m._31*m._44*m._22 );
	matPick._32 = -( m._11*m._32*m._44 - m._41*m._32*m._14 - m._11*m._34*m._42 + m._31*m._14*m._42 - m._31*m._12*m._44 + m._12*m._41*m._34 );
	matPick._33 = -(-m._14*m._21*m._42 + m._11*m._42*m._24 - m._11*m._44*m._22 + m._14*m._41*m._22 + m._12*m._21*m._44 - m._12*m._41*m._24 );
	matPick._34 = -(-m._12*m._21*m._34 - m._11*m._32*m._24 + m._31*m._12*m._24 + m._11*m._34*m._22 + m._14*m._21*m._32 - m._31*m._14*m._22 );
	// w
	matPick._41 =  ( m._41*m._32*m._23 + m._42*m._21*m._33 - m._43*m._21*m._32 - m._31*m._42*m._23 + m._31*m._43*m._22 - m._41*m._33*m._22 );
	matPick._42 =  (-m._11*m._33*m._42 - m._13*m._41*m._32 + m._12*m._41*m._33 + m._31*m._13*m._42 + m._11*m._32*m._43 - m._31*m._12*m._43 );
	matPick._43 =  (-m._11*m._43*m._22 - m._13*m._21*m._42 - m._12*m._41*m._23 + m._12*m._21*m._43 + m._11*m._42*m._23 + m._13*m._41*m._22 );
	matPick._44 =  (-m._11*m._32*m._23 + m._11*m._33*m._22 + m._13*m._21*m._32 + m._31*m._12*m._23 - m._12*m._21*m._33 - m._31*m._13*m._22 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::GetViewVolumeCrosses( const CVec2 &vPoint, CVec3 *pvNear, CVec3 *pvFar )
{
	CVec4 vNear4, vFar4;

	matPick.RotateHVector( &vNear4, CVec3(vPoint.x, vPoint.y, 0) );
	matPick.RotateHVector( &vFar4 , CVec3(vPoint.x, vPoint.y, 1) );
	//
	pvNear->Set( vNear4.x/vNear4.w, vNear4.y/vNear4.w, vNear4.z/vNear4.w );
	pvFar->Set( vFar4.x/vFar4.w, vFar4.y/vFar4.w, vFar4.z/vFar4.w );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** scene begin/end/flip/clear
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::IsActive()
{
	HRESULT dxrval = pD3DDevice->TestCooperativeLevel();
	if ( dxrval == D3DERR_DEVICELOST )
		return false; 
	if ( dxrval == D3D_OK )
		return true;
	/*
	if ( pp.Windowed )
	{
		GetBackBufferSize();
		if ( !FillPresent( videoMode ) )
			return false;
	}
	*/
	ResetDevice();
	dxrval = pD3DDevice->TestCooperativeLevel();
	if ( dxrval == D3DERR_DEVICELOST )
		return false;
	NI_ASSERTHR_T( dxrval == D3D_OK, "Unexpected error in test coop level" );
	return dxrval == D3D_OK;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::BeginScene()
{
	// reset statistics about passed vertices and triangles
	nNumPassedVertices = 0;
	nNumPassedPrimitives = 0;
	//
	HRESULT dxrval = pD3DDevice->BeginScene();
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't begin scene", return false );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::EndScene()
{
	/*
	// call frame reset for all dynamic containers
	for ( CVerticesMap::iterator pos1 = vertices.begin(); pos1 != vertices.end(); ++pos1 )
		std::for_each( pos1->second.begin(), pos1->second.end(), std::mem_fun(CDynamicVertexContainer::FrameReset) );
	for ( CIndicesMap::iterator pos2 = indices.begin(); pos2 != indices.end(); ++pos2 )
		std::for_each( pos2->second.begin(), pos2->second.end(), std::mem_fun(CDynamicIndexContainer::FrameReset) );
	*/
	// end scene
	HRESULT dxrval = pD3DDevice->EndScene();
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't end scene", return false );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::ForceFlushTempBuffers()
{
	// force flush for all temp buffers
	for ( std::unordered_map<DWORD, CPtr2<CTempVB> >::iterator it = tempVBs.begin(); it != tempVBs.end(); ++it )
		it->second->ForceFlush();
	for ( std::unordered_map<DWORD, CPtr2<CTempIB> >::iterator it = tempIBs.begin(); it != tempIBs.end(); ++it )
		it->second->ForceFlush();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::Flip()
{
	// frame statistics gathering
	dwLastFrameTime = GetTickCount() - dwLastFrameTime;
	//
	HRESULT dxrval = S_OK;
	if ( IsFullscreen() )
	{
		dxrval = pD3DDevice->Present( 0, 0, 0, 0 );
	}
	else
	{
		RECT rcSource = { 0, 0, rcScreen.right - rcScreen.left, rcScreen.bottom - rcScreen.top };
		dxrval = pD3DDevice->Present( &rcSource, &rcScreen, 0, 0 );
	}
	if ( dxrval == D3DERR_DEVICELOST )
		ResetDevice();
	//
	dxrval = pD3DDevice->TestCooperativeLevel();
	if ( dxrval != D3D_OK )
	{
		ResetDevice();
		dxrval = pD3DDevice->TestCooperativeLevel();
		if ( FAILED(dxrval) ) 
			return false;
	}
	//
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't present back buffer", return false );
	ForceFlushTempBuffers();
	ClearStates();
	++nCurrFrameNumber;
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::Clear( int nNumRects, RECT *pRects, DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil )
{
	// turn stencil clearing off, if no stencil available
	if ( nStencilBPP == 0 )
		dwFlags &= ~D3DCLEAR_STENCIL;
	if ( !pp.EnableAutoDepthStencil ) 
		dwFlags &= ~( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL );
	//
	HRESULT dxrval = pD3DDevice->Clear( nNumRects, reinterpret_cast<D3DRECT*>(pRects),
		                                  dwFlags, dwColor, fDepth, dwStencil );
	ClearStates();
	NI_ASSERTHR_SLOW_TF( dxrval, "Can't clear D3D device", return false );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** ������ � ����������
// ** �������� ���������/��������
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGFXVertices* CGraphicsEngine::CreateVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXVertices *pVertices )
{
	// create or assign buffer
	CVertexBuffer *pBuffer = 0;
	if ( pSVB != 0 ) 
	{
		NI_ASSERT_SLOW_T( pSVB->GetFormat() == dwFormat, NStr::Format("format mismatch during VB creation: %d != %d", pSVB->GetFormat(), dwFormat) );
		pBuffer = pSVB;
	}
	else
	{
		/*
		if ( eDynamic == GFXD_DYNAMIC )
			pBuffer = GetDynamicBuffer( nNumElements, dwFormat, dynVBs, (IDirect3DVertexBuffer8*)0, (CGraphicsEngine::SVBCreator*)0 );
		else
		*/
			pBuffer = CreateGeometryBuffer( nNumElements, dwFormat, eDynamic, (CStaticVB*)0, (IDirect3DVertexBuffer8*)0, (CGraphicsEngine::SVBCreator*)0 );
	}
	// allocate range
	SRangeLimits range;
	bool bAllocated = pBuffer->AllocateRange( nNumElements, &range );
	NI_ASSERT_SLOW_T( bAllocated, NStr::Format("Can't allocate vertex block for %d elements of %d format", nNumElements, dwFormat) );
	// wrap buffer to CVertices
	pVertices = pVertices == 0 ? new CVertices() : pVertices;
	NI_ASSERT_TF( pVertices != 0, "Can't create vertices", return 0 );
	CVertices *pVerts = dynamic_cast<CVertices*>( pVertices );
	NI_ASSERT_TF( pVerts != 0, "vertices of unknown type - use object factory to create proper vertices", return 0 );
	pVerts->Init( pBuffer, GFXPrimitiveToD3D(type), range );
	//
	return pVertices;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGFXIndices* CGraphicsEngine::CreateIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type, EGFXDynamic eDynamic, IGFXIndices *pIndices )
{
	// create or assign buffer
	CIndexBuffer *pBuffer = 0;
	if ( pSIB != 0 )
	{
		NI_ASSERT_SLOW_T( pSIB->GetFormat() == dwFormat, NStr::Format("format mismatch during IB creation: %d != %d", pSIB->GetFormat(), dwFormat) );
		pBuffer = pSIB;
	}
	else
	{
		/*
		if ( eDynamic == GFXD_DYNAMIC )
			pBuffer = GetDynamicBuffer( nNumElements, dwFormat, dynIBs, (IDirect3DIndexBuffer8*)0, (CGraphicsEngine::SIBCreator*)0 );
		else
		*/
			pBuffer = CreateGeometryBuffer( nNumElements, dwFormat, eDynamic, (CStaticIB*)0, (IDirect3DIndexBuffer8*)0, (CGraphicsEngine::SIBCreator*)0 );
	}
	// allocate range
	SRangeLimits range;
	bool bAllocated = pBuffer->AllocateRange( nNumElements, &range );
	NI_ASSERT_SLOW_T( bAllocated, NStr::Format("Can't allocate index block for %d elements of %d format", nNumElements, dwFormat) );
	// wrap buffer to CIndices
	pIndices = pIndices == 0 ? new CIndices() : pIndices;
	NI_ASSERT_TF( pIndices != 0, "Can't create indices", return 0 );
	CIndices *pInds = dynamic_cast<CIndices*>( pIndices );
	NI_ASSERT_TF( pInds != 0, "indices of unknown type - use object factory to create proper indices", return 0 );
	pInds->Init( pBuffer, GFXPrimitiveToD3D(type), range );
	//
	return pIndices;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::SetOptimizedBuffers( bool bEnable )
{
	bUseOptimizedBuffers = bEnable;
	for ( std::unordered_map<DWORD, CPtr2<CTempVB> >::iterator it = tempVBs.begin(); it != tempVBs.end(); ++it )
		it->second->UseOptimized( bEnable );
	for ( std::unordered_map<DWORD, CPtr2<CTempIB> >::iterator it = tempIBs.begin(); it != tempIBs.end(); ++it )
		it->second->UseOptimized( bEnable );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* CGraphicsEngine::GetTempVertices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type )
{
	if ( dwLastTempBufferFormat != dwFormat )
		ForceFlushTempBuffers();
	dwLastTempBufferFormat = dwFormat;
	//
	pTVB = tempVBs[dwFormat];
	// check size
	if ( pTVB )
	{
		if ( !pTVB->HaveSolidBlock( nNumElements ) )
		{
			pTVB = 0;
			tempVBs[dwFormat] = 0;
		}
	}
	// create new buffer
	if ( pTVB == 0 )
	{
		pTVB = CreateGeometryBuffer( nNumElements, dwFormat, GFXD_DYNAMIC, (CTempVB*)0, (IDirect3DVertexBuffer8*)0, (CGraphicsEngine::SVBCreator*)0 );
		pTVB->UseOptimized( bUseOptimizedBuffers );
		tempVBs[dwFormat] = pTVB;
	}
	pTVB->SetType( type );
	return pTVB->Lock( nNumElements );
}
void* CGraphicsEngine::GetTempIndices( int nNumElements, DWORD dwFormat, EGFXPrimitiveType type )
{
	pTIB = tempIBs[dwFormat];
	// check size
	if ( pTIB )
	{
		if ( !pTIB->HaveSolidBlock( nNumElements ) )
		{
			pTIB = 0;
			tempIBs[dwFormat] = 0;
		}
	}
	// create new buffer
	if ( pTIB == 0 )
	{
		pTIB = CreateGeometryBuffer( nNumElements, dwFormat, GFXD_DYNAMIC, (CTempIB*)0, (IDirect3DIndexBuffer8*)0, (CGraphicsEngine::SIBCreator*)0 );
		pTIB->UseOptimized( bUseOptimizedBuffers );
		tempIBs[dwFormat] = pTIB;
	}
	pTIB->SetType( type );
	return pTIB->Lock( nNumElements );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** ������ � ����������
// ** solid blocks
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::BeginSolidVertexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic )
{
	NI_ASSERT_SLOW_T( pSVB == 0, "Can't begin new solid vertex block - finish previous first!" );
	pSVB = CreateGeometryBuffer( nNumElements, dwFormat, eDynamic, (CStaticVB*)0, (IDirect3DVertexBuffer8*)0, (CGraphicsEngine::SVBCreator*)0 );
	return true;
}
bool CGraphicsEngine::EndSolidVertexBlock()
{
	pSVB = 0;
	return true;
}
bool CGraphicsEngine::BeginSolidIndexBlock( int nNumElements, DWORD dwFormat, EGFXDynamic eDynamic )
{
	NI_ASSERT_SLOW_T( pSIB == 0, "Can't begin new solid index block - finish previous first!" );
	pSIB = CreateGeometryBuffer( nNumElements, dwFormat, eDynamic, (CStaticIB*)0, (IDirect3DIndexBuffer8*)0, (CGraphicsEngine::SIBCreator*)0 );
	return true;
}
bool CGraphicsEngine::EndSolidIndexBlock()
{
	pSIB = 0;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** textures
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static D3DPOOL pools[3] = { D3DPOOL_DEFAULT, D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM };
IGFXTexture* CGraphicsEngine::CreateTexture( int nSizeX, int nSizeY, int nNumMipLevels, EGFXPixelFormat format, EGFXDynamic eDynamic, IGFXTexture *pTexture )
{
	const int nMemUsage = nSizeX * nSizeY * GetBPP( format ) / 8 * ( 1.0f + 1.0f - 1.0f / float(1 << (nNumMipLevels - 1)) );
	D3DPOOL pool = pools[eDynamic - 1];
	NWin32Helper::com_ptr<IDirect3DTexture8> pD3DTexture;
	HRESULT dxrval = pD3DDevice->CreateTexture( nSizeX, nSizeY, nNumMipLevels, 0, GFXPixelFormatToD3D(format), pool, pD3DTexture.GetAddr() );
	int nTryCounter =  0;
	while ( ((dxrval == D3DERR_OUTOFVIDEOMEMORY) || (dxrval == E_OUTOFMEMORY)) && (nTryCounter < 100) ) 
	{
		FreeVideoMemory( nCurrFrameNumber - 5, int(nMemUsage * 1.25f), true );
		dxrval = pD3DDevice->CreateTexture( nSizeX, nSizeY, nNumMipLevels, 0, GFXPixelFormatToD3D(format), pool, pD3DTexture.GetAddr() );
		if ( FAILED(dxrval) ) 
		{
			FreeVideoMemory( nCurrFrameNumber, int(nMemUsage * 1.25f), true );
			dxrval = pD3DDevice->CreateTexture( nSizeX, nSizeY, nNumMipLevels, 0, GFXPixelFormatToD3D(format), pool, pD3DTexture.GetAddr() );
			if ( FAILED(dxrval) ) 
			{
				FreeVideoMemory( nCurrFrameNumber + 1, int(nMemUsage * 1.25f), true );
				dxrval = pD3DDevice->CreateTexture( nSizeX, nSizeY, nNumMipLevels, 0, GFXPixelFormatToD3D(format), pool, pD3DTexture.GetAddr() );
			}
		}
	}
	NI_ASSERTHR_TF( dxrval, NStr::Format("Can't create texture %d:%d with %d mips of %d format", nSizeX, nSizeY, nNumMipLevels, format), return 0 );
	//
	pTexture = pTexture == 0 ? CreateObject<CTexture>( GFX_TEXTURE ) : pTexture;
	NI_ASSERT_TF( pTexture != 0, "can't create empty game texture", return 0 );
	CTexture *pTxtr = dynamic_cast<CTexture*>( pTexture );
	NI_ASSERT_TF( pTxtr != 0, "texture of unknown type - use object factory to create proper texture", return 0 );
	pTxtr->Init( pD3DTexture, nMemUsage );

	return pTexture;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGFXRTexture* CGraphicsEngine::CreateRTexture( int nSizeX, int nSizeY )
{
	NWin32Helper::com_ptr<IDirect3DTexture8> pD3DTexture;
	HRESULT dxrval = pD3DDevice->CreateTexture( nSizeX, nSizeY, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, pD3DTexture.GetAddr() );	
	NI_ASSERTHR_T( dxrval, "Can't create texture for render target" );
	// find best depth-stencil format
	static const D3DFORMAT s_fmtDepth[] = { D3DFMT_D32, D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D15S1 };
	D3DFORMAT fmtDepthStencil = D3DFMT_UNKNOWN;
	for ( int i = 0; i < 6; ++i )
	{
  	HRESULT dxrval = pD3D->CheckDeviceFormat( adapter.nIndex, adapter.capsHWDevice.DeviceType, displaymode.Format,
			                                        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, s_fmtDepth[i] );
		if ( FAILED(dxrval) )
			continue;
		dxrval = pD3D->CheckDepthStencilMatch( adapter.nIndex, adapter.capsHWDevice.DeviceType, 
				                                   displaymode.Format, D3DFMT_A8R8G8B8, s_fmtDepth[i] );
		if ( SUCCEEDED(dxrval) )
		{
			fmtDepthStencil = s_fmtDepth[i];
			break;
		}
	}
	NI_ASSERT_T( fmtDepthStencil != D3DFMT_UNKNOWN, "Can't find depth-stencil format" );
	if ( fmtDepthStencil != D3DFMT_UNKNOWN ) 
	{
		NWin32Helper::com_ptr<IDirect3DSurface8> pSurface;
		dxrval = pD3DDevice->CreateDepthStencilSurface( nSizeX, nSizeY, fmtDepthStencil, D3DMULTISAMPLE_NONE, pSurface.GetAddr() );
		CRenderTargetTexture *pTexture = CreateObject<CRenderTargetTexture>( GFX_RT_TEXTURE );
		pTexture->Init( pD3DTexture, pSurface, nSizeX * nSizeY * 4 * 2 );
		//
		return pTexture;
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::UpdateTexture( IGFXTexture *pSrcTexture, IGFXTexture *pDstTexture, bool bAsync )
{
	NI_ASSERT_TF( pSrcTexture != 0 && pDstTexture != 0, "Source and destination textures must be non-zero!", return false );
	IDirect3DTexture8 *pSrc = static_cast<CTexture*>( pSrcTexture )->GetInternalContainer();
	IDirect3DTexture8 *pDst = static_cast<CTexture*>( pDstTexture )->GetInternalContainer();
	//
	if ( bAsync ) 
	{
		HRESULT dxrval = pD3DDevice->UpdateTexture( pSrc, pDst );
		NI_ASSERTHR_TF( dxrval, "Can't update texture", return false );
	}
	else
	{
		NWin32Helper::com_ptr<IDirect3DSurface8> pDstSurface, pSrcSurface;
		pSrc->GetSurfaceLevel( 0, pSrcSurface.GetAddr() );
		pDst->GetSurfaceLevel( 0, pDstSurface.GetAddr() );
		//
		HRESULT dxrval = pD3DDevice->CopyRects( pSrcSurface, 0, 0, pDstSurface, 0 );
		NI_ASSERTHR_TF( dxrval, "Can't copy rects", return false );
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ** 
// ** rendering
// ** 
// ** 
// ** 
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set vertex shader and force flush temp buffers, if shader has changed
void CGraphicsEngine::SetVertexShader( DWORD dwFVF )
{
	pD3DDevice->SetVertexShader( dwFVF );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// low-level render range function
// just DrawPrimitive or DrawIndexedPrimitive (and set streaming source and indices)
HRESULT CGraphicsEngine::RenderRange( CVertices *pVertices, CIndices *pIndices )
{
	if ( pVertices == 0 )
		return D3DERR_INVALIDCALL;
	// apply state changes
	ApplyStates();
	//
  D3DPRIMITIVETYPE d3dptPrimitiveType = pIndices != 0 ? pIndices->GetPrimitiveType() : pVertices->GetPrimitiveType();
	// gather statistics about the data, passed to rendering
  DWORD dwNumPrimitives = pIndices == 0 ? pVertices->GetNumPrimitives() : pIndices->GetNumPrimitives();
	DWORD dwNumVertices = (pIndices == 0) || (pIndices->GetNumUsedVertices() == 0) ? pVertices->GetNumElements() : pIndices->GetNumUsedVertices();
 	nNumPassedVertices += dwNumVertices;
	nNumPassedPrimitives += dwNumPrimitives;
  //
	HRESULT dxrval = pD3DDevice->SetStreamSource( 0, pVertices->GetInternalContainer(), pVertices->GetElementSize() );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set streaming source %d", 0), return dxrval );
  if ( pIndices != 0 )
  {
		// render...
		dxrval = pD3DDevice->SetIndices( pIndices->GetInternalContainer(), pVertices->GetRangeStart() );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't set indices source", return dxrval );
		dxrval = pD3DDevice->DrawIndexedPrimitive( d3dptPrimitiveType, 0, dwNumVertices,
																							 pIndices->GetRangeStart(), dwNumPrimitives );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't draw indexed primitive", return dxrval );
  }
  else
  {
    dxrval = pD3DDevice->DrawPrimitive( d3dptPrimitiveType, pVertices->GetRangeStart(), dwNumPrimitives );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't draw primitive", return dxrval );
  }

	return dxrval;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CGraphicsEngine::RenderRange( IDirect3DVertexBuffer8 *pVertices, int nFirstVertex, int nNumVertices, int nVertexSize,
																		  IDirect3DIndexBuffer8 *pIndices, int nFirstIndex, 
																			int nNumPrimitives, D3DPRIMITIVETYPE d3dptPrimitiveType )
{
	if ( pVertices == 0 )
		return D3DERR_INVALIDCALL;
	// apply state changes
	ApplyStates();
	// gather statistics about the data, passed to rendering
 	nNumPassedVertices += nNumVertices;
	nNumPassedPrimitives += nNumPrimitives;
  //
	HRESULT dxrval = pD3DDevice->SetStreamSource( 0, pVertices, nVertexSize );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't set streaming source %d", 0), return dxrval );
  if ( pIndices != 0 )
  {
		// render...
		dxrval = pD3DDevice->SetIndices( pIndices, nFirstVertex );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't set indices source", return dxrval );
		dxrval = pD3DDevice->DrawIndexedPrimitive( d3dptPrimitiveType, 0, nNumVertices,
																							 nFirstIndex, nNumPrimitives );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't draw indexed primitive", return dxrval );
  }
  else
  {
    dxrval = pD3DDevice->DrawPrimitive( d3dptPrimitiveType, nFirstVertex, nNumPrimitives );
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't draw primitive", return dxrval );
  }

	return dxrval;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::Draw( IGFXVertices *pVerts, IGFXIndices *pInds )
{
	CVertices *pVertices = static_cast<CVertices*>( pVerts );
	CIndices *pIndices = static_cast<CIndices*>( pInds );
	//
	const DWORD dwFVF = pVertices->GetFormat();
	if ( dwLastVertexShader != dwFVF )
		ForceFlushTempBuffers();
	dwLastVertexShader = dwFVF;
	//
	SetVertexShader( dwFVF );
	// render with current setup
  D3DPRIMITIVETYPE d3dptPrimitiveType = pIndices != 0 ? pIndices->GetPrimitiveType() : pVertices->GetPrimitiveType();
	// gather statistics about the data, passed to rendering
	int nNumVertices = pVertices->GetNumElements();
  int nNumPrimitives;
	int nFirstIndex = 0;
	IDirect3DIndexBuffer8 *pIB = 0;
	if ( pIndices != 0 )
	{
		if ( pIndices->GetNumUsedVertices() != 0 )
			nNumVertices = pIndices->GetNumUsedVertices();
		nNumPrimitives = pIndices->GetNumPrimitives();
		pIB = pIndices->GetInternalContainer();
		nFirstIndex = pIndices->GetRangeStart();
	}
	else
	{
		nNumVertices = pVertices->GetNumElements();
		nNumPrimitives = pVertices->GetNumPrimitives();
	}
	//
	HRESULT dxrval = RenderRange( pVertices->GetInternalContainer(), pVertices->GetRangeStart(), nNumVertices, 
		                            pVertices->GetElementSize(), pIB, nFirstIndex, nNumPrimitives, d3dptPrimitiveType );
	NI_ASSERTHR_SLOW_TF( dxrval, "Failed to render geometry range with shader", return false );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::DrawTemp()
{
	if ( pTVB == 0 )
		return false;
	// apply state changes
	ApplyStates();
	//
	pTVB->UnlockAll();
	D3DPRIMITIVETYPE d3dptPrimitiveType = GFXPrimitiveToD3D( pTIB != 0 ? pTIB->GetType() : pTVB->GetType() );
	IDirect3DVertexBuffer8 *pVB = pTVB->GetInternalContainer();
	int nFirstVertex = pTVB->GetRangeStart();
	int nNumVertices = pTVB->GetNumElements();
	IDirect3DIndexBuffer8 *pIB = 0;
	int nNumPrimitives = GetNumPrimitives( d3dptPrimitiveType, pTVB->GetNumElements() );
	int nFirstIndex = 0;
	if ( pTIB != 0 )
	{
		pTIB->UnlockAll();
		pIB = pTIB->GetInternalContainer();
		nNumPrimitives = GetNumPrimitives( d3dptPrimitiveType, pTIB->GetNumElements() );
		nFirstIndex = pTIB->GetRangeStart();
	}
	//
	const DWORD dwFVF = pTVB->GetFormat();
	SetVertexShader( dwFVF );
	HRESULT dxrval = RenderRange( pTVB->GetInternalContainer(), pTVB->GetRangeStart(), pTVB->GetNumElements(), 
																pTVB->GetElementSize(), pIB, nFirstIndex, nNumPrimitives, d3dptPrimitiveType );
	pTVB = 0;
	pTIB = 0;
	NI_ASSERTHR_SLOW_TF( dxrval, "Failed to render temporary geometry", return false );
	//
	if ( dwLastVertexShader != dwFVF )
		ForceFlushTempBuffers();
	dwLastVertexShader = dwFVF;
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::DrawMesh( IGFXMesh *pMesh, const SHMatrix *matrices, int nNumMatrices )
{
	CGeometryMesh *pGMesh = static_cast<CGeometryMesh*>( pMesh );
	const CGeometryMesh::CMeshesList &figures = pGMesh->GetFigures();
	bool bRetVal = true;
	for ( CGeometryMesh::CMeshesList::const_iterator it = figures.begin(); it != figures.end(); ++it )
	{
		SetWorldTransforms( 0, matrices + it->nMatrixIndex, 1 );
		bRetVal = Draw( it->pVertices, it->pIndices ) && bRetVal;
	}
	return bRetVal;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::vector<SGFXLVertex> tempvertices;
static std::vector<WORD> tempindices;
bool CGraphicsEngine::DrawStringA( const char *pszString, int nX, int nY, DWORD dwColor )
{
  if ( pCurrentFont == 0 )
    return false;
	//
	pCurrentFont->FillGeometryData( pszString, nX, nY, dwColor, 0xff000000, tempvertices, tempindices );
	if ( tempvertices.empty() || tempindices.empty() )
		return true;
	// draw indexed primitive (triangle list)
	SetTexture( 0, pCurrentFont->GetTexture() );
	return ::DrawTemp( this, tempvertices, tempindices );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::DrawString( const wchar_t *pszString, int nX, int nY, DWORD dwColor )
{
  if ( pCurrentFont == 0 )
    return false;
	//
	pCurrentFont->FillGeometryData( pszString, nX, nY, dwColor, 0xff000000, tempvertices, tempindices );
	if ( tempvertices.empty() || tempindices.empty() )
		return true;
	// draw indexed primitive (triangle list)
	SetTexture( 0, pCurrentFont->GetTexture() );
	return ::DrawTemp( this, tempvertices, tempindices );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::DrawText( IGFXText *pTxt, const RECT &rect, int nY, DWORD dwFlags )
{
	SetShadingEffect( 3 );
	pTxt->SetWidth( rect.right - rect.left );
	CGFXText *pText = static_cast<CGFXText*>( pTxt );
	tempvertices.clear();
	tempindices.clear();
	pText->FillGeometryData( dwFlags, rect, rect.top + nY, 0, 0xff000000, tempvertices, tempindices );
	if ( tempvertices.empty() || tempindices.empty() )
		return true;
	// draw indexed primitive (triangle list)
	SetTexture( 0, static_cast<CFont*>( pText->GetFont() )->GetTexture() );
	return ::DrawTemp( this, tempvertices, tempindices );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::DrawRects( const SGFXRect2 *pRects, int nNumRects, bool bSolid )
{
	EGFXPrimitiveType type = bSolid ? GFXPT_TRIANGLELIST : GFXPT_LINELIST;
	CTempBufferLock<SGFXLVertex> vertices = GetTempVertices( nNumRects * 4, SGFXLVertex::format, type );
	DWORD dwSpecular = 0;
	for ( int i=0; i<nNumRects; ++i )
	{
		const SGFXRect2 &rect = pRects[i];
		//
		vertices->Setup( rect.rect.minx, rect.rect.maxy, rect.fZ, 1, rect.color, rect.specular, rect.maps.minx, rect.maps.maxy );
		++vertices;
		vertices->Setup( rect.rect.minx, rect.rect.miny, rect.fZ, 1, rect.color, rect.specular, rect.maps.minx, rect.maps.miny );
		++vertices;
		vertices->Setup( rect.rect.maxx, rect.rect.maxy, rect.fZ, 1, rect.color, rect.specular, rect.maps.maxx, rect.maps.maxy );
		++vertices;
		vertices->Setup( rect.rect.maxx, rect.rect.miny, rect.fZ, 1, rect.color, rect.specular, rect.maps.maxx, rect.maps.miny );
		++vertices;

		dwSpecular |= rect.specular;
	}
	// fill indices
	WORD wCurrVertex = 0;
	if ( bSolid )
	{
		WORD *pIndices = (WORD*)GetTempIndices( nNumRects * 6, GFXIF_INDEX16, type );
		for ( int i=0; i<nNumRects; ++i, wCurrVertex += 4 )
		{
			*pIndices++ = wCurrVertex + 2;
			*pIndices++ = wCurrVertex + 1;
			*pIndices++ = wCurrVertex + 0;
			*pIndices++ = wCurrVertex + 1;
			*pIndices++ = wCurrVertex + 2;
			*pIndices++ = wCurrVertex + 3;
		}
	}
	else
	{
		WORD *pIndices = (WORD*)GetTempIndices( nNumRects * 8, GFXIF_INDEX16, type );
		// 0, 1, 3, 2
		for ( int i=0; i<nNumRects; ++i, wCurrVertex += 4 )
		{
			*pIndices++ = wCurrVertex + 0;
			*pIndices++ = wCurrVertex + 1;

			*pIndices++ = wCurrVertex + 1;
			*pIndices++ = wCurrVertex + 3;

			*pIndices++ = wCurrVertex + 3;
			*pIndices++ = wCurrVertex + 2;

			*pIndices++ = wCurrVertex + 2;
			*pIndices++ = wCurrVertex + 0;
		}
	}
	// set specular (if it is)
	if ( dwSpecular & 0x00ffffff )
		SetRenderState( D3DRS_SPECULARENABLE, true );
	//
	const bool bRetVal = DrawTemp();
	// disable specular (restore), if it was set
	if ( dwSpecular & 0x00ffffff )
		SetRenderState( D3DRS_SPECULARENABLE, false );
	//
	return bRetVal;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetGammaRamp( const SGFXGammaRamp &ramp, bool bCalibrate )
{
	if ( pD3DDevice ) 
		pD3DDevice->SetGammaRamp( bCalibrate ? D3DSGR_CALIBRATE : D3DSGR_NO_CALIBRATION, (D3DGAMMARAMP*)(&ramp) );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::GetGammaRamp( const SGFXGammaRamp *pRamp )
{
	pD3DDevice->GetGammaRamp( (D3DGAMMARAMP*)pRamp );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::SetGammaCorrectionValues( const float _fBrightness, const float _fContrast, const float _fGamma )
{
	fBrightness = _fBrightness;
	fContrast = _fContrast;
	fGamma = _fGamma;
}
void CGraphicsEngine::GetGammaCorrectionValues( float *pfBrightness, float *pfContrast, float *pfGamma )
{
	*pfBrightness = fBrightness;
	*pfContrast = fContrast;
	*pfGamma = fGamma;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::TakeScreenShot( IImage *pImage )
{
	// extract real display mode
	D3DDISPLAYMODE mode;
	HRESULT dxrval = pD3D->GetAdapterDisplayMode( adapter.nIndex, &mode );
	// create surface and retrieve screen
	NWin32Helper::com_ptr<IDirect3DSurface8> pD3DSurface;
	dxrval = pD3DDevice->CreateImageSurface( mode.Width, mode.Height, D3DFMT_A8R8G8B8, pD3DSurface.GetAddr() );
	NI_ASSERTHR_TF( dxrval, NStr::Format("Can't create surface %d:%d:32 to take screenshot", mode.Width, mode.Height), return 0 );
	dxrval = pD3DDevice->GetFrontBuffer( pD3DSurface );
	NI_ASSERTHR_TF( dxrval, "Can't retrieve front buffer data for the screenshot", return 0 );
	//
	D3DLOCKED_RECT lrRect;
	dxrval = pD3DSurface->LockRect( &lrRect, &rcScreen, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY );
	if ( FAILED(dxrval) ) 
		return false;
	SColor *pDst = pImage->GetLFB();
	const int nWidth = rcScreen.right - rcScreen.left;
	const int nHeight = rcScreen.bottom - rcScreen.top;

	for ( int i = 0; i < nHeight; ++i )
	{
		memcpy( pDst, (void*)(DWORD(lrRect.pBits) + i*lrRect.Pitch), nWidth*sizeof(SColor) );
		pDst += nWidth;
	}
	pD3DSurface->UnlockRect();
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraphicsEngine::SetShadingEffect( int nEffect )
{
	CShadersMap::const_iterator pos = shaders.find( nEffect );
	if ( pos != shaders.end() ) 
	{
		const CShader::SShadeValues &shadevals = pos->second.GetSetValues();
		// render states
		for ( CShader::CShadesList::const_iterator it = shadevals.rses.begin(); it != shadevals.rses.end(); ++it )
			SetRenderState( D3DRENDERSTATETYPE(it->first), it->second );
		// texture stage states
		for ( int i = 0; i < shadevals.tsses.size(); ++i )
		{
			for ( CShader::CShadesList::const_iterator it = shadevals.tsses[i].begin(); it != shadevals.tsses[i].end(); ++it )
				SetTextureStageState( i, D3DTEXTURESTAGESTATETYPE(it->first), it->second );
		}
		//
		return true;
	}
	//
	switch ( nEffect )
	{
		case 1:															// sprite model rendering: alpha blend and alpha check 255
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 200 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
			break;
		case 2:															// mesh model rendering: no alpha blend and alpha check
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, false );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 3:															// sprite model rendering: alpha blend and alpha check 1
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 1 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
			break;
		case 4:
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
			SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
			//SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );

			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 5:
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			//SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			//SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 2 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
			//SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
			SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
			
			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 6:
			SetRenderState( D3DRS_STENCILENABLE, true );
			SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
			break;
		case 7:
			SetRenderState( D3DRS_STENCILENABLE, false );
			break;
		case 8:															// mesh model rendering: with alpha blend and alpha check
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 1 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 9:																// just texture with multiply
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 10:															// particles. ADD
			// depth buffer write off
			SetRenderState( D3DRS_ZWRITEENABLE, false );
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 1 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 11:
			// depth buffer write off
			SetRenderState( D3DRS_ZWRITEENABLE, true );
			break;
		case 12:															// particles. Modulate
			// depth buffer write off
			SetRenderState( D3DRS_ZWRITEENABLE, false );
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 5 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 13:
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 10 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 14:
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			SetRenderState( D3DRS_ZWRITEENABLE, false );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
			break;
		case 15:
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 16:
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 17:														// video rendering
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, false );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
			SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
			break;
		case 18:														// turn off video rendering addressing mode
			SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
			SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
			break;
		case 100:
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			
			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_POINT );
			break;
		case 101:														// terrain with noise rendering
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, false );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 102:														// terrain crosses with noise rendering
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 103:															// noise w/o crosses => just multiply SRC color with DST
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 104:														// noise with cross => SRC*DST + alpha ref check with cross alpha
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 50 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

			SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 110:															// shadow rendering with stencil check (pre-setup)
			SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
			if ( nStencilBPP > 0 ) 
			{
				SetRenderState( D3DRS_STENCILENABLE, true );
				SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
				SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
				SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
				SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
				SetRenderState( D3DRS_STENCILMASK, 0xffffffff );
		    SetRenderState( D3DRS_STENCILREF, 0 );
			}
			break;
		case 111:															// sprite shadow
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 50 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
			break;
		case 112:															// mesh shadow
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, false );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 113:															// shadow rendering with stencil check (post-setup)
			SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
			if ( nStencilBPP > 0 ) 
			{
				SetRenderState( D3DRS_STENCILENABLE, false );
				//SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
				//SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
			}
			break;
		case 200:															// sprite model rendering: alpha blend and alpha check 1
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 50 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			SetDepthBufferMode( GFXDB_NONE, GFXCMP_DEFAULT );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 300:
			// Clear the stencil buffer
			pD3DDevice->Clear( 0, 0, D3DCLEAR_STENCIL, 0, 0, 0 );
			ClearStates();
			// Turn stenciling
			SetRenderState( D3DRS_STENCILENABLE, TRUE );
			SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			SetRenderState( D3DRS_STENCILREF, 0 );
			SetRenderState( D3DRS_STENCILMASK, 0x00000000 );
			SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );

			// Increment the stencil buffer for each pixel drawn
			SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT );
			SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
			break;
		case 301:
			// Turn off the buffer
			SetRenderState( D3DRS_ZENABLE, FALSE );
			SetRenderState( D3DRS_ALPHABLENDENABLE, false );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			// Set up the stencil states
			SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
			SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
			SetRenderState( D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL );
			SetRenderState( D3DRS_STENCILREF, 0 );
			// Set the background to black
			pD3DDevice->Clear( 0, 0, D3DCLEAR_TARGET, 0, 0, 0 );
			ClearStates();
			// Set render states for drawing a rectangle that covers the viewport.
			// The color of the rectangle will be passed in D3DRS_TEXTUREFACTOR
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			break;
		case 302:
			SetRenderState( D3DRS_ZENABLE, TRUE );
			SetRenderState( D3DRS_STENCILENABLE, FALSE );
			SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
			break;
		case 303:
			// alpha ref check 
			SetRenderState( D3DRS_ALPHATESTENABLE, true );
			SetRenderState( D3DRS_ALPHAREF, 1 );
			SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
			// blending
			SetRenderState( D3DRS_ALPHABLENDENABLE, true );
			SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			//
			SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
			//
			SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			
			SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
			SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
			break;
		case 304:
			SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
			break;
		default:
			if ( (nEffect >= 310) && (nEffect < 330)  )
			{
				const DWORD colors[][3] = 
				{
					{ 0, 0, 85 },
					{ 0, 0, 170 },
					{ 0, 0, 255 },
					{ 0, 85, 255 },
					{ 0, 170, 255 },
					{ 0, 255, 255 },
					{ 0, 255, 170 },
					{ 0, 255, 85 },
					{ 0, 255, 0 },
					{ 85, 255, 0 },
					{ 170, 255, 0 },
					{ 255, 255, 0 },
					{ 255, 170, 0 },
					{ 255, 127, 0 },
					{ 255, 85, 0 },
					{ 255, 0, 0 },
					{ 255, 0, 42 },
					{ 255, 0, 85 },
					{ 255, 0, 170 },
					{ 255, 0, 255 }
				};
				const int i = nEffect - 310;
				const DWORD color = 0xff000000 | (colors[i][0] << 16) | (colors[i][1] << 8) | colors[i][2];
				SetRenderState( D3DRS_STENCILMASK, 0xffffffff );
		    SetRenderState( D3DRS_STENCILREF, i );
				SetRenderState( D3DRS_TEXTUREFACTOR, color );
				//SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
				if ( i == 19 )
					SetRenderState( D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL );
				return true;
			}
			return false;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraphicsEngine::SetupShaders()
{
	// sprite model rendering: alpha blend and alpha check 255
	{
		CShader &shader = shaders[1];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 200 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
	}
	// mesh model rendering: no alpha blend and alpha check
	{
		CShader &shader = shaders[2];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, false );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// sprite model rendering: alpha blend and alpha check 1
	{
		CShader &shader = shaders[3];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
	}
	//
	{
		CShader &shader = shaders[4];
		//
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
		//SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );

		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	//
	{
		CShader &shader = shaders[5];
		//
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
		//SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		//SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 2 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
		//SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
		
		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	//
	{
		CShader &shader = shaders[6];
		shader.SetRenderState( D3DRS_STENCILENABLE, true );
		shader.SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
		shader.SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
	}
	{
		CShader &shader = shaders[7];
		shader.SetRenderState( D3DRS_STENCILENABLE, false );
	}
	// mesh model rendering: with alpha blend and alpha check
	{
		CShader &shader = shaders[8];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	// just texture with multiply
	{
		CShader &shader = shaders[9];
		//
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	// particles. ADD
	{
		CShader &shader = shaders[10];
		// depth buffer write off
		shader.SetRenderState( D3DRS_ZWRITEENABLE, false );
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	{
		CShader &shader = shaders[11];
		shader.SetRenderState( D3DRS_ZWRITEENABLE, true );
	}
	// particles. Modulate
	{
		CShader &shader = shaders[12];
		// depth buffer write off
		shader.SetRenderState( D3DRS_ZWRITEENABLE, false );
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 5 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	{
		CShader &shader = shaders[13];
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 10 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	{
		CShader &shader = shaders[14];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetRenderState( D3DRS_ZWRITEENABLE, false );
		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
	}
	{
		CShader &shader = shaders[15];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	{
		CShader &shader = shaders[16];
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// video rendering
	{
		CShader &shader = shaders[17];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, false );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
	}
	// turn off video rendering addressing mode
	{
		CShader &shader = shaders[18];
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
	}
	// video with alpha rendering
	{
		CShader &shader = shaders[19];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
	}
	// multiply - for tracks rendering
	{
		CShader &shader = shaders[20];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		//shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		//shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_ADD );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	// minimap rendering
	{
		CShader &shader = shaders[21];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
		shader.SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
	}
	{
		CShader &shader = shaders[22];
		shader.SetRenderState( D3DRS_ZWRITEENABLE, false );
	}
	{
		CShader &shader = shaders[23];
		shader.SetRenderState( D3DRS_ZWRITEENABLE, true );
	}
	{
		CShader &shader = shaders[100];
		//
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_POINT );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_POINT );
	}
	// terrain with noise rendering
	{
		CShader &shader = shaders[101];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, false );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// terrain crosses with noise rendering
	{
		CShader &shader = shaders[102];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// noise w/o crosses => just multiply SRC color with DST
	{
		CShader &shader = shaders[103];
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// noise with cross => SRC*DST + alpha ref check with cross alpha
	{
		CShader &shader = shaders[104];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 50 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	// shadow rendering with stencil check (pre-setup)
	{
		CShader &shader = shaders[110];
		shader.SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		if ( GetGlobalVar("overdraw", 0) == 0 ) 
		{
			shader.SetRenderState( D3DRS_STENCILENABLE, true );
			shader.SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			shader.SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
			shader.SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
			shader.SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			shader.SetRenderState( D3DRS_STENCILMASK, 0xffffffff );
			shader.SetRenderState( D3DRS_STENCILREF, 0 );
		}
	}
	// sprite shadow
	{
		CShader &shader = shaders[111];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 50 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );	
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
	}
	// mesh shadow
	{
		CShader &shader = shaders[112];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, false );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	// shadow rendering with stencil check (post-setup)
	{
		CShader &shader = shaders[113];
		shader.SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
		if ( GetGlobalVar("overdraw", 0) == 0 ) 
			shader.SetRenderState( D3DRS_STENCILENABLE, false );
	}
	// sprite model rendering: alpha blend and alpha check 1
	{
		CShader &shader = shaders[200];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 50 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		shader.SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	{
		CShader &shader = shaders[300];
		// Turn stenciling
		shader.SetRenderState( D3DRS_STENCILENABLE, TRUE );
		shader.SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
		shader.SetRenderState( D3DRS_STENCILREF, 0 );
		shader.SetRenderState( D3DRS_STENCILMASK, 0x00000000 );
		shader.SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );
		// Increment the stencil buffer for each pixel drawn
		shader.SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT );
		shader.SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
		shader.SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT );
	}
	{
		CShader &shader = shaders[301];
		// Turn off the buffer
		shader.SetRenderState( D3DRS_ZENABLE, FALSE );
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, false );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		// Set up the stencil states
		shader.SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		shader.SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
		shader.SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
		shader.SetRenderState( D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL );
		shader.SetRenderState( D3DRS_STENCILREF, 0 );
		// Set render states for drawing a rectangle that covers the viewport.
		// The color of the rectangle will be passed in D3DRS_TEXTUREFACTOR
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	}
	{
		CShader &shader = shaders[302];
		shader.SetRenderState( D3DRS_ZENABLE, TRUE );
		shader.SetRenderState( D3DRS_STENCILENABLE, FALSE );
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
	}
	{
		CShader &shader = shaders[303];
		// alpha ref check 
		shader.SetRenderState( D3DRS_ALPHATESTENABLE, true );
		shader.SetRenderState( D3DRS_ALPHAREF, 1 );
		shader.SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
		// blending
		shader.SetRenderState( D3DRS_ALPHABLENDENABLE, true );
		shader.SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		shader.SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
		//
		shader.SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		shader.SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		shader.SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		shader.SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		shader.SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );		
		shader.SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
		shader.SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	}
	{
		CShader &shader = shaders[304];
	}
	{
		CShader &shader = shaders[304];
		shader.SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
