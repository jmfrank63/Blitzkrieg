#include "StdAfx.h"

#include "TextureGpu.h"

#include <limits>

namespace
{
    int BytesPerPixel( EGFXPixelFormat format )
    {
        switch ( format )
        {
            case GFXPF_ARGB8888: return 4;
            case GFXPF_ARGB4444:
            case GFXPF_ARGB1555:
            case GFXPF_ARGB0565:
            case GFXPF_UV88:
            case GFXPF_LUV655: return 2;
            case GFXPF_DP3: return 4;
            default: return 0;
        }
    }

    int RowPitch( EGFXPixelFormat format, int width )
    {
        if ( format == GFXPF_DXT1 ) return ((width + 3) / 4) * 8;
        if ( format >= GFXPF_DXT2 && format <= GFXPF_DXT5 ) return ((width + 3) / 4) * 16;
        const int bpp = BytesPerPixel( format );
        return bpp > 0 ? width * bpp : 0;
    }
}

TextureGpu::TextureGpu( GraphicsEngineGpu *owner, int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage )
    : owner_( owner ), width_( width ), height_( height ), mips_( mips ), format_( format ), usage_( usage )
{
    if ( !owner_ || !owner_->CreateTextureHandle( width_, height_, mips_, format_, usage_, &handle_ ) )
        handle_ = 0;
}

TextureGpu::~TextureGpu()
{
    if ( handle_ && owner_ ) owner_->DestroyTextureHandle( handle_ );
    handle_ = 0;
}

void STDCALL TextureGpu::SwapData( ISharedResource * ) {}

bool STDCALL TextureGpu::Lock( int level, SSurfaceLockInfo *lock )
{
    if ( !lock || locked_ || level < 0 || level >= mips_ || !handle_ ) return false;
    const int width = GetSizeX( level );
    const int height = GetSizeY( level );
    locked_pitch_ = RowPitch( format_, width );
    if ( locked_pitch_ <= 0 || height <= 0 ) return false;
    const size_t bytes = static_cast<size_t>( locked_pitch_ ) * static_cast<size_t>( height );
    if ( bytes > (static_cast<size_t>( -1 ) / 2) ) return false;
    try { lock_bytes_.assign( bytes, 0 ); } catch ( ... ) { return false; }
    locked_ = true;
    locked_level_ = level;
    lock->nPitch = locked_pitch_;
    lock->pData = lock_bytes_.data();
    return true;
}

bool STDCALL TextureGpu::Unlock( int level )
{
    if ( !locked_ || level != locked_level_ || !owner_ ) return false;
    const bool uploaded = owner_->UploadTexture( handle_, level, lock_bytes_.data(), lock_bytes_.size(), locked_pitch_ );
    lock_bytes_.clear();
    locked_ = false;
    locked_level_ = -1;
    locked_pitch_ = 0;
    return uploaded;
}

int STDCALL TextureGpu::GetSizeX( int level ) const
{
    if ( level < 0 || level >= mips_ ) return 0;
    return width_ >> level > 0 ? width_ >> level : 1;
}

int STDCALL TextureGpu::GetSizeY( int level ) const
{
    if ( level < 0 || level >= mips_ ) return 0;
    return height_ >> level > 0 ? height_ >> level : 1;
}

RenderTargetGpu::RenderTargetGpu( GraphicsEngineGpu *owner, int width, int height, EGFXPixelFormat format )
    : owner_( owner ), width_( width ), height_( height ), format_( format )
{
    if ( !owner_ || !owner_->CreateRenderTargetHandle( width_, height_, format_, &handle_ ) )
        handle_ = 0;
}

RenderTargetGpu::~RenderTargetGpu()
{
    if ( handle_ && owner_ ) owner_->DestroyTextureHandle( handle_ );
    handle_ = 0;
}

void STDCALL RenderTargetGpu::SwapData( ISharedResource * ) {}
