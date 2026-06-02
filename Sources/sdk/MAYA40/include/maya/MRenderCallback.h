#ifndef LINUX
#pragma once
#endif
#ifndef _MRenderCallback
#define _MRenderCallback


#if defined __cplusplus

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class MRenderShadowData;
class MRenderData;

/**
*/
class OPENMAYARENDER_EXPORT MRenderCallback {
public:
                    MRenderCallback();
	virtual			~MRenderCallback();

    virtual bool    shadowCastCallback(const MRenderShadowData& data);
    virtual bool    renderCallback(const MRenderData& data);
    virtual bool    postProcessCallback(const MRenderData& data);

    static void     addCallback(MRenderCallback*, int priority = 0);
    static void     removeCallback(MRenderCallback*);

private:
    const void*     internalData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MRenderCallback */

