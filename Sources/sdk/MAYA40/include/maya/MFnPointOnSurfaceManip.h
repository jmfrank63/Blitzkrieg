#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPointOnSurfaceManip
#define _MFnPointOnSurfaceManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnPointOnSurfaceManip is the function set for point on surface manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnPointOnSurfaceManip : public MFnManip3D
{
	declareDagMFn(MFnPointOnSurfaceManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &paramName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToSurfacePlug(MPlug &surfacePlug);
	MStatus		connectToParamPlug(MPlug &paramPlug);
	MStatus		setDrawSurface(bool state);
	MStatus		setDrawArrows(bool state);
	MStatus		setParameters(double u, double v);
	MStatus		getParameters(double &u, double &v);
	bool		isDrawSurfaceOn(MStatus *ReturnStatus = NULL) const;
	unsigned	surfaceIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	paramIndex(MStatus *ReturnStatus = NULL) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPointOnSurfaceManip */
