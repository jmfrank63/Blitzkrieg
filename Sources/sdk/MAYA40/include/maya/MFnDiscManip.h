#ifndef LINUX
#pragma once
#endif
#ifndef _MFnDiscManip
#define _MFnDiscManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;
class MVector;
class MAngle;



/**
MFnDiscManip is the function set for disc manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnDiscManip : public MFnManip3D
{
	declareDagMFn(MFnDiscManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &angleName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToAnglePlug(MPlug &anglePlug);
	MStatus		setCenterPoint(const MPoint &centerPoint);
	MStatus		setNormal(const MVector &normal);
	MStatus		setRadius(double radius);
	MStatus		setAngle(MAngle angle);
	unsigned	centerIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	axisIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	angleIndex(MStatus *ReturnStatus = NULL) const;
	  
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnDiscManip */
