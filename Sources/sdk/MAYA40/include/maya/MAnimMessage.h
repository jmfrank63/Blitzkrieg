#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimMessage
#define _MAnimMessage

#if defined __cplusplus



#include <maya/MMessage.h>
#include <maya/MString.h>



class MObjectArray;


 
/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimMessage : public MMessage
{ 
public:
	static MCallbackId	addAnimCurveEditedCallback (
								void (*func)(MObjectArray &editedCurves,
											  void *clientData),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL);

private: 
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MAnimMessage */
