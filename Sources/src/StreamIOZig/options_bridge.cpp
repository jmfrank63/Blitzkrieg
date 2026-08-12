#include "../Platform/DynamicLibrary.h"
#include "../Platform/LegacyVariant.h"
#include "../Platform/Paths.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
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
// Must match IConsoleBuffer in StreamIO/Globals.h exactly. It carries wchar_t,
// which is 32 bits off Windows while the Zig core stores UTF-16, so the bridge
// converts rather than declaring the narrower type: the vtable slots line up
// either way, so a mismatch is silent. Declaring unsigned short here made a
// write stop at the first character (its high half reads as a NUL) and a read
// run off the end of the buffer for the second one, which is why the top-left
// messages came out as "GG", "OO" and "RR" plus a garbage glyph.
struct IConsoleBuffer : public IRefCount {
    virtual bool BK_STDCALL Configure(const char *) = 0;
    virtual void BK_STDCALL Write(int, const wchar_t *, unsigned long, bool) = 0;
    virtual void BK_STDCALL WriteASCII(int, const char *, unsigned long, bool) = 0;
    virtual const wchar_t *BK_STDCALL Read(int, unsigned long *) = 0;
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
// The bridge resolves callbacks from StreamIO itself.  Keeping this handle
// process-lifetime avoids a shutdown-order race where the bridge's static
// destructor dlcloses StreamIO after StreamIO has already begun teardown.
static NPlatform::DynamicLibrary &CoreModule() {
    static NPlatform::DynamicLibrary *module = new NPlatform::DynamicLibrary();
    return *module;
}

template <class T> static T Resolve(const char *name) { return reinterpret_cast<T>(CoreModule().GetFunction(name)); }
static bool ResolveCore() {
    if (!CoreModule().IsLoaded()) {
#if defined(_WIN32) || defined(_WIN64)
        const char *module_name = "StreamIO.dll";
#elif defined(__APPLE__)
        const char *module_name = "libStreamIO.dylib";
#else
        const char *module_name = "libStreamIO.so";
#endif
        std::string module_path = NPlatform::Paths::ModuleRoot();
        if (!module_path.empty() && module_path.back() != '/' && module_path.back() != '\\') module_path += '/';
        module_path += module_name;
        if (!CoreModule().Load(module_path.c_str()) && !CoreModule().Load(module_name)) return false;
    }
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

static std::string LowerAsciiCopy(const char *text) {
    std::string result;
    if (text) for (const char *p = text; *p; ++p) result.push_back(FoldAscii(*p));
    return result;
}
using GetGlobalVarFunc = const char *(*)(const char *);
static GetGlobalVarFunc ResolveGetGlobalVar() { return Resolve<GetGlobalVarFunc>("bk_bridge_get_global_var"); }
// Which display the resolution dropdown should list modes for. Mirrors
// GraphicsEngineGpu::SelectedDisplay() (GraphicsEngineGpu.cpp) exactly - same
// GFX.Monitor.Name-then-GFX.Monitor.Index resolution against the same two
// globals - so the dropdown always matches the display SetMode will actually
// target. This module does not link against GFXGPU (separate dylib), so the
// globals are read back through legacy_bridge.cpp's bk_bridge_get_global_var,
// a sibling file in this same module, instead of duplicating GlobalVars here.
// Returns 0 (like SelectedDisplay()) when neither the name nor the index
// resolves to a connected display - the caller falls back to displays[0].
static SDL_DisplayID SelectedDisplayForOptions(SDL_DisplayID *displays, int count) {
    if (!displays || count <= 0) return 0;
    GetGlobalVarFunc get_var = ResolveGetGlobalVar();
    const std::string wanted = get_var ? LowerAsciiCopy(get_var("GFX.Monitor.Name")) : std::string();
    int selected = -1;
    if (!wanted.empty()) {
        for (int i = 0; i < count && selected < 0; ++i)
            if (LowerAsciiCopy(SDL_GetDisplayName(displays[i])).find(wanted) != std::string::npos) selected = i;
    }
    if (selected < 0) {
        const char *index_text = get_var ? get_var("GFX.Monitor.Index") : nullptr;
        int index = index_text ? std::atoi(index_text) : 0;
        if (index < 0) index = 0;
        if (index < count) selected = index;
    }
    return selected >= 0 ? displays[selected] : 0;
}
static void FillMonitors(std::vector<OptionDropValue> *drops) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (displays) {
        // "MonitorN" means display index N-1, matching the -monitorN command
        // line. The game has no notion of a primary display, so the first one
        // is just Monitor1 ("Primary" from older configs still parses as it).
        for (int i = 0; i < count; ++i) {
            char text[32];
            std::snprintf(text, sizeof(text), "Monitor%d", i + 1);
            drops->push_back({text});
        }
        SDL_free(displays);
    }
    if (drops->empty()) drops->push_back({"Monitor1"});
}
static void FillVideoModes(std::vector<OptionDropValue> *drops) {
    // "Auto" (the desktop resolution of the selected display) first, then the
    // real modes ordered by pixel count so the click-switch cycles
    // Auto -> smallest -> ... -> largest -> Auto.
    drops->push_back({"Auto"});
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    if (!displays) return;
    // Only the SELECTED display's own modes (2026-08-12-resolution-presentation,
    // Part B): listing every display's modes offered sizes this one could
    // never reach, and picking one just silently clamped down once applied.
    // Falls back to the first enumerable display if the selection does not
    // resolve to a connected one (stale/disconnected GFX.Monitor config) -
    // SelectedDisplay() itself falls back to "whatever display the window is
    // on" in that case, which has no equivalent here since there is no window.
    SDL_DisplayID selected = SelectedDisplayForOptions(displays, display_count);
    if (selected == 0) selected = displays[0];
    if (getenv("BK_GFX_TRACE")) {
        std::fprintf(stderr, "BK_GFX_TRACE: FillVideoModes selected display \"%s\" of %d\n",
            SDL_GetDisplayName(selected), display_count);
        std::fflush(stderr);
    }
    std::vector<std::pair<long long, std::string> > modes_sorted;
    int mode_count = 0;
    if (SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(selected, &mode_count)) {
        for (int i = 0; i < mode_count; ++i) {
            const SDL_DisplayMode *mode = modes[i];
            if (!mode || mode->w <= 0 || mode->h <= 0) continue;
            char text[64]; std::snprintf(text, sizeof(text), "%dx%dx32", mode->w, mode->h);
            // The width joins the pixel count as a tie-breaker so equal-area
            // modes of different shapes get a stable, sensible order. Modes
            // of the same size at different refresh rates dedup to one entry.
            const long long key = (long long)mode->w * mode->h * 100000 + mode->w;
            bool exists = false;
            for (const auto &entry : modes_sorted) if (entry.second == text) { exists = true; break; }
            if (!exists) modes_sorted.push_back({key, text});
        }
        SDL_free(modes);
    }
    SDL_free(displays);
    std::sort(modes_sorted.begin(), modes_sorted.end());
    for (const auto &entry : modes_sorted) drops->push_back({entry.second.c_str()});
    // One-time dump of the resolution dropdown's actual contents: this list is
    // built entirely from SDL_GetFullscreenDisplayModes, so an odd-looking
    // entry here (e.g. a narrow scaled-HiDPI mode the display offers, not a
    // corrupted/computed value) is enough to explain the option ending up set
    // to it - see the GFX.Mode Set() trace in OptionSystem::Set below.
    static bool traced = false;
    if (!traced && getenv("BK_GFX_TRACE")) {
        traced = true;
        std::fprintf(stderr, "BK_GFX_TRACE: FillVideoModes ->");
        for (const auto &d : *drops) std::fprintf(stderr, " %s", d.program_name.c_str());
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }
}

class OptionSystem;
class OptionIterator final : public IOptionSystemIterator {
    OptionSystem *owner_; std::vector<int> sorted_; size_t position_ = 0; int refs_ = 0;
    int Current() const { return position_ < sorted_.size() ? sorted_[position_] : -1; }
public:
    OptionIterator(OptionSystem *owner, unsigned long mask);
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
    bool BK_STDCALL Set(const std::string &name, const variant_t &value) override {
        const std::string text = VariantText(value);
        // Permanent, opt-in diagnostic for the GFX.Mode-drifts-on-its-own class
        // of report (docs/superpowers/sdd/2026-08-12-resolution-presentation):
        // this is the option's ONLY writer (COptionSelection::Apply, driven by
        // the resolution dropdown's click-switch), so a run that ends with an
        // unexpected value always shows the exact Set() call(s) that produced
        // it here, in order - same convention as the ChangeResolution trace in
        // InterfaceScreenBase.cpp.
        if (getenv("BK_GFX_TRACE") && EqualAsciiIgnoreCase(name.c_str(), "GFX.Mode")) {
            unsigned short old_type = 0; const char *old_text = api.value(state_, name.c_str(), &old_type);
            std::fprintf(stderr, "BK_GFX_TRACE: option Set(\"GFX.Mode\") old=\"%s\" new=\"%s\"\n", old_text ? old_text : "<none>", text.c_str());
            std::fflush(stderr);
        }
        const bool ok = api.set(state_, name.c_str(), text.c_str(), value.vt); if (ok) ApplyAction(name); return ok;
    }
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
        else if (fill && std::strcmp(fill, "GetTextureQuality") == 0) { values[0] = "Low"; values[1] = "Compressed"; values[2] = "High"; values[3] = "Ultra"; count = 4; }
        for (int i = 0; i < count; ++i) drops_.push_back({values[i]}); return drops_;
    }
    IOptionSystemIterator *BK_STDCALL CreateIterator(unsigned long mask) override { return new OptionIterator(this, mask); }
    bool BK_STDCALL SerializeConfig(IDataTree *tree) override { return api.serialize_tree(state_, tree, false) >= 0; }
    void BK_STDCALL Init() override { for (int i = 0; i < Count(); ++i) if (NameAt(i)) ApplyAction(NameAt(i)); }
    void BK_STDCALL Repair(IDataTree *tree, bool to_default) override { api.load_tree(state_, tree, !to_default); }
};
// The options screen shows entries in iterator order, so the iterator has to
// impose the same order as the legacy SOptionSortCmp: Order first, name as
// the tie-break. The store itself keeps config-file order, and the config is
// rewritten on every exit, so file order is not stable across sessions.
OptionIterator::OptionIterator(OptionSystem *owner, unsigned long mask) : owner_(owner) {
    if (!owner_) return;
    for (int i = 0; i < owner_->Count(); ++i) {
        unsigned long flags = 0;
        owner_->Metadata(i, nullptr, &flags, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (mask == 0xffffffff || (flags & mask)) sorted_.push_back(i);
    }
    std::stable_sort(sorted_.begin(), sorted_.end(), [this](int left, int right) {
        int order_left = 0, order_right = 0;
        owner_->Metadata(left, nullptr, nullptr, &order_left, nullptr, nullptr, nullptr, nullptr, nullptr);
        owner_->Metadata(right, nullptr, nullptr, &order_right, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (order_left != order_right) return order_left < order_right;
        const char *name_left = owner_->NameAt(left), *name_right = owner_->NameAt(right);
        return std::strcmp(name_left ? name_left : "", name_right ? name_right : "") < 0;
    });
}
bool BK_STDCALL OptionIterator::Next() { if (position_ < sorted_.size()) ++position_; return !IsEnd(); }
bool BK_STDCALL OptionIterator::IsEnd() const { return position_ >= sorted_.size(); }
bool BK_STDCALL OptionIterator::Get(variant_t *name, variant_t *value) const { const char *key = IsEnd() ? nullptr : owner_->NameAt(Current()); return key && AssignVariant(name, VT_BSTR, key) && owner_->Get(key, value); }
const OptionDesc *BK_STDCALL OptionIterator::GetDesc() const { const char *key = IsEnd() ? nullptr : owner_->NameAt(Current()); return key ? owner_->GetDesc(key) : nullptr; }
const std::vector<OptionDropValue> &BK_STDCALL OptionIterator::GetDropValues() const { static const std::vector<OptionDropValue> empty; const char *key = IsEnd() ? nullptr : owner_->NameAt(Current()); return key ? owner_->GetDropValues(key) : empty; }

class ConsoleBuffer final : public IConsoleBuffer {
    void *state_; int refs_ = 0;
    // Read hands back a pointer the caller keeps using until its next read, the
    // way CConsoleBuffer::szTempString does.
    std::wstring read_stash_;
public:
    ConsoleBuffer() : state_(api.console_create()) {} ~ConsoleBuffer() { api.console_destroy(state_); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { if ((refs_ -= count) <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return state_ != nullptr; }
    bool BK_STDCALL Configure(const char *config) override { return config && api.console_configure(state_, config); }
    void BK_STDCALL Write(int channel, const wchar_t *text, unsigned long color, bool backup) override
    {
        if (!text) return;
        std::vector<unsigned short> utf16;
        for (const wchar_t *it = text; *it; ++it) utf16.push_back(static_cast<unsigned short>(*it));
        utf16.push_back(0);
        api.console_write(state_, channel, utf16.data(), color, backup);
    }
    void BK_STDCALL WriteASCII(int channel, const char *text, unsigned long color, bool backup) override { if (text) api.console_write_ascii(state_, channel, text, color, backup); }
    const wchar_t *BK_STDCALL Read(int channel, unsigned long *color) override
    {
        const unsigned short *line = api.console_read(state_, channel, color);
        if (!line) return nullptr;
        read_stash_.clear();
        for (const unsigned short *it = line; *it; ++it) read_stash_.push_back(static_cast<wchar_t>(*it));
        return read_stash_.c_str();
    }
    const char *BK_STDCALL ReadASCII(int channel, unsigned long *color) override { return api.console_read_ascii(state_, channel, color); }
    bool BK_STDCALL DumpLog(int) override { return true; }
};

BK_EXPORT void *bk_option_bridge_create() { if (!ResolveCore()) return nullptr; return new OptionSystem(); }
BK_EXPORT void *bk_console_bridge_create() { if (!ResolveCore()) return nullptr; return new ConsoleBuffer(); }
BK_EXPORT const void *GetModuleDescriptor() { return nullptr; }
