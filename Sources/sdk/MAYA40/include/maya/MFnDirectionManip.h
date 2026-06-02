#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDirectionManip
#define _MFnDirectionManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnDirectionManip is the function set for direction manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnDirectionManip : public MFnManip3D
{
	declareDagMFn(MFnDirectionManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &directionName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToDirectionPlug(MPlug &directionPlug);
	MStatus		setNormalizeDirection(bool state);
	MStatus		setDrawStart(bool state);
	MStatus		setStartPoint(MPoint &startPoint);
	unsigned	startPointIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	endPointIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	directionIndex(MStatus *ReturnStatus = NULL) const;
	  
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDirectionManip */
