#ifndef LINUX
#pragma once
#endif
#ifndef _MDGMessage
#define _MDGMessage

#if defined __cplusplus



#include <maya/MMessage.h>
#include <maya/MString.h>



class MTime;
class MObject;
class MPlug;

#define kDefaultNodeType "dependNode"



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MDGMessage : public MMessage
{ 
public:
	static MCallbackId	addTimeChangeCallback(
								void (*func)( MTime& time,
											  void * clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MCallbackId  addForceUpdateCallback(
								void (*func)( MTime& time,
								void * clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MCallbackId	addNodeAddedCallback(
								void (*func)( MObject& node,
											  void* clientData ),
								const MString& nodeType = kDefaultNodeType,
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MCallbackId	addNodeRemovedCallback(
								void (*func)( MObject& node,
											  void* clientData ),
								const MString& nodeType = kDefaultNodeType,
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MCallbackId	addConnectionCallback(
								void (*func)( MPlug& srcPlug,
											  MPlug& destPlug,
											  bool made,
											  void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

private: 
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MDGMessage */
