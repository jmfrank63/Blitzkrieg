#include <windows.h>
#include <oleauto.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define BK_STDCALL __stdcall

struct IRefCount {
    virtual void BK_STDCALL AddRef(int = 1, int = 0x7fffffff) = 0;
    virtual void BK_STDCALL Release(int = 1, int = 0x7fffffff) = 0;
    virtual bool BK_STDCALL IsValid() const = 0;
    virtual IRefCount *BK_STDCALL QI(int) { return 0; }
    virtual int BK_STDCALL And(void *) { return 0; }
};
struct IDataTree;
struct IVarIterator : public IRefCount {
    virtual bool BK_STDCALL Next() = 0;
    virtual bool BK_STDCALL IsEnd() const = 0;
    virtual bool BK_STDCALL Get(VARIANT *, VARIANT *) const = 0;
};
struct OptionDesc {
    std::string division, name;
    int data_type = 0, editor_type = 0;
    unsigned long flags = 0;
    VARIANT default_value;
    bool instant_apply = false;
    OptionDesc() { VariantInit(&default_value); }
    ~OptionDesc() { VariantClear(&default_value); }
};
struct OptionDropValue { std::string program_name; };
struct IOptionSystemIterator : public IVarIterator {
    virtual const OptionDesc *BK_STDCALL GetDesc() const = 0;
    virtual const std::vector<OptionDropValue> &BK_STDCALL GetDropValues() const = 0;
};
struct IOptionSystem : public IRefCount {
    virtual bool BK_STDCALL Get(const std::string &, VARIANT *) const = 0;
    virtual bool BK_STDCALL Set(const std::string &, const VARIANT &) = 0;
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
    void *(__cdecl *create)() = 0;
    void (__cdecl *destroy)(void *) = 0;
    int (__cdecl *count)(void *) = 0;
    const char *(__cdecl *name_at)(void *, int) = 0;
    const char *(__cdecl *value)(void *, const char *, unsigned short *) = 0;
    bool (__cdecl *set)(void *, const char *, const char *, unsigned short) = 0;
    bool (__cdecl *remove)(void *, const char *) = 0;
    void (__cdecl *remove_prefix)(void *, const char *) = 0;
    bool (__cdecl *changed)(void *) = 0;
    bool (__cdecl *metadata)(void *, int, int *, unsigned long *, int *, bool *, const char **, const char **, const char **, unsigned short *) = 0;
    int (BK_STDCALL *load_tree)(void *, void *, bool) = 0;
    void *(__cdecl *console_create)() = 0;
    bool (__cdecl *console_configure)(void *, const char *) = 0;
    void (__cdecl *console_write)(void *, int, const unsigned short *, unsigned long, bool) = 0;
    void (__cdecl *console_write_ascii)(void *, int, const char *, unsigned long, bool) = 0;
    const unsigned short *(__cdecl *console_read)(void *, int, unsigned long *) = 0;
    const char *(__cdecl *console_read_ascii)(void *, int, unsigned long *) = 0;
};
static CoreApi api;

template <class T> static T Resolve(HMODULE module, const char *name) { return reinterpret_cast<T>(GetProcAddress(module, name)); }
static bool ResolveCore() {
    HMODULE module = GetModuleHandleA("StreamIO.dll");
    if (!module) return false;
    api.create = Resolve<decltype(api.create)>(module, "bk_options_create");
    api.destroy = Resolve<decltype(api.destroy)>(module, "bk_options_destroy");
    api.count = Resolve<decltype(api.count)>(module, "bk_options_count");
    api.name_at = Resolve<decltype(api.name_at)>(module, "bk_options_name_at");
    api.value = Resolve<decltype(api.value)>(module, "bk_options_value");
    api.set = Resolve<decltype(api.set)>(module, "bk_options_set");
    api.remove = Resolve<decltype(api.remove)>(module, "bk_options_remove");
    api.remove_prefix = Resolve<decltype(api.remove_prefix)>(module, "bk_options_remove_prefix");
    api.changed = Resolve<decltype(api.changed)>(module, "bk_options_changed");
    api.metadata = Resolve<decltype(api.metadata)>(module, "bk_options_metadata");
    api.load_tree = Resolve<decltype(api.load_tree)>(module, "bk_options_load_legacy_tree");
    api.console_create = Resolve<decltype(api.console_create)>(module, "bk_console_create");
    api.console_configure = Resolve<decltype(api.console_configure)>(module, "bk_console_configure");
    api.console_write = Resolve<decltype(api.console_write)>(module, "bk_console_write");
    api.console_write_ascii = Resolve<decltype(api.console_write_ascii)>(module, "bk_console_write_ascii");
    api.console_read = Resolve<decltype(api.console_read)>(module, "bk_console_read");
    api.console_read_ascii = Resolve<decltype(api.console_read_ascii)>(module, "bk_console_read_ascii");
    return api.create && api.destroy && api.count && api.name_at && api.value && api.set && api.remove && api.remove_prefix && api.changed && api.metadata && api.load_tree && api.console_create && api.console_configure && api.console_write && api.console_write_ascii && api.console_read && api.console_read_ascii;
}

static BSTR AnsiToBstr(const char *value) {
    const int length = value ? MultiByteToWideChar(CP_ACP, 0, value, -1, 0, 0) : 0;
    if (length <= 0) return 0;
    BSTR result = SysAllocStringLen(0, static_cast<unsigned int>(length - 1));
    if (result) MultiByteToWideChar(CP_ACP, 0, value, -1, result, length);
    return result;
}
static bool AssignVariant(VARIANT *out, unsigned short type, const char *text) {
    if (!out || !text) return false;
    VariantClear(out); VariantInit(out); out->vt = type;
    switch (type) {
        case VT_BSTR: out->bstrVal = AnsiToBstr(text); return out->bstrVal != 0;
        case VT_UI1: out->bVal = static_cast<unsigned char>(std::strtoul(text, 0, 10)); break;
        case VT_I2: out->iVal = static_cast<short>(std::strtol(text, 0, 10)); break;
        case VT_I4: out->lVal = static_cast<long>(std::strtol(text, 0, 10)); break;
        case VT_R4: out->fltVal = static_cast<float>(std::strtod(text, 0)); break;
        case VT_R8: out->dblVal = std::strtod(text, 0); break;
        case VT_BOOL: out->boolVal = std::strtol(text, 0, 10) ? VARIANT_TRUE : VARIANT_FALSE; break;
        default: out->vt = VT_BSTR; out->bstrVal = AnsiToBstr(text); return out->bstrVal != 0;
    }
    return true;
}
static std::string VariantText(const VARIANT &value) {
    char buffer[96] = {};
    switch (value.vt) {
        case VT_BSTR: {
            if (!value.bstrVal) return {};
            const int length = WideCharToMultiByte(CP_ACP, 0, value.bstrVal, -1, 0, 0, 0, 0);
            if (length <= 1) return {};
            std::vector<char> text(static_cast<size_t>(length));
            WideCharToMultiByte(CP_ACP, 0, value.bstrVal, -1, text.data(), length, 0, 0);
            return text.data();
        }
        case VT_UI1: std::snprintf(buffer, sizeof(buffer), "%u", unsigned(value.bVal)); break;
        case VT_I2: std::snprintf(buffer, sizeof(buffer), "%d", int(value.iVal)); break;
        case VT_I4: std::snprintf(buffer, sizeof(buffer), "%ld", value.lVal); break;
        case VT_R4: std::snprintf(buffer, sizeof(buffer), "%.9g", double(value.fltVal)); break;
        case VT_R8: std::snprintf(buffer, sizeof(buffer), "%.17g", value.dblVal); break;
        case VT_BOOL: std::snprintf(buffer, sizeof(buffer), "%d", value.boolVal != VARIANT_FALSE); break;
        default: return {};
    }
    return buffer;
}

class OptionSystem;
class OptionIterator final : public IOptionSystemIterator {
    OptionSystem *owner_; unsigned long mask_; int index_ = 0, refs_ = 1;
    void Advance();
public:
    OptionIterator(OptionSystem *owner, unsigned long mask) : owner_(owner), mask_(mask) { Advance(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { if ((refs_ -= count) <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return owner_ != 0; }
    bool BK_STDCALL Next() override;
    bool BK_STDCALL IsEnd() const override;
    bool BK_STDCALL Get(VARIANT *, VARIANT *) const override;
    const OptionDesc *BK_STDCALL GetDesc() const override;
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues() const override;
};

class OptionSystem final : public IOptionSystem {
    void *state_; int refs_ = 1;
    mutable OptionDesc desc_; mutable std::vector<OptionDropValue> drops_;
public:
    OptionSystem() : state_(api.create()) {}
    ~OptionSystem() { api.destroy(state_); }
    int Count() const { return api.count(state_); }
    const char *NameAt(int index) const { return api.name_at(state_, index); }
    int Find(const std::string &name) const { for (int i = 0; i < Count(); ++i) if (_stricmp(NameAt(i), name.c_str()) == 0) return i; return -1; }
    bool Metadata(int index, int *editor, unsigned long *flags, int *order, bool *instant, const char **action, const char **fill, const char **fallback, unsigned short *type) const { return api.metadata(state_, index, editor, flags, order, instant, action, fill, fallback, type); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; }
    bool BK_STDCALL IsValid() const override { return state_ != 0; }
    bool BK_STDCALL Get(const std::string &name, VARIANT *value) const override { unsigned short type = VT_EMPTY; const char *text = api.value(state_, name.c_str(), &type); return text && AssignVariant(value, type, text); }
    bool BK_STDCALL Set(const std::string &name, const VARIANT &value) override { const std::string text = VariantText(value); return api.set(state_, name.c_str(), text.c_str(), value.vt); }
    bool BK_STDCALL Remove(const std::string &name) override { return api.remove(state_, name.c_str()); }
    bool BK_STDCALL RemoveByMatch(const std::string &prefix) override { api.remove_prefix(state_, prefix.c_str()); return true; }
    bool BK_STDCALL ChangeSerialize(const std::string &, bool) override { return true; }
    bool BK_STDCALL IsChanged() const override { return api.changed(state_); }
    const OptionDesc *BK_STDCALL GetDesc(const std::string &name) const override {
        const int index = Find(name); if (index < 0) return 0;
        int editor = 0, order = 0; unsigned long flags = 0; bool instant = false; unsigned short type = 0; const char *fallback = 0;
        if (!Metadata(index, &editor, &flags, &order, &instant, 0, 0, &fallback, &type)) return 0;
        desc_.name = name; desc_.division = name.substr(0, name.find('.')); desc_.data_type = type; desc_.editor_type = editor; desc_.flags = flags; desc_.instant_apply = instant;
        AssignVariant(&desc_.default_value, type, fallback ? fallback : ""); return &desc_;
    }
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues(const std::string &name) const override {
        drops_.clear(); const int index = Find(name); const char *fill = 0; if (index >= 0) Metadata(index, 0, 0, 0, 0, 0, &fill, 0, 0);
        const char *values[5] = {}; int count = 0;
        if (fill && std::string(fill) == "GetOnOff") { values[0]="ON"; values[1]="OFF"; count=2; }
        else if (fill && std::string(fill) == "GetDifficulty") { values[0]="Easy"; values[1]="Normal"; values[2]="Hard"; values[3]="Ironman"; count=4; }
        else if (fill && std::string(fill) == "GetGameSpeed") { values[0]="VerySlow"; values[1]="Slow"; values[2]="Normal"; values[3]="Fast"; values[4]="VeryFast"; count=5; }
        else if (fill && std::string(fill) == "GetTextureQuality") { values[0]="Low"; values[1]="Compressed"; values[2]="High"; count=3; }
        for (int i=0; i<count; ++i) drops_.push_back({values[i]}); return drops_;
    }
    IOptionSystemIterator *BK_STDCALL CreateIterator(unsigned long mask) override { return new OptionIterator(this, mask); }
    bool BK_STDCALL SerializeConfig(IDataTree *tree) override { return api.load_tree(state_, tree, false) >= 0; }
    void BK_STDCALL Init() override {}
    void BK_STDCALL Repair(IDataTree *tree, bool to_default) override { api.load_tree(state_, tree, !to_default); }
};

void OptionIterator::Advance() { while (owner_ && index_ < owner_->Count()) { unsigned long flags=0; owner_->Metadata(index_,0,&flags,0,0,0,0,0,0); if (mask_==0xffffffff || (flags&mask_)) break; ++index_; } }
bool BK_STDCALL OptionIterator::Next() { if (!IsEnd()) ++index_; Advance(); return !IsEnd(); }
bool BK_STDCALL OptionIterator::IsEnd() const { return !owner_ || index_ >= owner_->Count(); }
bool BK_STDCALL OptionIterator::Get(VARIANT *name, VARIANT *value) const { const char *key=IsEnd()?0:owner_->NameAt(index_); return key && AssignVariant(name,VT_BSTR,key) && owner_->Get(key,value); }
const OptionDesc *BK_STDCALL OptionIterator::GetDesc() const { const char *key=IsEnd()?0:owner_->NameAt(index_); return key?owner_->GetDesc(key):0; }
const std::vector<OptionDropValue> &BK_STDCALL OptionIterator::GetDropValues() const { static const std::vector<OptionDropValue> empty; const char *key=IsEnd()?0:owner_->NameAt(index_); return key?owner_->GetDropValues(key):empty; }

class ConsoleBuffer final : public IConsoleBuffer {
    void *state_; int refs_ = 1;
public:
    ConsoleBuffer() : state_(api.console_create()) {}
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; }
    bool BK_STDCALL IsValid() const override { return state_ != 0; }
    bool BK_STDCALL Configure(const char *config) override { return config && api.console_configure(state_, config); }
    void BK_STDCALL Write(int channel, const unsigned short *text, unsigned long color, bool backup) override { if (text) api.console_write(state_, channel, text, color, backup); }
    void BK_STDCALL WriteASCII(int channel, const char *text, unsigned long color, bool backup) override { if (text) api.console_write_ascii(state_, channel, text, color, backup); }
    const unsigned short *BK_STDCALL Read(int channel, unsigned long *color) override { return api.console_read(state_, channel, color); }
    const char *BK_STDCALL ReadASCII(int channel, unsigned long *color) override { return api.console_read_ascii(state_, channel, color); }
    bool BK_STDCALL DumpLog(int) override { return true; }
};

extern "C" __declspec(dllexport) void *BK_STDCALL bk_option_bridge_create() {
    if (!ResolveCore()) return 0;
    return new OptionSystem();
}
extern "C" __declspec(dllexport) void *BK_STDCALL bk_console_bridge_create() {
    if (!ResolveCore()) return 0;
    return new ConsoleBuffer();
}

extern "C" __declspec(dllexport) const void *BK_STDCALL GetModuleDescriptor() { return 0; }
