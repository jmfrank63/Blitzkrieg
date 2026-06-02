#ifndef LINUX
#pragma once
#endif
#ifndef _MPxHwShaderNode
#define _MPxHwShaderNode

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MDrawRequest.h>
#include <maya/M3dView.h>







/**
  Create user defined hardware shaders.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxHwShaderNode : public MPxNode  
{
public:
	enum Writeable {
		kWriteNone				= 0x0000,
		kWriteVertexArray		= 0x0001,
		kWriteNormalArray		= 0x0002,
		kWriteColorArrays		= 0x0004,
		kWriteTexCoordArrays	= 0x0008,
		kWriteAll				= 0x000f
	};

	MPxHwShaderNode();

	virtual ~MPxHwShaderNode();

	virtual MPxNode::Type type() const;



	virtual MStatus		bind( const MDrawRequest& request,
							  M3dView& view );

	virtual MStatus		unbind( const MDrawRequest& request,
								M3dView& view );

	virtual MStatus		geometry( const MDrawRequest& request,
								  M3dView& view,
								  int prim,
								  unsigned int writable,
								  int indexCount,
								  const unsigned int * indexArray,
								  int vertexCount,
								  const float * vertexArray,
								  int normalCount,
								  const float ** normalArrays,
								  int colorCount,
								  const float ** colorArrays,
								  int texCoordCount,
								  const float ** texCoordArrays);

	virtual	int		normalsPerVertex();

	virtual int		colorsPerVertex();

	virtual int		texCoordsPerVertex();

	virtual int		getTexCoordSetNames(MStringArray& names);

	virtual bool	hasTransparency();

	static MObject outColor;
	static MObject outColorR;
	static MObject outColorG;
	static MObject outColorB;

	static MObject outTransparency;
	static MObject outTransparencyR;
	static MObject outTransparencyG;
	static MObject outTransparencyB;

	static MObject outMatteOpacity;
	static MObject outMatteOpacityR;
	static MObject outMatteOpacityG;
	static MObject outMatteOpacityB;

	static MObject outGlowColor;
	static MObject outGlowColorR;
	static MObject outGlowColorG;
	static MObject outGlowColorB;


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
