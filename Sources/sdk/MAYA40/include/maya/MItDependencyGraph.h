#ifndef LINUX
#pragma once
#endif
#ifndef _MItDependencyGraph
#define _MItDependencyGraph

#if defined __cplusplus



#include <maya/MFn.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>
#include <maya/MTypes.h>





/**

  Iterate over Dependency Graph (DG) Nodes or Plugs starting at a specified
  root Node or Plug.

  Set and query the root of the iteration.

  Set and query the direction (downstream or upstream), traversal priority
  (depth first or breadth first) and level of detail (Node level or Plug
  level) of the iteration.

  Set and disable a filter to iterate over only speicifc types (MFn::Type) of
  Nodes.

  Reset the root, filter, direction, traversal priority and level of detail
  of the iteration.

  Prune branches of the graph from iteration.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItDependencyGraph  
{
public:
	            enum	Direction
				{
					kDownstream,
					kUpstream
				};

	            enum	Traversal
				{
					kDepthFirst,
					kBreadthFirst
				};

                enum	Level
				{
					kNodeLevel,
					kPlugLevel
				};
				MItDependencyGraph ( MObject& rootNode,
									 MFn::Type filter = MFn::kInvalid,
									 Direction direction = kDownstream,
									 Traversal traversal = kDepthFirst,
									 Level level = kNodeLevel,
									 MStatus* ReturnStatus = NULL );
				MItDependencyGraph ( MPlug& rootPlug,
									 MFn::Type filter = MFn::kInvalid,
									 Direction direction = kDownstream,
									 Traversal traversal = kDepthFirst,
									 Level level = kPlugLevel,
									 MStatus* ReturnStatus = NULL );
	            ~MItDependencyGraph ();
	MStatus		reset();
	MStatus		resetTo( MObject& rootNode,
						 MFn::Type filter = MFn::kInvalid,
						 Direction direction = kDownstream,
						 Traversal traversal = kDepthFirst,
						 Level level = kNodeLevel );
	MStatus		resetTo( MPlug& rootPlug,
						 MFn::Type filter = MFn::kInvalid,
						 Direction direction = kDownstream,
						 Traversal traversal = kDepthFirst,
						 Level level = kPlugLevel );
	MObject		rootNode( MStatus* ReturnStatus = NULL );
	MPlug		rootPlug( MStatus* ReturnStatus = NULL );
	MFn::Type	currentFilter( MStatus* ReturnStatus = NULL );
	MStatus		setCurrentFilter( MFn::Type filter = MFn::kInvalid );
	MStatus		resetFilter();
	bool		isPruningOnFilter( MStatus* ReturnStatus = NULL );
	MStatus		enablePruningOnFilter();
	MStatus		disablePruningOnFilter();
	bool		isDirectionDownStream(  MStatus* ReturnStatus = NULL );
	Direction	currentDirection(   MStatus* ReturnStatus = NULL );
	MStatus		toggleDirection( );
	bool		isTraversalDepthFirst(  MStatus* ReturnStatus = NULL );
	Traversal	currentTraversal(   MStatus* ReturnStatus = NULL );
	MStatus		toggleTraversal( );
	bool		atNodeLevel( MStatus* ReturnStatus = NULL );
	Level		currentLevel(   MStatus* ReturnStatus = NULL );
	MStatus		toggleLevel( );
	MStatus		next( );
	bool		isDone( MStatus* ReturnStatus = NULL );
	MStatus		prune( );
	MObject		thisNode( MStatus* ReturnStatus = NULL );
	bool		thisNodeHasUnknownType( MStatus* ReturnStatus = NULL );
	MPlug		thisPlug( MStatus* ReturnStatus = NULL );
	MPlug		previousPlug( MStatus* ReturnStatus = NULL );
	MStatus		getNodesVisited( MObjectArray& nodesVisted ) const;
	MStatus		getPlugsVisited( MPlugArray& plugsVisted ) const;
	MStatus		getNodePath( MObjectArray& path ) const;
	MStatus		getPlugPath( MPlugArray& path ) const;

protected:

private:

	static const char*		className();
	void *					currentIterator;
	MFn::Type				currentTypeFilter;
	bool					pruneOnNoMatch;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItDependencyGraph */
