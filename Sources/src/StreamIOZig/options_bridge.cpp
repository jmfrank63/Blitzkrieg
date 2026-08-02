#include "../Platform/DynamicLibrary.h"
#include "../Platform/LegacyVariant.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#define BK_STDCALL __stdcall
#define BK_CDECL __cdecl
#define BK_EXPORT extern "C" __declspec(dllexport)
#else
#define BK_STDCALL
#define BK_CDECL
#define BK_EXPORT extern "C"
#endif

struct IRefCount {
    virtual void BK_STDCALL AddRef(int = 1, int = 0x7fffffff) = 0;
    virtual void BK_STDCALL Release(int = 1, int = 0x7fffffff) = 0;
    virtual bool BK_STDCALL IsValid() const = 0;
    virtual IRefCount *BK_STDCALL QI(int) { return nullptr; }
    virtual int BK_STDCALL operator&(void *) { return 0; }
};
struct IDataTree;
struct IVarIterator : public IRefCount {
    virtual bool BK_STDCALL Next() = 0;
    virtual bool BK_STDCALL IsEnd() const = 0;
    virtual bool BK_STDCALL Get(variant_t *, variant_t *) const = 0;
};
struct OptionDesc {
    std::string division, name;
    int data_type = 0, editor_type = 0;
    unsigned long flags = 0;
    variant_t default_value;
    bool instant_apply = false;
};
struct OptionDropValue { std::string program_name; };
struct IOptionSystemIterator : public IVarIterator {
    virtual const OptionDesc *BK_STDCALL GetDesc() const = 0;
    virtual const std::vector<OptionDropValue> &BK_STDCALL GetDropValues() const = 0;
};
struct IOptionSystem : public IRefCount {
    virtual bool BK_STDCALL Get(const std::string &, variant_t *) const = 0;
    virtual bool BK_STDCALL Set(const std::string &, const variant_t &) = 0;
    virtual bool BK_STDCALL Remove(const std::string &) = 0;
    virtual bool BK_STDCALL RemoveByMatch(const std::string &) = 0;
    virtual bool BK_STDCALL ChangeSerialize(const std::string &, bool) = 0;
    virtual bool BK_STDCALL IsChanged() const = 0;
    virtual const OptionDesc *BK_STDCALL GetDesc(const std::string &) const = 0;
    virtual const std::vector<OptionDropValue> &BK_STDCALL GetDropValues(const std::string &) const = 0;
    virtual IOptionSystemIterator *BK_STDCALL CreateIterator(unsigned long = 0xffffffff) = 0;
    virtual bool BK_STDCALL SerializeConfig(IDataTree *) = 0;
    virtual void BK_STDCALL Init() = 0;
    virtual void BK_STDCALL Repair(IDataTree *, bool) = 0;
};
struct IConsoleBuffer : public IRefCount {
    virtual bool BK_STDCALL Configure(const char *) = 0;
    virtual void BK_STDCALL Write(int, const unsigned short *, unsigned long, bool) = 0;
    virtual void BK_STDCALL WriteASCII(int, const char *, unsigned long, bool) = 0;
    virtual const unsigned short *BK_STDCALL Read(int, unsigned long *) = 0;
    virtual const char *BK_STDCALL ReadASCII(int, unsigned long *) = 0;
    virtual bool BK_STDCALL DumpLog(int) = 0;
};

struct CoreApi {
    void *(BK_CDECL *create)() = nullptr;
    void (BK_CDECL *destroy)(void *) = nullptr;
    int (BK_CDECL *count)(void *) = nullptr;
    const char *(BK_CDECL *name_at)(void *, int) = nullptr;
    const char *(BK_CDECL *value)(void *, const char *, unsigned short *) = nullptr;
    bool (BK_CDECL *set)(void *, const char *, const char *, unsigned short) = nullptr;
    bool (BK_CDECL *remove)(void *, const char *) = nullptr;
    void (BK_CDECL *remove_prefix)(void *, const char *) = nullptr;
    bool (BK_CDECL *changed)(void *) = nullptr;
    bool (BK_CDECL *metadata)(void *, int, int *, unsigned long *, int *, bool *, const char **, const char **, const char **, unsigned short *) = nullptr;
    int (BK_STDCALL *load_tree)(void *, void *, bool) = nullptr;
    int (BK_STDCALL *serialize_tree)(void *, void *, bool) = nullptr;
    void *(BK_CDECL *console_create)() = nullptr;
    void (BK_CDECL *console_destroy)(void *) = nullptr;
    bool (BK_CDECL *console_configure)(void *, const char *) = nullptr;
    void (BK_CDECL *console_write)(void *, int, const unsigned short *, unsigned long, bool) = nullptr;
    void (BK_CDECL *console_write_ascii)(void *, int, const char *, unsigned long, bool) = nullptr;
    const unsigned short *(BK_CDECL *console_read)(void *, int, unsigned long *) = nullptr;
    const char *(BK_CDECL *console_read_ascii)(void *, int, unsigned long *) = nullptr;
};
static CoreApi api;
static NPlatform::DynamicLibrary &CoreModule() { static NPlatform::DynamicLibrary module; return module; }

template <class T> static T Resolve(const char *name) { return reinterpret_cast<T>(CoreModule().GetFunction(name)); }
static bool ResolveCore() {
    if (!CoreModule().IsLoaded() && !CoreModule().Load("StreamIO.dll")) return false;
    api.create = Resolve<decltype(api.create)>("bk_options_create");
    api.destroy = Resolve<decltype(api.destroy)>("bk_options_destroy");
    api.count = Resolve<decltype(api.count)>("bk_options_count");
    api.name_at = Resolve<decltype(api.name_at)>("bk_options_name_at");
    api.value = Resolve<decltype(api.value)>("bk_options_value");
    api.set = Resolve<decltype(api.set)>("bk_options_set");
    api.remove = Resolve<decltype(api.remove)>("bk_options_remove");
    api.remove_prefix = Resolve<decltype(api.remove_prefix)>("bk_options_remove_prefix");
    api.changed = Resolve<decltype(api.changed)>("bk_options_changed");
    api.metadata = Resolve<decltype(api.metadata)>("bk_options_metadata");
    api.load_tree = Resolve<decltype(api.load_tree)>("bk_options_load_legacy_tree");
    api.serialize_tree = Resolve<decltype(api.serialize_tree)>("bk_options_serialize_legacy_tree");
    api.console_create = Resolve<decltype(api.console_create)>("bk_console_create");
    api.console_destroy = Resolve<decltype(api.console_destroy)>("bk_console_destroy");
    api.console_configure = Resolve<decltype(api.console_configure)>("bk_console_configure");
    api.console_write = Resolve<decltype(api.console_write)>("bk_console_write");
    api.console_write_ascii = Resolve<decltype(api.console_write_ascii)>("bk_console_write_ascii");
    api.console_read = Resolve<decltype(api.console_read)>("bk_console_read");
    api.console_read_ascii = Resolve<decltype(api.console_read_ascii)>("bk_console_read_ascii");
    return api.create && api.destroy && api.count && api.name_at && api.value && api.set && api.remove && api.remove_prefix && api.changed && api.metadata && api.load_tree && api.serialize_tree && api.console_create && api.console_destroy && api.console_configure && api.console_write && api.console_write_ascii && api.console_read && api.console_read_ascii;
}

static bool AssignVariant(variant_t *out, unsigned short type, const char *text) {
    if (!out || !text) return false;
    switch (type) {
        case VT_UI1: *out = variant_t(static_cast<unsigned char>(std::strtoul(text, nullptr, 10))); break;
        case VT_I2: *out = variant_t(static_cast<short>(std::strtol(text, nullptr, 10))); break;
        case VT_I4: *out = variant_t(static_cast<long>(std::strtol(text, nullptr, 10))); break;
        case VT_R4: *out = variant_t(static_cast<float>(std::strtod(text, nullptr))); break;
        case VT_R8: *out = variant_t(std::strtod(text, nullptr)); break;
        case VT_BOOL: *out = variant_t(std::strtol(text, nullptr, 10) != 0); break;
        case VT_BSTR: default: *out = variant_t(text); break;
    }
    return true;
}
static std::string VariantText(const variant_t &value) {
    char buffer[96] = {};
    switch (value.vt) {
        case VT_BSTR:
#if defined(_WIN32) || defined(_WIN64)
            return value.bstrVal ? std::string(_bstr_t(value.bstrVal)) : std::string();
#else
            return value.bstrVal ? std::string(bstr_t(value.bstrVal)) : std::string();
#endif
        case VT_UI1: std::snprintf(buffer, sizeof(buffer), "%u", unsigned(value.bVal)); break;
        case VT_I2: std::snprintf(buffer, sizeof(buffer), "%d", int(value.iVal)); break;
        case VT_I4: std::snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(value.lVal)); break;
        case VT_R4: std::snprintf(buffer, sizeof(buffer), "%.9g", double(value.fltVal)); break;
        case VT_R8: std::snprintf(buffer, sizeof(buffer), "%.17g", value.dblVal); break;
        case VT_BOOL: std::snprintf(buffer, sizeof(buffer), "%d", value.boolVal != VARIANT_FALSE); break;
        default: return {};
    }
    return buffer;
}
static char FoldAscii(char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value; }
static bool EqualAsciiIgnoreCase(const char *left, const char *right) {
    if (!left || !right) return left == right;
    while (*left && *right && FoldAscii(*left) == FoldAscii(*right)) { ++left; ++right; }
    return *left == *right;
}

static void FillMonitors(std::vector<OptionDropValue> *drops) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (displays) {
        int ordinal = 1;
        for (int i = 0; i < count; ++i) {
            const SDL_DisplayID id = displays[i];
            const char *name = SDL_GetDisplayName(id);
            if (i == 0) drops->push_back({"Primary"});
            else { char text[32]; std::snprintf(text, sizeof(text), "Monitor%d", ordinal++); drops->push_back({text}); }
            (void)name;
        }
        SDL_free(displays);
    }
    if (drops->empty()) drops->push_back({"Primary"});
}
static void FillVideoModes(std::vector<OptionDropValue> *drops) {
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    if (!displays) return;
    for (int d = 0; d < display_count; ++d) {
        int mode_count = 0;
        SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(displays[d], &mode_count);
        if (!modes) continue;
        for (int i = 0; i < mode_count; ++i) {
            const SDL_DisplayMode *mode = modes[i];
            if (!mode || mode->w <= 0 || mode->h <= 0) continue;
            char text[64]; std::snprintf(text, sizeof(text), "%dx%dx32", mode->w, mode->h);
            bool exists = false;
            for (const OptionDropValue &drop : *drops) if (drop.program_name == text) { exists = true; break; }
            if (!exists) drops->push_back({text});
        }
        SDL_free(modes);
    }
    SDL_free(displays);
}

class OptionSystem;
class OptionIterator final : public IOptionSystemIterator {
    OptionSystem *owner_; unsigned long mask_; int index_ = 0, refs_ = 0; void Advance();
public:
    OptionIterator(OptionSystem *owner, unsigned long mask) : owner_(owner), mask_(mask) { Advance(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { if ((refs_ -= count) <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return owner_ != nullptr; }
    bool BK_STDCALL Next() override; bool BK_STDCALL IsEnd() const override;
    bool BK_STDCALL Get(variant_t *, variant_t *) const override;
    const OptionDesc *BK_STDCALL GetDesc() const override;
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues() const override;
};
using ApplyOptionActionFunc = void (*)(const char *, const char *, const char *);
static ApplyOptionActionFunc ResolveApplyOptionAction() { return Resolve<ApplyOptionActionFunc>("bk_bridge_apply_option_action"); }

class OptionSystem final : public IOptionSystem {
    void *state_; int refs_ = 0; mutable OptionDesc desc_; mutable std::vector<OptionDropValue> drops_;
    void ApplyAction(const std::string &name) const {
        const int index = Find(name); if (index < 0) return;
        const char *action = nullptr; if (!Metadata(index, nullptr, nullptr, nullptr, nullptr, &action, nullptr, nullptr, nullptr) || !action || !*action) return;
        ApplyOptionActionFunc apply = ResolveApplyOptionAction(); if (!apply) return;
        unsigned short type = 0; const char *text = api.value(state_, name.c_str(), &type); apply(action, name.c_str(), text ? text : "");
    }
public:
    OptionSystem() : state_(api.create()) {} ~OptionSystem() { api.destroy(state_); }
    int Count() const { return api.count(state_); } const char *NameAt(int index) const { return api.name_at(state_, index); }
    int Find(const std::string &name) const { for (int i = 0; i < Count(); ++i) if (EqualAsciiIgnoreCase(NameAt(i), name.c_str())) return i; return -1; }
    bool Metadata(int index, int *editor, unsigned long *flags, int *order, bool *instant, const char **action, const char **fill, const char **fallback, unsigned short *type) const { return api.metadata(state_, index, editor, flags, order, instant, action, fill, fallback, type); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { if ((refs_ -= count) <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return state_ != nullptr; }
    bool BK_STDCALL Get(const std::string &name, variant_t *value) const override { unsigned short type = VT_EMPTY; const char *text = api.value(state_, name.c_str(), &type); return text && AssignVariant(value, type, text); }
    bool BK_STDCALL Set(const std::string &name, const variant_t &value) override { const std::string text = VariantText(value); const bool ok = api.set(state_, name.c_str(), text.c_str(), value.vt); if (ok) ApplyAction(name); return ok; }
    bool BK_STDCALL Remove(const std::string &name) override { return api.remove(state_, name.c_str()); }
    bool BK_STDCALL RemoveByMatch(const std::string &prefix) override { api.remove_prefix(state_, prefix.c_str()); return true; }
    bool BK_STDCALL ChangeSerialize(const std::string &, bool) override { return true; }
    bool BK_STDCALL IsChanged() const override { return api.changed(state_); }
    const OptionDesc *BK_STDCALL GetDesc(const std::string &name) const override {
        const int index = Find(name); if (index < 0) return nullptr; int editor = 0, order = 0; unsigned long flags = 0; bool instant = false; unsigned short type = 0; const char *fallback = nullptr;
        if (!Metadata(index, &editor, &flags, &order, &instant, nullptr, nullptr, &fallback, &type)) return nullptr;
        desc_.name = name; desc_.division = name.substr(0, name.find('.')); desc_.data_type = type; desc_.editor_type = editor; desc_.flags = flags; desc_.instant_apply = instant; AssignVariant(&desc_.default_value, type, fallback ? fallback : ""); return &desc_;
    }
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues(const std::string &name) const override {
        drops_.clear(); const int index = Find(name); const char *fill = nullptr; if (index >= 0) Metadata(index, nullptr, nullptr, nullptr, nullptr, nullptr, &fill, nullptr, nullptr);
        const char *values[5] = {}; int count = 0;
        if (fill && std::strcmp(fill, "GetOnOff") == 0) { values[0] = "ON"; values[1] = "OFF"; count = 2; }
        else if (fill && std::strcmp(fill, "GetDifficulty") == 0) { values[0] = "Easy"; values[1] = "Normal"; values[2] = "Hard"; values[3] = "Ironman"; count = 4; }
        else if (fill && std::strcmp(fill, "GetGameSpeed") == 0) { values[0] = "VerySlow"; values[1] = "Slow"; values[2] = "Normal"; values[3] = "Fast"; values[4] = "VeryFast"; count = 5; }
        else if (fill && std::strcmp(fill, "GetVideoModes") == 0) FillVideoModes(&drops_);
        else if (fill && std::strcmp(fill, "GetMonitors") == 0) FillMonitors(&drops_);
        else if (fill && std::strcmp(fill, "GetTextureQuality") == 0) { values[0] = "Low"; values[1] = "Compressed"; values[2] = "High"; count = 3; }
        for (int i = 0; i < count; ++i) drops_.push_back({values[i]}); return drops_;
    }
    IOptionSystemIterator *BK_STDCALL CreateIterator(unsigned long mask) override { return new OptionIterator(this, mask); }
    bool BK_STDCALL SerializeConfig(IDataTree *tree) override { return api.serialize_tree(state_, tree, false) >= 0; }
    void BK_STDCALL Init() override { for (int i = 0; i < Count(); ++i) if (NameAt(i)) ApplyAction(NameAt(i)); }
    void BK_STDCALL Repair(IDataTree *tree, bool to_default) override { api.load_tree(state_, tree, !to_default); }
};
void OptionIterator::Advance() { while (owner_ && index_ < owner_->Count()) { unsigned long flags = 0; owner_->Metadata(index_, nullptr, &flags, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr); if (mask_ == 0xffffffff || (flags & mask_)) break; ++index_; } }
bool BK_STDCALL OptionIterator::Next() { if (!IsEnd()) ++index_; Advance(); return !IsEnd(); }
bool BK_STDCALL OptionIterator::IsEnd() const { return !owner_ || index_ >= owner_->Count(); }
bool BK_STDCALL OptionIterator::Get(variant_t *name, variant_t *value) const { const char *key = IsEnd() ? nullptr : owner_->NameAt(index_); return key && AssignVariant(name, VT_BSTR, key) && owner_->Get(key, value); }
const OptionDesc *BK_STDCALL OptionIterator::GetDesc() const { const char *key = IsEnd() ? nullptr : owner_->NameAt(index_); return key ? owner_->GetDesc(key) : nullptr; }
const std::vector<OptionDropValue> &BK_STDCALL OptionIterator::GetDropValues() const { static const std::vector<OptionDropValue> empty; const char *key = IsEnd() ? nullptr : owner_->NameAt(index_); return key ? owner_->GetDropValues(key) : empty; }

class ConsoleBuffer final : public IConsoleBuffer {
    void *state_; int refs_ = 0;
public:
    ConsoleBuffer() : state_(api.console_create()) {} ~ConsoleBuffer() { api.console_destroy(state_); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { if ((refs_ -= count) <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return state_ != nullptr; }
    bool BK_STDCALL Configure(const char *config) override { return config && api.console_configure(state_, config); }
    void BK_STDCALL Write(int channel, const unsigned short *text, unsigned long color, bool backup) override { if (text) api.console_write(state_, channel, text, color, backup); }
    void BK_STDCALL WriteASCII(int channel, const char *text, unsigned long color, bool backup) override { if (text) api.console_write_ascii(state_, channel, text, color, backup); }
    const unsigned short *BK_STDCALL Read(int channel, unsigned long *color) override { return api.console_read(state_, channel, color); }
    const char *BK_STDCALL ReadASCII(int channel, unsigned long *color) override { return api.console_read_ascii(state_, channel, color); }
    bool BK_STDCALL DumpLog(int) override { return true; }
};

BK_EXPORT void *bk_option_bridge_create() { if (!ResolveCore()) return nullptr; return new OptionSystem(); }
BK_EXPORT void *bk_console_bridge_create() { if (!ResolveCore()) return nullptr; return new ConsoleBuffer(); }
BK_EXPORT const void *GetModuleDescriptor() { return nullptr; }
