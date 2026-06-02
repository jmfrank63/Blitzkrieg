#ifndef LINUX
#pragma once
#endif
#ifndef _MDrawRequest
#define _MDrawRequest
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/M3dView.h>



class MSelectionMask;
class MPoint;
class MPointArray;
class MVector;
class MSelectionList;
class MMatrix;
class MPxSurfaceShapeUI;
class MMaterial;
class MDrawData;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDrawRequest
{
public:
	MDrawRequest();
	MDrawRequest( const MDrawRequest& in );
	~MDrawRequest();

public:

	M3dView					view() const;
	void					setView( M3dView & );
	const MDagPath			multiPath() const;
	void					setMultiPath( const MDagPath & );
	MObject 				component() const;
	void					setComponent( MObject & );
	MDrawData 				drawData() const;
	void					setDrawData( MDrawData & );
	M3dView::DisplayStatus	displayStatus() const;
	void					setDisplayStatus( M3dView::DisplayStatus );
	bool					displayCulling() const;
	void					setDisplayCulling( bool );
	bool					displayCullOpposite() const;
	void					setDisplayCullOpposite( bool );
	M3dView::DisplayStyle	displayStyle() const;
	void					setDisplayStyle( M3dView::DisplayStyle );
	int						color( M3dView::ColorTable table ) const;
	void					setColor( int, M3dView::ColorTable table );
	MMaterial 				material() const;
	void					setMaterial( MMaterial& );
	bool					isTransparent() const;
	void					setIsTransparent( bool );
	bool					drawLast() const;
	void					setDrawLast( bool );
	int						token() const;
	void					setToken( int );

	MDrawRequest&	operator = ( const MDrawRequest& other );

protected:

private:
	const char*	 className() const;


    MDrawRequest( void* in, bool own );
	void*	fDrawRequest;
	bool    fOwn;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDrawRequest */
