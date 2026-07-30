#ifndef BLITZKRIEG_TEXTURE_GPU_H
#define BLITZKRIEG_TEXTURE_GPU_H

#include "GraphicsEngineGpu.h"

#include <string>
#include <vector>

class TextureGpu final : public IGFXTexture
{
public:
    TextureGpu( GraphicsEngineGpu *owner, int width, int height, int mips, EGFXPixelFormat format, EGFXDynamic usage );
    ~TextureGpu();
    static IRefCount * STDCALL CreateNewClassInstanceInternal() { return nullptr; }
    void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ += nRef; }
    void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ -= nRef; if ( ref_count_ == 0 ) delete this; }
    bool STDCALL IsValid() const override { return ref_count_ >= 0 && handle_ != 0; }
    void STDCALL SwapData( ISharedResource *pResource ) override;
    int STDCALL GetRefCounter() const override { return ref_count_; }
    const char * STDCALL GetSharedResourceName() const override { return name_.c_str(); }
    void STDCALL SetSharedResourceName( const std::string &name ) override { name_ = name; }
    bool STDCALL Load( bool bPreLoad = false ) override { (void)bPreLoad; return true; }
    void STDCALL ClearInternalContainer() override {}
    bool STDCALL Lock( int nLevel, SSurfaceLockInfo *pLockInfo ) override;
    bool STDCALL Unlock( int nLevel ) override;
    bool STDCALL AddDirtyRect( const RECT *pRect ) override { (void)pRect; dirty_ = true; return true; }
    int STDCALL GetSizeX( int nLevel ) const override;
    int STDCALL GetSizeY( int nLevel ) const override;
    EGFXPixelFormat STDCALL GetFormat() const override { return format_; }

    GfxGpuHandle Handle() const { return handle_; }

private:
    GraphicsEngineGpu *owner_;
    GfxGpuHandle handle_ = 0;
    int width_;
    int height_;
    int mips_;
    EGFXPixelFormat format_;
    EGFXDynamic usage_;
    int ref_count_ = 0;
    bool locked_ = false;
    bool dirty_ = false;
    int locked_level_ = -1;
    int locked_pitch_ = 0;
    std::vector<unsigned char> lock_bytes_;
    std::string name_;
};

class RenderTargetGpu final : public IGFXRTexture
{
public:
    RenderTargetGpu( GraphicsEngineGpu *owner, int width, int height, EGFXPixelFormat format );
    ~RenderTargetGpu();
    void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ += nRef; }
    void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ -= nRef; if ( ref_count_ == 0 ) delete this; }
    bool STDCALL IsValid() const override { return ref_count_ >= 0 && handle_ != 0; }
    void STDCALL SwapData( ISharedResource *pResource ) override;
    int STDCALL GetRefCounter() const override { return ref_count_; }
    const char * STDCALL GetSharedResourceName() const override { return name_.c_str(); }
    void STDCALL SetSharedResourceName( const std::string &name ) override { name_ = name; }
    bool STDCALL Load( bool bPreLoad = false ) override { (void)bPreLoad; return true; }
    void STDCALL ClearInternalContainer() override {}
    int STDCALL GetSizeX( int nLevel ) const override { return nLevel == 0 ? width_ : 0; }
    int STDCALL GetSizeY( int nLevel ) const override { return nLevel == 0 ? height_ : 0; }
    EGFXPixelFormat STDCALL GetFormat() const override { return format_; }
    GfxGpuHandle Handle() const { return handle_; }

private:
    GraphicsEngineGpu *owner_;
    GfxGpuHandle handle_ = 0;
    int width_;
    int height_;
    EGFXPixelFormat format_;
    int ref_count_ = 0;
    std::string name_;
};

#endif
