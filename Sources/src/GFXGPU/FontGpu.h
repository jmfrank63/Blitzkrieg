#ifndef BLITZKRIEG_FONT_GPU_H
#define BLITZKRIEG_FONT_GPU_H

#include "..//GFX//GFX.H"
#include <utility>
#include <vector>
#include "..//Formats//fmtFont.h"
#include "..//GFX//GFXHelper.h"

#include <string>
#include <vector>

class FontGpu final : public IGFXFont
{
public:
    FontGpu();
    static IRefCount * STDCALL CreateNewClassInstanceInternal() { return nullptr; }
    void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ += nRef; }
    void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) override { (void)nMask; ref_count_ -= nRef; if ( ref_count_ == 0 ) delete this; }
    bool STDCALL IsValid() const override { return ref_count_ >= 0 && texture_ != nullptr; }
    void STDCALL SwapData( ISharedResource *resource ) override;
    int STDCALL GetRefCounter() const override { return ref_count_; }
    const char * STDCALL GetSharedResourceName() const override { return name_.c_str(); }
    void STDCALL SetSharedResourceName( const std::string &name ) override { name_ = name; }
    bool STDCALL Load( bool bPreLoad = false ) override;
    void STDCALL ClearInternalContainer() override {}
    int STDCALL GetHeight() const override { return format_.metrics.nHeight; }
    int STDCALL GetLineSpace() const override { return format_.GetLineSpace(); }
    int STDCALL GetAscent() const override { return format_.metrics.nAscent; }
    int STDCALL GetDescent() const override { return format_.metrics.nDescent; }
    const SFontFormat & STDCALL GetFormat() const override { return format_; }
    int STDCALL GetTextWidth( const char *text, int count = 2000000000 ) const override;
    int STDCALL GetTextWidth( const WORD *text, int count = 2000000000 ) const override;
    float TextWidthFloat( const WORD *text, int count = 2000000000 ) const;

    IGFXTexture *Texture() const { return texture_; }
    bool AppendGeometry( const wchar_t *text, float x, float y, float scale, DWORD color,
        std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;

private:
    ~FontGpu();
    template <typename T> float TextWidth( const T *text, int count ) const;
    int ref_count_ = 0;
    std::string name_;
    SFontFormat format_{};
    IGFXTexture *texture_ = nullptr;
};

// Shared greedy line breaking, so the row height reported by
// TextGpu::GetNumLines and the lines actually emitted by
// GraphicsEngineGpu::DrawText can never disagree. Returns [begin,end)
// ranges over the UTF-16 string; carriage returns are skipped.
void WrapTextLines( const FontGpu *font, const WORD *text, float scale, float wrap_width,
    std::vector<std::pair<size_t, size_t> > &lines );

class IGFXTextGpuFontProvider
{
public:
    virtual ~IGFXTextGpuFontProvider() = default;
    virtual IGFXFont *Font() const = 0;
    virtual DWORD Color() const = 0;
    virtual float Scale() const = 0;
};

#endif
