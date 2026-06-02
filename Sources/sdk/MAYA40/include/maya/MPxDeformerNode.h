#ifndef LINUX
#pragma once
#endif
#ifndef _MPxDeformerNode
#define _MPxDeformerNode

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>






class MItGeometry;
class MDagModifier;

/**
  Create user defined Deformers.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MPxDeformerNode : public MPxNode  
{
public:
	MPxDeformerNode();

	virtual ~MPxDeformerNode();

	virtual MPxNode::Type type() const;



    virtual MStatus        deform(MDataBlock& block,
								  MItGeometry& iter,
								  const MMatrix& mat,
								  unsigned int multiIndex);

	virtual MObject&    	accessoryAttribute() const;

	virtual MStatus			accessoryNodeSetup(MDagModifier& cmd);

	float						weightValue( MDataBlock& mblock,
											 unsigned int multiIndex,
											 unsigned int wtIndex);
	
	static MObject input;
		static MObject inputGeom;
		static MObject groupId;
	static MObject outputGeom;
	static MObject weightList;
		static MObject weights;
	static MObject envelope;
protected:
	  
private:
	static void				initialSetup();
	static const char*	    className();



};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxNode */
