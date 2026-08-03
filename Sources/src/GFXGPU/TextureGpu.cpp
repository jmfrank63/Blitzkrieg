#include "StdAfx.h"

#include "TextureGpu.h"

#include "..//Formats//fmtTexture.h"
#include "..//Image//Image.h"
#include "..//StreamIO//StreamIO.h"

#include <limits>
#include <climits>

namespace
{
    EGFXPixelFormat GfxFormatFromDDS( const SDDSPixelFormat &format )
    {
        if ( format.dwFlags & DDS_FOURCC )
        {
            switch ( format.dwFourCC )
            {
                case MAKEFOURCC('D', 'X', 'T', '1'): return GFXPF_DXT1;
                case MAKEFOURCC('D', 'X', 'T', '2'): return GFXPF_DXT2;
                case MAKEFOURCC('D', 'X', 'T', '3'): return GFXPF_DXT3;
                case MAKEFOURCC('D', 'X', 'T', '4'): return GFXPF_DXT4;
                case MAKEFOURCC('D', 'X', 'T', '5'): return GFXPF_DXT5;
            }
        }
        else if ( (format.dwFlags & DDS_ARGB) == DDS_ARGB )
        {
            if ( format.dwRGBBitCount == 32 ) return GFXPF_ARGB8888;
            if ( format.dwRGBBitCount == 16 ) return format.dwRBitMask == 0x00007c00 ? GFXPF_ARGB1555 : GFXPF_ARGB4444;
        }
        else if ( (format.dwFlags & DDS_RGB) == DDS_RGB && format.dwRBitMask == 0x0000f800 ) return GFXPF_ARGB0565;
        return GFXPF_UNKNOWN;
    }

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

bool STDCALL TextureGpu::Load( bool bPreLoad )
{
    if ( handle_ ) return true;
    ISingleton *globals = GetSingletonGlobal();
    if ( !globals || name_.empty() ) return false;
    IDataStorage *storage = GetSingleton<IDataStorage>( globals );
    if ( !storage ) return false;
    CPtr<IDataStream> stream = storage->OpenStream( name_.c_str(), STREAM_ACCESS_READ );
    if ( !stream ) return false;

    DWORD signature = 0;
    SDDSHeader header;
    if ( stream->Read( &signature, sizeof(signature) ) != sizeof(signature) || signature != SDDSHeader::SIGNATURE ) return false;
    if ( stream->Read( &header, sizeof(header) ) != sizeof(header) ) return false;
    const EGFXPixelFormat format = GfxFormatFromDDS( header.ddspf );
    const int mip_count = (header.dwHeaderFlags & DDS_HEADER_FLAGS_MIPMAP) && header.dwMipMapCount != 0 ? static_cast<int>(header.dwMipMapCount) : 1;
    if ( format == GFXPF_UNKNOWN || header.dwWidth == 0 || header.dwHeight == 0 || mip_count <= 0 || mip_count > 16 ) return false;
    width_ = static_cast<int>( header.dwWidth );
    height_ = static_cast<int>( header.dwHeight );
    mips_ = mip_count;
    format_ = format;
    if ( bPreLoad ) return true;

    // SDL_GPU's portable baseline accepts RGBA8/BGRA8 sampled textures. The
    // game data is predominantly DXT, so decode those legacy formats through
    // the renderer-neutral image service before uploading them.
    if ( format_ != GFXPF_ARGB8888 )
    {
        IImageProcessor *processor = GetSingleton<IImageProcessor>( globals );
        if ( !processor ) return false;
        CPtr<IDataStream> dds_stream = storage->OpenStream( name_.c_str(), STREAM_ACCESS_READ );
        if ( !dds_stream ) return false;
        CPtr<IDDSImage> dds = processor->LoadDDSImage( dds_stream );
        CPtr<IImage> image = dds ? processor->Decompress( dds ) : nullptr;
        if ( !image ) return false;
        width_ = image->GetSizeX();
        height_ = image->GetSizeY();
        mips_ = 1;
        format_ = GFXPF_ARGB8888;
        if ( !owner_ || !owner_->CreateTextureHandle( width_, height_, mips_, format_, usage_, &handle_ ) ) return false;
        SSurfaceLockInfo lock{};
        if ( !Lock( 0, &lock ) ) return false;
        for ( int row = 0; row < height_; ++row )
            std::memcpy( static_cast<unsigned char *>( lock.pData ) + static_cast<size_t>( row ) * lock.nPitch, image->GetLine( row ), static_cast<size_t>( width_ ) * sizeof( SColor ) );
        return Unlock( 0 );
    }

    if ( !owner_ || !owner_->CreateTextureHandle( width_, height_, mips_, format_, usage_, &handle_ ) ) return false;

    for ( int level = 0; level < mips_; ++level )
    {
        SSurfaceLockInfo lock{};
        if ( !Lock( level, &lock ) ) return false;
        const int rows = (format_ >= GFXPF_DXT1 && format_ <= GFXPF_DXT5) ? ((GetSizeY( level ) + 3) / 4) : GetSizeY( level);
        const size_t bytes = static_cast<size_t>( lock.nPitch ) * static_cast<size_t>( rows );
        const bool read_ok = bytes <= static_cast<size_t>( INT_MAX ) && stream->Read( lock.pData, static_cast<int>( bytes ) ) == static_cast<int>( bytes );
        if ( !read_ok || !Unlock( level ) ) return false;
    }
    return true;
}


TextureGpu::TextureGpu( GraphicsEngineGpu *owner, int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage )
    : owner_( owner ), width_( width ), height_( height ), mips_( mips ), format_( format ), usage_( usage )
{
    if ( !owner_ || width_ <= 0 || height_ <= 0 || mips_ <= 0 || format_ == GFXPF_UNKNOWN || !owner_->CreateTextureHandle( width_, height_, mips_, format_, usage_, &handle_ ) )
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
