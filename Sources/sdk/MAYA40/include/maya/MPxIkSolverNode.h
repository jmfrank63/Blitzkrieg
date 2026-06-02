#ifndef LINUX
#pragma once
#endif
#ifndef _MPxIkSolverNode
#define _MPxIkSolverNode

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>



class MString;
class MArgList;
class MIkHandleGroup;
class MMatrix;
class MDoubleArray;



/**

Derive from this class to create user-defined IK solvers.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MPxIkSolverNode : public MPxNode
{
public:
	virtual ~MPxIkSolverNode();
	virtual MPxNode::Type type() const;

	virtual MStatus		preSolve();
	virtual MStatus		doSolve();
	virtual MStatus		postSolve( MStatus );

	virtual MString		solverTypeName() const;

	bool				rotatePlane(MStatus *ReturnStatus = NULL) const;
	MStatus				setRotatePlane(bool rotatePlane);
	bool				singleChainOnly(MStatus *ReturnStatus = NULL) const;
	MStatus				setSingleChainOnly(bool singleChainOnly);
	bool				positionOnly(MStatus *ReturnStatus = NULL) const;
	MStatus				setPositionOnly(bool positionOnly);
	bool				supportJointLimits(MStatus *ReturnStatus = NULL) const;
	MStatus				setSupportJointLimits(bool supportJointLimits);
	bool				uniqueSolution(MStatus *ReturnStatus = NULL) const;
	MStatus				setUniqueSolution(bool uniqueSolution); 

	virtual bool		isSingleChainOnly() const;
	virtual bool		isPositionOnly() const;
	virtual bool		hasJointLimitSupport() const;
	virtual bool		hasUniqueSolution() const;
	virtual bool		groupHandlesByTopology() const;

	virtual MStatus		setFuncValueTolerance( double tolerance );
	virtual MStatus		setMaxIterations( int value );

	MIkHandleGroup * 	handleGroup() const;
	virtual void 		setHandleGroup( MIkHandleGroup* );
	const MMatrix *		toWorldSpace() const;
	const MMatrix *		toSolverSpace() const;
	double 				funcValueTolerance() const;
	int 				maxIterations() const;
	virtual void 		snapHandle( MObject& handle );

	void				create();

protected:

	MStatus				getJointAngles( MDoubleArray& ) const;
	MStatus				setJointAngles( const MDoubleArray& );
	void				setToRestAngles();

	MPxIkSolverNode();

	static const char*	className();

private:


	void*	instance;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxIkSolverNode */
