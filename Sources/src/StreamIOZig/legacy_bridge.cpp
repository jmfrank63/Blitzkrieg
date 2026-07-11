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
    void *BK_STDCALL OpenStorage(const char *, unsigned long, unsigned long) override { return 0; }
    void *BK_STDCALL CreateStorage(const char *, unsigned long, unsigned long) override { return 0; }
    void *BK_STDCALL OpenDataBase(const char *, unsigned long, unsigned long) override { return 0; }
    void *BK_STDCALL OpenDataTable(void *, const char *) override { return 0; }
    void *BK_STDCALL OpenIniDataTable(void *) override { return 0; }
};

static Singleton singleton;
static SaveLoadSystem save_load_system;

extern "C" void *BK_STDCALL GetTempRawBuffer_Hook(int size, int index) { return bk_streamio_temp_buffer(size, index); }
extern "C" ISingleton *BK_STDCALL GetSingletonGlobal_Hook() { return &singleton; }
extern "C" ISaveLoadSystem *BK_STDCALL GetSLS_Hook() { return &save_load_system; }
extern "C" const void *BK_STDCALL GetModuleDescriptor() { return 0; }
