#include "StdAfx.h"

#include "VideoCheck.h"

#include <ddraw.h>
#include <dinput.h>
#include <d3d9.h>

#include "..\Misc\Win32Helper.h"

typedef HRESULT(WINAPI * DIRECTDRAWCREATE)( GUID*, LPDIRECTDRAW*, IUnknown* );
typedef HRESULT(WINAPI * DIRECTDRAWCREATEEX)( GUID*, VOID**, REFIID, IUnknown* );
typedef HRESULT(WINAPI * DIRECTINPUTCREATE)( HINSTANCE, DWORD, LPDIRECTINPUT*, IUnknown* );
typedef IDirect3D9*(WINAPI * DIRECT3DCREATE8)( UINT SDKVersion );
static const GUID CLSID_DirectMusic_Compat =
{ 0x636b9f10, 0x0c7d, 0x11d1, { 0x95, 0xb2, 0x00, 0x20, 0xaf, 0xdc, 0x74, 0x21 } };
const wchar_t* STDCALL NVideoCheck::GetAPIName()
{
	return L"DirectX";
}
DWORD STDCALL NVideoCheck::GetAPIVersion()
{
	HINSTANCE            hDDrawDLL   = 0;
	HINSTANCE            hDInputDLL  = 0;
	HINSTANCE            hD3D8DLL    = 0;
	LPDIRECTDRAW         pDDraw      = 0;
	LPDIRECTDRAW2        pDDraw2     = 0;
	LPDIRECTDRAWSURFACE  pSurf       = 0;
	LPDIRECTDRAWSURFACE3 pSurf3      = 0;
	LPDIRECTDRAWSURFACE4 pSurf4      = 0;
	DWORD                dwDXVersion = 0;

	hDDrawDLL = LoadLibrary( "DDRAW.DLL" );
	if ( hDDrawDLL == NULL )
	{
		dwDXVersion = 0;
		return dwDXVersion;
	}

	DIRECTDRAWCREATE DirectDrawCreate = (DIRECTDRAWCREATE)GetProcAddress( hDDrawDLL, "DirectDrawCreate" );
	if ( DirectDrawCreate == NULL )
	{
		dwDXVersion = 0;
		FreeLibrary( hDDrawDLL );
		OutputDebugString( "Couldn't LoadLibrary DDraw\r\n" );
		return dwDXVersion;
	}

	HRESULT hr = DirectDrawCreate( NULL, &pDDraw, NULL );
	if ( FAILED(hr) )
	{
		dwDXVersion = 0;
		FreeLibrary( hDDrawDLL );
		OutputDebugString( "Couldn't create DDraw\r\n" );
		return dwDXVersion;
	}

	dwDXVersion = 0x100;

	hr = pDDraw->QueryInterface( IID_IDirectDraw2, (VOID**)&pDDraw2 );
	if ( FAILED(hr) )
	{
		pDDraw->Release();
		FreeLibrary( hDDrawDLL );
		OutputDebugString( "Couldn't QI DDraw2\r\n" );
		return dwDXVersion;
	}

	pDDraw2->Release();
	dwDXVersion = 0x200;



	hDInputDLL = LoadLibrary( "DINPUT.DLL" );
	if ( hDInputDLL == NULL )
	{
		OutputDebugString( "Couldn't LoadLibrary DInput\r\n" );
		pDDraw->Release();
		return dwDXVersion;
	}

	DIRECTINPUTCREATE DirectInputCreate = (DIRECTINPUTCREATE)GetProcAddress( hDInputDLL, "DirectInputCreateA" );
	if ( DirectInputCreate == NULL )
	{
		FreeLibrary( hDInputDLL );
		FreeLibrary( hDDrawDLL );
		pDDraw->Release();
		OutputDebugString( "Couldn't GetProcAddress DInputCreate\r\n" );
		return dwDXVersion;
	}

	dwDXVersion = 0x300;
	FreeLibrary( hDInputDLL );




	DDSURFACEDESC ddsd;
	ZeroMemory( &ddsd, sizeof(ddsd) );
	ddsd.dwSize         = sizeof(ddsd);
	ddsd.dwFlags        = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	hr = pDDraw->SetCooperativeLevel( NULL, DDSCL_NORMAL );
	if ( FAILED(hr) )
	{
		pDDraw->Release();
		FreeLibrary( hDDrawDLL );
		dwDXVersion = 0;
		OutputDebugString( "Couldn't Set coop level\r\n" );
		return dwDXVersion;
	}

	hr = pDDraw->CreateSurface( &ddsd, &pSurf, NULL );
	if ( FAILED(hr) )
	{
		pDDraw->Release();
		FreeLibrary( hDDrawDLL );
		dwDXVersion = 0;
		OutputDebugString( "Couldn't CreateSurface\r\n" );
		return dwDXVersion;
	}

	if ( FAILED( pSurf->QueryInterface( IID_IDirectDrawSurface3, (VOID**)&pSurf3 ) ) )
	{
		pDDraw->Release();
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	dwDXVersion = 0x500;



	if ( FAILED( pSurf->QueryInterface( IID_IDirectDrawSurface4, (VOID**)&pSurf4 ) ) )
	{
		pDDraw->Release();
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	dwDXVersion = 0x600;
	pSurf->Release();
	pDDraw->Release();



	IUnknown *pDMusic = NULL;
	CoInitialize( NULL );
	hr = CoCreateInstance( CLSID_DirectMusic_Compat, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, (VOID**)&pDMusic );
	if ( FAILED(hr) )
	{
		OutputDebugString( "Couldn't create CLSID_DirectMusic\r\n" );
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	dwDXVersion = 0x601;
	pDMusic->Release();
	CoUninitialize();



	LPDIRECTDRAW7 pDD7;
	DIRECTDRAWCREATEEX DirectDrawCreateEx = (DIRECTDRAWCREATEEX)GetProcAddress( hDDrawDLL, "DirectDrawCreateEx" );
	if ( NULL == DirectDrawCreateEx )
	{
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	if ( FAILED( DirectDrawCreateEx( NULL, (VOID**)&pDD7, IID_IDirectDraw7, NULL ) ) )
	{
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	dwDXVersion = 0x700;
	pDD7->Release();



	hD3D8DLL = LoadLibrary( "D3D8.DLL" );
	if ( hD3D8DLL == NULL )
	{
		FreeLibrary( hDDrawDLL );
		return dwDXVersion;
	}

	dwDXVersion = 0x800;



	FreeLibrary( hDDrawDLL );
	FreeLibrary( hD3D8DLL );

	return dwDXVersion;
}
bool STDCALL NVideoCheck::GetVideoMemory( SVideoMemory *pMemory )
{
	NWin32Helper::CDLLHandle handle( "ddraw.dll" );
	if ( !handle.IsLoaded() ) 
		return false;
	DIRECTDRAWCREATEEX pfnDirectDrawCreateEx = handle.GetProcAddress( "DirectDrawCreateEx", (DIRECTDRAWCREATEEX)0 );
	if ( pfnDirectDrawCreateEx == 0 ) 
		return false;
	LPDIRECTDRAW7 pDD7Temp = 0;
	HRESULT dxrval = (*pfnDirectDrawCreateEx)( NULL, (void**)&pDD7Temp, IID_IDirectDraw7, NULL );
	if ( FAILED(dxrval) ) 
		return false;
	NWin32Helper::com_ptr<IDirectDraw7> pDD = pDD7Temp;
	pDD7Temp->Release();
	Zero( *pMemory );
	DWORD dwTotal = 0, dwFree = 0;
	DDSCAPS2 caps;
	Zero( caps );
	caps.dwCaps = DDSCAPS_VIDEOMEMORY;
	dxrval = pDD->GetAvailableVidMem( &caps, &dwTotal, &dwFree );
	if ( SUCCEEDED(dxrval) ) 
	{
		pMemory->local.dwTotal = dwTotal;
		pMemory->local.dwFree = dwFree;
	}
	caps.dwCaps = DDSCAPS_NONLOCALVIDMEM;
	dxrval = pDD->GetAvailableVidMem( &caps, &dwTotal, &dwFree );
	if ( SUCCEEDED(dxrval) ) 
	{
		pMemory->nonlocal.dwTotal = dwTotal;
		pMemory->nonlocal.dwFree = dwFree;
	}
	caps.dwCaps = DDSCAPS_TEXTURE;
	pDD->GetAvailableVidMem( &caps, &dwTotal, &dwFree );
	if ( SUCCEEDED(dxrval) ) 
	{
		pMemory->texture.dwTotal = dwTotal;
		pMemory->texture.dwFree = dwFree;
	}

	return true;
}
