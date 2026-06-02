#ifndef LINUX
#pragma once
#endif
#ifndef _MIkHandleGroup
#define _MIkHandleGroup

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MObject.h>





/**
  Ik handle group class.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MIkHandleGroup  
{
public:
					MIkHandleGroup();
					~MIkHandleGroup();

	int			priority(MStatus *ReturnStatus = NULL) const;
	int			solverID(MStatus *ReturnStatus = NULL) const;
	int			solverPriority(MStatus *ReturnStatus = NULL) const;
	MStatus			setPriority( int );
	MStatus			setSolverID( int );
	bool			checkEffectorAtGoal(MStatus *ReturnStatus = NULL);
	MStatus			solve();


	int 			dofCount(MStatus *ReturnStatus = NULL) const;
	int				handleCount(MStatus *ReturnStatus = NULL) const;
	MObject			handle( int ith, MStatus *ReturnStatus = NULL );



protected:

private:
	friend class MPxIkSolver;
	friend class MPxIkSolverNode;
	static const char* className();
	MIkHandleGroup( void * );
 	void * f_data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MIkHandleGroup */
