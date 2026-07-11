// Deliberately self-contained legacy ABI shim.  These declarations mirror the
// vtable order used by the old headers without importing their MSXML/MFC stack.
extern "C" void *bk_streamio_temp_buffer(int size, int index);

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
    virtual int BK_STDCALL And(void *) { return 0; }
};

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
    virtual void BK_STDCALL SetWVar(const char *, const unsigned short *) = 0;
    virtual void BK_STDCALL RemoveWVar(const char *) = 0;
    virtual bool BK_STDCALL DumpVars(const char *) = 0;
    virtual void BK_STDCALL SerializeVarsByMatch(void *, const char *) = 0;
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
    int BK_STDCALL GetAllObjects(IRefCount ***out, int *count) override { if (!out || !count) return -1; *count = 0; *out = 0; return 0; }
    void BK_STDCALL Done() override { for (auto &entry : entries) if (entry.object) { entry.object->Release(); entry.object = 0; } }
};

class SaveLoadSystem final : public ISaveLoadSystem {
    void *factory_ = 0; void *gdb_ = 0;
public:
    void BK_STDCALL AddFactory(void *factory) override { if (!factory_) factory_ = factory; }
    void *BK_STDCALL GetCommonFactory() override { return factory_; }
    void BK_STDCALL SetGDB(void *gdb) override { gdb_ = gdb; }
    void *BK_STDCALL CreateStructureSaver(void *, int, void *) override { return 0; }
    void *BK_STDCALL CreateDataTreeSaver(void *, int, const char *) override { return 0; }
    void *BK_STDCALL OpenStorage(const char *, unsigned long, unsigned long) override;
    void *BK_STDCALL CreateStorage(const char *, unsigned long, unsigned long) override;
    void *BK_STDCALL OpenDataBase(const char *, unsigned long, unsigned long) override { return 0; }
    void *BK_STDCALL OpenDataTable(void *, const char *) override { return 0; }
    void *BK_STDCALL OpenIniDataTable(void *) override { return 0; }
};

class DataStorage final : public IDataStorage {
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    bool BK_STDCALL IsStreamExist(const char *) override { return false; }
    void *BK_STDCALL CreateStream(const char *, unsigned long) override { return 0; }
    void *BK_STDCALL OpenStream(const char *, unsigned long) override { return 0; }
    bool BK_STDCALL GetStreamStats(const char *, void *) override { return false; }
    bool BK_STDCALL DestroyElement(const char *) override { return false; }
    bool BK_STDCALL RenameElement(const char *, const char *) override { return false; }
    void *BK_STDCALL CreateEnumerator() override { return 0; }
    const char *BK_STDCALL GetName() const override { return "ZigStorage"; }
    bool BK_STDCALL AddStorage(IDataStorage *, const char *) override { return true; }
    bool BK_STDCALL RemoveStorage(const char *) override { return true; }
};

class GlobalVars final : public IGlobalVars {
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    const char *BK_STDCALL GetVar(const char *) const override { return 0; }
    void BK_STDCALL SetVar(const char *, const char *) override {}
    void BK_STDCALL RemoveVar(const char *) override {}
    void BK_STDCALL RemoveVarsByMatch(const char *) override {}
    const unsigned short *BK_STDCALL GetWVar(const char *) const override { return 0; }
    void BK_STDCALL SetWVar(const char *, const unsigned short *) override {}
    void BK_STDCALL RemoveWVar(const char *) override {}
    bool BK_STDCALL DumpVars(const char *) override { return false; }
    void BK_STDCALL SerializeVarsByMatch(void *, const char *) override {}
};

class RandomGen final : public IRandomGen {
    unsigned int state_ = 0x9e3779b9u;
public:
    void BK_STDCALL AddRef(int, int) override {}
    void BK_STDCALL Release(int, int) override {}
    bool BK_STDCALL IsValid() const override { return true; }
    void BK_STDCALL Init() override { state_ = 0x9e3779b9u; }
    void BK_STDCALL SetSeed(void *) override {}
    void *BK_STDCALL GetSeed() override { return 0; }
    unsigned int BK_STDCALL Get() override { state_ = state_ * 1664525u + 1013904223u; return state_; }
    void BK_STDCALL Store(void *) override {}
    void BK_STDCALL Restore(void *) override {}
};

static Singleton singleton;
static SaveLoadSystem save_load_system;
static GlobalVars global_vars;
static RandomGen random_gen;
static DataStorage data_storage;

void *BK_STDCALL SaveLoadSystem::OpenStorage(const char *, unsigned long, unsigned long) { return &data_storage; }
void *BK_STDCALL SaveLoadSystem::CreateStorage(const char *, unsigned long, unsigned long) { return &data_storage; }

static void EnsureCoreServices()
{
    static bool initialized = false;
    if ( initialized ) return;
    singleton.Register(-1, &global_vars);
    singleton.Register(-3, &random_gen);
    initialized = true;
}

extern "C" void *BK_STDCALL GetTempRawBuffer_Hook(int size, int index) { return bk_streamio_temp_buffer(size, index); }
extern "C" ISingleton *BK_STDCALL GetSingletonGlobal_Hook() { EnsureCoreServices(); return &singleton; }
extern "C" ISaveLoadSystem *BK_STDCALL GetSLS_Hook() { return &save_load_system; }
extern "C" const void *BK_STDCALL GetModuleDescriptor() { return 0; }
