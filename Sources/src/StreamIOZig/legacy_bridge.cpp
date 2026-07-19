// Deliberately self-contained legacy ABI shim.  These declarations mirror the
// vtable order used by the old headers without importing their MSXML/MFC stack.
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
extern "C" bool bk_structure_start(void *saver, unsigned char id);
extern "C" void bk_structure_finish(void *saver);
extern "C" void bk_structure_data(void *saver, unsigned char id, void *output, int size);
extern "C" int bk_structure_count(void *saver, unsigned char id);
extern "C" void bk_structure_set_counter(void *saver, int counter);
extern "C" const char *bk_global_get(const char *key);
extern "C" void bk_global_set(const char *key, const char *value);
extern "C" void bk_global_remove(const char *key);
extern "C" void bk_random_init();
extern "C" unsigned int bk_random_get();
extern "C" int bk_table_get_int(void *stream, const char *row, const char *entry, int fallback);
extern "C" double bk_table_get_double(void *stream, const char *row, const char *entry, double fallback);
extern "C" void *bk_options_create();
extern "C" void bk_options_destroy(void *options);
extern "C" int bk_options_load_tree(void *options, void *tree, bool only_missing);
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
extern "C" int bk_tree_start(void *tree, const char *name);
extern "C" void bk_tree_finish(void *tree);
extern "C" int bk_tree_size(void *tree);
extern "C" bool bk_tree_string(void *tree, void *destination);
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

struct IRefCount {
    virtual void BK_STDCALL AddRef(int = 1, int = 0x7fffffff) = 0;
    virtual void BK_STDCALL Release(int = 1, int = 0x7fffffff) = 0;
    virtual bool BK_STDCALL IsValid() const = 0;
    virtual IRefCount *BK_STDCALL QI(int) { return 0; }
    virtual int BK_STDCALL operator&(void *) { return 0; }
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

struct IConsoleBuffer : public IRefCount {
    virtual bool BK_STDCALL Configure(const char *) = 0;
    virtual void BK_STDCALL Write(int, const unsigned short *, unsigned long, bool) = 0;
    virtual void BK_STDCALL WriteASCII(int, const char *, unsigned long, bool) = 0;
    virtual const unsigned short *BK_STDCALL Read(int, unsigned long *) = 0;
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
    int BK_STDCALL GetObjectTypeID(IRefCount *) const override { return -1; }
};

class Singleton final : public ISingleton {
    struct Entry { int id; IRefCount *object; } entries[512] = {};
public:
    bool BK_STDCALL Register(int id, IRefCount *object) override {
        if (!object || Get(id)) return false;
        for (auto &entry : entries) if (!entry.object) { entry.id = id; entry.object = object; object->AddRef(); return true; }
        return false;
    }
    bool BK_STDCALL UnRegister(int id) override {
        for (auto &entry : entries) if (entry.object && entry.id == id) { entry.object->Release(); entry.object = 0; return true; }
        return true;
    }
    bool BK_STDCALL UnRegister(IRefCount *object) override {
        for (auto &entry : entries) if (entry.object == object) { entry.object->Release(); entry.object = 0; return true; }
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
    void BK_STDCALL Done() override { for (auto &entry : entries) if (entry.object) { entry.object->Release(); entry.object = 0; } }
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

class ZigDataStream final : public IDataStream {
    void *stream_;
    int refs_ = 0;
public:
    explicit ZigDataStream(void *stream) : stream_(stream) {}
    ~ZigDataStream() { bk_stream_destroy(stream_); }
    void *Native() const { return stream_; }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return stream_ != 0; }
    int BK_STDCALL Read(void *buffer, int length) override { return bk_stream_read(stream_, buffer, length); }
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
    const char *BK_STDCALL GetString(const char *, const char *, const char *fallback, char *buffer, int size) override {
        if (!buffer || size <= 0) return fallback ? fallback : "";
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
    ~ZigDataTree() { bk_tree_destroy(tree_); source_->Release(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return tree_ != 0; }
    bool BK_STDCALL IsReading() const override { return mode_ == 2; }
    int BK_STDCALL StartChunk(const char *name) override { return name ? bk_tree_start(tree_, name) : 0; }
    void BK_STDCALL FinishChunk() override { bk_tree_finish(tree_); }
    int BK_STDCALL GetChunkSize() override { return bk_tree_size(tree_); }
    bool BK_STDCALL RawData(void *, int) override { return false; }
    bool BK_STDCALL StringData(char *data) override { return data && bk_tree_string(tree_, data); }
    bool BK_STDCALL StringData(unsigned short *data) override {
        if (!data) return false;
        const int size = bk_tree_size(tree_); char buffer[4096];
        if (size < 0 || size >= int(sizeof(buffer)) || !bk_tree_string(tree_, buffer)) return false;
        for (int i = 0; i <= size; ++i) data[i] = static_cast<unsigned char>(buffer[i]);
        return true;
    }
    bool BK_STDCALL DataChunk(const char *name, int *data) override { return name && data && bk_tree_int(tree_, name, data); }
    bool BK_STDCALL DataChunk(const char *name, double *data) override { return name && data && bk_tree_double(tree_, name, data); }
    int BK_STDCALL CountChunks(const char *name) override { return bk_tree_count(tree_, name ? name : ""); }
    bool BK_STDCALL SetChunkCounter(int index) override { return bk_tree_set_counter(tree_, index); }
    int BK_STDCALL StartContainerChunk(const char *name) override { return bk_tree_start_container(tree_, name ? name : ""); }
    void BK_STDCALL FinishContainerChunk() override { bk_tree_finish_container(tree_); }
};

class ZigStructureSaver final : public IStructureSaver {
    void *saver_;
    ZigDataStream *source_;
    void *gdb_;
    int refs_ = 0;
public:
    ZigStructureSaver(void *saver, ZigDataStream *source, void *gdb) : saver_(saver), source_(source), gdb_(gdb) { source_->AddRef(); }
    ~ZigStructureSaver() { bk_structure_destroy(saver_); source_->Release(); }
    void BK_STDCALL AddRef(int count = 1, int = 0x7fffffff) override { refs_ += count; }
    void BK_STDCALL Release(int count = 1, int = 0x7fffffff) override { refs_ -= count; if (refs_ <= 0) delete this; }
    bool BK_STDCALL IsValid() const override { return saver_ != 0; }
    bool BK_STDCALL StartChunk(char id) override { return bk_structure_start(saver_, static_cast<unsigned char>(id)); }
    void BK_STDCALL FinishChunk() override { bk_structure_finish(saver_); }
    void BK_STDCALL DataChunk(char id, void *data, int size) override { bk_structure_data(saver_, static_cast<unsigned char>(id), data, size); }
    void BK_STDCALL DataChunk(IDataStream *) override {}
    int BK_STDCALL CountChunks(char id) override { return bk_structure_count(saver_, static_cast<unsigned char>(id)); }
    void BK_STDCALL SetChunkCounter(int counter) override { bk_structure_set_counter(saver_, counter); }
    bool BK_STDCALL IsReading() const override { return true; }
    IRefCount *BK_STDCALL LoadObject() override { return 0; }
    void BK_STDCALL StoreObject(IRefCount *) override {}
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
        for (int index = 0; index < Count(); ++index) { const char *candidate = NameAt(index); if (candidate && _stricmp(candidate, name.c_str()) == 0) return index; }
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
    while (owner_ && index_ < owner_->Count()) { unsigned long flags = 0; owner_->Metadata(index_, 0, &flags, 0, 0, 0, 0, 0, 0); if (mask_ == 0xffffffff || (flags & mask_) != 0) break; ++index_; }
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

void *BK_STDCALL SaveLoadSystem::OpenDataTable(void *stream, const char *) { ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream); return zig_stream ? new ZigDataTable(zig_stream) : 0; }
void *BK_STDCALL SaveLoadSystem::CreateDataTreeSaver(void *stream, int mode, const char *base) {
    ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream);
    void *tree = zig_stream ? bk_tree_create(zig_stream->Native(), mode, base ? base : "base") : 0;
    return tree ? new ZigDataTree(tree, zig_stream, mode) : 0;
}

void *BK_STDCALL SaveLoadSystem::CreateStructureSaver(void *stream, int mode, void *) {
    ZigDataStream *zig_stream = static_cast<ZigDataStream *>(stream);
    void *saver = zig_stream ? bk_structure_create(zig_stream->Native(), mode) : 0;
    return saver ? new ZigStructureSaver(saver, zig_stream, gdb_) : 0;
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
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    const char *BK_STDCALL GetVar(const char *key) const override { return key ? bk_global_get(key) : 0; }
    void BK_STDCALL SetVar(const char *key, const char *value) override { if (key && value) bk_global_set(key, value); }
    void BK_STDCALL RemoveVar(const char *key) override { if (key) bk_global_remove(key); }
    void BK_STDCALL RemoveVarsByMatch(const char *) override {}
    const unsigned short *BK_STDCALL GetWVar(const char *) const override { return 0; }
    void BK_STDCALL SetVar(const char *, const unsigned short *) override {}
    void BK_STDCALL RemoveWVar(const char *) override {}
    bool BK_STDCALL DumpVars(const char *) override { return false; }
    void BK_STDCALL SerializeVarsByMatch(void *, const char *) override {}
};

#if 0
class ConsoleBuffer final : public IConsoleBuffer {
    void *console_ = bk_console_create();
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return console_ != 0; }
    bool BK_STDCALL Configure(const char *config) override { return config && bk_console_configure(console_, config); }
    void BK_STDCALL Write(int channel, const unsigned short *text, unsigned long color, bool backup) override { if (text) bk_console_write(console_, channel, text, color, backup); }
    void BK_STDCALL WriteASCII(int channel, const char *text, unsigned long color, bool backup) override { if (text) bk_console_write_ascii(console_, channel, text, color, backup); }
    const unsigned short *BK_STDCALL Read(int channel, unsigned long *color) override { return bk_console_read(console_, channel, color); }
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

static Singleton singleton;
static SaveLoadSystem save_load_system;
static GlobalVars global_vars;
static IRefCount *console_buffer = 0;
static RandomGen random_gen;
static IRefCount *option_system = 0;
void *BK_STDCALL SaveLoadSystem::OpenStorage(const char *name, unsigned long access, unsigned long type) { void *storage = bk_storage_create(name, access, type); return storage ? new DataStorage(storage) : 0; }
void *BK_STDCALL SaveLoadSystem::CreateStorage(const char *name, unsigned long access, unsigned long type) { void *storage = bk_storage_create(name, access, type); return storage ? new DataStorage(storage) : 0; }

static void EnsureCoreServices()
{
    static bool initialized = false;
    if ( initialized ) return;
    singleton.Register(-1, &global_vars);
    console_buffer = static_cast<IRefCount *>(bk_console_bridge_create());
    if (console_buffer) singleton.Register(-2, console_buffer);
    singleton.Register(-3, &random_gen);
    option_system = static_cast<IRefCount *>(bk_option_bridge_create());
    if (option_system) singleton.Register(-4, option_system);
    initialized = true;
}

extern "C" void *BK_STDCALL GetTempRawBuffer_Hook(int size, int index) { return bk_streamio_temp_buffer(size, index); }
extern "C" ISingleton *BK_STDCALL GetSingletonGlobal_Hook() { EnsureCoreServices(); return &singleton; }
extern "C" ISaveLoadSystem *BK_STDCALL GetSLS_Hook() { return &save_load_system; }
extern "C" const void *BK_STDCALL GetModuleDescriptor() { return 0; }
