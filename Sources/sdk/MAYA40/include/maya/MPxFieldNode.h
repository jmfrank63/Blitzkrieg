#ifndef LINUX
#pragma once
#endif
#ifndef _MPxFieldNode
#define _MPxFieldNode

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

class OPENMAYAFX_EXPORT MPxFieldNode : public MPxNode
{
public:
	MPxFieldNode();
	virtual ~MPxFieldNode();
	virtual MPxNode::Type type() const;
	virtual MStatus		compute( const MPlug& plug, MDataBlock& dataBlock );
    virtual MStatus getForceAtPoint(const MVectorArray&  point,
                            const MVectorArray&  velocity,
                            const MDoubleArray&  mass,
                            MVectorArray&        force,
                            double deltaTime);


	static MObject	mMagnitude;
	static MObject	mAttenuation;
	static MObject	mMaxDistance;
	static MObject	mUseMaxDistance;
	static MObject	mApplyPerVertex;

	static MObject	mInputData;
	static MObject	mInputPositions;
	static MObject	mInputVelocities;
	static MObject	mInputMass;
	static MObject	mDeltaTime;

	static MObject	mInputForce;

	static MObject	mOutputForce;

	static MObject	mOwnerCentroidX;
	static MObject	mOwnerCentroidY;
	static MObject	mOwnerCentroidZ;
	static MObject	mOwnerCentroid;
	static MObject	mOwnerPosData;
	static MObject	mOwnerVelData;

	static MObject	mWorldMatrix;

protected:

private:

	static void			initialSetup();
	static const char*	className();





};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MPxFieldNode */
