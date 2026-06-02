#ifndef LINUX
#pragma once
#endif
#ifndef _MPxSelectionContext
#define _MPxSelectionContext

#if defined __cplusplus



#include <maya/MPxContext.h>
#include <maya/MPoint.h>



class MString;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxSelectionContext : public MPxContext
{
public:
	MPxSelectionContext ();
	virtual	~MPxSelectionContext ();	

	virtual MStatus		doPress ( MEvent & event );
	virtual MStatus		doRelease ( MEvent & event );
	virtual MStatus		doDrag ( MEvent & event );
	virtual MStatus		doHold ( MEvent & event );
	virtual MStatus     addManipulator( const MObject & manipulator );
	virtual MStatus     deleteManipulators(); 
	MStatus				setImage( const MString & image, ImageIndex index );
	MStatus				getImage( MString & image, ImageIndex index ) const;

protected:

	bool        isSelecting();

	MPoint 		startPoint();		// point where dragging started
	MPoint 		lastDragPoint();	// preview drag point (both in WS)


	virtual MPxToolCommand *	newToolCommand();

	virtual const char*	className() const;

public:
	virtual void		abortAction();
	virtual bool		processNumericalInput( const MDoubleArray &values,
											   const MIntArray &flags,
											   bool isAbsolute );
	virtual bool		feedbackNumericalInput() const;
	virtual MSyntax::MArgType	argTypeNumericalInput( unsigned index ) const;
	virtual	void		getClassName( MString & name ) const;

private:



};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxSelectionContext */
