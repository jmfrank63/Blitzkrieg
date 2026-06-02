#ifndef LINUX
#pragma once
#endif
#ifndef _MPxSpringNode
#define _MPxSpringNode

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>
#include <maya/MDoubleArray.h>






/**
*/



#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MPxSpringNode : public MPxNode
{
public:
	MPxSpringNode();
	virtual ~MPxSpringNode();

	virtual MPxNode::Type	type() const;

	virtual MStatus	applySpringLaw( double stiffness,
						double damping, double restLength,
						double endMass1, double endMass2,
						const MVector &endP1, const MVector &endP2,
						const MVector &endV1, const MVector &endV2,
						MVector &forceV1, MVector &forceV2 );



	static MObject	mEnd1Weight;
	static MObject	mEnd2Weight;

	static MObject	mDeltaTime;

protected:

private:

	static void			initialSetup();
	static const char*	className();




};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MPxSpringNode */

