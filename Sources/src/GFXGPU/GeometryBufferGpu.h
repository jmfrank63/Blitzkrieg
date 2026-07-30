#ifndef BLITZKRIEG_GEOMETRY_BUFFER_GPU_H
#define BLITZKRIEG_GEOMETRY_BUFFER_GPU_H
#include "GraphicsEngineGpu.h"
#include <string>
#include <vector>

class VerticesGpu final : public IGFXVertices {
public:
    VerticesGpu(GraphicsEngineGpu *owner, int elements, DWORD format, int stride, EGFXDynamic usage, EGFXPrimitiveType type);
    ~VerticesGpu();
    void STDCALL AddRef(int n=1,int m=0x7fffffff) override {(void)m; refs_+=n;} void STDCALL Release(int n=1,int m=0x7fffffff) override {(void)m; refs_-=n;if(refs_==0)delete this;} bool STDCALL IsValid() const override{return refs_>=0&&handle_;}
    void STDCALL SwapData(ISharedResource*) override{} int STDCALL GetRefCounter() const override{return refs_;} const char* STDCALL GetSharedResourceName() const override{return name_.c_str();} void STDCALL SetSharedResourceName(const std::string& n) override{name_=n;} bool STDCALL Load(bool=false) override{return true;} void STDCALL ClearInternalContainer() override{}
    void* STDCALL Lock() override; void STDCALL Unlock() override;
    GfxGpuHandle Handle() const{return handle_;} uint32_t Count() const{return elements_;} EGFXPrimitiveType Type() const{return type_;}
private: GraphicsEngineGpu* owner_; GfxGpuHandle handle_=0; uint32_t elements_; uint32_t format_; uint32_t stride_; EGFXDynamic usage_; EGFXPrimitiveType type_; int refs_=0; bool locked_=false; std::vector<unsigned char> bytes_; std::string name_;
};

class IndicesGpu final : public IGFXIndices {
public:
    IndicesGpu(GraphicsEngineGpu *owner, int elements, DWORD format, int stride, EGFXDynamic usage, EGFXPrimitiveType type);
    ~IndicesGpu();
    void STDCALL AddRef(int n=1,int m=0x7fffffff) override {(void)m; refs_+=n;} void STDCALL Release(int n=1,int m=0x7fffffff) override {(void)m; refs_-=n;if(refs_==0)delete this;} bool STDCALL IsValid() const override{return refs_>=0&&handle_;}
    void STDCALL SwapData(ISharedResource*) override{} int STDCALL GetRefCounter() const override{return refs_;} const char* STDCALL GetSharedResourceName() const override{return name_.c_str();} void STDCALL SetSharedResourceName(const std::string& n) override{name_=n;} bool STDCALL Load(bool=false) override{return true;} void STDCALL ClearInternalContainer() override{}
    void* STDCALL Lock() override; void STDCALL Unlock() override; void STDCALL SetNumUsedVertices(int n) override{used_vertices_=n;}
    GfxGpuHandle Handle() const{return handle_;} uint32_t Count() const{return elements_;} uint32_t Stride() const{return stride_;} int UsedVertices() const{return used_vertices_;}
private: GraphicsEngineGpu* owner_; GfxGpuHandle handle_=0; uint32_t elements_; uint32_t format_; uint32_t stride_; EGFXDynamic usage_; EGFXPrimitiveType type_; int refs_=0; int used_vertices_=0; bool locked_=false; std::vector<unsigned char> bytes_; std::string name_;
};
#endif
