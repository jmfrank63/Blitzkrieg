#include "imgui_backend.h"

// stb_truetype for the Devanagari loader below. ImGui's own copy in
// imgui_draw.cpp is static to that file; this one is too, and sits in a
// namespace as ImGui's does (IMGUI_STB_NAMESPACE) because imgui_internal.h
// forward declares the rect packer types the header defines.
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
namespace BkStb
{
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
}

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <SDL3/SDL.h>

#include "Shaper.h"

#include <stdio.h>
#include <string.h> // memset
#include <vector>

extern "C" bool bk_imgui_backend_init( void *sdl_window, void *gpu_device, uint32_t color_target_format )
{
    SDL_Window *window = static_cast<SDL_Window *>( sdl_window );
    if ( !ImGui_ImplSDL3_InitForSDLGPU( window ) )
        return false;
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = static_cast<SDL_GPUDevice *>( gpu_device );
    info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>( color_target_format );
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if ( !ImGui_ImplSDLGPU3_Init( &info ) )
    {
        ImGui_ImplSDL3_Shutdown();
        return false;
    }
    return true;
}

extern "C" void bk_imgui_backend_shutdown( void )
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

extern "C" bool bk_imgui_backend_process_event( const void *sdl_event )
{
    return ImGui_ImplSDL3_ProcessEvent( static_cast<const SDL_Event *>( sdl_event ) );
}

extern "C" void bk_imgui_backend_new_frame( void )
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

extern "C" void bk_imgui_backend_use_global_mouse( bool enabled )
{
    // From 2.0.22, set unconditionally at Init already (#5710) - restated
    // here so a caller reading only this function sees the full story.
    SDL_SetHint( SDL_HINT_MOUSE_AUTO_CAPTURE, enabled ? "1" : "0" );
    ImGui_ImplSDL3_SetMouseCaptureMode( enabled ? ImGui_ImplSDL3_MouseCaptureMode_Enabled : ImGui_ImplSDL3_MouseCaptureMode_Disabled );
}

extern "C" void bk_imgui_backend_render( void *command_buffer, void *target )
{
    ImDrawData *draw_data = ImGui::GetDrawData();
    if ( draw_data == nullptr || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f )
        return;
    SDL_GPUCommandBuffer *command = static_cast<SDL_GPUCommandBuffer *>( command_buffer );
    // Uploads happen in a copy pass, which must not overlap a render pass.
    ImGui_ImplSDLGPU3_PrepareDrawData( draw_data, command );
    SDL_GPUColorTargetInfo color = {};
    color.texture = static_cast<SDL_GPUTexture *>( target );
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( command, &color, 1, nullptr );
    if ( pass == nullptr )
        return;
    ImGui_ImplSDLGPU3_RenderDrawData( draw_data, command, pass );
    SDL_EndGPURenderPass( pass );
}

static void CopyWindowName( char *buffer, size_t capacity, const ImGuiWindow *window )
{
    size_t length = 0;
    if ( window != nullptr && window->Name != nullptr )
    {
        while ( length + 1 < capacity && window->Name[length] != '\0' )
        {
            buffer[length] = window->Name[length];
            length++;
        }
    }
    buffer[length] = '\0';
}

extern "C" void bk_imgui_backend_pointer_state( BkImguiPointerState *out )
{
    if ( out == nullptr )
        return;
    memset( out, 0, sizeof( *out ) );
    ImGuiContext *context = ImGui::GetCurrentContext();
    if ( context == nullptr )
        return;
    ImGuiContext &g = *context;
    const ImGuiIO &io = g.IO;
    out->mouse_x = io.MousePos.x;
    out->mouse_y = io.MousePos.y;
    out->display_w = io.DisplaySize.x;
    out->display_h = io.DisplaySize.y;
    out->want_capture_mouse = io.WantCaptureMouse;
    out->app_focus_lost = io.AppFocusLost;
    for ( int button = 0; button < 3; button++ )
    {
        out->mouse_down[button] = io.MouseDown[button];
        out->mouse_down_owned[button] = io.MouseDownOwned[button];
    }
    out->open_popups = g.OpenPopupStack.Size;
    out->want_capture_mouse_next_frame = g.WantCaptureMouseNextFrame;
    out->queued_events = g.InputEventsQueue.Size;
    for ( const ImGuiInputEvent &event : g.InputEventsQueue )
    {
        if ( event.Type == ImGuiInputEventType_MousePos )
        {
            out->queued_mouse_pos++;
            out->queued_mouse_pos_valid = true;
            out->queued_mouse_x = event.MousePos.PosX;
            out->queued_mouse_y = event.MousePos.PosY;
        }
        else if ( event.Type == ImGuiInputEventType_MouseButton )
            out->queued_mouse_button++;
        else if ( event.Type == ImGuiInputEventType_MouseWheel )
            out->queued_mouse_wheel++;
        else if ( event.Type == ImGuiInputEventType_Key )
            out->queued_key++;
        else if ( event.Type == ImGuiInputEventType_Focus )
            out->queued_focus++;
    }
    CopyWindowName( out->hovered_window, sizeof( out->hovered_window ), g.HoveredWindow );
    CopyWindowName( out->hovered_before_clear, sizeof( out->hovered_before_clear ), g.HoveredWindowBeforeClear );
    CopyWindowName( out->active_window, sizeof( out->active_window ), g.ActiveIdWindow );
    out->active_id = g.ActiveId;
    out->hovered_id = g.HoveredId;
    out->want_capture_keyboard = io.WantCaptureKeyboard;
    out->want_text_input = io.WantTextInput;
    CopyWindowName( out->nav_window, sizeof( out->nav_window ), g.NavWindow );
    // Frontmost first, the order FindHoveredWindowEx walks them in.
    for ( int n = g.Windows.Size - 1; n >= 0; n-- )
    {
        const ImGuiWindow *window = g.Windows[n];
        if ( !window->WasActive || window->Hidden )
            continue;
        if ( window->OuterRectClipped.Contains( io.MousePos ) )
        {
            CopyWindowName( out->window_at_pointer, sizeof( out->window_at_pointer ), window );
            break;
        }
    }
}

// View > Reset layout (05-11, PARITY V3/R15): forgets every top-level window's
// remembered position, size and collapse state - in the running windows and in
// the settings the .ini would write - so the next SetNextWindowPos/Size with
// ImGuiCond_FirstUseEver applies again and the panels go back to the layout
// the app gave them. ImGui's own ClearWindowSettings is the call the debug
// tools use for the same thing; it needs the internal header, which is why this
// lives here and not in the Zig.
extern "C" void bk_imgui_reset_window_layout( void )
{
    ImGuiContext *context = ImGui::GetCurrentContext();
    if ( !context )
        return;
    // ClearWindowSettings does not add or remove windows, but copy the names
    // first anyway: it is not documented as safe over a list it may touch.
    ImVector<const char *> names;
    for ( int i = 0; i < context->Windows.Size; ++i )
    {
        const ImGuiWindow *window = context->Windows[i];
        if ( window->Flags & ( ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip ) )
            continue;
        names.push_back( window->Name );
    }
    for ( int i = 0; i < names.Size; ++i )
        ImGui::ClearWindowSettings( names[i] );
    ImGui::ClearIniSettings();
    ImGui::MarkIniSettingsDirty();
}

// ---------------------------------------------------------------------------
// Devanagari: ImGui draws one glyph per code point and cannot shape. A Hindi
// text is stored as PUA characters (one per shaped cluster, CBkClusterTable),
// and this loader gives each PUA character one glyph: the cluster shaped with
// HarfBuzz and rasterised, glyph by glyph, into one bitmap. Half forms take
// the advance of the cluster shaped alone (the S01 README's accepted limit).

using namespace BkStb;

static CBkClusterTable g_clusters;
static std::vector<unsigned char> g_devanagari;
static bool g_tableFullLogged = false;

struct SDevanagariSrc
{
	stbtt_fontinfo info;
	float fScale;
};

struct SPlacedGlyph
{
	int nGlyph;
	float fX, fY;			// the pen position in layout pixels, y down
};

static bool DevanagariSrcInit( ImFontAtlas *, ImFontConfig *src )
{
	const unsigned char *pData = static_cast<const unsigned char *>( src->FontData );
	const int nOffset = stbtt_GetFontOffsetForIndex( pData, src->FontNo );
	SDevanagariSrc *pSrc = IM_NEW( SDevanagariSrc );
	if ( nOffset < 0 || !stbtt_InitFont( &pSrc->info, pData, nOffset ) )
	{
		IM_DELETE( pSrc );
		return false;
	}
	const float fRef = src->DstFont->Sources[0]->SizePixels;
	if ( src->MergeMode && src->SizePixels == 0.0f )
		src->SizePixels = fRef;
	pSrc->fScale = stbtt_ScaleForPixelHeight( &pSrc->info, 1.0f );
	if ( src->MergeMode && src->SizePixels != 0.0f && fRef != 0.0f )
		pSrc->fScale *= src->SizePixels / fRef;
	pSrc->fScale *= src->ExtraSizeScale;
	src->FontLoaderData = pSrc;
	return true;
}

static void DevanagariSrcDestroy( ImFontAtlas *, ImFontConfig *src )
{
	IM_DELETE( static_cast<SDevanagariSrc *>( src->FontLoaderData ) );
	src->FontLoaderData = nullptr;
}

static bool DevanagariContainsGlyph( ImFontAtlas *, ImFontConfig *src, ImWchar cp )
{
	if ( g_clusters.Lookup( cp ) != nullptr )
		return true;
	SDevanagariSrc *pSrc = static_cast<SDevanagariSrc *>( src->FontLoaderData );
	return stbtt_FindGlyphIndex( &pSrc->info, cp ) != 0;
}

static bool DevanagariBakedInit( ImFontAtlas *, ImFontConfig *, ImFontBaked *, void * )
{
	return true;
}

static bool DevanagariLoadGlyph( ImFontAtlas *atlas, ImFontConfig *src, ImFontBaked *baked, void *, ImWchar cp, ImFontGlyph *out_glyph, float *out_advance_x )
{
	SDevanagariSrc *pSrc = static_cast<SDevanagariSrc *>( src->FontLoaderData );
	const float fLayout = pSrc->fScale * baked->Size;
	const float fDensity = src->RasterizerDensity * baked->RasterizerDensity;
	const float fRaster = fLayout * fDensity;

	std::vector<SPlacedGlyph> glyphs;
	float fAdvance = 0.0f;
	const std::u16string *pCluster = g_clusters.Lookup( cp );
	if ( pCluster != nullptr )
	{
		SBkShaped *pShaped = BkShapeClusters( static_cast<const unsigned char *>( src->FontData ), static_cast<size_t>( src->FontDataSize ),
			reinterpret_cast<const unsigned short *>( pCluster->data() ), pCluster->size(), "Deva", "hi", nullptr, 0 );
		if ( pShaped == nullptr )
			return false;
		// HarfBuzz reports font units, the same units stb_truetype scales.
		float fPen = 0.0f;
		for ( size_t i = 0; i < pShaped->nGlyphs; i++ )
		{
			const SBkShapedGlyph &g = pShaped->pGlyphs[i];
			SPlacedGlyph placed = { static_cast<int>( g.nGlyphId ), ( fPen + g.nXOffset ) * fLayout, -g.nYOffset * fLayout };
			glyphs.push_back( placed );
			fPen += g.nXAdvance;
		}
		fAdvance = fPen * fLayout;
		BkShapeFree( pShaped );
	}
	else
	{
		const int nGlyph = stbtt_FindGlyphIndex( &pSrc->info, cp );
		if ( nGlyph == 0 )
			return false;
		int nAdvance, nBearing;
		stbtt_GetGlyphHMetrics( &pSrc->info, nGlyph, &nAdvance, &nBearing );
		SPlacedGlyph placed = { nGlyph, 0.0f, 0.0f };
		glyphs.push_back( placed );
		fAdvance = nAdvance * fLayout;
	}
	if ( out_advance_x != nullptr )
	{
		*out_advance_x = fAdvance;
		return true;
	}

	// The union of the glyph boxes, in raster pixels from the baseline origin.
	struct SBox { int x0, y0, x1, y1; };
	std::vector<SBox> boxes( glyphs.size() );
	int nX0 = 0, nY0 = 0, nX1 = 0, nY1 = 0;
	bool bAny = false;
	for ( size_t i = 0; i < glyphs.size(); i++ )
	{
		SBox &b = boxes[i];
		stbtt_GetGlyphBitmapBox( &pSrc->info, glyphs[i].nGlyph, fRaster, fRaster, &b.x0, &b.y0, &b.x1, &b.y1 );
		const int nDX = static_cast<int>( glyphs[i].fX * fDensity + ( glyphs[i].fX < 0 ? -0.5f : 0.5f ) );
		const int nDY = static_cast<int>( glyphs[i].fY * fDensity + ( glyphs[i].fY < 0 ? -0.5f : 0.5f ) );
		b.x0 += nDX; b.x1 += nDX; b.y0 += nDY; b.y1 += nDY;
		if ( b.x0 == b.x1 || b.y0 == b.y1 )
			continue;
		if ( !bAny ) { nX0 = b.x0; nY0 = b.y0; nX1 = b.x1; nY1 = b.y1; bAny = true; }
		else { nX0 = b.x0 < nX0 ? b.x0 : nX0; nY0 = b.y0 < nY0 ? b.y0 : nY0; nX1 = b.x1 > nX1 ? b.x1 : nX1; nY1 = b.y1 > nY1 ? b.y1 : nY1; }
	}

	out_glyph->Codepoint = cp;
	out_glyph->AdvanceX = fAdvance;
	if ( !bAny )
		return true;

	const int nW = nX1 - nX0, nH = nY1 - nY0;
	std::vector<unsigned char> pixels( static_cast<size_t>( nW ) * nH, 0 ), one;
	for ( size_t i = 0; i < glyphs.size(); i++ )
	{
		const SBox &b = boxes[i];
		const int nGW = b.x1 - b.x0, nGH = b.y1 - b.y0;
		if ( nGW <= 0 || nGH <= 0 )
			continue;
		one.assign( static_cast<size_t>( nGW ) * nGH, 0 );
		stbtt_MakeGlyphBitmap( &pSrc->info, one.data(), nGW, nGH, nGW, fRaster, fRaster, glyphs[i].nGlyph );
		// Marks overlap (a matra over its consonant): the darker pixel wins.
		for ( int y = 0; y < nGH; y++ )
			for ( int x = 0; x < nGW; x++ )
			{
				unsigned char &dst = pixels[static_cast<size_t>( b.y0 - nY0 + y ) * nW + ( b.x0 - nX0 + x )];
				const unsigned char v = one[static_cast<size_t>( y ) * nGW + x];
				if ( v > dst )
					dst = v;
			}
	}

	ImFontAtlasRectId nPack = ImFontAtlasPackAddRect( atlas, nW, nH );
	if ( nPack == ImFontAtlasRectId_Invalid )
		return false;
	ImTextureRect *pRect = ImFontAtlasPackGetRect( atlas, nPack );
	const float fRecip = 1.0f / fDensity;
	const float fOffY = IM_ROUND( baked->Ascent );
	out_glyph->X0 = nX0 * fRecip;
	out_glyph->Y0 = nY0 * fRecip + fOffY;
	out_glyph->X1 = ( nX0 + static_cast<int>( pRect->w ) ) * fRecip;
	out_glyph->Y1 = ( nY0 + static_cast<int>( pRect->h ) ) * fRecip + fOffY;
	out_glyph->Visible = true;
	out_glyph->PackId = nPack;
	ImFontAtlasBakedSetFontGlyphBitmap( atlas, baked, src, out_glyph, pRect, pixels.data(), ImTextureFormat_Alpha8, nW );
	return true;
}

static const ImFontLoader *DevanagariLoader()
{
	static ImFontLoader loader;
	loader.Name = "bk_devanagari";
	loader.FontSrcInit = DevanagariSrcInit;
	loader.FontSrcDestroy = DevanagariSrcDestroy;
	loader.FontSrcContainsGlyph = DevanagariContainsGlyph;
	loader.FontBakedInit = DevanagariBakedInit;
	loader.FontBakedLoadGlyph = DevanagariLoadGlyph;
	return &loader;
}

extern "C" bool bkFontsAddDevanagari( const unsigned char *data, size_t size, float size_pixels )
{
	if ( data == nullptr || size == 0 || ImGui::GetCurrentContext() == nullptr )
		return false;
	// The face stays in memory for the encoder and every later atlas; the
	// same file is installed again on a reload.
	if ( g_devanagari.size() != size )
		g_devanagari.assign( data, data + size );
	ImFontConfig config;
	config.FontData = g_devanagari.data();
	config.FontDataSize = static_cast<int>( g_devanagari.size() );
	config.FontDataOwnedByAtlas = false;
	config.MergeMode = true;
	config.SizePixels = size_pixels;
	config.FontLoader = DevanagariLoader();
	strncpy( config.Name, "Noto Sans Devanagari", sizeof( config.Name ) - 1 );
	return ImGui::GetIO().Fonts->AddFont( &config ) != nullptr;
}

extern "C" int bkFontsEncodeHindi( const char *utf8, char *out, size_t cap )
{
	if ( utf8 == nullptr || out == nullptr || cap == 0 )
		return -1;
	std::string input( utf8 );
	std::string result = input;
	if ( !input.empty() && !g_devanagari.empty() )
	{
		const std::u16string wide = BkUtf8ToUtf16( input );
		SBkShaped *pShaped = BkShapeClusters( g_devanagari.data(), g_devanagari.size(),
			reinterpret_cast<const unsigned short *>( wide.data() ), wide.size(), "Deva", "hi", nullptr, 0 );
		if ( pShaped != nullptr )
		{
			std::u16string encoded;
			std::string error;
			if ( g_clusters.Encode( reinterpret_cast<const unsigned short *>( wide.data() ), *pShaped, &encoded, &error ) )
				result = BkUtf16ToUtf8( encoded );
			else if ( !g_tableFullLogged )
			{
				g_tableFullLogged = true;
				fprintf( stderr, "l10n: cluster table full\n" );
			}
			BkShapeFree( pShaped );
		}
	}
	if ( result.size() + 1 > cap )
		return -1;
	memcpy( out, result.c_str(), result.size() + 1 );
	return static_cast<int>( result.size() );
}

extern "C" size_t bkFontsClusterCount( void )
{
	return g_clusters.Size();
}
