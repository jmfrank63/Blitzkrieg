#ifndef LINUX
#pragma once
#endif
#ifndef _MFnFreePointTriadManip
#define _MFnFreePointTriadManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>





/**
MFnFreePointTriadManip is the function set for free point triad manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnFreePointTriadManip : public MFnManip3D
{
	declareDagMFn(MFnFreePointTriadManip, MFnManip3D);

public:
	enum ManipPlane {
		kYZPlane = 0,
		kXZPlane,
		kXYPlane,
		kViewPlane
	};

	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &pointName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToPointPlug(MPlug &pointPlug);
	MStatus		setDrawAxes(bool state);
	MStatus		setSnapMode(bool state);
	MStatus		setKeyframeAll(bool state);
	MStatus		setDrawArrowHead(bool state);
	MStatus		setGlobalTriadPlane(ManipPlane whichPlane);
	bool		isDrawAxesOn(MStatus *ReturnStatus = NULL) const;
	bool		isSnapModeOn(MStatus *ReturnStatus = NULL) const;
	bool		isKeyframeAllOn(MStatus *ReturnStatus = NULL) const;
	unsigned	pointIndex(MStatus *ReturnStatus = NULL) const;

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnFreePointTriadManip */
