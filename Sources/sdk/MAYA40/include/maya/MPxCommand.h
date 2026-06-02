#ifndef LINUX
#pragma once
#endif
#ifndef _MPxCommand
#define _MPxCommand

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MString.h>



class MFileObject;
class MArgList;
class MIntArray;
class MDoubleArray;
class MStringArray;
class MSyntax;



/**
  Base class for creating user defined Maya commands.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxCommand  
{
public:
						MPxCommand();
	virtual 			~MPxCommand();
	virtual MStatus   	doIt( const MArgList& args ) = 0;
	virtual MStatus   	undoIt( );
	virtual MStatus   	redoIt( );
	virtual bool		isUndoable() const;
	virtual bool		hasSyntax() const;
	MSyntax				syntax() const;
	bool                isHistoryOn() const;
	MString      		commandString() const;
	MStatus            	setHistoryOn( bool state );
	MStatus            	setCommandString( const MString & );

	static void			displayWarning( const MString & theWarning );
	static void			displayError( const MString & theError );

	enum MResultType {
		kLong,
		kDouble,
		kString,
		kNoArg
	};

	static void         clearResult();

	static void         setResult( int val );
	static void         setResult( double val );
	static void         setResult( bool val );
	static void         setResult( const char* val );
	static void         setResult( const MString& val );
	static void         setResult( const MIntArray& val );
	static void         setResult( const MDoubleArray& val );
	static void         setResult( const MStringArray& val );

	static void         appendToResult( int val );
	static void         appendToResult( double val );
	static void         appendToResult( bool val );
	static void         appendToResult( const char* val );
	static void         appendToResult( const MString& val );
	static void         appendToResult( const MStringArray& val );

	static MResultType  currentResultType();
	static MStatus      getCurrentResult( int& val );
	static MStatus      getCurrentResult( double& val );
	static MStatus      getCurrentResult( MString& val );
	static MStatus      getCurrentResult( MIntArray& val );
	static MStatus      getCurrentResult( MDoubleArray& val );
	static MStatus      getCurrentResult( MStringArray& val );

	MStatus            	setUndoable( bool state );

protected:
	static const char*	className();


	void setData( void * ptr );
	void*				instance;
private:
	MString command;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxCommand */
