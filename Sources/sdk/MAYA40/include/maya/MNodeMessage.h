#ifndef LINUX
#pragma once
#endif
#ifndef _MNodeMessage
#define _MNodeMessage

#if defined __cplusplus



#include <maya/MMessage.h>



class MPlug;
class MObject;
class MDGModifier;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MNodeMessage : public MMessage
{ 
public:
	enum AttributeMessage {
		kConnectionMade			= 0x01,
		kConnectionBroken		= 0x02,
		kAttributeEval			= 0x04,
		kAttributeSet			= 0x08,
		kAttributeLocked		= 0x10,
		kAttributeUnlocked 		= 0x20,
		kAttributeAdded			= 0x40,
		kAttributeRemoved		= 0x80,
		kAttributeRenamed		= 0x100,
		kAttributeKeyable		= 0x200,
		kAttributeUnkeyable		= 0x400,
		kIncomingDirection		= 0x800,
		kAttributeArrayAdded	= 0x1000,
		kAttributeArrayRemoved	= 0x2000,
		kOtherPlugSet			= 0x4000
	};

public:
	static MCallbackId	addAttributeChangedCallback(
								MObject& node,
								void (*func)( AttributeMessage msg,
											  MPlug & plug,
											  MPlug & otherPlug,
											  void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );
								
	static MCallbackId	addAttributeAddedOrRemovedCallback(
								MObject& node,
								void (*func)( AttributeMessage msg,
											  MPlug & plug,
											  void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );
	
	static MCallbackId	addNodeDirtyCallback(
								MObject& node,
								void (*func)( void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );
	
	static MCallbackId	addNameChangedCallback(
								MObject& node,
								void (*func)( MObject & node,
											  void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MCallbackId	addNodeAboutToDeleteCallback(
								MObject& node,
								void (*func)( MDGModifier& modifier,
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
#endif /* _MNodeMessage */
