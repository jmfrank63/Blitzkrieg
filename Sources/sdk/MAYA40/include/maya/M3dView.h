#ifndef LINUX
#pragma once
#endif
#ifndef _M3dView
#define _M3dView

#if defined __cplusplus


#include <maya/MStatus.h>
#include <maya/MObject.h>

#ifndef _WIN32
#include <GL/glx.h>
#include <X11/Intrinsic.h>
typedef  Widget MWindow;
#else
#include "windows.h"
#include <gl/Gl.h>
typedef  HWND MWindow;
#endif // _WIN32



class MString;
class MDagPath;
class MPoint;
class MVector;
class MMatrix;
class MColor;
class MPxGlBuffer;



/**

3-D view class.

*/

#ifndef _WIN32
typedef Window M3dWindow;
#else
typedef HWND M3dWindow;
#endif // _WIN32

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT M3dView {

public:

    enum DisplayStyle {
        kBoundingBox,
        kFlatShaded,
        kGouraudShaded,
        kWireFrame,
        kPoints
    }; 


    enum DisplayStatus {
        kActive,
        kLive,
        kDormant,
        kInvisible,
        kHilite,
        kTemplate,
        kActiveTemplate, 
        kActiveComponent,
		kLead,
		kIntermediateObject,
		kActiveAffected,
        kNoStatus
    }; 

    enum ColorTable {
        kActiveColors = kActive, 
        kDormantColors = kDormant, 
        kTemplateColor = kTemplate,
        kBackgroundColor
    };  

    enum TextPosition {
        kLeft, 
        kCenter, 
        kRight
    };
        
    M3dView();
    virtual ~M3dView();

    static M3dView		active3dView( MStatus * ReturnStatus = NULL );
	static unsigned     numberOf3dViews();
	static MStatus      get3dView( const unsigned index, 
							                M3dView & view );

#ifndef _WIN32
    Display *			display( MStatus * ReturnStatus = NULL );
	GLXContext	        glxContext( MStatus * ReturnStatus = NULL );
#else
	HGLRC  				display( MStatus * ReturnStatus = NULL );
	HDC     			deviceContext( MStatus * ReturnStatus = NULL );
#endif

	static MWindow		applicationShell( MStatus * ReturnStatus = NULL );

	M3dWindow  	window( MStatus * ReturnStatus = NULL );
    int         portWidth( MStatus * ReturnStatus = NULL );
    int         portHeight( MStatus * ReturnStatus = NULL );


    MStatus     beginGL();
    MStatus     endGL();

    void            beginSelect (GLuint *buffer = NULL, GLsizei size = 0);
    GLint           endSelect   ();
    bool            selectMode  () const;
    void            loadName    (GLuint name);
    void            pushName    (GLuint name);
    void            popName     ();
    void            initNames   ();



    MStatus     beginOverlayDrawing();  
    MStatus     endOverlayDrawing();
    MStatus     clearOverlayPlane();    


    MStatus     setDrawColor( unsigned index,
							  ColorTable table = kActiveColors );
    MStatus     setDrawColor( const MColor & color );

    bool        isColorIndexMode( MStatus * ReturnStatus = NULL ); 
    unsigned    numDormantColors( MStatus * ReturnStatus = NULL );
    unsigned    numActiveColors( MStatus * ReturnStatus = NULL );
    unsigned    numUserDefinedColors( MStatus * ReturnStatus = NULL );
    
    MStatus     setUserDefinedColor( unsigned index, const MColor & color );
    unsigned    userDefinedColorIndex( unsigned index, 
                                       MStatus * ReturnStatus = NULL );
  
    MColor      templateColor( MStatus * ReturnStatus = NULL ); 
    MColor      backgroundColor( MStatus * ReturnStatus = NULL );

    MColor      colorAtIndex( unsigned index, ColorTable table = kActiveColors,
                              MStatus * ReturnStatus = NULL ); 
	MStatus		getColorIndexAndTable( unsigned glindex, unsigned &index, 
									   ColorTable &table ) const;


    MStatus     drawText( const MString & text, const MPoint position,
                          TextPosition textPosition = kLeft );


    MStatus     getCamera( MDagPath & camera );
    MStatus     setCamera( MDagPath & camera );

    MStatus     refresh( bool all = false, bool force = false );
    MStatus     refresh( MPxGlBuffer &buffer );


    MStatus     viewToWorld( short x_pos, short y_pos,
                             MPoint & worldPt, MVector & worldVector ) const;
    MStatus     viewToWorld( short x_pos, short y_pos,
                             MPoint & nearClipPt, MPoint & farClipPt ) const;
    MStatus     viewToObjectSpace( short x_pos, short y_pos,
                                   const MMatrix & localMatrixInverse,
                                   MPoint & oPt, MVector & oVector ) const;
    bool        worldToView( const MPoint& worldPt, 
                             short& x_pos, short& y_pos,
                             MStatus * ReturnStatus = NULL ) const;


    DisplayStyle    displayStyle( MStatus * ReturnStatus = NULL ) const;
    bool        isShadeActiveOnly( MStatus * ReturnStatus = NULL ) const;
    MStatus     setDisplayStyle ( DisplayStyle style, bool activeOnly = false);
    
protected:

private:


    static const char* className();
    M3dView( const void * );
    const void * fPtr;
};


#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _M3dView */
