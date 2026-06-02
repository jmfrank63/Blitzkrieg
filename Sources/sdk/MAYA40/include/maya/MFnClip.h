#ifndef LINUX
#pragma once
#endif
#ifndef _MFnClip
#define _MFnClip

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>

class MObject;
class MTime;
class MDGModifier;



/**
MFnClip is the function set for creating, querying and
editing animation clips.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnClip : public MFnDependencyNode
{
	declareMFn(MFnClip, MFnDependencyNode);

public:
	MObject		createSourceClip ( const MTime& sourceStart,
								   const MTime& sourceDuration,
								   MDGModifier& dgMod,
								   MStatus * ReturnStatus = NULL );
	MObject		createInstancedClip ( MObject& sourceClip,
									  const MTime& start,
									  MDGModifier& dgMod,				  
									  MStatus * ReturnStatus = NULL,
									  bool absolute = 1,
									  double cycle = 1.0,
									  double weight = 1.0,
									  double scale = 1.0 );
	bool 		isInstancedClip(MStatus *ReturnStatus = NULL);
	bool		isPose(MStatus *ReturnStatus = NULL);
	MObject 	sourceClip(MStatus *ReturnStatus = NULL);
	double		getCycle(MStatus *ReturnStatus = NULL);
	double		getWeight(MStatus *ReturnStatus = NULL);	
	double 		getScale(MStatus *ReturnStatus = NULL);
	bool 		getAbsolute(MStatus *ReturnStatus = NULL);
	bool 		getEnabled(MStatus *ReturnStatus = NULL);
	MTime		getStartFrame(MStatus *ReturnStatus = NULL);
	MTime		getSourceStart(MStatus *ReturnStatus = NULL);
	MTime		getSourceDuration(MStatus *ReturnStatus = NULL);
	MStatus		getMemberAnimCurves(MObjectArray& curves,
									MPlugArray& associatedAttrs);
	MStatus		setPoseClip(bool state, MDGModifier* mod = NULL);
	MStatus		setCycle(double cycle, MDGModifier* mod = NULL);
	MStatus		setWeight(double wt, MDGModifier* mod = NULL);
	MStatus		setScale(double scale, MDGModifier* mod = NULL);
	MStatus		setAbsolute(bool abs, MDGModifier* mod = NULL);
	MStatus		setEnabled(bool val, MDGModifier* mod = NULL);
	MStatus		setStartFrame(const MTime& start, MDGModifier* mod = NULL);
	MStatus		setSourceData(const MTime& start,const MTime& duration,
							  MDGModifier* mod = NULL);
	
protected:

private:
	
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnClip */
