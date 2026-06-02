#ifndef LINUX
#pragma once
#endif
#ifndef _MFnStateManip
#define _MFnStateManip

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MFnManip3D.h>
#include <maya/MObject.h>



class MPoint;



/**
MFnStateManip is the function set for state manipulators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MFnStateManip : public MFnManip3D
{
	declareDagMFn(MFnStateManip, MFnManip3D);

public:
	MObject		create(MStatus *ReturnStatus = NULL);
	MObject		create(const MString &manipName,
					   const MString &stateName,
					   MStatus *ReturnStatus = NULL);
	MStatus		connectToStatePlug(MPlug &statePlug);
	MStatus		setInitialState(unsigned initialState);
	MStatus		setMaxStates(unsigned numStates);
	unsigned	maxStates(MStatus *ReturnStatus = NULL) const;
	unsigned	state(MStatus *ReturnStatus = NULL) const;
	unsigned	positionIndex(MStatus *ReturnStatus = NULL) const;
	unsigned	stateIndex(MStatus *ReturnStatus = NULL) const;
	  
protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnStateManip */
