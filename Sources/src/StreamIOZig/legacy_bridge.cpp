// Deliberately self-contained legacy ABI shim.  These declarations mirror the
// vtable order used by the old headers without importing their MSXML/MFC stack.
#include "../Platform/Clock.h"
#include "../Platform/Debug.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" void *bk_streamio_temp_buffer(int size, int index);
extern "C" void *bk_storage_create(const char *name, unsigned long access, unsigned long type);
extern "C" void bk_storage_destroy(void *storage);
extern "C" bool bk_storage_add(void *storage, void *child, const char *name);
extern "C" void *bk_storage_remove(void *storage, const char *name);
extern "C" const char *bk_storage_name(void *storage);
extern "C" bool bk_storage_exists(void *storage, const char *name);
extern "C" void *bk_storage_open(void *storage, const char *name, unsigned long access);
extern "C" void *bk_storage_create_stream(void *storage, const char *name, unsigned long access);
extern "C" bool bk_storage_stats(void *storage, const char *name, void *stats);
extern "C" void *bk_storage_enumerator_create(void *storage);
extern "C" void bk_enumerator_destroy(void *enumerator);
extern "C" void bk_enumerator_reset(void *enumerator);
extern "C" bool bk_enumerator_next(void *enumerator);
extern "C" bool bk_enumerator_stats(void *enumerator, void *stats);
extern "C" int bk_stream_read(void *stream, void *destination, int length);
extern "C" int bk_stream_write(void *stream, const void *source, int length);
extern "C" void *bk_stream_create_memory(const void *src, int len);
extern "C" int bk_stream_seek(void *stream, int offset, int origin);
extern "C" int bk_stream_position(void *stream);
extern "C" int bk_stream_size(void *stream);
extern "C" bool bk_stream_set_size(void *stream, int size);
extern "C" int bk_stream_lock_begin(void *stream);
extern "C" int bk_stream_unlock_begin(void *stream);
extern "C" bool bk_stream_flush(void *stream);
extern "C" bool bk_stream_stats(void *stream, void *stats);
extern "C" void bk_stream_destroy(void *stream);
extern "C" void *bk_structure_create(void *stream, int mode);
extern "C" void bk_structure_destroy(void *saver);
extern "C" float bk_structure_progress(void *saver);
extern "C" bool bk_structure_start(void *saver, unsigned char id);
extern "C" void bk_structure_finish(void *saver);
extern "C" void bk_structure_data(void *saver, unsigned char id, void *output, int size);
extern "C" int bk_structure_count(void *saver, unsigned char id);
extern "C" void bk_structure_set_counter(void *saver, int counter);
extern "C" bool bk_structure_has_directory(void *saver);
extern "C" bool bk_structure_directory_entry(void *saver, int index, int *out_type, int *out_ptr, unsigned char *out_valid);
extern "C" int bk_structure_object_count(void *saver);
extern "C" bool bk_structure_enter_object(void *saver, int index);
extern "C" bool bk_structure_read_raw(void *saver, void *output, int size);
extern "C" const char *bk_global_get(const char *key);
extern "C" void bk_global_set(const char *key, const char *value);
extern "C" void bk_global_remove(const char *key);
extern "C" int bk_global_count();
extern "C" int bk_global_key_at(int index, char *buffer, int capacity);
extern "C" void bk_global_clear();
extern "C" void bk_random_init();
extern "C" unsigned int bk_random_get();
static char FoldAscii(char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value; }
static bool EqualAsciiIgnoreCase(const char *left, const char *right) {
    if (!left || !right) return left == right;
    while (*left && *right && FoldAscii(*left) == FoldAscii(*right)) { ++left; ++right; }
    return *left == *right;
}
static bool StartsAsciiIgnoreCase(const char *value, const char *prefix, std::size_t length) {
    if (!value || !prefix) return false;
    for (std::size_t i = 0; i < length; ++i) if (!value[i] || FoldAscii(value[i]) != FoldAscii(prefix[i])) return false;
    return true;
}
static unsigned long LegacyTickCount() { return NPlatform::MonotonicMilliseconds(); }
static unsigned long LegacyThreadId() {
    return static_cast<unsigned long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}
extern "C" unsigned long long bk_structure_scan_iters();
extern "C" int bk_table_get_int(void *stream, const char *row, const char *entry, int fallback);
extern "C" double bk_table_get_double(void *stream, const char *row, const char *entry, double fallback);
extern "C" void *bk_options_create();
extern "C" void bk_options_destroy(void *options);
extern "C" int bk_options_load_tree(void *options, void *tree, bool only_missing);
extern "C" int bk_options_save_tree(void *options, void *tree);
extern "C" bool bk_table_get_string(void *stream, const char *row, const char *entry, char *buffer, int size);
extern "C" int bk_options_count(void *options);
extern "C" const char *bk_options_name_at(void *options, int index);
extern "C" const char *bk_options_value(void *options, const char *name, unsigned short *value_type);
extern "C" bool bk_options_set(void *options, const char *name, const char *value, unsigned short value_type);
extern "C" bool bk_options_remove(void *options, const char *name);
extern "C" void bk_options_remove_prefix(void *options, const char *prefix);
extern "C" bool bk_options_changed(void *options);
extern "C" bool bk_options_metadata(void *options, int index, int *editor, unsigned long *flags, int *order, bool *instant, const char **action, const char **action_fill, const char **default_value, unsigned short *value_type);
extern "C" void *bk_option_bridge_create();
extern "C" void *bk_console_bridge_create();
extern "C" void *bk_console_create();
extern "C" void bk_console_destroy(void *console);
extern "C" bool bk_console_configure(void *console, const char *config);
extern "C" void bk_console_write(void *console, int channel, const unsigned short *text, unsigned long color, bool backup);
extern "C" void bk_console_write_ascii(void *console, int channel, const char *text, unsigned long color, bool backup);
extern "C" const unsigned short *bk_console_read(void *console, int channel, unsigned long *color);
extern "C" const char *bk_console_read_ascii(void *console, int channel, unsigned long *color);
extern "C" void *bk_tree_create(void *stream, int mode, const char *base);
extern "C" void bk_tree_destroy(void *tree);
extern "C" bool bk_tree_flush(void *tree, void *stream);
extern "C" bool bk_tree_write_int(void *tree, const char *name, int value);
extern "C" bool bk_tree_write_double(void *tree, const char *name, double value);
extern "C" bool bk_tree_write_string(void *tree, const char *text);
extern "C" bool bk_tree_write_wstring(void *tree, const unsigned short *text);
extern "C" bool bk_tree_write_raw(void *tree, const void *data, int size);
extern "C" int bk_tree_start(void *tree, const char *name);
extern "C" void bk_tree_finish(void *tree);
extern "C" int bk_tree_size(void *tree);
extern "C" bool bk_tree_string(void *tree, void *destination);
extern "C" bool bk_tree_raw(void *tree, void *destination, int length);
extern "C" bool bk_tree_int(void *tree, const char *name, int *value);
extern "C" bool bk_tree_double(void *tree, const char *name, double *value);
extern "C" int bk_tree_start_container(void *tree, const char *name);
extern "C" int bk_tree_count(void *tree, const char *name);
extern "C" bool bk_tree_set_counter(void *tree, int index);
extern "C" void bk_tree_finish_container(void *tree);

#if defined(_MSC_VER)
#define BK_STDCALL __stdcall
#else
#define BK_STDCALL __attribute__((stdcall))
#endif

struct IStructureSaver;
struct IRefCount {
    virtual void BK_STDCALL AddRef(int = 1, int = 0x7fffffff) = 0;
    virtual void BK_STDCALL Release(int = 1, int = 0x7fffffff) = 0;
    virtual bool BK_STDCALL IsValid() const = 0;
    virtual IRefCount *BK_STDCALL QI(int) { return 0; }
    // Matches the real IRefCount (Misc/Basic.h) slot so operator&(IStructureSaver&)
    // dispatches to the game object's real serializer across the DLL boundary.
    virtual int BK_STDCALL operator&( IStructureSaver & ) { return 0; }
};

// Vtable mirror of StreamIO/ProgressHook.h IProgressHook, for driving the
// loading-progress movie from the reader's stream position. Only SetCurrPos
// is called; the other slots exist to keep the vtable layout aligned.
struct IBridgeProgressHook : public IRefCount {
    virtual void BK_STDCALL SetNumSteps(int nRange, float fPercentage) = 0;
    virtual void BK_STDCALL Step() = 0;
    virtual void BK_STDCALL Recover() = 0;
    virtual void BK_STDCALL SetCurrPos(int nPos) = 0;
    virtual int BK_STDCALL GetCurrPos() const = 0;
    virtual void Stop() = 0;
};

#if 0
struct IDataTree;
struct IVarIterator : public IRefCount {
    virtual bool BK_STDCALL Next() = 0;
    virtual bool BK_STDCALL IsEnd() const = 0;
    virtual bool BK_STDCALL Get(VARIANT *, VARIANT *) const = 0;
};

struct OptionDesc {
    std::string division;
    std::string name;
    int data_type;
    int editor_type;
    unsigned long flags;
    VARIANT default_value;
    bool instant_apply;
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
#endif

struct ISingleton {
    virtual bool BK_STDCALL Register(int, IRefCount *) = 0;
    virtual bool BK_STDCALL UnRegister(int) = 0;
    virtual bool BK_STDCALL UnRegister(IRefCount *) = 0;
    virtual IRefCount *BK_STDCALL Get(int) = 0;
    virtual int BK_STDCALL GetAllObjects(IRefCount ***, int *) = 0;
    virtual void BK_STDCALL Done() = 0;
};

struct ISaveLoadSystem {
    virtual void BK_STDCALL AddFactory(void *) = 0;
    virtual void *BK_STDCALL GetCommonFactory() = 0;
    virtual void BK_STDCALL SetGDB(void *) = 0;
    virtual void *BK_STDCALL CreateStructureSaver(void *, int, void *) = 0;
    virtual void *BK_STDCALL CreateDataTreeSaver(void *, int, const char *) = 0;
    virtual void *BK_STDCALL OpenStorage(const char *, unsigned long, unsigned long) = 0;
    virtual void *BK_STDCALL CreateStorage(const char *, unsigned long, unsigned long) = 0;
    virtual void *BK_STDCALL OpenDataBase(const char *, unsigned long, unsigned long) = 0;
    virtual void *BK_STDCALL OpenDataTable(void *, const char *) = 0;
    virtual void *BK_STDCALL OpenIniDataTable(void *) = 0;
};

struct IGlobalVars : public IRefCount {
    virtual const char *BK_STDCALL GetVar(const char *) const = 0;
    virtual void BK_STDCALL SetVar(const char *, const char *) = 0;
    virtual void BK_STDCALL RemoveVar(const char *) = 0;
    virtual void BK_STDCALL RemoveVarsByMatch(const char *) = 0;
    virtual const unsigned short *BK_STDCALL GetWVar(const char *) const = 0;
    virtual void BK_STDCALL SetVar(const char *, const unsigned short *) = 0;
    virtual void BK_STDCALL RemoveWVar(const char *) = 0;
    virtual bool BK_STDCALL DumpVars(const char *) = 0;
    virtual void BK_STDCALL SerializeVarsByMatch(void *, const char *) = 0;
};

// First slots of SFX/SFX.h ISFX (through SetStreamMasterVolume), needed by the
// option-action dispatch below. ISFX re-declares QI, which shares the base
// IRefCount slot, so the vtable continues directly with IsInitialized.
struct ISFXMinimal : public IRefCount {
    virtual bool BK_STDCALL IsInitialized() = 0;
    virtual bool BK_STDCALL Init(void *wnd, int driver, int output, int mix_rate, int max_channels) = 0;
    virtual void BK_STDCALL Done() = 0;
    virtual void BK_STDCALL EnableSFX(bool enable) = 0;
    virtual void BK_STDCALL EnableStreaming(bool enable) = 0;
    virtual bool BK_STDCALL IsSFXEnabled() const = 0;
    virtual bool BK_STDCALL IsStreamingEnabled() const = 0;
    virtual void BK_STDCALL SetDistanceFactor(float factor) = 0;
    virtual void BK_STDCALL SetRolloffFactor(float factor) = 0;
    virtual void BK_STDCALL SetSFXMasterVolume(float volume) = 0;
    virtual unsigned char BK_STDCALL GetSFXMasterVolume() const = 0;
    virtual void BK_STDCALL SetStreamMasterVolume(float volume) = 0;
};

// wchar_t, matching StreamIO/Globals.h. See options_bridge.cpp: the Zig core
// stores UTF-16, so the implementation converts; declaring the narrower type
// here truncates every message to its first character.
struct IConsoleBuffer : public IRefCount {
    virtual bool BK_STDCALL Configure(const char *) = 0;
    virtual void BK_STDCALL Write(int, const wchar_t *, unsigned long, bool) = 0;
    virtual void BK_STDCALL WriteASCII(int, const char *, unsigned long, bool) = 0;
    virtual const wchar_t *BK_STDCALL Read(int, unsigned long *) = 0;
    virtual const char *BK_STDCALL ReadASCII(int, unsigned long *) = 0;
    virtual bool BK_STDCALL DumpLog(int) = 0;
};

struct IRandomGen : public IRefCount {
    virtual void BK_STDCALL Init() = 0;
    virtual void BK_STDCALL SetSeed(void *) = 0;
    virtual void *BK_STDCALL GetSeed() = 0;
    virtual unsigned int BK_STDCALL Get() = 0;
    virtual void BK_STDCALL Store(void *) = 0;
    virtual void BK_STDCALL Restore(void *) = 0;
};

struct IDataStorage : public IRefCount {
    virtual bool BK_STDCALL IsStreamExist(const char *) = 0;
    virtual void *BK_STDCALL CreateStream(const char *, unsigned long) = 0;
    virtual void *BK_STDCALL OpenStream(const char *, unsigned long) = 0;
    virtual bool BK_STDCALL GetStreamStats(const char *, void *) = 0;
    virtual bool BK_STDCALL DestroyElement(const char *) = 0;
    virtual bool BK_STDCALL RenameElement(const char *, const char *) = 0;
    virtual void *BK_STDCALL CreateEnumerator() = 0;
    virtual const char *BK_STDCALL GetName() const = 0;
    virtual bool BK_STDCALL AddStorage(IDataStorage *, const char *) = 0;
    virtual bool BK_STDCALL RemoveStorage(const char *) = 0;
};

struct IStorageEnumerator : public IRefCount {
    virtual void BK_STDCALL Reset(const char *) = 0;
    virtual bool BK_STDCALL Next() = 0;
    virtual const void *BK_STDCALL GetStats() const = 0;
};

struct IDataStream : public IRefCount {
    virtual int BK_STDCALL Read(void *, int) = 0;
    virtual int BK_STDCALL Write(const void *, int) = 0;
    virtual int BK_STDCALL LockBegin() = 0;
    virtual int BK_STDCALL UnlockBegin() = 0;
    virtual int BK_STDCALL GetPos() const = 0;
    virtual int BK_STDCALL Seek(int, int) = 0;
    virtual int BK_STDCALL GetSize() const = 0;
    virtual bool BK_STDCALL SetSize(int) = 0;
    virtual int BK_STDCALL CopyTo(IDataStream *, int) = 0;
    virtual void BK_STDCALL Flush() = 0;
    virtual void BK_STDCALL GetStats(void *) = 0;
};

struct IDataTable : public IRefCount {
    virtual int BK_STDCALL GetRowNames(char *, int) = 0;
    virtual int BK_STDCALL GetEntryNames(const char *, char *, int) = 0;
    virtual void BK_STDCALL ClearRow(const char *) = 0;
    virtual int BK_STDCALL GetInt(const char *, const char *, int) = 0;
    virtual double BK_STDCALL GetDouble(const char *, const char *, double) = 0;
    virtual const char *BK_STDCALL GetString(const char *, const char *, const char *, char *, int) = 0;
    virtual int BK_STDCALL GetRawData(const char *, const char *, void *, int) = 0;
    virtual void BK_STDCALL SetInt(const char *, const char *, int) = 0;
    virtual void BK_STDCALL SetDouble(const char *, const char *, double) = 0;
    virtual void BK_STDCALL SetString(const char *, const char *, const char *) = 0;
    virtual void BK_STDCALL SetRawData(const char *, const char *, const void *, int) = 0;
};

struct IDataTree : public IRefCount {
    virtual bool BK_STDCALL IsReading() const = 0;
    virtual int BK_STDCALL StartChunk(const char *) = 0;
    virtual void BK_STDCALL FinishChunk() = 0;
    virtual int BK_STDCALL GetChunkSize() = 0;
    virtual bool BK_STDCALL RawData(void *, int) = 0;
    virtual bool BK_STDCALL StringData(char *) = 0;
    virtual bool BK_STDCALL StringData(unsigned short *) = 0;
    virtual bool BK_STDCALL DataChunk(const char *, int *) = 0;
    virtual bool BK_STDCALL DataChunk(const char *, double *) = 0;
    virtual int BK_STDCALL CountChunks(const char *) = 0;
    virtual bool BK_STDCALL SetChunkCounter(int) = 0;
    virtual int BK_STDCALL StartContainerChunk(const char *) = 0;
    virtual void BK_STDCALL FinishContainerChunk() = 0;
};

struct IStructureSaver : public IRefCount {
    virtual bool BK_STDCALL StartChunk(char) = 0;
    virtual void BK_STDCALL FinishChunk() = 0;
    virtual void BK_STDCALL DataChunk(char, void *, int) = 0;
    virtual void BK_STDCALL DataChunk(IDataStream *) = 0;
    virtual int BK_STDCALL CountChunks(char) = 0;
    virtual void BK_STDCALL SetChunkCounter(int) = 0;
    virtual bool BK_STDCALL IsReading() const = 0;
    virtual IRefCount *BK_STDCALL LoadObject() = 0;
    virtual void BK_STDCALL StoreObject(IRefCount *) = 0;
    virtual void *BK_STDCALL GetGDB() = 0;
};

typedef IRefCount *(BK_STDCALL *ObjectFactoryNewFunc)();
struct SObjectFactoryTypeInfo { int nTypeID; const void *pTypeInfo; ObjectFactoryNewFunc newFunc; };
struct IObjectFactory {
    virtual IRefCount *BK_STDCALL CreateObject(int) = 0;
    virtual void BK_STDCALL RegisterType(int, ObjectFactoryNewFunc) = 0;
    virtual void BK_STDCALL Aggregate(IObjectFactory *) = 0;
    virtual int BK_STDCALL GetNumKnownTypes() = 0;
    virtual void BK_STDCALL GetKnownTypes(SObjectFactoryTypeInfo *, int) = 0;
    virtual int BK_STDCALL GetObjectTypeID(IRefCount *) const = 0;
};

class FactoryAggregate final : public IObjectFactory {
    SObjectFactoryTypeInfo types_[2048] = {};
    int count_ = 0;
public:
    IRefCount *BK_STDCALL CreateObject(int id) override {
        for (int i = 0; i != count_; ++i) if (types_[i].nTypeID == id) return types_[i].newFunc ? types_[i].newFunc() : 0;
        return 0;
    }
    void BK_STDCALL RegisterType(int id, ObjectFactoryNewFunc create) override {
        for (int i = 0; i != count_; ++i) if (types_[i].nTypeID == id) { types_[i].newFunc = create; return; }
        if (count_ < 2048) types_[count_++] = { id, 0, create };
    }
    // Bridge-owned types must carry their type_info: GetObjectTypeID matches
    // only entries with pTypeInfo set, and StoreObject drops any object whose
    // type does not resolve — the save silently loses it.
    void RegisterTypeWithInfo(int id, ObjectFactoryNewFunc create, const std::type_info *info) {
        RegisterType(id, create);
        for (int i = 0; i != count_; ++i) if (types_[i].nTypeID == id) { types_[i].pTypeInfo = info; return; }
    }
    void BK_STDCALL Aggregate(IObjectFactory *factory) override {
        if (!factory) return;
        const int count = factory->GetNumKnownTypes();
        if (count <= 0 || count > 2048) return;
        SObjectFactoryTypeInfo incoming[2048];
        factory->GetKnownTypes(incoming, count);
        for (int i = 0; i != count; ++i) {
            bool replaced = false;
            for (int own = 0; own != count_; ++own) if (types_[own].nTypeID == incoming[i].nTypeID) { types_[own] = incoming[i]; replaced = true; break; }
            if (!replaced && count_ < 2048) types_[count_++] = incoming[i];
        }
    }
    int BK_STDCALL GetNumKnownTypes() override { return count_; }
    void BK_STDCALL GetKnownTypes(SObjectFactoryTypeInfo *out, int capacity) override { for (int i = 0; out && i < count_ && i < capacity; ++i) out[i] = types_[i]; }
    // Mirror CBasicObjectFactory::GetObjectTypeID (Misc/BasicObjectFactory.h):
    // match typeid(*pObj) against each registered type's type_info. Compared by
    // name() because the game's type_info objects live in different DLLs than
    // this bridge, so pointer identity is unreliable; the mangled name is stable
    // across modules compiled by the same (clang) toolchain.
    int BK_STDCALL GetObjectTypeID(IRefCount *pObj) const override {
        if (!pObj) return -1;
        const char *const name = typeid(*pObj).name();
        if (!name) return -1;
        for (int i = 0; i != count_; ++i) {
            if (types_[i].pTypeInfo) {
                const std::type_info *registered = static_cast<const std::type_info*>(types_[i].pTypeInfo);
                if (registered->name() && std::strcmp(registered->name(), name) == 0)
                    return types_[i].nTypeID;
            }
        }
        return -1;
    }
};

class Singleton final : public ISingleton {
    struct Entry { int id; IRefCount *object; } entries[512] = {};
public:
    bool BK_STDCALL Register(int id, IRefCount *object) override {
        if (!object || Get(id)) return false;
        for (auto &entry : entries) if (!entry.object) {
            entry.id = id;
            entry.object = object;
            // The original CSingleton stores CPtr<IRefCount>: every registered
            // object gets a registry-owned reference, negative core-service ids
            // included. Bridge objects self-delete at refcount zero, so skipping
            // this ref lets any transient CPtr hold (0->1->0) free a singleton
            // the registry still hands out.
            object->AddRef();
            return true;
        }
        return false;
    }
    bool BK_STDCALL UnRegister(int id) override {
        for (auto &entry : entries) if (entry.object && entry.id == id) {
            entry.object->Release();
            entry.object = 0;
            return true;
        }
        return true;
    }
    bool BK_STDCALL UnRegister(IRefCount *object) override {
        for (auto &entry : entries) if (entry.object == object) {
            entry.object->Release();
            entry.object = 0;
            return true;
        }
        return false;
    }
    IRefCount *BK_STDCALL Get(int id) override { for (auto &entry : entries) if (entry.object && entry.id == id) return entry.object; return 0; }
    int BK_STDCALL GetAllObjects(IRefCount ***out, int *count) override {
        if (!out || !count) return -1;
        int n = 0;
        for (auto &entry : entries) if (entry.object) ++n;
        // Mirror the legacy implementation: the object list lives in temp
        // buffer 0 and is valid even when empty.
        const int request = (n > 0 ? n : 1) * static_cast<int>(sizeof(IRefCount *));
        IRefCount **buffer = static_cast<IRefCount **>(bk_streamio_temp_buffer(request, 0));
        if (!buffer) { *count = 0; *out = 0; return -1; }
        int i = 0;
        for (auto &entry : entries) if (entry.object) buffer[i++] = entry.object;
        *count = n;
        *out = buffer;
        return n;
    }
    void BK_STDCALL Done() override {
        for (auto &entry : entries) if (entry.object) {
            entry.object->Release();
            entry.object = 0;
        }
    }
};

class SaveLoadSystem final : public ISaveLoadSystem {
    FactoryAggregate factory_; void *gdb_ = 0;
public:
    void BK_STDCALL AddFactory(void *factory) override { factory_.Aggregate(static_cast<IObjectFactory *>(factory)); }
    void *BK_STDCALL GetCommonFactory() override { return &factory_; }
    void BK_STDCALL SetGDB(void *gdb) override { gdb_ = gdb; }
    void *BK_STDCALL CreateStructureSaver(void *, int, void *) override;
    void *BK_STDCALL CreateDataTreeSaver(void *, int, const char *) override;
    void *BK_STDCALL OpenStorage(const char *, unsigned long, unsigned long) override;
    void *BK_STDCALL CreateStorage(const char *, unsigned long, unsigned long) override;
    void *BK_STDCALL OpenDataBase(const char *, unsigned long, unsigned long) override { return 0; }
    void *BK_STDCALL OpenDataTable(void *, const char *) override;
    void *BK_STDCALL OpenIniDataTable(void *) override { return 0; }
};

// Save-game loads spend most of their time inside manager[0]'s deserialize,
// which loads the map/terrain/textures from data files — during that stretch no
// save-stream chunk finishes, so the position-based progress pump never fires
// and the loading bar freezes. Every one of those data-file reads runs through
// ZigDataStream::Read, so a throttled wall-clock heartbeat there creeps the bar
// instead (to 75% over ~15s); real reader progress wins whenever it is ahead.
static void PumpLoadProgressHeartbeat();
static void RegisterProgressReader(class ZigStructureSaver *reader);
static void UnregisterProgressReader(class ZigStructureSaver *reader);

class ZigDataStream final : public IDataStream {
    void *stream_;
    int refs_ = 0;
public:
    explicit ZigDataStream(void *stream) : stream_(stream) {}
    ~ZigDataStream() { bk_stream_destroy(stream_); }
    void *Native() const { return stream_; }
    // Vtable-pointer identity so callers holding a bare IDataStream* can tell
    // whether it is actually a ZigDataStream (and thus safe to Native()) without
    // adding a virtual (which would desync the ABI-mirror vtable from the game's
    // IDataStream). A temporary with a null stream is safe: ~ZigDataStream calls
    // bk_stream_destroy(null) which is a no-op.
    static const void *Vtable() {
        ZigDataStream tmp(0);
        return *reinterpret_cast<void *const *>(&tmp);
    }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return stream_ != 0; }
    int BK_STDCALL Read(void *buffer, int length) override { PumpLoadProgressHeartbeat(); return bk_stream_read(stream_, buffer, length); }
    int BK_STDCALL Write(const void *buffer, int length) override { return bk_stream_write(stream_, buffer, length); }
    int BK_STDCALL LockBegin() override { return bk_stream_lock_begin(stream_); }
    int BK_STDCALL UnlockBegin() override { return bk_stream_unlock_begin(stream_); }
    int BK_STDCALL GetPos() const override { return bk_stream_position(stream_); }
    int BK_STDCALL Seek(int offset, int from) override { return bk_stream_seek(stream_, offset, from); }
    int BK_STDCALL GetSize() const override { return bk_stream_size(stream_); }
    bool BK_STDCALL SetSize(int size) override { return bk_stream_set_size(stream_, size); }
    int BK_STDCALL CopyTo(IDataStream *destination, int length) override {
        char buffer[4096]; int total = 0;
        while (length > 0) { const int count = Read(buffer, length < 4096 ? length : 4096); if (!count) break; total += destination->Write(buffer, count); length -= count; }
        return total;
    }
    void BK_STDCALL Flush() override { bk_stream_flush(stream_); }
    void BK_STDCALL GetStats(void *stats) override { if (stats) bk_stream_stats(stream_, stats); }
};

// Replacement for StreamIO.dll's CMemFileStream (STREAMIO_MEMORY_STREAM):
// a growable in-memory IDataStream the game creates through the object
// factory for AI update suspension and multiplayer packets.
class MemoryStream final : public IDataStream {
    std::vector<unsigned char> data_;
    int begin_ = 0;
    int pos_ = 0;
    int refs_ = 0;
    void ResizeToFit(int size) {
        if (size > (int)data_.size()) {
            data_.reserve((size_t)(size * 1.3));
            data_.resize(size);
        }
    }
public:
    MemoryStream() { data_.reserve(1024); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return true; }
    int BK_STDCALL Read(void *buffer, int length) override {
        const int available = (int)data_.size() - pos_;
        if (length > available) length = available;
        if (length > 0) { std::memcpy(buffer, &data_[pos_], length); pos_ += length; }
        return length > 0 ? length : 0;
    }
    int BK_STDCALL Write(const void *buffer, int length) override {
        ResizeToFit(pos_ + length);
        if (length > 0) { std::memcpy(&data_[pos_], buffer, length); pos_ += length; }
        return length;
    }
    int BK_STDCALL LockBegin() override { begin_ = pos_; return begin_; }
    int BK_STDCALL UnlockBegin() override { const int old = begin_; begin_ = 0; return old; }
    int BK_STDCALL GetPos() const override { return pos_ - begin_; }
    int BK_STDCALL Seek(int offset, int from) override {
        switch (from) {
            case 0: pos_ = begin_ + offset; break;          // STREAM_SEEK_SET
            case 1: pos_ += offset; break;                  // STREAM_SEEK_CUR
            case 2: pos_ = (int)data_.size() + offset; break; // STREAM_SEEK_END
        }
        if (pos_ < begin_) pos_ = begin_;
        if (pos_ > (int)data_.size()) pos_ = (int)data_.size();
        return pos_;
    }
    int BK_STDCALL GetSize() const override { return (int)data_.size() - begin_; }
    bool BK_STDCALL SetSize(int size) override {
        data_.resize(begin_ + size);
        if (pos_ > begin_ + size) pos_ = begin_ + size;
        return true;
    }
    int BK_STDCALL CopyTo(IDataStream *destination, int length) override {
        const int available = (int)data_.size() - pos_;
        if (length > available) length = available;
        const int last = pos_;
        pos_ += length > 0 ? length : 0;
        return length > 0 ? destination->Write(&data_[last], length) : 0;
    }
    void BK_STDCALL Flush() override {}
    void BK_STDCALL GetStats(void *stats) override {
        // Only the leading {pszName, type, nSize} fields of SStorageElementStats.
        struct SStatsHead { const char *name; int type; int size; };
        if (stats) { SStatsHead *head = static_cast<SStatsHead *>(stats); head->name = 0; head->type = 1; head->size = GetSize(); }
    }
    // Savegame serialization, mirroring CMemFileStream::operator&
    // (StreamIO/MemFileSystem.cpp). The bridge stream has no parent storage,
    // name, or stats to persist; contents and positions are the whole state.
    int BK_STDCALL operator&(IStructureSaver &ss) override {
        int nSize = (int)data_.size();
        ss.DataChunk('\x01', &nSize, sizeof(nSize));
        if (ss.IsReading()) data_.assign(nSize > 0 ? (size_t)nSize : 0, 0);
        if (nSize > 0) ss.DataChunk('\x02', &data_[0], nSize);
        ss.DataChunk('\x03', &begin_, sizeof(begin_));
        ss.DataChunk('\x04', &pos_, sizeof(pos_));
        if (ss.IsReading()) {
            if (begin_ < 0 || begin_ > (int)data_.size()) begin_ = 0;
            if (pos_ < begin_ || pos_ > (int)data_.size()) pos_ = begin_;
        }
        return 0;
    }
};

// Vtable mirror of IRandomGenSeed (StreamIO/RandomGen.h): Init,
// InitByZeroSeed, operator&(IDataTree&), Store, Restore.
struct IRandomGenSeed : public IRefCount {
    virtual void BK_STDCALL Init() = 0;
    virtual void BK_STDCALL InitByZeroSeed() = 0;
    virtual int BK_STDCALL SerializeTree(void *) = 0;
    virtual void BK_STDCALL Store(IDataStream *) = 0;
    virtual void BK_STDCALL Restore(IDataStream *) = 0;
};

// Replacement for StreamIO.dll's CRandomGenSeed (STREAMIO_RANDOM_GEN_SEED):
// an ISAAC-sized state blob the game stores/restores through streams and
// passes to IRandomGen::SetSeed at mission start.
class RandomGenSeed final : public IRandomGenSeed {
    enum { RAND_SIZE = 256 };
    struct SRandData { unsigned int cnt; unsigned int rsl[RAND_SIZE]; unsigned int mem[RAND_SIZE]; unsigned int a, b, c; } rnd_ = {};
    int refs_ = 0;
public:
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return true; }
    void BK_STDCALL Init() override {
        rnd_.cnt = 0;
        for (int i = 0; i != RAND_SIZE; ++i) { rnd_.rsl[i] = bk_random_get(); rnd_.mem[i] = bk_random_get(); }
        rnd_.a = bk_random_get(); rnd_.b = bk_random_get(); rnd_.c = bk_random_get();
    }
    void BK_STDCALL InitByZeroSeed() override { std::memset(&rnd_, 0, sizeof(rnd_)); }
    int BK_STDCALL SerializeTree(void *) override { return 0; }
    void BK_STDCALL Store(IDataStream *stream) override { if (stream) stream->Write(&rnd_, sizeof(rnd_)); }
    void BK_STDCALL Restore(IDataStream *stream) override { if (stream) stream->Read(&rnd_, sizeof(rnd_)); }
    // Savegame serialization, mirroring CRandomGenSeed::operator&
    // (StreamIO/RandomGenInternal.cpp) — the whole ISAAC state round-trips.
    int BK_STDCALL operator&(IStructureSaver &ss) override {
        ss.DataChunk('\x01', &rnd_, sizeof(rnd_));
        return 0;
    }
};

static IRefCount *BK_STDCALL CreateMemoryStreamObject() { return new MemoryStream(); }
static IRefCount *BK_STDCALL CreateRandomGenSeedObject() { return new RandomGenSeed(); }

class ZigDataTable final : public IDataTable {
    ZigDataStream *source_;
    int refs_ = 0;
public:
    explicit ZigDataTable(ZigDataStream *stream) : source_(stream) { source_->AddRef(); }
    ~ZigDataTable() { source_->Release(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return source_ != 0; }
    int BK_STDCALL GetRowNames(char *, int) override { return 0; }
    int BK_STDCALL GetEntryNames(const char *, char *, int) override { return 0; }
    void BK_STDCALL ClearRow(const char *) override {}
    int BK_STDCALL GetInt(const char *row, const char *entry, int fallback) override { return (row && entry) ? bk_table_get_int(source_->Native(), row, entry, fallback) : fallback; }
    double BK_STDCALL GetDouble(const char *row, const char *entry, double fallback) override { return (row && entry) ? bk_table_get_double(source_->Native(), row, entry, fallback) : fallback; }
    const char *BK_STDCALL GetString(const char *row, const char *entry, const char *fallback, char *buffer, int size) override {
        if (!buffer || size <= 0) return fallback ? fallback : "";
        // Real lookup (attribute or child-element text, mirroring
        // CDataTableXML::GetNode); this was a fallback-only stub, which made
        // every string const read through tables silently default — e.g. the
        // Actions.User.Friendly priority list, without which BOARD never won
        // the cursor resolution and transports could not be entered.
        if (row && entry && bk_table_get_string(source_->Native(), row, entry, buffer, size)) return buffer;
        const char *value = fallback ? fallback : ""; int i = 0;
        for (; value[i] && i + 1 < size; ++i) buffer[i] = value[i];
        buffer[i] = 0; return buffer;
    }
    int BK_STDCALL GetRawData(const char *, const char *, void *, int) override { return 0; }
    void BK_STDCALL SetInt(const char *, const char *, int) override {}
    void BK_STDCALL SetDouble(const char *, const char *, double) override {}
    void BK_STDCALL SetString(const char *, const char *, const char *) override {}
    void BK_STDCALL SetRawData(const char *, const char *, const void *, int) override {}
};

class StorageEnumerator final : public IStorageEnumerator {
    void *enumerator_;
    mutable struct { const char *name; int type; int size; unsigned int ctime, mtime, atime; } stats_ = {};
    int refs_ = 0;
public:
    explicit StorageEnumerator(void *enumerator) : enumerator_(enumerator) {}
    ~StorageEnumerator() { bk_enumerator_destroy(enumerator_); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return enumerator_ != 0; }
    void BK_STDCALL Reset(const char *) override { bk_enumerator_reset(enumerator_); }
    bool BK_STDCALL Next() override { return bk_enumerator_next(enumerator_); }
    const void *BK_STDCALL GetStats() const override { return bk_enumerator_stats(enumerator_, &stats_) ? &stats_ : 0; }
};

class ZigDataTree final : public IDataTree {
    void *tree_;
    ZigDataStream *source_;
    int mode_;
    int refs_ = 0;
public:
    ZigDataTree(void *tree, ZigDataStream *source, int mode) : tree_(tree), source_(source), mode_(mode) { source_->AddRef(); }
    void *Native() const { return tree_; }
    ~ZigDataTree() {
        // CDataTreeXML saves the document into the stream from its destructor.
        if (mode_ == 1) bk_tree_flush(tree_, source_->Native());
        bk_tree_destroy(tree_);
        source_->Release();
    }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return tree_ != 0; }
    bool BK_STDCALL IsReading() const override { return mode_ == 2; }
    int BK_STDCALL StartChunk(const char *name) override { return name ? bk_tree_start(tree_, name) : 0; }
    void BK_STDCALL FinishChunk() override { bk_tree_finish(tree_); }
    int BK_STDCALL GetChunkSize() override { return bk_tree_size(tree_); }
    bool BK_STDCALL RawData(void *data, int size) override {
        if (!data) return false;
        if (mode_ == 1) return bk_tree_write_raw(tree_, data, size);
        return bk_tree_raw(tree_, data, size);
    }
    bool BK_STDCALL StringData(char *data) override {
        if (!data) return false;
        if (mode_ == 1) return bk_tree_write_string(tree_, data);
        return bk_tree_string(tree_, data);
    }
    bool BK_STDCALL StringData(unsigned short *data) override {
        if (!data) return false;
        if (mode_ == 1) return bk_tree_write_wstring(tree_, data);
        const int size = bk_tree_size(tree_); char buffer[4096];
        if (size < 0 || size >= int(sizeof(buffer)) || !bk_tree_string(tree_, buffer)) return false;
        for (int i = 0; i <= size; ++i) data[i] = static_cast<unsigned char>(buffer[i]);
        return true;
    }
    bool BK_STDCALL DataChunk(const char *name, int *data) override {
        if (!name || !data) return false;
        if (mode_ == 1) return bk_tree_write_int(tree_, name, *data);
        return bk_tree_int(tree_, name, data);
    }
    bool BK_STDCALL DataChunk(const char *name, double *data) override {
        if (!name || !data) return false;
        if (mode_ == 1) return bk_tree_write_double(tree_, name, *data);
        return bk_tree_double(tree_, name, data);
    }
    int BK_STDCALL CountChunks(const char *name) override { return bk_tree_count(tree_, name ? name : ""); }
    bool BK_STDCALL SetChunkCounter(int index) override { return bk_tree_set_counter(tree_, index); }
    int BK_STDCALL StartContainerChunk(const char *name) override { return bk_tree_start_container(tree_, name ? name : ""); }
    void BK_STDCALL FinishContainerChunk() override { bk_tree_finish_container(tree_); }
};

class ZigStructureSaver final : public IStructureSaver {
    void *saver_;
    ZigDataStream *source_;
    void *gdb_;
    IObjectFactory *factory_;
    std::unordered_map<unsigned int, IRefCount*> objects_;  // ptrID -> loaded object
    std::vector<IRefCount*> created_;                       // owns directory-created objects (AddRef'd)
    bool dirLoaded_ = false;
    int refs_ = 0;

    // [reader-diag] phase timestamps for the load-freeze investigation.
    void TraceReaderPhase(const char *phase, int count) {
        if (!progress_) return;
        FILE *f = fopen("load_trace.log", "ab");
        if (f) {
            fprintf(f, "%lu [reader] %s count=%d iters=%llu\n", LegacyTickCount(), phase, count, bk_structure_scan_iters());
            fclose(f);
        }
    }
    void EnsureDirectoryLoaded() {
        if (dirLoaded_) return;
        dirLoaded_ = true;  // set first to avoid re-entry
        if (!factory_ || !bk_structure_has_directory(saver_)) return;
        TraceReaderPhase("phase1-create begin", 0);
        // Pass 1: create every object from the directory so LoadObject (called
        // during pass 2 deserialization) always resolves. Mirrors the original
        // CStructureSaver2::Start two-phase load.
        for (int i = 0; ; ++i) {
            int typeID = 0, ptrID = 0;
            unsigned char valid = 0;
            if (!bk_structure_directory_entry(saver_, i, &typeID, &ptrID, &valid)) break;
            IRefCount *obj = factory_->CreateObject(typeID);
            if (!obj) {
                fprintf(stderr, "[struct-warn] LoadObject: factory returned null for typeID=0x%08x ptrID=0x%08x\n", (unsigned)typeID, (unsigned)ptrID);
                continue;
            }
            obj->AddRef();
            created_.push_back(obj);
            objects_[(unsigned int)ptrID] = obj;
            // No chunk closes and no file reads happen while tens of
            // thousands of objects are factory-created, so neither regular
            // pump fires — drive the wall-clock creep here or the bar
            // freezes for the whole phase. (100ms-throttled internally.)
            if ((i & 63) == 0) PumpLoadProgressHeartbeat();
        }
        TraceReaderPhase("phase1-create end", (int)created_.size());
        // Pass 2: deserialize each object's content (chunk-id-1 under chunk 2).
        const int n = bk_structure_object_count(saver_);
        TraceReaderPhase("phase2-deserialize begin", n);
        for (int i = 0; i < n; ++i) {
            if (!bk_structure_enter_object(saver_, i)) continue;
            int ptrID = 0;
            DataChunk('\x00', &ptrID, 4);   // content's ptrID (sub-chunk 0) — verifies identity
            std::unordered_map<unsigned int, IRefCount*>::iterator it = objects_.find((unsigned int)ptrID);
            if (it != objects_.end() && it->second) {
                if (StartChunk('\x01')) {
                    it->second->operator &( *this );
                    FinishChunk();
                }
            }
            FinishChunk();
        }
        TraceReaderPhase("phase2-deserialize end", n);
    }
public:
    IBridgeProgressHook *progress_ = 0;
    int lastProgressPos_ = 0;
public:
    ZigStructureSaver(void *saver, ZigDataStream *source, void *gdb, IObjectFactory *factory) : saver_(saver), source_(source), gdb_(gdb), factory_(factory) { source_->AddRef(); }
    ~ZigStructureSaver() {
        // [reader-diag] scan-iteration report for the save-game reader only
        // (progress hook == CICLoad); strip with the load-speed diagnostics.
        if (progress_) {
            FILE *f = fopen("load_trace.log", "ab");
            if (f) {
                fprintf(f, "%lu [reader] scan_iters=%llu stream_bytes=%d\n",
                        LegacyTickCount(), bk_structure_scan_iters(), source_ ? bk_stream_size(source_->Native()) : -1);
                fclose(f);
            }
        }
        UnregisterProgressReader(this);
        bk_structure_destroy(saver_);
        for (size_t i = 0; i < created_.size(); ++i) created_[i]->Release();
        source_->Release();
    }
    // Lifetime is guaranteed by the caller (CICLoad holds the hook in a CPtr
    // across the whole Serialize), so no AddRef/Release here.
    void SetProgressHook(IBridgeProgressHook *hook) {
        progress_ = hook;
        if (hook) RegisterProgressReader(this);
        else UnregisterProgressReader(this);
    }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return saver_ != 0; }
    bool BK_STDCALL StartChunk(char id) override { return bk_structure_start(saver_, static_cast<unsigned char>(id)); }
    void BK_STDCALL FinishChunk() override {
        bk_structure_finish(saver_);
        if (progress_) PumpCombinedProgress();
    }
    // Best of stream position and wall-clock creep, monotonic. Fires on every
    // chunk close, so the bar moves from the first seconds of the load — the
    // read-side heartbeat only has to cover the chunk-free map-load stretch.
    void PumpCombinedProgress();
    // Single funnel for SetCurrPos: the hook's Draw flips a frame through GFX,
    // which can read files and re-enter the heartbeat — the guard breaks that
    // cycle (Draw is not reentrancy-safe: nested BeginScene).
    void PumpProgressGuarded(int pos);
    void BK_STDCALL DataChunk(char id, void *data, int size) override { bk_structure_data(saver_, static_cast<unsigned char>(id), data, size); }
    void BK_STDCALL DataChunk(IDataStream *pStream) override {
        if (!pStream) return;
        int nSize = 0;
        DataChunk('\x01', &nSize, sizeof(nSize));
        if (nSize > 0) {
            std::vector<unsigned char> buffer(nSize);
            DataChunk('\x02', &buffer[0], nSize);
            pStream->Write(&buffer[0], nSize);
        }
    }
    int BK_STDCALL CountChunks(char id) override { return bk_structure_count(saver_, static_cast<unsigned char>(id)); }
    void BK_STDCALL SetChunkCounter(int counter) override { bk_structure_set_counter(saver_, counter); }
    bool BK_STDCALL IsReading() const override { return true; }
    IRefCount *BK_STDCALL LoadObject() override {
        EnsureDirectoryLoaded();
        unsigned int ptrID = 0;
        if (!bk_structure_read_raw(saver_, &ptrID, 4)) return 0;
        std::unordered_map<unsigned int, IRefCount*>::iterator it = objects_.find(ptrID);
        // Bare pointer, exactly like CStructureSaver2::LoadObject — the caller
        // (SSHelper's CPtr::operator=) takes the single reference. AddRef'ing
        // here over-counted every deserialized reference.
        if (it != objects_.end()) return it->second;
        return 0;
    }
    void BK_STDCALL StoreObject(IRefCount *) override {}  // write-only
    void *BK_STDCALL GetGDB() override { return gdb_; }
};

namespace {
    // The one reader created with a live progress hook (CICLoad's save-game
    // load). Loads run on the game's main thread, but ZigDataStream::Read is
    // hit from other threads too (SFX streaming reads during the music
    // fade-out that overlaps the load) — the heartbeat thread-gates on the
    // registering thread so GFX is only ever touched from the main thread.
    ZigStructureSaver *g_pProgressReader = 0;
    unsigned long g_progressThreadId = 0;
    unsigned long g_progressStartTick = 0;
    unsigned long g_progressLastPumpTick = 0;
    bool g_bInProgressPump = false;

    // Wall-clock creep: 1 -> 75 over ~15s. Stops there — the reader's
    // position pump and the outer Serialize milestones (92..99) carry the
    // bar the rest of the way, and the monotonic guard keeps whichever
    // source is ahead.
    int CreepPos(unsigned long now) {
        const unsigned long elapsed = now - g_progressStartTick;
        int pos = 1 + (int)((elapsed * 74ul) / 15000ul);
        return pos > 75 ? 75 : pos;
    }
}

void ZigStructureSaver::PumpProgressGuarded(int pos) {
    if (g_bInProgressPump || !progress_) return;
    g_bInProgressPump = true;
    progress_->SetCurrPos(pos);
    g_bInProgressPump = false;
}

void ZigStructureSaver::PumpCombinedProgress() {
    // reader position maps onto the first 90% of the movie; the outer
    // Serialize milestones cover the remainder.
    int pos = 1 + (int)(bk_structure_progress(saver_) * 90.0f);
    if (g_pProgressReader == this) {
        const int creep = CreepPos(LegacyTickCount());
        if (creep > pos) pos = creep;
    }
    if (pos > lastProgressPos_) {
        lastProgressPos_ = pos;
        PumpProgressGuarded(pos);
    }
}

static void RegisterProgressReader(ZigStructureSaver *reader) {
    g_pProgressReader = reader;
    g_progressThreadId = LegacyThreadId();
    g_progressStartTick = LegacyTickCount();
    g_progressLastPumpTick = g_progressStartTick;
}

static void UnregisterProgressReader(ZigStructureSaver *reader) {
    if (g_pProgressReader == reader) g_pProgressReader = 0;
}

static void PumpLoadProgressHeartbeat() {
    ZigStructureSaver *reader = g_pProgressReader;
    if (!reader || g_bInProgressPump) return;
    if (LegacyThreadId() != g_progressThreadId) return;
    const unsigned long now = LegacyTickCount();
    if (now - g_progressLastPumpTick < 100) return;
    g_progressLastPumpTick = now;
    const int pos = CreepPos(now);
    if (pos > reader->lastProgressPos_) {
        reader->lastProgressPos_ = pos;
        reader->PumpProgressGuarded(pos);
    }
}

// Write-mode structure saver. Emits the compact chunk format the zig reader
// (shortChunkAt) parses: [id:u8][len:u8, or u32 LE with bit0 set][payload].
// A save stream holds up to three top-level chunks: id 1 = the caller's main
// data tree, id 0 = the object directory ([typeID:u32][ptrID:u32][valid:u8]
// per record), id 2 = per-object content (chunk-id-1 children). Mirrors
// CStructureSaver2's object-graph layout. Save files are written and read by
// the same build, so the exact byte layout only needs internal consistency.
class ZigStructureWriter final : public IStructureSaver {
    ZigDataStream *source_;
    void *gdb_;
    IObjectFactory *factory_;
    typedef std::pair<unsigned char, std::vector<unsigned char> > Frame;
    std::vector<Frame> stack_;     // main data tree (becomes top-level chunk 1)
    std::vector<Frame> content_;   // object-content frames (become top-level chunk 2)
    std::vector<unsigned char> objDir_;  // directory records (top-level chunk 0)
    std::unordered_map<IRefCount*, unsigned int> stored_;
    std::deque<IRefCount*> toStore_;
    unsigned int nextPtrID_ = 1;   // sequential object IDs; 0 stays the null reference
    bool inContent_ = false;       // route StartChunk/DataChunk/FinishChunk to content_ while draining objects
    int refs_ = 0;

    static void AppendChunk(std::vector<unsigned char> &out, unsigned char id, const unsigned char *data, size_t size) {
        out.push_back(id);
        const unsigned int encoded = (unsigned int)(size << 1);
        if (size < 128) {
            out.push_back((unsigned char)encoded);
        } else {
            out.push_back((unsigned char)(encoded | 1));
            out.push_back((unsigned char)(encoded >> 8));
            out.push_back((unsigned char)(encoded >> 16));
            out.push_back((unsigned char)(encoded >> 24));
        }
        if (size) out.insert(out.end(), data, data + size);
    }
    static void AppendU32(std::vector<unsigned char> &out, unsigned int v) {
        out.push_back((unsigned char)(v & 0xff));
        out.push_back((unsigned char)((v >> 8) & 0xff));
        out.push_back((unsigned char)((v >> 16) & 0xff));
        out.push_back((unsigned char)((v >> 24) & 0xff));
    }
    std::vector<Frame> &Active() { return inContent_ ? content_ : stack_; }

    void DrainObjects() {
        // Serializing one object may StoreObject() transitively-referenced
        // objects (appending to toStore_); drain until empty, mirroring the
        // original's queue loop.
        inContent_ = true;
        while ( !toStore_.empty() ) {
            IRefCount *pObj = toStore_.front();
            toStore_.pop_front();
            const unsigned int ptrID = stored_[pObj];
            // Object content = chunk-id-1: [sub-chunk 0: ptrID][sub-chunk 1: operator& tree]
            StartChunk( '\x01' );
            unsigned int ptr = ptrID;
            DataChunk( '\x00', &ptr, 4 );
            if ( StartChunk( '\x01' ) ) {
                pObj->operator &( *this );
                FinishChunk();
            }
            FinishChunk();
        }
        inContent_ = false;
    }
public:
    ZigStructureWriter(ZigDataStream *source, void *gdb, IObjectFactory *factory) : source_(source), gdb_(gdb), factory_(factory) {
        source_->AddRef();
        stack_.push_back(Frame((unsigned char)1, std::vector<unsigned char>()));
        content_.push_back(Frame((unsigned char)2, std::vector<unsigned char>()));  // synthetic root accumulating object chunks
    }
    ~ZigStructureWriter() {
        while (stack_.size() > 1) FinishChunk();
        DrainObjects();
        std::vector<unsigned char> file;
        AppendChunk(file, 0, objDir_.empty() ? 0 : &objDir_[0], objDir_.size());           // directory
        AppendChunk(file, stack_[0].first, stack_[0].second.empty() ? 0 : &stack_[0].second[0], stack_[0].second.size());  // main data
        AppendChunk(file, content_[0].first, content_[0].second.empty() ? 0 : &content_[0].second[0], content_[0].second.size());  // object content
        source_->Write(&file[0], (int)file.size());
        source_->Flush();
        source_->Release();
    }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return true; }
    bool BK_STDCALL StartChunk(char id) override {
        Active().push_back(Frame((unsigned char)id, std::vector<unsigned char>()));
        return true;
    }
    void BK_STDCALL FinishChunk() override {
        std::vector<Frame> &s = Active();
        if (s.size() <= 1) return;
        Frame top;
        top.first = s.back().first;
        top.second.swap(s.back().second);
        s.pop_back();
        AppendChunk(s.back().second, top.first, top.second.empty() ? 0 : &top.second[0], top.second.size());
    }
    void BK_STDCALL DataChunk(char id, void *data, int size) override {
        if (size < 0 || (size > 0 && !data)) return;
        AppendChunk(Active().back().second, (unsigned char)id, (const unsigned char *)data, (size_t)size);
    }
    void BK_STDCALL DataChunk(IDataStream *pStream) override {
        // Mirror CStructureSaver2::DataChunk(IDataStream*): two sub-chunks,
        // id 1 = the byte count, id 2 = the bytes themselves.
        if (!pStream) return;
        const int nStreamPos = pStream->GetPos();
        int nSize = pStream->GetSize();
        DataChunk('\x01', &nSize, sizeof(nSize));
        if (nSize > 0) {
            std::vector<unsigned char> buffer(nSize);
            pStream->Seek(nStreamPos, 0);
            pStream->Read(&buffer[0], nSize);
            DataChunk('\x02', &buffer[0], nSize);
        }
        pStream->Seek(nStreamPos, 0);
    }
    int BK_STDCALL CountChunks(char) override { return 0; }
    void BK_STDCALL SetChunkCounter(int) override {}
    bool BK_STDCALL IsReading() const override { return false; }
    IRefCount *BK_STDCALL LoadObject() override { return 0; }
    void BK_STDCALL StoreObject(IRefCount *pObj) override {
        // Sequential 4-byte IDs, not truncated pointer values: on x64 two live
        // objects can collide in their low 32 bits, which would cross-wire the
        // object graph on load. IDs are opaque keys, so the format is unchanged.
        unsigned int ptrID = 0;
        if (pObj) {
            std::unordered_map<IRefCount*, unsigned int>::iterator it = stored_.find(pObj);
            if (it != stored_.end()) {
                ptrID = it->second;
            } else {
                ptrID = nextPtrID_++;
                const int typeID = factory_ ? factory_->GetObjectTypeID(pObj) : -1;
                if (typeID == -1) {
                    fprintf(stderr, "[struct-warn] StoreObject: unregistered object type \"%s\" id=0x%08x — save will be incomplete\n",
                            typeid(*pObj).name(), ptrID);
                }
                const unsigned char valid = pObj->IsValid() ? 1 : 0;
                AppendU32(objDir_, (unsigned int)typeID);
                AppendU32(objDir_, ptrID);
                objDir_.push_back(valid);
                stored_[pObj] = ptrID;
                toStore_.push_back(pObj);
            }
        }
        AppendU32(Active().back().second, ptrID);
    }
    void *BK_STDCALL GetGDB() override { return gdb_; }
};

#if 0
static BSTR AnsiToBstr(const char *value) {
    if (!value) return 0;
    const int length = MultiByteToWideChar(CP_ACP, 0, value, -1, 0, 0);
    if (length <= 0) return 0;
    BSTR result = SysAllocStringLen(0, static_cast<unsigned int>(length - 1));
    if (!result) return 0;
    MultiByteToWideChar(CP_ACP, 0, value, -1, result, length);
    return result;
}

static bool AssignVariant(VARIANT *out, unsigned short type, const char *value) {
    if (!out || !value) return false;
    VariantClear(out);
    VariantInit(out);
    out->vt = type;
    switch (type) {
        case VT_BSTR: out->bstrVal = AnsiToBstr(value); return out->bstrVal != 0;
        case VT_UI1: out->bVal = static_cast<unsigned char>(std::strtoul(value, 0, 10)); break;
        case VT_I2: out->iVal = static_cast<short>(std::strtol(value, 0, 10)); break;
        case VT_I4: out->lVal = static_cast<long>(std::strtol(value, 0, 10)); break;
        case VT_R4: out->fltVal = static_cast<float>(std::strtod(value, 0)); break;
        case VT_R8: out->dblVal = std::strtod(value, 0); break;
        case VT_BOOL: out->boolVal = std::strtol(value, 0, 10) != 0 ? VARIANT_TRUE : VARIANT_FALSE; break;
        default: out->vt = VT_BSTR; out->bstrVal = AnsiToBstr(value); return out->bstrVal != 0;
    }
    return true;
}

static std::string VariantText(const VARIANT &value) {
    char buffer[96] = {};
    switch (value.vt) {
        case VT_BSTR: {
            if (!value.bstrVal) return std::string();
            const int length = WideCharToMultiByte(CP_ACP, 0, value.bstrVal, -1, 0, 0, 0, 0);
            if (length <= 1) return std::string();
            std::string result(static_cast<size_t>(length - 1), '\0');
            WideCharToMultiByte(CP_ACP, 0, value.bstrVal, -1, &result[0], length, 0, 0);
            return result;
        }
        case VT_UI1: std::snprintf(buffer, sizeof(buffer), "%u", unsigned(value.bVal)); break;
        case VT_I2: std::snprintf(buffer, sizeof(buffer), "%d", int(value.iVal)); break;
        case VT_I4: std::snprintf(buffer, sizeof(buffer), "%ld", value.lVal); break;
        case VT_R4: std::snprintf(buffer, sizeof(buffer), "%.9g", double(value.fltVal)); break;
        case VT_R8: std::snprintf(buffer, sizeof(buffer), "%.17g", value.dblVal); break;
        case VT_BOOL: std::snprintf(buffer, sizeof(buffer), "%d", value.boolVal != VARIANT_FALSE); break;
        default: return std::string();
    }
    return buffer;
}

class ZigOptionSystem;

class ZigOptionIterator final : public IOptionSystemIterator {
    ZigOptionSystem *owner_;
    unsigned long mask_;
    int index_ = 0;
    int refs_ = 1;
    void Advance();
public:
    ZigOptionIterator(ZigOptionSystem *owner, unsigned long mask) : owner_(owner), mask_(mask) { Advance(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return owner_ != 0; }
    bool BK_STDCALL Next() override;
    bool BK_STDCALL IsEnd() const override;
    bool BK_STDCALL Get(VARIANT *name, VARIANT *value) const override;
    const OptionDesc *BK_STDCALL GetDesc() const override;
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues() const override;
};

class ZigOptionSystem final : public IOptionSystem {
    void *options_ = bk_options_create();
    int refs_ = 1;
    mutable OptionDesc descriptor_;
    mutable std::vector<OptionDropValue> drop_values_;
public:
    ~ZigOptionSystem() { bk_options_destroy(options_); }
    void *Native() const { return options_; }
    int Count() const { return bk_options_count(options_); }
    const char *NameAt(int index) const { return bk_options_name_at(options_, index); }
    bool Metadata(int index, int *editor, unsigned long *flags, int *order, bool *instant, const char **action, const char **fill, const char **fallback, unsigned short *type) const {
        return bk_options_metadata(options_, index, editor, flags, order, instant, action, fill, fallback, type);
    }
    int FindIndex(const std::string &name) const {
        for (int index = 0; index < Count(); ++index) { const char *candidate = NameAt(index); if (candidate && EqualAsciiIgnoreCase(candidate, name.c_str())) return index; }
        return -1;
    }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; }
    bool BK_STDCALL IsValid() const override { return options_ != 0; }
    bool BK_STDCALL Get(const std::string &name, VARIANT *value) const override {
        unsigned short type = VT_EMPTY; const char *text = bk_options_value(options_, name.c_str(), &type);
        return text && AssignVariant(value, type, text);
    }
    bool BK_STDCALL Set(const std::string &name, const VARIANT &value) override {
        const std::string text = VariantText(value);
        return bk_options_set(options_, name.c_str(), text.c_str(), value.vt);
    }
    bool BK_STDCALL Remove(const std::string &name) override { return bk_options_remove(options_, name.c_str()); }
    bool BK_STDCALL RemoveByMatch(const std::string &prefix) override { bk_options_remove_prefix(options_, prefix.c_str()); return true; }
    bool BK_STDCALL ChangeSerialize(const std::string &, bool) override { return true; }
    bool BK_STDCALL IsChanged() const override { return bk_options_changed(options_); }
    const OptionDesc *BK_STDCALL GetDesc(const std::string &name) const override {
        const int index = FindIndex(name); if (index < 0) return 0;
        int editor = 0, order = 0; unsigned long flags = 0; bool instant = false; unsigned short type = VT_EMPTY;
        const char *action = 0, *fill = 0, *fallback = 0;
        if (!Metadata(index, &editor, &flags, &order, &instant, &action, &fill, &fallback, &type)) return 0;
        descriptor_.name = name;
        const std::string::size_type dot = name.find('.'); descriptor_.division = name.substr(0, dot);
        descriptor_.data_type = type; descriptor_.editor_type = editor; descriptor_.flags = flags; descriptor_.instant_apply = instant;
        AssignVariant(&descriptor_.default_value, type, fallback ? fallback : "");
        return &descriptor_;
    }
    const std::vector<OptionDropValue> &BK_STDCALL GetDropValues(const std::string &name) const override {
        drop_values_.clear();
        const int index = FindIndex(name); const char *fill = 0;
        if (index >= 0) Metadata(index, 0, 0, 0, 0, 0, &fill, 0, 0);
        const char *values[5] = {};
        int count = 0;
        if (fill && std::string(fill) == "GetOnOff") { values[0] = "ON"; values[1] = "OFF"; count = 2; }
        else if (fill && std::string(fill) == "GetDifficulty") { values[0] = "Easy"; values[1] = "Normal"; values[2] = "Hard"; values[3] = "Ironman"; count = 4; }
        else if (fill && std::string(fill) == "GetGameSpeed") { values[0] = "VerySlow"; values[1] = "Slow"; values[2] = "Normal"; values[3] = "Fast"; values[4] = "VeryFast"; count = 5; }
        else if (fill && std::string(fill) == "GetTextureQuality") { values[0] = "Low"; values[1] = "Compressed"; values[2] = "High"; count = 3; }
        for (int i = 0; i < count; ++i) { OptionDropValue value; value.program_name = values[i]; drop_values_.push_back(value); }
        return drop_values_;
    }
    IOptionSystemIterator *BK_STDCALL CreateIterator(unsigned long mask) override { return new ZigOptionIterator(this, mask); }
    bool BK_STDCALL SerializeConfig(IDataTree *tree) override {
        ZigDataTree *zig_tree = static_cast<ZigDataTree *>(tree);
        return zig_tree && bk_options_load_tree(options_, zig_tree->Native(), false) >= 0;
    }
    void BK_STDCALL Init() override {}
    void BK_STDCALL Repair(IDataTree *tree, bool to_default) override {
        ZigDataTree *zig_tree = static_cast<ZigDataTree *>(tree);
        if (zig_tree) bk_options_load_tree(options_, zig_tree->Native(), !to_default);
    }
};

void ZigOptionIterator::Advance() {
    while (owner_ && index_ < owner_->Count()) {
        const char *key = owner_->NameAt(index_);
        if (!key || !*key) {
            char message[160] = {};
            std::snprintf(message, sizeof(message), "[StreamIOZig] skipping option index %d: missing key\\n", index_);
            NPlatform::DebugWrite(message);
            ++index_;
            continue;
        }

        unsigned long flags = 0;
        if (!owner_->Metadata(index_, 0, &flags, 0, 0, 0, 0, 0, 0)) {
            char message[224] = {};
            std::snprintf(message, sizeof(message), "[StreamIOZig] skipping option index %d (%s): metadata unavailable\\n", index_, key);
            NPlatform::DebugWrite(message);
            ++index_;
            continue;
        }

        if (mask_ == 0xffffffff || (flags & mask_) != 0) break;
        ++index_;
    }
}
bool BK_STDCALL ZigOptionIterator::Next() { if (!IsEnd()) ++index_; Advance(); return !IsEnd(); }
bool BK_STDCALL ZigOptionIterator::IsEnd() const { return !owner_ || index_ >= owner_->Count(); }
bool BK_STDCALL ZigOptionIterator::Get(VARIANT *name, VARIANT *value) const { const char *key = IsEnd() ? 0 : owner_->NameAt(index_); return key && AssignVariant(name, VT_BSTR, key) && owner_->Get(key, value); }
const OptionDesc *BK_STDCALL ZigOptionIterator::GetDesc() const { const char *key = IsEnd() ? 0 : owner_->NameAt(index_); return key ? owner_->GetDesc(key) : 0; }
const std::vector<OptionDropValue> &BK_STDCALL ZigOptionIterator::GetDropValues() const { static const std::vector<OptionDropValue> empty; const char *key = IsEnd() ? 0 : owner_->NameAt(index_); return key ? owner_->GetDropValues(key) : empty; }
#endif

extern "C" int BK_STDCALL bk_options_load_legacy_tree(void *options, void *tree, bool only_missing) {
    ZigDataTree *zig_tree = static_cast<ZigDataTree *>(tree);
    return zig_tree ? bk_options_load_tree(options, zig_tree->Native(), only_missing) : 0;
}

// COptionSystem::SerializeConfig follows the tree's direction: load options
// from a READ tree, dump them into a WRITE tree (the config.cfg save path).
extern "C" int BK_STDCALL bk_options_serialize_legacy_tree(void *options, void *tree, bool only_missing) {
    ZigDataTree *zig_tree = static_cast<ZigDataTree *>(tree);
    if (!zig_tree) return 0;
    if (!zig_tree->IsReading()) return bk_options_save_tree(options, zig_tree->Native());
    return bk_options_load_tree(options, zig_tree->Native(), only_missing);
}

void *BK_STDCALL SaveLoadSystem::OpenDataTable(void *stream, const char *) { ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream); return zig_stream ? new ZigDataTable(zig_stream) : 0; }
void *BK_STDCALL SaveLoadSystem::CreateDataTreeSaver(void *stream, int mode, const char *base) {
    ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream);
    void *tree = zig_stream ? bk_tree_create(zig_stream->Native(), mode, base ? base : "base") : 0;
    return tree ? new ZigDataTree(tree, zig_stream, mode) : 0;
}

void *BK_STDCALL SaveLoadSystem::CreateStructureSaver(void *stream, int mode, void *progressHook) {
    if (!stream) return 0;
    IObjectFactory *factory = static_cast<IObjectFactory *>(GetCommonFactory());
    if (mode == 2) {   // IStructureSaver::WRITE
        ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream);
        return new ZigStructureWriter(zig_stream, gdb_, factory);
    }
    // READ: if the stream is actually a ZigDataStream, use its handle directly
    // (fast path; this is what every startup/mission structure read passes). If
    // it is NOT — e.g. CICLoad's CStreamRangeAdaptor wrapping the save file —
    // static_cast<ZigDataStream*> would be a bad cast, so snapshot the bytes
    // into an owned memory Stream instead. Distinguish by vtable identity.
    if (stream && *reinterpret_cast<void *const *>(stream) == ZigDataStream::Vtable()) {
        ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream);
        void *saver = bk_structure_create(zig_stream->Native(), mode);
        if (!saver) return 0;
        ZigStructureSaver *reader = new ZigStructureSaver(saver, zig_stream, gdb_, factory);
        reader->SetProgressHook(static_cast<IBridgeProgressHook *>(progressHook));
        return reader;
    }
    IDataStream *ids = static_cast<IDataStream *>(stream);
    const int nSize = ids->GetSize();
    std::vector<unsigned char> buffer(nSize > 0 ? nSize : 0);
    if (nSize > 0) {
        ids->Seek(0, 0);
        ids->Read(buffer.data(), nSize);
    }
    void *memStream = bk_stream_create_memory(nSize > 0 ? buffer.data() : 0, nSize);
    if (!memStream) return 0;
    ZigDataStream *zig_stream = new ZigDataStream(memStream);   // owns + frees the memory Stream
    void *saver = bk_structure_create(zig_stream->Native(), mode);
    if (!saver) { zig_stream->Release(); return 0; }
    ZigStructureSaver *reader = new ZigStructureSaver(saver, zig_stream, gdb_, factory);
    reader->SetProgressHook(static_cast<IBridgeProgressHook *>(progressHook));
    return reader;
}


class DataStorage final : public IDataStorage {
    void *storage_;
    int refs_ = 0;
    DataStorage *attached_[64] = {};
public:
    explicit DataStorage(void *storage) : storage_(storage) {}
    ~DataStorage() {
        for (int i = 0; i < 64; ++i) if (attached_[i]) attached_[i]->Release();
        bk_storage_destroy(storage_);
    }
    void *Native() const { return storage_; }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return storage_ != 0; }
    bool BK_STDCALL IsStreamExist(const char *name) override { return bk_storage_exists(storage_, name); }
    void *BK_STDCALL CreateStream(const char *name, unsigned long access) override { void *stream = bk_storage_create_stream(storage_, name, access); return stream ? new ZigDataStream(stream) : 0; }
    void *BK_STDCALL OpenStream(const char *name, unsigned long access) override { void *stream = bk_storage_open(storage_, name, access); return stream ? new ZigDataStream(stream) : 0; }
    bool BK_STDCALL GetStreamStats(const char *name, void *stats) override {
        // Binary .gdb files are an optional cache.  Until the Zig structure
        // serializer can validate and decode that format, expose the XML
        // source of truth rather than claiming a readable cache.
        const char *extension = 0;
        for (const char *p = name; p && *p; ++p) if (*p == '.') extension = p;
        if (extension && extension[1] == 'g' && extension[2] == 'd' && extension[3] == 'b' && extension[4] == 0) return false;
        return name && stats && bk_storage_stats(storage_, name, stats);
    }
    bool BK_STDCALL DestroyElement(const char *) override { return false; }
    bool BK_STDCALL RenameElement(const char *, const char *) override { return false; }
    void *BK_STDCALL CreateEnumerator() override { void *enumerator = bk_storage_enumerator_create(storage_); return enumerator ? new StorageEnumerator(enumerator) : 0; }
    const char *BK_STDCALL GetName() const override { const char *name = bk_storage_name(storage_); return name ? name : ""; }
    bool BK_STDCALL AddStorage(IDataStorage *storage, const char *name) override {
        DataStorage *child = static_cast<DataStorage *>(storage);
        if (!child || !name || !bk_storage_add(storage_, child->Native(), name)) return false;
        for (int i = 0; i < 64; ++i) if (!attached_[i]) { attached_[i] = child; child->AddRef(); return true; }
        bk_storage_remove(storage_, name);
        return false;
    }
    bool BK_STDCALL RemoveStorage(const char *name) override {
        void *removed = name ? bk_storage_remove(storage_, name) : 0;
        if (!removed) return false;
        for (int i = 0; i < 64; ++i) if (attached_[i] && attached_[i]->Native() == removed) { attached_[i]->Release(); attached_[i] = 0; return true; }
        return true;
    }
};

class GlobalVars final : public IGlobalVars {
    enum { MAX_WVARS = 64, MAX_WKEY = 128, MAX_WVALUE = 4096 };
    struct WVarEntry {
        bool used;
        char key[MAX_WKEY];
        unsigned short value[MAX_WVALUE];
    };
    WVarEntry wvalues_[MAX_WVARS];

    static bool SameKey(const char *left, const char *right) {
        if (!left || !right) return false;
        while (*left && *right) {
            if (*left != *right) return false;
            ++left;
            ++right;
        }
        return *left == 0 && *right == 0;
    }

    int FindWIndex(const char *key) const {
        if (!key) return -1;
        for (int i = 0; i < MAX_WVARS; ++i) {
            if (wvalues_[i].used && SameKey(wvalues_[i].key, key)) return i;
        }
        return -1;
    }

    static void CopyKey(char *destination, const char *source, int capacity) {
        if (!destination || capacity <= 0) return;
        destination[0] = 0;
        if (!source) return;
        int i = 0;
        for (; source[i] && i < capacity - 1; ++i) destination[i] = source[i];
        destination[i] = 0;
    }

    static void CopyWide(unsigned short *destination, const unsigned short *source, int capacity) {
        if (!destination || capacity <= 0) return;
        destination[0] = 0;
        if (!source) return;
        int i = 0;
        for (; source[i] && i < capacity - 1; ++i) destination[i] = source[i];
        destination[i] = 0;
    }

    static bool HasPrefix(const char *text, const char *prefix) {
        return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
    }

    // String chunk in the CSaverAccessor layout (SSHelper.h basic_string path):
    // [id]{ [1] = length int, [2] = raw characters }.
    static void WriteNarrowString(IStructureSaver &ss, char id, const char *text) {
        if (!ss.StartChunk(id)) return;
        int nSize = text ? (int)std::strlen(text) : 0;
        ss.DataChunk('\x01', &nSize, sizeof(nSize));
        ss.DataChunk('\x02', const_cast<char *>(text ? text : ""), nSize);
        ss.FinishChunk();
    }
    static bool ReadNarrowString(IStructureSaver &ss, char id, std::string *out) {
        out->clear();
        if (!ss.StartChunk(id)) return false;
        int nSize = 0;
        ss.DataChunk('\x01', &nSize, sizeof(nSize));
        if (nSize > 0) {
            out->resize((size_t)nSize);
            ss.DataChunk('\x02', &(*out)[0], nSize);
        }
        ss.FinishChunk();
        return true;
    }
    static void WriteWideString(IStructureSaver &ss, char id, const unsigned short *text) {
        if (!ss.StartChunk(id)) return;
        int nChars = 0;
        while (text && text[nChars]) ++nChars;
        ss.DataChunk('\x01', &nChars, sizeof(nChars));
        ss.DataChunk('\x02', const_cast<unsigned short *>(text), nChars * 2);
        ss.FinishChunk();
    }
    static bool ReadWideString(IStructureSaver &ss, char id, std::vector<unsigned short> *out) {
        out->clear();
        if (!ss.StartChunk(id)) return false;
        int nChars = 0;
        ss.DataChunk('\x01', &nChars, sizeof(nChars));
        if (nChars > 0) {
            out->resize((size_t)nChars);
            ss.DataChunk('\x02', &(*out)[0], nChars * 2);
        }
        out->push_back(0);
        ss.FinishChunk();
        return true;
    }

public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    const char *BK_STDCALL GetVar(const char *key) const override { return key ? bk_global_get(key) : 0; }
    void BK_STDCALL SetVar(const char *key, const char *value) override { if (key && value) bk_global_set(key, value); }
    void BK_STDCALL RemoveVar(const char *key) override { if (key) bk_global_remove(key); }
    void BK_STDCALL RemoveVarsByMatch(const char *match) override {
        if (!match) return;
        // Keys live lowercased in the zig store; lowercase the prefix to match.
        std::string prefix(match);
        for (size_t i = 0; i < prefix.size(); ++i)
            if (prefix[i] >= 'A' && prefix[i] <= 'Z') prefix[i] += 'a' - 'A';
        std::vector<std::string> doomed;
        char key[512];
        const int count = bk_global_count();
        for (int i = 0; i < count; ++i) {
            if (bk_global_key_at(i, key, sizeof key) < 0) continue;
            if (HasPrefix(key, prefix.c_str())) doomed.push_back(key);
        }
        for (size_t i = 0; i < doomed.size(); ++i) bk_global_remove(doomed[i].c_str());
        for (int i = 0; i < MAX_WVARS; ++i) {
            if (wvalues_[i].used && StartsAsciiIgnoreCase(wvalues_[i].key, match, std::strlen(match))) {
                wvalues_[i].used = false;
                wvalues_[i].key[0] = 0;
                wvalues_[i].value[0] = 0;
            }
        }
    }
    const unsigned short *BK_STDCALL GetWVar(const char *key) const override {
        const int index = FindWIndex(key);
        return index >= 0 ? wvalues_[index].value : 0;
    }
    void BK_STDCALL SetVar(const char *key, const unsigned short *value) override {
        if (!key || !value) return;
        int index = FindWIndex(key);
        if (index < 0) {
            for (int i = 0; i < MAX_WVARS; ++i) {
                if (!wvalues_[i].used) {
                    index = i;
                    wvalues_[i].used = true;
                    CopyKey(wvalues_[i].key, key, MAX_WKEY);
                    break;
                }
            }
        }
        if (index >= 0) CopyWide(wvalues_[index].value, value, MAX_WVALUE);
    }
    void BK_STDCALL RemoveWVar(const char *key) override {
        const int index = FindWIndex(key);
        if (index >= 0) {
            wvalues_[index].used = false;
            wvalues_[index].key[0] = 0;
            wvalues_[index].value[0] = 0;
        }
    }
    bool BK_STDCALL DumpVars(const char *) override { return false; }
    void BK_STDCALL SerializeVarsByMatch(void *, const char *) override {}

    // Savegame serialization, mirroring CGlobalVars::operator& (StreamIO/
    // GlobalVars.h): chunk 1 = narrow-var map, chunk 2 = wide-var map, both in
    // the CSaverAccessor hash-map layout (interleaved key chunks id 1 / value
    // chunks id 2). GFX./Options. vars are excluded on write and preserved
    // across a load, exactly like the original. Without this override the
    // default no-op operator& ran, so loaded games came back with an empty
    // global-var set (no AreWeInMission, no Mission.Current.*, ...).
    int BK_STDCALL operator&(IStructureSaver &ss) override {
        if (ss.IsReading()) {
            if (ss.StartChunk('\x01')) {
                const int count = ss.CountChunks('\x01');
                // Saves from builds before this override carry an empty chunk;
                // that means "no data written", not "no vars" — leave the
                // current store untouched instead of wiping it.
                if (count > 0) {
                    std::vector<std::string> keys((size_t)count), vals((size_t)count);
                    for (int i = 0; i < count; ++i) { ss.SetChunkCounter(i + 1); ReadNarrowString(ss, '\x01', &keys[(size_t)i]); }
                    for (int i = 0; i < count; ++i) { ss.SetChunkCounter(i + 1); ReadNarrowString(ss, '\x02', &vals[(size_t)i]); }
                    std::vector<std::pair<std::string, std::string> > preserved;
                    char key[512];
                    const int nCurrent = bk_global_count();
                    for (int i = 0; i < nCurrent; ++i) {
                        if (bk_global_key_at(i, key, sizeof key) < 0) continue;
                        if (HasPrefix(key, "gfx.") || HasPrefix(key, "options.")) {
                            const char *value = bk_global_get(key);
                            preserved.push_back(std::make_pair(std::string(key), std::string(value ? value : "")));
                        }
                    }
                    bk_global_clear();
                    for (size_t i = 0; i < keys.size(); ++i) bk_global_set(keys[i].c_str(), vals[i].c_str());
                    for (size_t i = 0; i < preserved.size(); ++i) bk_global_set(preserved[i].first.c_str(), preserved[i].second.c_str());
                }
                ss.FinishChunk();
            }
            if (ss.StartChunk('\x02')) {
                const int count = ss.CountChunks('\x01');
                if (count > 0) {
                    for (int i = 0; i < MAX_WVARS; ++i) {
                        wvalues_[i].used = false;
                        wvalues_[i].key[0] = 0;
                        wvalues_[i].value[0] = 0;
                    }
                    std::vector<std::string> keys((size_t)count);
                    for (int i = 0; i < count; ++i) { ss.SetChunkCounter(i + 1); ReadNarrowString(ss, '\x01', &keys[(size_t)i]); }
                    std::vector<unsigned short> wide;
                    for (int i = 0; i < count; ++i) {
                        ss.SetChunkCounter(i + 1);
                        ReadWideString(ss, '\x02', &wide);
                        SetVar(keys[(size_t)i].c_str(), &wide[0]);
                    }
                }
                ss.FinishChunk();
            }
        } else {
            if (ss.StartChunk('\x01')) {
                char key[512];
                const int count = bk_global_count();
                for (int i = 0; i < count; ++i) {
                    if (bk_global_key_at(i, key, sizeof key) < 0) continue;
                    if (HasPrefix(key, "gfx.") || HasPrefix(key, "options.")) continue;
                    const char *value = bk_global_get(key);
                    WriteNarrowString(ss, '\x01', key);
                    WriteNarrowString(ss, '\x02', value ? value : "");
                }
                ss.FinishChunk();
            }
            if (ss.StartChunk('\x02')) {
                for (int i = 0; i < MAX_WVARS; ++i) {
                    if (!wvalues_[i].used) continue;
                    WriteNarrowString(ss, '\x01', wvalues_[i].key);
                    WriteWideString(ss, '\x02', wvalues_[i].value);
                }
                ss.FinishChunk();
            }
        }
        return 0;
    }
};

#if 0
class ConsoleBuffer final : public IConsoleBuffer {
    void *console_ = bk_console_create();
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return console_ != 0; }
    bool BK_STDCALL Configure(const char *config) override { return config && bk_console_configure(console_, config); }
    void BK_STDCALL Write(int channel, const wchar_t *text, unsigned long color, bool backup) override { if (text) bk_console_write_wide(console_, channel, text, color, backup); }
    void BK_STDCALL WriteASCII(int channel, const char *text, unsigned long color, bool backup) override { if (text) bk_console_write_ascii(console_, channel, text, color, backup); }
    const wchar_t *BK_STDCALL Read(int channel, unsigned long *color) override { return bk_console_read_wide(console_, channel, color); }
    const char *BK_STDCALL ReadASCII(int channel, unsigned long *color) override { return bk_console_read_ascii(console_, channel, color); }
    bool BK_STDCALL DumpLog(int) override { return true; }
};
#endif

class RandomGen final : public IRandomGen {
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    void BK_STDCALL Init() override { bk_random_init(); }
    void BK_STDCALL SetSeed(void *) override {}
    void *BK_STDCALL GetSeed() override { return 0; }
    unsigned int BK_STDCALL Get() override { return bk_random_get(); }
    void BK_STDCALL Store(void *) override {}
    void BK_STDCALL Restore(void *) override {}
};

static Singleton *singleton = 0;
static SaveLoadSystem *save_load_system = 0;
static GlobalVars *global_vars = 0;
static IRefCount *console_buffer = 0;
static RandomGen *random_gen = 0;
static IRefCount *option_system = 0;
void *BK_STDCALL SaveLoadSystem::OpenStorage(const char *name, unsigned long access, unsigned long type) { void *storage = bk_storage_create(name, access, type); return storage ? new DataStorage(storage) : 0; }
void *BK_STDCALL SaveLoadSystem::CreateStorage(const char *name, unsigned long access, unsigned long type) { void *storage = bk_storage_create(name, access, type); return storage ? new DataStorage(storage) : 0; }

static void EnsureCoreServices()
{
    static bool initialized = false;
    if ( initialized ) return;

    if (!singleton) singleton = new Singleton();
    if (!save_load_system) save_load_system = new SaveLoadSystem();
    if (!global_vars) global_vars = new GlobalVars();
    if (!random_gen) random_gen = new RandomGen();

    // Object types the original StreamIO.dll registered in its own factory
    // (StreamIOObjectFactory.cpp); without them CreateObject returns null.
    FactoryAggregate *factory = static_cast<FactoryAggregate *>(save_load_system->GetCommonFactory());
    factory->RegisterTypeWithInfo(0x100b0004, &CreateMemoryStreamObject, &typeid(MemoryStream));    // STREAMIO_MEMORY_STREAM
    factory->RegisterTypeWithInfo(0x100b0005, &CreateRandomGenSeedObject, &typeid(RandomGenSeed));  // STREAMIO_RANDOM_GEN_SEED

    singleton->Register(-1, global_vars);
    // The bridge keeps raw static pointers to these beyond the registry's
    // lifetime, so each gets a bridge-owned reference on top of the registry's.
    console_buffer = static_cast<IRefCount *>(bk_console_bridge_create());
    if (console_buffer) { console_buffer->AddRef(); singleton->Register(-2, console_buffer); }
    singleton->Register(-3, random_gen);
    option_system = static_cast<IRefCount *>(bk_option_bridge_create());
    if (option_system) { option_system->AddRef(); singleton->Register(-4, option_system); }
    initialized = true;
}

// Option-action dispatch, mirroring COptionSystem::InnerSet for the volume
// actions (OptionSystemInternal.cpp:229-242). Without it option values load
// but never take effect — menu music stays at the backend's zero volume.
// Other actions (game speed, difficulty, gamma...) still TODO.

// Video-mode option values are "WxHxB" strings or "Auto", which stands for the
// desktop resolution of the selected display. The actions only write the GFX.*
// globals: CInterfaceScreenBase::ChangeResolution compares them against the
// current mode on the next screen-focus change (options dialog closing) and
// drives IGFX::SetMode, so a new resolution or display applies live. "Auto"
// stays 0x0 here because only the GFX module can ask the windowing system
// which display the window will land on.
static bool IsAutoVideoMode(const char *value)
{
    return value && (value[0] == 'A' || value[0] == 'a') && (value[1] == 'U' || value[1] == 'u') &&
           (value[2] == 'T' || value[2] == 't') && (value[3] == 'O' || value[3] == 'o') && value[4] == 0;
}
static void SetModeSizeVars(int width, int height)
{
    static const char *prefixes[] = {"GFX.Mode.Mission.", "GFX.Mode.InterMission."};
    char key[64], text[32];
    for (const char *prefix : prefixes) {
        std::snprintf(key, sizeof key, "%sSizeX", prefix);
        std::snprintf(text, sizeof text, "%d", width);
        global_vars->SetVar(key, text);
        std::snprintf(key, sizeof key, "%sSizeY", prefix);
        std::snprintf(text, sizeof text, "%d", height);
        global_vars->SetVar(key, text);
    }
}
static void ApplyVideoModeAction(const char *value)
{
    int width = 0, height = 0, bpp = 32;
    if (!IsAutoVideoMode(value) && (std::sscanf(value, "%dx%dx%d", &width, &height, &bpp) < 2 || width <= 0 || height <= 0))
        return;
    SetModeSizeVars(width, height);
    global_vars->SetVar("GFX.Mode.Program", value);
}
static void ApplyMonitorAction(const char *value)
{
    // "MonitorN" is display N-1, matching the -monitorN command line. Anything
    // else - including "Primary", which older configs stored for display 0 -
    // selects the first display.
    int index = 0;
    if (std::strncmp(value, "Monitor", 7) == 0) {
        index = std::atoi(value + 7) - 1;
        if (index < 0) index = 0;
    }
    char text[32];
    std::snprintf(text, sizeof text, "%d", index);
    global_vars->SetVar("GFX.Monitor.Index", text);
    // The name match (a -monitor="..." command line) would override the index
    // inside MoveToSelectedDisplay, so an explicit choice in options drops it.
    global_vars->SetVar("GFX.Monitor.Name", "");
    // An automatic resolution follows the display it lives on.
    if (IsAutoVideoMode(global_vars->GetVar("GFX.Mode.Program")))
        SetModeSizeVars(0, 0);
}

static void ApplyFullScreenAction(const char *value)
{
    // ON means exclusive fullscreen (GFXFS_FULLSCREEN=1), anything else a
    // window (GFXFS_WINDOWED=2). Like the video mode, the action only writes
    // the desired-mode globals; CInterfaceScreenBase::ChangeResolution diffs
    // them against GFX.Mode.Current.* and drives IGFX::SetMode live.
    const bool on = (value[0] == 'O' || value[0] == 'o') && (value[1] == 'N' || value[1] == 'n') && value[2] == 0;
    const char *text = on ? "1" : "2";
    global_vars->SetVar("GFX.Mode.Mission.FullScreen", text);
    global_vars->SetVar("GFX.Mode.InterMission.FullScreen", text);
    // The legacy pair the Windows frame still consults (IsWindowedMode).
    global_vars->SetVar("windowed", on ? "0" : "1");
    global_vars->SetVar("fullscreen", on ? "1" : "0");
}

// Read-only access to a single global var, for sibling files in this same
// module that need one without linking the whole GlobalVars machinery -
// options_bridge.cpp's resolution dropdown (FillVideoModes) resolves
// GFX.Monitor.Name/GFX.Monitor.Index through this to filter the list down to
// the SELECTED display, mirroring GraphicsEngineGpu::SelectedDisplay()'s own
// read of the same two globals (2026-08-12-resolution-presentation, Part B).
extern "C" __declspec(dllexport) const char *bk_bridge_get_global_var(const char *name)
{
    return (global_vars && name) ? global_vars->GetVar(name) : nullptr;
}

extern "C" __declspec(dllexport) void bk_bridge_apply_option_action(const char *action, const char *name, const char *value)
{
    if (!action || !*action) return;
    if (global_vars && value) {
        if (std::strcmp(action, "SetVideoMode") == 0) { ApplyVideoModeAction(value); return; }
        if (std::strcmp(action, "SetMonitor") == 0) { ApplyMonitorAction(value); return; }
        if (std::strcmp(action, "SetFullScreen") == 0) { ApplyFullScreenAction(value); return; }
        if (std::strcmp(action, "SetTextureQuality") == 0) {
            // ITextureManager lives in the GFX module; CInterfaceScreenBase::Step
            // picks the value up and forwards it (same pattern as GFX.DisplayChanged).
            global_vars->SetVar("GFX.Texture.QualityPending", value);
            return;
        }
    }
    const bool music = std::strcmp(action, "SetMusicVolume") == 0;
    const bool sfx = std::strcmp(action, "SetSFXVolume") == 0;
    if (!music && !sfx) return;
    if (!singleton) return;
    ISFXMinimal *sfx_system = static_cast<ISFXMinimal *>(singleton->Get(0x10090001));  // SFX_SFX
    if (!sfx_system) return;

    double master = 1.0;   // Sound.*MasterVolume default, mission scripts may scale it
    if (global_vars) {
        const char *master_text = global_vars->GetVar(music ? "Sound.StreamMasterVolume" : "Sound.SFXMasterVolume");
        if (master_text && *master_text) master = std::atof(master_text);
    }
    const float volume = static_cast<float>(static_cast<short>((value ? std::atof(value) : 0.0) * master)) / 100.0f;
    if (music)
        sfx_system->SetStreamMasterVolume(volume);
    else
        sfx_system->SetSFXMasterVolume(volume);

    if (name && *name && global_vars) {
        char key[256];
        std::snprintf(key, sizeof key, "Options.%s", name);
        global_vars->SetVar(key, value ? value : "");
    }
}

extern "C" void *BK_STDCALL GetTempRawBuffer_Hook(int size, int index) { return bk_streamio_temp_buffer(size, index); }
extern "C" ISingleton *BK_STDCALL GetSingletonGlobal_Hook() { EnsureCoreServices(); return singleton; }
extern "C" ISaveLoadSystem *BK_STDCALL GetSLS_Hook() { EnsureCoreServices(); return save_load_system; }
extern "C" const void *BK_STDCALL GetModuleDescriptor() { return 0; }
