#ifndef __GFXPLATFORM_H__
#define __GFXPLATFORM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// Opaque native window handle passed across the renderer boundary. Backend
// implementations interpret value according to their platform; the public
// GFX contract must not expose a platform window type.
struct GFXNativeWindow
{
	// Borrowed, non-owning native window value. The renderer never releases it.
	void *value;

	// Transitional compatibility for legacy callers. Platform code should
	// construct this boundary value explicitly once it is migrated.
	GFXNativeWindow( void *nativeWindow ) : value( nativeWindow ) {}
};

static_assert( sizeof( GFXNativeWindow ) == sizeof( void * ),
	"GFXNativeWindow must remain a single opaque native handle" );

#endif // __GFXPLATFORM_H__
