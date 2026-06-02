#ifndef LINUX
#pragma once
#endif
#ifndef _MTesselationParams
#define _MTesselationParams
#if defined __cplusplus

#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MMatrix;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MTesselationParams  
{
public:
	
	enum TessFormat {
		kTriangleCountFormat,
		kStandardFitFormat,
		kGeneralFormat
	};

	enum PolyType {
		kTriangles,
		kQuads
	};
	
	enum IsoparmType {
		kSurface3DDistance,
		kSurface3DEquiSpaced,
		kSurfaceEquiSpaced,
		kSpanEquiSpaced
	};
	
	enum SubdivisionType {
		kUseFractionalTolerance,
		kUseChordHeightRatio,
		kUseMinEdgeLength,
		kUseMaxEdgeLength,
		kUseMaxNumberPolys,
		kUseMaxSubdivisionLevel,
		kUseMinScreenSize,
		kUseMaxUVRectangleSize,
		kUseTriangleEdgeSwapping,
		kUseRelativeTolerance,
		kUseEdgeSmooth,
		kLastFlag
	};


	MTesselationParams ( TessFormat format = kStandardFitFormat,
						PolyType = kTriangles );
	MTesselationParams ( const MTesselationParams & );
	~MTesselationParams ();


	void		setFormatType( TessFormat type );
	void		setOutputType( PolyType type );


	void		setTriangleCount( int );


	void		setStdChordHeightRatio( double );
	void		setStdFractionalTolerance( double );
	void		setStdMinEdgeLength( double );


	void		setSubdivisionFlag( SubdivisionType type, bool use );
	void		setFitTolerance( double );
	void		setChordHeightRatio( double );
	void		setMinEdgeLength( double );
	void		setMaxEdgeLength( double );
	void		setMaxNumberPolys( int );
	void		setMaxSubdivisionLevel( double );
	void		setMinScreenSize( double, double );
	void		setWorldspaceToScreenTransform( MMatrix & );
	void		setMaxUVRectangleSize( double, double );
	void		setRelativeFitTolerance( double );
	void		setEdgeSmoothFactor( double );
	void		set3DDelta( double );


	void		setUIsoparmType( IsoparmType type );
	void		setVIsoparmType( IsoparmType type );
	void		setUNumber( int count );
	void		setVNumber( int count );
	void		setBoundingBoxDiagonal( double distance );
	void		setUDistanceFraction( double value );
	void		setVDistanceFraction( double value );

	MTesselationParams &operator= ( const MTesselationParams &rhs);		

	
	static	MTesselationParams 	fsDefaultTesselationParams;

protected:

private:
	friend class MFnNurbsSurface;
	const char*		 className() const;
	void * data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MTesselationParams */
