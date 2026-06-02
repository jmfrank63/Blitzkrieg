#ifndef LINUX
#pragma once
#endif
#ifndef _MPxIkSolver
#define _MPxIkSolver

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>



class MString;
class MArgList;
class MIkHandleGroup;
class MMatrix;
class MDoubleArray;



/**

MPxIkSolver is OBSOLETE, and will be removed in a future version of Maya.
The MPxIkSolver class has been replaced with the MPxIkSolverNode class.
This is an obsolete base class for writing user-defined IK solvers.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MPxIkSolver  
{
public:
	virtual ~MPxIkSolver();

	static void			registerSolver( const MString & solverName,
								MCreatorFunction creatorFunction );


	virtual MStatus		preSolve();
	virtual MStatus		doSolve();
	virtual MStatus		postSolve( MStatus );


	virtual MString		solverTypeName() const;
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

	MPxIkSolver();

	virtual const char*	className() const;

private:

	void*	instance;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxIkSolver */
