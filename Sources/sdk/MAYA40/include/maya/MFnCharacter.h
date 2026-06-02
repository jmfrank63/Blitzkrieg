#ifndef LINUX
#pragma once
#endif
#ifndef _MFnCharacter
#define _MFnCharacter

#if defined __cplusplus



#include <maya/MString.h>
#include <maya/MFnSet.h>
#include <maya/MObject.h>


 
class MObjectArray;
class MSelectionList;
class MDagPath;
class TsetCmd;
class Tstring;
class MDGModifier;



/**
    Function set for characters
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnCharacter : public MFnSet
{
	declareMFn(MFnCharacter, MFnSet);

public:
	MStatus		attachSourceToCharacter( MObject& sourceClip,
										 MDGModifier& dgMod);
	MStatus		attachInstanceToCharacter( MObject& instanceClip,
										   MDGModifier& dgMod);
	MStatus		addCurveToClip( MObject& curve,
								MObject& sourceClip,
								MPlug& characterPlug,
								MDGModifier& dgMod);
	MObject		createBlend( MObject& instancedClip1,
							 MObject& instancedClip2,
							 MObject& blendAnimCurve,
							 MDGModifier& dgMod,
							 MStatus *ReturnStatus = NULL);
	bool		blendExists(MObject& instancedClip1,
							MObject& instancedClip2,
							MObject& blendResult);
	bool		removeBlend(MObject& instancedClip1,
							MObject& instancedClip2,
							MDGModifier& dgMod,							
							MStatus* ReturnStatus = NULL);

	bool	getCharacterThatOwnsPlug(MPlug& plug, MObject& result) const;
	MStatus getMemberPlugs(MPlugArray& result) const;
	MStatus getSubCharacters(MSelectionList& result) const;
	MObject getClipScheduler(MStatus * ReturnStatus = NULL);
	int 	getScheduledClipCount(MStatus * ReturnStatus = NULL);
	MObject	getScheduledClip(int index, MStatus * ReturnStatus = NULL);
	int 	getSourceClipCount(MStatus * ReturnStatus = NULL);
	MObject	getSourceClip(int index, MStatus * ReturnStatus = NULL);
    int		getBlendCount(MStatus * ReturnStatus = NULL);
    MObject getBlend(int index, MStatus * ReturnStatus = NULL);
    MStatus getBlendClips(int index, MObject& clip1, MObject& clip2);
	
protected:
	virtual		Tstring setCommandString();
	virtual		TsetCmd* setCommand();	
private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnCharacter */
