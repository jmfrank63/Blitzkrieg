#ifndef LINUX
#pragma once
#endif
#ifndef _MFnLatticeDeformer
#define _MFnLatticeDeformer

#if defined __cplusplus



#include <maya/MFnDependencyNode.h>


class MDagPath;
class MObjectArray;



/**
 Function set for FFD lattice deformer
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MFnLatticeDeformer : public MFnDependencyNode
{

	declareMFn(MFnLatticeDeformer, MFnDependencyNode );

public:

	MObject create( unsigned xDiv, unsigned yDiv, unsigned zDiv,
					MStatus * ReturnStatus = NULL );

	MStatus addGeometry( const MObject & object );
	MStatus removeGeometry( const MObject & object );

	MStatus getAffectedGeometry( MObjectArray & objects );
	
	MStatus getDivisions( unsigned & x, unsigned & y, unsigned & z );
	MStatus setDivisions( unsigned   x, unsigned   y, unsigned   z );

	MStatus resetLattice( bool centerLattice = false );

	MObject deformLattice( MStatus * ReturnStatus = NULL );
	MObject baseLattice( MStatus * ReturnStatus = NULL );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnLatticeDeformer */
