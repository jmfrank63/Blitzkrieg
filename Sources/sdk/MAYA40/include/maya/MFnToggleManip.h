#ifndef LINUX
#pragma once
#endif
#ifndef _MFnToggleManip
#define _MFnToggleManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnToggleManip is the function set for toggle manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnToggleManip : public MFnManip3D
{
	declareDagMFn(MFnToggleManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &toggleName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToTogglePlug(MPlug &togglePlug);
	MPoint		startPoint(MStatus *ReturnStatus = NULL) const;
	MVector		direction(MStatus *ReturnStatus = NULL) const;
	double		length(MStatus *ReturnStatus = NULL) const;
	bool		toggle(MStatus *ReturnStatus = NULL) const;
	unsigned	startPointIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	directionIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	lengthIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	toggleIndex(MStatus *ReturnStatus = NULL) const;
	MStatus		setStartPoint(MPoint &startPoint);
	MStatus		setDirection(MVector &direction);
	MStatus		setLength(double length);
	MStatus		setToggle(bool toggle);

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnToggleManip */
