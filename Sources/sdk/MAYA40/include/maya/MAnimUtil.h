#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimUtil
#define _MAnimUtil

#if defined __cplusplus



#include <maya/MObject.h>



class MPlug;
class MPlugArray;
class MObjectArray;
class MSelectionList;
class MDagPath;




class OPENMAYAANIM_EXPORT MAnimUtil  
{
public:
	static bool		isAnimated(	const MObject &node,
								bool checkParent = false,
								MStatus * ReturnStatus = NULL );
	static bool		isAnimated(	const MDagPath &path,
								bool checkParent = false,
								MStatus * ReturnStatus = NULL );
	static bool		isAnimated(	const MPlug &plug,
								bool checkParent = false,
								MStatus * ReturnStatus = NULL );
	static bool		isAnimated(	const MSelectionList &selectionList,
								bool checkParent = false,
								MStatus * ReturnStatus = NULL );
	static bool		findAnimatedPlugs(	const MObject &node,
										MPlugArray &animatedPlugs,
										bool checkParent = false,
										MStatus * ReturnStatus = NULL );
	static bool		findAnimatedPlugs(	const MDagPath &path,
										MPlugArray &animatedPlugs,
										bool checkParent = false,
										MStatus * ReturnStatus = NULL );
	static bool		findAnimatedPlugs(	const MSelectionList &selectionList,
										MPlugArray &animatedPlugs,
										bool checkParent = false,
										MStatus * ReturnStatus = NULL );
	static bool		findAnimation(	const MPlug &plug,
									MObjectArray &animation,
									MStatus * ReturnStatus = NULL );
protected:
	static const char* className();
private:
	~MAnimUtil();
#ifdef __GNUC__
	friend class shutUpAboutPrivateDestructors;
#endif
};

#endif /* __cplusplus */
#endif /* _MAnimUtil */
