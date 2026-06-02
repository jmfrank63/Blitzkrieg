#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPointOnCurveManip
#define _MFnPointOnCurveManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnPointOnCurveManip is the function set for point on curve manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnPointOnCurveManip : public MFnManip3D
{
	declareDagMFn(MFnPointOnCurveManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &paramName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToCurvePlug(MPlug &curvePlug);
	MStatus		connectToParamPlug(MPlug &paramPlug);
	MStatus		setDrawCurve(bool state);
	MStatus		setParameter(double parameter);
	bool		isDrawCurveOn(MStatus *ReturnStatus = NULL) const;
	double		parameter(MStatus *ReturnStatus = NULL) const;
	MPoint		curvePoint(MStatus *ReturnStatus = NULL) const;
	unsigned	curveIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	paramIndex(MStatus *ReturnStatus = NULL) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPointOnCurveManip */
