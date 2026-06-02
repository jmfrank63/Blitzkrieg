#ifndef LINUX
#pragma once
#endif
#ifndef _MRenderShadowData
#define _MRenderShadowData


#if defined __cplusplus

#include <maya/MFloatPoint.h>
#include <maya/MFloatMatrix.h>

/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

/**
*/
class OPENMAYARENDER_EXPORT MRenderShadowData {
public:
						MRenderShadowData();

    void                worldToZbuffer(
                            const MFloatPoint&  worldPoint,
                            MFloatPoint&        screenPoint) const;

    void                zbufferToWorld(
                            const MFloatPoint& screenPoint,
                            MFloatPoint&        worldPoint) const;

    bool                perspective;
    bool                useMidDistMap;

    enum LightType
    {
        kInvalid,
        kPoint,
        kDirectional,
        kSpot
    };

    LightType           lightType;

    unsigned short      shadowResX;
    unsigned short      shadowResY;
    MFloatPoint         lightPosition;
    MFloatMatrix        projectionMatrix;
    MFloatMatrix        perspectiveMatrix;

    float               *depthMaps;
    float               *midDistMaps;

    const void*         internalData;

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MRenderShadowData */

