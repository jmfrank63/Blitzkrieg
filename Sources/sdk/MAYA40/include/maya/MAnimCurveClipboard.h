#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimCurveClipboard
#define _MAnimCurveClipboard

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MAnimCurveClipboardItemArray.h>





/**
	This class provide access to the animation or API clipboard which are used
	to hold on to anim curves during cut/copy/paste operations.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimCurveClipboard  
{
public:
										MAnimCurveClipboard();
										~MAnimCurveClipboard();
	
	static MAnimCurveClipboard &		theAPIClipboard();
	MStatus		set( const MAnimCurveClipboard &cb );
	MStatus		set( const MAnimCurveClipboardItemArray &clipboardItemArray );
	MStatus		set( const MAnimCurveClipboardItemArray &clipboardItemArray,
					 const MTime &startTime, const MTime &endTime,
					 const float &startUnitlessInput,
					 const float &endUnitlessInput );
	MStatus		clear ();

	bool		isEmpty( MStatus * ReturnStatus = NULL ) const;
	const MAnimCurveClipboardItemArray 	clipboardItems( MStatus *
												ReturnStatus = NULL) const;
	MTime		startTime( MStatus * ReturnStatus = NULL ) const;
	MTime		endTime( MStatus * ReturnStatus = NULL ) const;
	float		startUnitlessInput( MStatus * ReturnStatus = NULL ) const;
	float		endUnitlessInput( MStatus * ReturnStatus = NULL ) const;


protected:

private:
	void *							fClipboard;

	static MAnimCurveClipboard		fsAPIClipboard;
	
	MAnimCurveClipboard& operator = (const MAnimCurveClipboard&) const;
	MAnimCurveClipboard& operator = (const MAnimCurveClipboard&);
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAnimCurveClipboard */
