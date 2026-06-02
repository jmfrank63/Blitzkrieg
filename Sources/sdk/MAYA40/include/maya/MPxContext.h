#ifndef LINUX
#pragma once
#endif
#ifndef _MPxContext
#define _MPxContext

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MEvent.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>
#include <maya/MObject.h>



class MString;
class MDoubleArray;
class MIntArray;
class MPxToolCommand;
class MCursor;



/**
  The base class for user defined contexts
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxContext  
{
public:
	enum ImageIndex {
		kImage1 = 0,
		kImage2,
		kImage3
	};

	MPxContext ();
	virtual	~MPxContext ();	

	virtual void        toolOnSetup( MEvent & event );
	virtual void        toolOffCleanup();

	virtual MStatus		doPress ( MEvent & event );
	virtual MStatus		doRelease ( MEvent & event );
	virtual MStatus		doDrag ( MEvent & event );
	virtual MStatus		doHold ( MEvent & event );
	virtual MStatus 	doEnterRegion ( MEvent & event );
	virtual void		deleteAction();
	virtual void		completeAction();
	virtual MStatus     addManipulator( const MObject & manipulator );
	virtual MStatus     deleteManipulators(); 
	MStatus				setImage( const MString & image, ImageIndex index );
	MStatus				getImage( MString & image, ImageIndex index ) const;

protected:
	MStatus			    setHelpString( const MString & str );
	MStatus			    setTitleString( const MString & str );

	MStatus				setCursor( const MCursor & cursor );

	MStatus		        beginMarquee( MEvent & event );
	MStatus             dragMarquee( MEvent & event );
	MStatus             releaseMarquee( MEvent & event,
                                        short& top, short& left,
                                        short& bottom, short& right );

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

protected:
	static bool 		ignoreEntry( const MIntArray &flags, 
									 unsigned entry );

private:



	friend class MPxSelectionContext;
	friend class MToolsInfo;
	void setData( void * ptr );
	void * data;
	MString title;
	MString help;
	MString fImage;
	unsigned fImageIndex;
	MCursor * cursor;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxContext */
