#ifndef LINUX
#pragma once
#endif
#ifndef _MFnManip3D
#define _MFnManip3D

#if defined __cplusplus



#include <maya/MFnTransform.h>
#include <maya/MObject.h>





/**
MFnManip3D is the function set for 3D manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32


class OPENMAYAUI_EXPORT MFnManip3D : public MFnTransform 
{
	declareDagMFn(MFnManip3D, MFnTransform);
public:
	bool 			isVisible(MStatus *ReturnStatus) const;
	MStatus			setVisible(bool isVisible);
	float			manipScale(MStatus *ReturnStatus) const;
	MStatus			setManipScale(float size);
	bool			isOptimizePlaybackOn(MStatus *ReturnStatus) const;
	MStatus			setOptimizePlayback(bool optimizePlayback);
	static float	globalSize();
	static void		setGlobalSize(float size);
	static float	handleSize();
	static void		setHandleSize(float size);
	static float	lineSize();
	static void		setLineSize(float size);

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnManip3D */
