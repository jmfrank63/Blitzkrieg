#include "StdAfx.h"
#include "GeometryBufferGpu.h"

VerticesGpu::VerticesGpu(GraphicsEngineGpu* o,int e,DWORD f,int s,EGFXDynamic u,EGFXPrimitiveType t):owner_(o),elements_(e),format_(f),stride_(s),usage_(u),type_(t){if(!owner_||!owner_->CreateBufferHandle(elements_,format_,stride_,usage_,&handle_))handle_=0;}
VerticesGpu::~VerticesGpu(){if(handle_&&owner_)owner_->DestroyBufferHandle(handle_);}
void* STDCALL VerticesGpu::Lock(){if(locked_||!handle_||!stride_)return nullptr;try{bytes_.assign((size_t)elements_*stride_,0);}catch(...){return nullptr;}locked_=true;return bytes_.data();}
void STDCALL VerticesGpu::Unlock(){if(!locked_)return;owner_->UploadBuffer(handle_,bytes_.data(),bytes_.size());bytes_.clear();locked_=false;}

IndicesGpu::IndicesGpu(GraphicsEngineGpu* o,int e,DWORD f,int s,EGFXDynamic u,EGFXPrimitiveType t):owner_(o),elements_(e),format_(f),stride_(s),usage_(u),type_(t){if(!owner_||!owner_->CreateBufferHandle(elements_,format_,stride_,usage_,&handle_))handle_=0;}
IndicesGpu::~IndicesGpu(){if(handle_&&owner_)owner_->DestroyBufferHandle(handle_);}
void* STDCALL IndicesGpu::Lock(){if(locked_||!handle_||!stride_)return nullptr;try{bytes_.assign((size_t)elements_*stride_,0);}catch(...){return nullptr;}locked_=true;return bytes_.data();}
void STDCALL IndicesGpu::Unlock(){if(!locked_)return;owner_->UploadBuffer(handle_,bytes_.data(),bytes_.size());bytes_.clear();locked_=false;}
