#ifndef LINUX
#pragma once
#endif
#ifndef _MFnCurveSegmentManip
#define _MFnCurveSegmentManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnCurveSegmentManip is the function set for curve segment manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnCurveSegmentManip : public MFnManip3D
{
	declareDagMFn(MFnCurveSegmentManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &startParamName,
					   const MString &endParamName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToCurvePlug(MPlug &curvePlug);
	MStatus		connectToStartParamPlug(MPlug &startParamPlug);
	MStatus		connectToEndParamPlug(MPlug &endParamPlug);
	MStatus		setStartParameter(double startParameter);
	MStatus		setEndParameter(double endParameter);
	double		startParameter(MStatus *ReturnStatus = NULL) const;
	double		endParameter(MStatus *ReturnStatus = NULL) const;
	unsigned	curveIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	startParamIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	endParamIndex(MStatus *ReturnStatus = NULL) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnCurveSegmentManip */
