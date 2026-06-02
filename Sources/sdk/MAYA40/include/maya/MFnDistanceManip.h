#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDistanceManip
#define _MFnDistanceManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>





/**
MFnDistanceManip is the function set for distance manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnDistanceManip : public MFnManip3D
{
	declareDagMFn(MFnDistanceManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &distanceName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToDistancePlug(MPlug &distancePlug);
	MStatus		setStartPoint(const MPoint &point);
	MStatus		setDirection(const MVector &vector);
	MStatus		setDrawStart(bool state);
	MStatus		setDrawLine(bool state);
	MStatus		setScalingFactor(double scalingFactor);
	bool		isDrawStartOn(MStatus *ReturnStatus = NULL) const;
	bool		isDrawLineOn(MStatus *ReturnStatus = NULL) const;
	double		scalingFactor(MStatus *ReturnStatus = NULL) const;
	unsigned	distanceIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	directionIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	startPointIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	currentPointIndex(MStatus *ReturnStatus = NULL) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDistanceManip */
