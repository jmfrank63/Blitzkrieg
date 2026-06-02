#ifndef LINUX
#pragma once
#endif
#ifndef _MPxEmitterNode
#define _MPxEmitterNode

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


class OPENMAYAFX_EXPORT MPxEmitterNode : public MPxNode
{
public:
	MPxEmitterNode();
	virtual ~MPxEmitterNode();
	virtual MPxNode::Type	type() const;
	virtual MStatus			compute(const MPlug& plug, MDataBlock& dataBlock);



	static MObject	mRate;
	static MObject	mSpeed;
	static MObject	mDirection;
	static MObject	mDirectionX;
	static MObject	mDirectionY;
	static MObject	mDirectionZ;

	static MObject	mOwnerPosData;
	static MObject	mOwnerVelData;
	static MObject	mOwnerCentroid;
	static MObject	mOwnerCentroidX;
	static MObject	mOwnerCentroidY;
	static MObject	mOwnerCentroidZ;

	static MObject	mSweptGeometry;

	static MObject	mWorldMatrix;

	static MObject	mStartTime;
	static MObject	mDeltaTime;
	static MObject	mIsFull;
	static MObject	mInheritFactor;
	static MObject	mSeed;

	static MObject	mCurrentTime;

	static MObject mOutput;

protected:

private:

	static void			initialSetup();
	static const char*	className();




};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxEmitterNode */

