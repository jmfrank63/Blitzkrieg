#include "StdAfx.h"

#include "GFXObjectFactory.h"

#include "GraphicsEngine.h"
#include "TextureManager.h"
#include "GeometryManager.h"
#include "FontManager.h"

#include "Texture.h"
#include "GeometryBuffer.h"
#include "Font.h"
#include "Text.h"
#include "GeometryMesh.h"
#include "VideoCheck.h"
static CGFXObjectFactory theGFXObjectFactory;
CGFXObjectFactory::CGFXObjectFactory()
{
	REGISTER_CLASS( this, GFX_GFX, CGraphicsEngine );
	REGISTER_CLASS( this, GFX_TEXTURE_MANAGER, CTextureManager );
	REGISTER_CLASS( this, GFX_MESH_MANAGER, CMeshManager );
	REGISTER_CLASS( this, GFX_FONT_MANAGER, CFontManager );
	REGISTER_CLASS( this, GFX_TEXTURE, CTexture );
	REGISTER_CLASS( this, GFX_RT_TEXTURE, CRenderTargetTexture );
	REGISTER_CLASS( this, GFX_VERTICES, CVertices );
	REGISTER_CLASS( this, GFX_INDICES, CIndices );
	REGISTER_CLASS( this, GFX_MESH, CGeometryMesh );
	REGISTER_CLASS( this, GFX_FONT, CFont );
	REGISTER_CLASS( this, GFX_TEXT, CGFXText );
}
static CGFXModuleChecker theGFXModuleChecker;
int STDCALL CGFXModuleChecker::CheckFunctionality() const
{
	return 0;
}
void STDCALL CGFXModuleChecker::SetModuleFunctionalityLimits() const
{
	NVideoCheck::SVideoMemory memory;
	NVideoCheck::GetVideoMemory( &memory );
	const int MB = 1024*1024;
	if ( memory.local.dwTotal > 32*MB ) 
	{
		SetGlobalVar( "GFX.Limit.Mode.SizeX", 1000000 );
		SetGlobalVar( "GFX.Limit.Mode.SizeY", 1000000 );
		SetGlobalVar( "GFX.Limit.Mode.BPP", 32 );
	}
	if ( memory.local.dwTotal > 24*MB ) 
	{
		SetGlobalVar( "GFX.Limit.Mode.SizeX", 1600 );
		SetGlobalVar( "GFX.Limit.Mode.SizeY", 1200 );
		SetGlobalVar( "GFX.Limit.Mode.BPP", 32 );
	}
	else if ( memory.local.dwTotal > 12*MB ) 
	{
		SetGlobalVar( "GFX.Limit.Mode.SizeX", 1024 );
		SetGlobalVar( "GFX.Limit.Mode.SizeY", 768 );
		SetGlobalVar( "GFX.Limit.Mode.BPP", 32 );
	}
	else if ( memory.local.dwTotal > 4*MB ) 
	{
		SetGlobalVar( "GFX.Limit.Mode.SizeX", 800 );
		SetGlobalVar( "GFX.Limit.Mode.SizeY", 600 );
		SetGlobalVar( "GFX.Limit.Mode.BPP", 16 );
	}
	if ( memory.texture.dwTotal > 32*MB ) 
		SetGlobalVar( "GFX.Limit.TextureQuality", 2 );
	else if ( memory.texture.dwTotal > 12*MB ) 
		SetGlobalVar( "GFX.Limit.TextureQuality", 1 );
}
static SModuleDescriptor theModuleDescriptor( "Graphics (DX8)", GFX_GFX, 0x0100, &theGFXObjectFactory, &theGFXModuleChecker );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
