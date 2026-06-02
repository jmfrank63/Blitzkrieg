#ifndef LINUX
#pragma once
#endif
#ifndef _MRenderData
#define _MRenderData


#if defined __cplusplus

#include <maya/MFloatVector.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatMatrix.h>

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

/**
*/
class OPENMAYARENDER_EXPORT MRenderData {
public:
						MRenderData();

    void                worldToScreen(
                            const MFloatPoint&  worldPoint,
                            MFloatPoint&        screenPoint) const;

    void                screenToWorld(
                            const MFloatPoint& screenPoint,
                            MFloatPoint&        worldPoint) const;

    bool                perspective;
    unsigned short      resX;
    unsigned short      resY;

    unsigned short      left;
    unsigned short      bottom;
    unsigned short      right;
    unsigned short      top;

    unsigned short      bytesPerChannel;
    unsigned short      xsize;
    unsigned short      ysize;

    float               fieldOfView;
    float               aspectRatio;

    MFloatVector        viewDirection;
    MFloatPoint         eyePoint;
    MFloatMatrix        worldToEyeMatrix;

    unsigned char       *rgbaArr;
    float               *depthArr;

    const void*         internalData;

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MRenderData */

