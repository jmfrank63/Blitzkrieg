#ifndef LINUX
#pragma once
#endif
#ifndef _MMaterial
#define _MMaterial
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/M3dView.h>



class MDagPath;
class MVector;
class MDrawData;
class MPxHwShaderNode;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MMaterial  
{
public:
	MMaterial();
	MMaterial( const MMaterial& in );
	~MMaterial();

public:
	MStatus		evaluateMaterial(M3dView&, const MDagPath& );
	MStatus		evaluateShininess();
	MStatus		evaluateDiffuse();
	MStatus		evaluateEmission();
	MStatus		evaluateSpecular();
	MStatus		evaluateTexture( MDrawData & data );
	MStatus		evaluateTextureTransformation();

	bool		materialIsTextured() const;

	MStatus		setMaterial(const MDagPath&, bool hasTransparency);
	MStatus		getShininess( float & );
	MStatus		getDiffuse( MColor & );
	MStatus		getEmission( MColor & );
	MStatus		getSpecular( MColor & );
    MStatus		getHasTransparency( bool & );
	MStatus		getTextureTransformation(float& scaleU,
										 float& scaleV,
										 float& translateU,
										 float& translateV,
										 float& rotate);

	void		applyTexture( M3dView&, MDrawData& );

	MPxHwShaderNode *	getHwShaderNode( MStatus * ReturnStatus = NULL );

protected:

private:
	const char*	 className() const;



    MMaterial( void* in );

	void*	 fMaterial;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MMaterial */
